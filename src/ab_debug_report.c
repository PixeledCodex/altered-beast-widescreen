#define WIN32_LEAN_AND_MEAN
#include "ab_debug_report.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <bcrypt.h>
static int executable_hash(const WCHAR*path,char hex[65]){
    FILE*f=_wfopen(path,L"rb");if(!f)return 0;
    BCRYPT_ALG_HANDLE algorithm=NULL;BCRYPT_HASH_HANDLE hash=NULL;unsigned char digest[32],buffer[8192];int ok=0;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,NULL,0)>=0&&BCryptCreateHash(algorithm,&hash,NULL,0,NULL,0,0)>=0){
        size_t n;ok=1;while((n=fread(buffer,1,sizeof buffer,f))!=0)if(BCryptHashData(hash,buffer,(ULONG)n,0)<0){ok=0;break;}
        if(ferror(f)||BCryptFinishHash(hash,digest,sizeof digest,0)<0)ok=0;
        if(ok){for(unsigned i=0;i<32;++i)sprintf(hex+i*2,"%02X",digest[i]);hex[64]=0;}
    }if(hash)BCryptDestroyHash(hash);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);fclose(f);return ok;
}
enum {EDIT_DESCRIPTION=101};
typedef struct {WCHAR text[4096];unsigned automated;} Description;
static INT_PTR CALLBACK description_proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
    Description*d=(Description*)GetWindowLongPtrW(w,DWLP_USER);
    if(msg==WM_INITDIALOG){
        d=(Description*)lp;SetWindowLongPtrW(w,DWLP_USER,(LONG_PTR)d);
        SetWindowTextW(w,L"Port diagnostic — describe the issue (game paused)");
        SetWindowPos(w,NULL,0,0,650,360,SWP_NOMOVE|SWP_NOZORDER);
        HWND label=CreateWindowW(L"STATIC",L"What looks wrong? Include the stage, objects and expected behaviour.",WS_CHILD|WS_VISIBLE,14,12,600,24,w,NULL,NULL,NULL);
        HWND edit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|ES_MULTILINE|ES_AUTOVSCROLL|ES_WANTRETURN,14,40,602,218,w,(HMENU)EDIT_DESCRIPTION,NULL,NULL);
        SendMessageW(edit,EM_SETLIMITTEXT,4095,0);
        HWND save=CreateWindowW(L"BUTTON",L"Save report",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,378,270,114,30,w,(HMENU)IDOK,NULL,NULL);
        HWND cancel=CreateWindowW(L"BUTTON",L"Cancel",WS_CHILD|WS_VISIBLE|WS_TABSTOP,502,270,114,30,w,(HMENU)IDCANCEL,NULL,NULL);
        HWND controls[]={label,edit,save,cancel};for(unsigned i=0;i<4;++i)SendMessageW(controls[i],WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);
        SetFocus(edit);
        if(d->automated){SetWindowTextW(edit,L"Automated diagnostic dialog: quotes \" and Unicode Ω; no human scene verification.");PostMessageW(w,WM_COMMAND,d->automated==2?IDCANCEL:IDOK,0);}
        return FALSE;
    }
    if(msg==WM_CLOSE){EndDialog(w,IDCANCEL);return TRUE;}
    if(msg==WM_COMMAND&&LOWORD(wp)==IDCANCEL){EndDialog(w,IDCANCEL);return TRUE;}
    if(msg==WM_COMMAND&&LOWORD(wp)==IDOK){GetDlgItemTextW(w,EDIT_DESCRIPTION,d->text,4096);EndDialog(w,IDOK);return TRUE;}
    return FALSE;
}
void ab_debug_record(ABDebugHistory*h,const ABGame*g,uint8_t p1,uint8_t p2){
    h->inputs[h->next]=(ABDebugInput){g->raster.presented,g->raster.sound.master_cycles,p1,p2};h->next=(h->next+1)%600;if(h->count<600)++h->count;
}
static FILE*output(const WCHAR*folder,const WCHAR*name){WCHAR path[32768];if(swprintf(path,32768,L"%ls/%ls",folder,name)<0)return NULL;return _wfopen(path,L"wb");}
static int bytes(const WCHAR*folder,const WCHAR*name,const void*data,size_t size){FILE*f=output(folder,name);if(!f)return 0;int ok=fwrite(data,1,size,f)==size;if(fclose(f))ok=0;return ok;}
static unsigned word(const uint8_t*v,unsigned a){return ((unsigned)v[a&65535]<<8)|v[(a+1)&65535];}
static int save(const WCHAR*folder,const Description*d,const ABGame*g,const uint32_t*display,unsigned width,unsigned height,const ABDebugHistory*h){
    const ABRaster*r=&g->raster;const GVDP*v=&r->bus.vdp;
    BITMAPFILEHEADER file={0};BITMAPINFOHEADER info={0};file.bfType=0x4d42;file.bfOffBits=sizeof file+sizeof info;file.bfSize=file.bfOffBits+width*height*4;
    info.biSize=sizeof info;info.biWidth=(LONG)width;info.biHeight=-(LONG)height;info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;
    FILE*f=output(folder,L"frame.bmp");if(!f)return 0;
    int ok=fwrite(&file,1,sizeof file,f)==sizeof file&&fwrite(&info,1,sizeof info,f)==sizeof info&&fwrite(display,4,width*height,f)==width*height;if(fclose(f))ok=0;if(!ok)return 0;
    char text[16384];int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,d->text,-1,text,sizeof text,NULL,NULL);if(!n||!bytes(folder,L"description.txt",text,(size_t)n-1))return 0;
    if(!bytes(folder,L"ram.bin",r->bus.ram,sizeof r->bus.ram)||!bytes(folder,L"vram.bin",v->vram,sizeof v->vram)||!bytes(folder,L"vdp-registers.bin",v->reg,sizeof v->reg))return 0;
    /* Explicit big-endian words, independent of the Windows host byte order. */
    uint8_t words[256];for(unsigned i=0;i<GVDP_CRAM_ENTRIES;++i){words[i*2]=(uint8_t)(v->cram[i]>>8);words[i*2+1]=(uint8_t)v->cram[i];}
    if(!bytes(folder,L"cram.bin",words,GVDP_CRAM_ENTRIES*2))return 0;
    for(unsigned i=0;i<GVDP_VSRAM_ENTRIES;++i){words[i*2]=(uint8_t)(v->vsram[i]>>8);words[i*2+1]=(uint8_t)v->vsram[i];}
    if(!bytes(folder,L"vsram.bin",words,GVDP_VSRAM_ENTRIES*2))return 0;
    f=output(folder,L"sprites.csv");if(!f)return 0;
    unsigned base=(v->reg[5]&((v->reg[12]&1)?0x7e:0x7f))<<9,link=0;uint8_t visited[128]={0};
    ok=fprintf(f,"order,index,link,x,y,width_cells,height_cells,tile,palette,priority,hflip,vflip,raw_attribute\n")>0;
    for(unsigned order=0;order<((v->reg[12]&1)?80u:64u)&&!visited[link];++order){
        visited[link]=1;unsigned a=(base+link*8)&65535,attr=word(v->vram,a+4),size=v->vram[(a+2)&65535],next=v->vram[(a+3)&65535]&127;
        if(fprintf(f,"%u,%u,%u,%d,%d,%u,%u,%u,%u,%u,%u,%u,%u\n",order,link,next,(int)(word(v->vram,a+6)&0x3ff)-128,(int)(word(v->vram,a)&0x3ff)-((v->reg[12]&6)==6?256:128),((size>>2)&3)+1,(size&3)+1,attr&2047,(attr>>13)&3,attr>>15,(attr>>11)&1,(attr>>12)&1,attr)<0)ok=0;
        if(!next)break;link=next;
    }if(fclose(f))ok=0;if(!ok)return 0;
    f=output(folder,L"inputs.csv");if(!f)return 0;ok=fprintf(f,"display_frame,master_clock,p1,p2\n")>0;
    for(unsigned i=0;i<h->count;++i){const ABDebugInput*p=&h->inputs[(h->next+600-h->count+i)%600];if(fprintf(f,"%llu,%llu,%u,%u\n",(unsigned long long)p->frame,(unsigned long long)p->master,p->p1,p->p2)<0)ok=0;}if(fclose(f))ok=0;if(!ok)return 0;
    WCHAR exe[32768],manifest[32768],destination[32768];if(!GetModuleFileNameW(NULL,exe,32768))return 0;
    char exe_hash[65];if(!executable_hash(exe,exe_hash))return 0;
    WCHAR*slash=wcsrchr(exe,L'\\');if(!slash)return 0;*slash=0;
    if(swprintf(manifest,32768,L"%ls/../../manifest.json",exe)<0||swprintf(destination,32768,L"%ls/validated-build.json",folder)<0)return 0;
    /* Missing manifest is explicit, useful for exploratory builds. */
    int provenance=CopyFileW(manifest,destination,TRUE)!=0;
    f=output(folder,L"report.json");if(!f)return 0;
    ok=fprintf(f,"{\n\"schema_version\":1,\"complete\":true,\"snapshot\":\"completed display; CPU/VDP memories at frame end, not per-scanline\",\"build_manifest_copied\":%s,\"frame\":%llu,\"width\":%u,\"height\":%u,\"master_clock\":%llu,\"raster_line\":%u,\"pc\":%u,\"sr\":%u,\"instructions\":%llu,\"clock_debt\":%llu,\"pending_irq\":%u,\"z80_pc\":%u,\"z80_irq_line\":%u,\"bus_fault\":%u,\"vdp_code\":%u,\"vdp_address\":%u,\"sprite_overflow\":%u,\"sprite_collision\":%u,\"renderer_sat_base\":%u,\"registers\":[",provenance?"true":"false",(unsigned long long)r->presented,width,height,(unsigned long long)r->sound.master_cycles,r->line,g->cpu.state.regs.PC,g->cpu.state.regs.SR,(unsigned long long)g->instructions,(unsigned long long)g->clock_debt,r->pending,r->sound.cpu.pc,r->sound.irq_line,r->bus.fault,v->code,v->address,v->sprite_overflow,v->sprite_collision,base)>0;
    for(unsigned i=0;i<8;++i)if(fprintf(f,"%s%u,%u",i?",":"",g->cpu.state.regs.D[i],g->cpu.state.regs.A[i])<0)ok=0;
    if(fprintf(f,"],\"executable_sha256\":\"%s\",\"rom_sha1\":\"38945360D824D2FB9535B4FD7F25B9AA9B32F019\",\"source_revision\":\"f9245076a83655fdce75edf16b244527b6879422\",\"recompiler_revision\":\"9cedc86d96c4886919083e4ab2321aa22ffa4078\",",exe_hash)<0)ok=0;
    if(fprintf(f,"\"vdp_table_address_rules\":\"mode5-width-masked\",\"register_order\":\"D0,A0,...,D7,A7\",\"input_frames\":%u}\n",h->count)<0)ok=0;if(fclose(f))ok=0;return ok;
}
int ab_debug_report(HWND owner,const ABGame*g,const uint32_t*display,unsigned width,unsigned height,const ABDebugHistory*h,unsigned automated){
    if(!g->raster.presented||!width||width>512||!height||height>240)return -1;
    struct {DLGTEMPLATE dialog;WORD menu,kind,title;} layout={0};layout.dialog.style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME;layout.dialog.cx=320;layout.dialog.cy=180;
    Description d={0};d.automated=automated;
    INT_PTR choice=DialogBoxIndirectParamW(GetModuleHandleW(NULL),&layout.dialog,owner,description_proc,(LPARAM)&d);
    if(choice==IDCANCEL)return 0;if(choice!=IDOK)return -1;
    WCHAR root[32768],folder[32768];if(!GetFullPathNameW(L"build/debug-reports",32768,root,NULL))return -1;
    if(!CreateDirectoryW(root,NULL)&&GetLastError()!=ERROR_ALREADY_EXISTS)return -1;
    SYSTEMTIME t;GetSystemTime(&t);int created=0;
    for(unsigned i=0;i<100;++i){if(swprintf(folder,32768,L"%ls/%04u%02u%02u-%02u%02u%02u-%03u-%lu-%u",root,t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,t.wMilliseconds,GetCurrentProcessId(),i)<0)return -1;if(CreateDirectoryW(folder,NULL)){created=1;break;}if(GetLastError()!=ERROR_ALREADY_EXISTS)return -1;}
    if(!created)return -1;int ok=save(folder,&d,g,display,width,height,h);
    if(!automated){WCHAR message[32768];swprintf(message,32768,L"%ls\n\n%ls",ok?L"Report saved. Gameplay will resume.":L"Report failed; this folder may contain partial files.",folder);MessageBoxW(owner,message,ok?L"Diagnostic captured":L"Capture error",MB_OK|(ok?MB_ICONINFORMATION:MB_ICONERROR));}
    char folder_utf8[32768];if(WideCharToMultiByte(CP_UTF8,0,folder,-1,folder_utf8,sizeof folder_utf8,NULL,NULL))printf("Diagnostic report %s: %s\n",ok?"saved":"FAILED",folder_utf8);return ok?1:-1;
}
