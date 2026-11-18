#define WIN32_LEAN_AND_MEAN
#include "ab_host_menu.h"
enum {MENU_LIST=201,MENU_HELP=202};
typedef struct {ABMenuPoll poll;void*context;unsigned previous,stage_view,automated;ULONGLONG repeat;} Menu;
static void contents(HWND w,Menu*m){
    HWND list=GetDlgItem(w,MENU_LIST);SendMessageW(list,LB_RESETCONTENT,0,0);
    static const WCHAR*main_items[]={L"Resume game",L"Level select...",L"Back to Title (Soft Reset)",L"Beast select (restarts)",L"Quit"};
    static const WCHAR*stages[]={L"Stage 1",L"Stage 2",L"Stage 3",L"Stage 4",L"Stage 5",L"Back"};
    for(unsigned i=0;i<(m->stage_view?6u:5u);++i)SendMessageW(list,LB_ADDSTRING,0,(LPARAM)(m->stage_view?stages[i]:main_items[i]));
    SendMessageW(list,LB_SETCURSEL,0,0);SetWindowTextW(w,m->stage_view?L"Level select — starts a new game":L"Game menu — paused");
    SetDlgItemTextW(w,MENU_HELP,m->stage_view?L"This starts a new game at the selected stage.":L"Game paused. Choose an action below.");SetFocus(list);
}
static void cancel(HWND w,Menu*m){if(m->stage_view){m->stage_view=0;contents(w,m);}else EndDialog(w,AB_MENU_RESUME);}
static void choose(HWND w,Menu*m){
    LRESULT selected=SendDlgItemMessageW(w,MENU_LIST,LB_GETCURSEL,0,0);if(selected==LB_ERR)return;
    if(m->stage_view){if(selected==5){m->stage_view=0;contents(w,m);}else EndDialog(w,AB_MENU_STAGE_BASE+selected);return;}
    if(selected==1){m->stage_view=1;contents(w,m);}else EndDialog(w,selected==0?AB_MENU_RESUME:selected==2?AB_MENU_TITLE:selected==3?AB_MENU_BEAST:AB_MENU_QUIT);
}
static INT_PTR CALLBACK menu_proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
    Menu*m=(Menu*)GetWindowLongPtrW(w,DWLP_USER);
    if(msg==WM_INITDIALOG){
        m=(Menu*)lp;SetWindowLongPtrW(w,DWLP_USER,(LONG_PTR)m);SetWindowPos(w,NULL,0,0,570,355,SWP_NOMOVE|SWP_NOZORDER);
        HWND help=CreateWindowW(L"STATIC",L"",WS_CHILD|WS_VISIBLE,16,12,520,36,w,(HMENU)MENU_HELP,NULL,NULL);
        HWND list=CreateWindowExW(WS_EX_CLIENTEDGE,L"LISTBOX",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|LBS_NOTIFY|WS_VSCROLL,16,52,520,156,w,(HMENU)MENU_LIST,NULL,NULL);
        HWND footer=CreateWindowW(L"STATIC",L"D-pad: choose | Bottom face / Enter: select | Right face / Esc: back",WS_CHILD|WS_VISIBLE,16,216,520,32,w,NULL,NULL,NULL);
        HWND select=CreateWindowW(L"BUTTON",L"Select",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,300,264,110,30,w,(HMENU)IDOK,NULL,NULL);
        HWND back=CreateWindowW(L"BUTTON",L"Back / Resume",WS_CHILD|WS_VISIBLE|WS_TABSTOP,420,264,116,30,w,(HMENU)IDCANCEL,NULL,NULL);
        HWND controls[]={help,list,footer,select,back};for(unsigned i=0;i<5;++i)SendMessageW(controls[i],WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);
        contents(w,m);if(!m->poll(m->context,&m->previous)){EndDialog(w,AB_MENU_ERROR);return TRUE;}SetTimer(w,1,16,NULL);return FALSE;
    }
    if(msg==WM_TIMER){
        unsigned buttons;if(!m->poll(m->context,&buttons)){EndDialog(w,AB_MENU_ERROR);return TRUE;}
        unsigned edge=buttons&~m->previous;m->previous=buttons;if(!m->automated&&GetForegroundWindow()!=w)return TRUE;
        if(edge&(AB_MENU_CANCEL|AB_MENU_TOGGLE)){cancel(w,m);return TRUE;}if(edge&AB_MENU_CONFIRM){choose(w,m);return TRUE;}
        unsigned directions=buttons&(AB_MENU_UP|AB_MENU_DOWN);ULONGLONG now=GetTickCount64();
        if((edge&directions)||(directions&&now>=m->repeat)){
            HWND list=GetDlgItem(w,MENU_LIST);int count=(int)SendMessageW(list,LB_GETCOUNT,0,0),selected=(int)SendMessageW(list,LB_GETCURSEL,0,0);
            if(directions==AB_MENU_UP)selected=(selected+count-1)%count;else if(directions==AB_MENU_DOWN)selected=(selected+1)%count;
            SendMessageW(list,LB_SETCURSEL,selected,0);m->repeat=now+((edge&directions)?400:100);
        }return TRUE;
    }
    if(msg==WM_COMMAND){if(LOWORD(wp)==IDOK||(LOWORD(wp)==MENU_LIST&&HIWORD(wp)==LBN_DBLCLK)){choose(w,m);return TRUE;}if(LOWORD(wp)==IDCANCEL){cancel(w,m);return TRUE;}}
    if(msg==WM_CLOSE){cancel(w,m);return TRUE;}return FALSE;
}
unsigned ab_host_menu_run(HWND owner,ABMenuPoll poll,void*context,unsigned automated){
    struct {DLGTEMPLATE dialog;WORD menu,kind,title;} layout={0};layout.dialog.style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME;layout.dialog.cx=285;layout.dialog.cy=178;
    Menu m={0};m.poll=poll;m.context=context;m.automated=automated;INT_PTR result=DialogBoxIndirectParamW(GetModuleHandleW(NULL),&layout.dialog,owner,menu_proc,(LPARAM)&m);return result<0?AB_MENU_ERROR:(unsigned)result;
}
