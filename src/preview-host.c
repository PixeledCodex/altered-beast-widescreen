/* Standalone diagnostic host. Gameplay stays in the compiled original code. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <bcrypt.h>
#include "ab_game.h"
#include "preview-beast.h"
#include "ab_sdl_virtual.h"
#include "ab_host_menu.h"
#ifdef AB_DEBUG_REPORTS
#include "ab_debug_report.h"
static ABDebugHistory debug_history;
static unsigned debug_requested,report_smoke,report_saved,report_cancelled;
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static ABGame game;
static uint8_t rom[524289],pad;
static uint32_t display[512*240];
static unsigned width=320,height=224,running=1;
static HWAVEOUT device;
static WAVEHDR wave[8];static int16_t pcm[8][2048];
static unsigned next_wave,queued,audio_enabled=1;
static unsigned smoke,seen_start,seen_right,paints,gdi_fault;
static unsigned controller_smoke,seen_p2_b;
static ABControllerRouting controllers;
static ABSDLControllers sdl_input;
static ABSDLVirtual virtual_pads[2];
static unsigned menu_requested,menu_held,input_suppressed,menu_smoke,menu_smoke_phase,menu_ticks,menu_next=30,host_steps,menu_release_at,menu_blocked,menu_disconnect_stable;
static uint64_t game_signature(void){const uint8_t*p=(const uint8_t*)&game;uint64_t h=UINT64_C(14695981039346656037);for(size_t i=0;i<sizeof game;++i){h^=p[i];h*=UINT64_C(1099511628211);}return h;}
static int menu_poll(void*unused,unsigned*out){
    (void)unused;
    if(menu_smoke){
        ++menu_ticks;if(menu_smoke_phase==0&&menu_ticks==2)ab_sdl_virtual_detach(&virtual_pads[0]);
        for(unsigned p=0;p<2;++p)if(virtual_pads[p].joystick)for(unsigned i=0;i<12;++i)if(!SDL_SetJoystickVirtualButton(virtual_pads[p].joystick,i,false))return 0;
        unsigned down=0,confirm=0,cancel=0;
        if(menu_smoke_phase==0){down=menu_ticks==5;confirm=menu_ticks==9;cancel=menu_ticks==13||menu_ticks==17;}
        if(menu_smoke_phase==1){down=menu_ticks==5||menu_ticks==13||menu_ticks==17;confirm=menu_ticks==9||menu_ticks==21;}
        if(menu_smoke_phase==2){down=menu_ticks==5||menu_ticks==9;confirm=menu_ticks==13;}
        if(menu_smoke_phase==3){down=menu_ticks==5||menu_ticks==9||menu_ticks==13;confirm=menu_ticks==17;}
        if(menu_smoke_phase==4){down=menu_ticks==5||menu_ticks==9||menu_ticks==13||menu_ticks==17;confirm=menu_ticks==21;}
        unsigned p=virtual_pads[0].joystick?0:1;SDL_Joystick*j=virtual_pads[p].joystick;
        if(!SDL_SetJoystickVirtualHat(j,0,down?SDL_HAT_DOWN:SDL_HAT_CENTERED)||!SDL_SetJoystickVirtualButton(j,p?5:1,confirm!=0)||!SDL_SetJoystickVirtualButton(j,p?3:2,cancel!=0))return 0;
        if(menu_ticks>300)return 0;
    }
    ABControllerState states[AB_CONTROLLER_SLOTS];if(!ab_sdl_controllers_poll(&sdl_input,states))return 0;ab_controller_assign(&controllers,states);*out=0;
    if(menu_smoke&&menu_smoke_phase==0&&menu_ticks==3)menu_disconnect_stable=controllers.slot[0]==AB_CONTROLLER_NONE&&controllers.slot[1]==1;
    for(unsigned p=0;p<2;++p)if(controllers.slot[p]<AB_CONTROLLER_SLOTS){ABControllerState*s=&states[controllers.slot[p]];uint8_t directions=ab_controller_mask(s);
        if(directions&1)*out|=AB_MENU_UP;if(directions&2)*out|=AB_MENU_DOWN;
        if(s->buttons&AB_PAD_A)*out|=AB_MENU_CONFIRM;if(s->buttons&AB_PAD_B)*out|=AB_MENU_CANCEL;if(s->buttons&AB_PAD_BACK)*out|=AB_MENU_TOGGLE;
    }return 1;
}
static int poll_controllers(uint8_t output[2],unsigned focused){
    ABControllerState state[AB_CONTROLLER_SLOTS]={{0}};
    unsigned previous[2]={controllers.slot[0],controllers.slot[1]};
    if(controller_smoke){
        uint64_t frame=game.raster.presented;
        if(frame>=1340&&virtual_pads[0].id)ab_sdl_virtual_detach(&virtual_pads[0]);
        if(virtual_pads[0].id){uint8_t mask=frame>=500&&frame<505?0x80:frame>=1300&&frame<1340?8:0;if(!ab_sdl_virtual_set(&virtual_pads[0],0,mask))return 0;}
        if(!ab_sdl_virtual_set(&virtual_pads[1],1,frame>=1350&&frame<1355?0x10:0))return 0;
    }
    if(!ab_sdl_controllers_poll(&sdl_input,state))return 0;
    ab_controller_assign(&controllers,state);
    unsigned back=0;for(unsigned p=0;p<2;++p)if(controllers.slot[p]<AB_CONTROLLER_SLOTS)back|=state[controllers.slot[p]].buttons&AB_PAD_BACK;
    if(back&&!menu_held&&focused)menu_requested=1;menu_held=back!=0;
    for(unsigned p=0;p<2;++p){
        if(previous[p]!=controllers.slot[p]){
            if(controllers.slot[p]==AB_CONTROLLER_NONE)printf("Controller player %u disconnected\n",p+1);
            else printf("SDL gamepad '%s' assigned to player %u\n",SDL_GetGamepadName(sdl_input.pad[controllers.slot[p]]),p+1);
        }
        uint8_t buttons=controllers.slot[p]<AB_CONTROLLER_SLOTS?ab_controller_mask(&state[controllers.slot[p]]):0;
        output[p]=ab_controller_merge(p?0:pad,buttons,focused);
    }
    if(input_suppressed){unsigned active=pad!=0;for(unsigned p=0;p<2;++p)if(controllers.slot[p]<AB_CONTROLLER_SLOTS){ABControllerState*s=&state[controllers.slot[p]];active|=ab_controller_mask(s)!=0||(s->buttons&AB_PAD_BACK)!=0;}output[0]=output[1]=0;if(!active)input_suppressed=0;else if(menu_smoke)++menu_blocked;}
    return 1;
}
static unsigned button(WPARAM key){
    switch(key){case VK_UP:return 1;case VK_DOWN:return 2;case VK_LEFT:return 4;case VK_RIGHT:return 8;
    case 'Z':return 0x40;case 'X':return 0x10;case 'C':return 0x20;case VK_RETURN:return 0x80;default:return 0;}
}
static void messages(void);
#include "widescreen-focus.c"
static LRESULT CALLBACK window_proc(HWND window,UINT message,WPARAM w,LPARAM l){
    switch(message){
    case WM_CLOSE:DestroyWindow(window);return 0;
    case WM_DESTROY:running=0;PostQuitMessage(0);return 0;
    case WM_KILLFOCUS:pad=0;input_suppressed=1;return 0;
    case WM_ACTIVATEAPP:
        focus_inactive=!w;pad=0;input_suppressed=1;
        if(w)InvalidateRect(window,NULL,FALSE);return 0;
    case WM_SIZE:
        focus_minimized=w==SIZE_MINIMIZED;
        if(!focus_minimized)InvalidateRect(window,NULL,FALSE);return 0;
    case FOCUS_RESTORE:
        SendMessageA(window,WM_SIZE,SIZE_RESTORED,MAKELPARAM(1200,672));
        SendMessageA(window,WM_ACTIVATEAPP,TRUE,0);return 0;
    case WM_KEYDOWN:
#ifdef AB_DEBUG_REPORTS
        if(w==VK_F8){if(!(l&(1L<<30)))debug_requested=1;return 0;}
#endif
        if(w==VK_ESCAPE){if(!(l&(1L<<30)))menu_requested=1;return 0;}pad|=(uint8_t)button(w);return 0;
    case WM_KEYUP:pad&=(uint8_t)~button(w);return 0;
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{
        PAINTSTRUCT ps;HDC dc=BeginPaint(window,&ps);RECT area;GetClientRect(window,&area);
        if(IsIconic(window)||area.right<=0||area.bottom<=0||!width||!height){EndPaint(window,&ps);return 0;}
        int dw=area.right,dh=dw*(int)height/(int)width;
        if(dh>area.bottom){dh=area.bottom;dw=dh*(int)width/(int)height;}
        PatBlt(dc,0,0,area.right,area.bottom,BLACKNESS);
        BITMAPINFO info={0};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=(LONG)width;
        info.bmiHeader.biHeight=-(LONG)height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        SetStretchBltMode(dc,COLORONCOLOR);
        int drawn=StretchDIBits(dc,(area.right-dw)/2,(area.bottom-dh)/2,dw,dh,0,0,(int)width,(int)height,display,&info,DIB_RGB_COLORS,SRCCOPY);
        ++paints;if(drawn==GDI_ERROR||drawn==0)gdi_fault=1;
        EndPaint(window,&ps);return 0;}
    }return DefWindowProcA(window,message,w,l);
}
static void messages(void){MSG message;while(PeekMessageA(&message,NULL,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageA(&message);}}
static int verify_rom(size_t size){
    /* Each standalone load is authenticated; this does not rebuild/recheck the
     * already accepted reference build during development. */
    static const uint8_t expected[20]={0x38,0x94,0x53,0x60,0xd8,0x24,0xd2,0xfb,0x95,0x35,0xb4,0xfd,0x7f,0x25,0xb9,0xaa,0x9b,0x32,0xf0,0x19};
    BCRYPT_ALG_HANDLE algorithm=NULL;BCRYPT_HASH_HANDLE hash=NULL;uint8_t digest[20];int valid=0;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA1_ALGORITHM,NULL,0)<0)return 0;
    if(BCryptCreateHash(algorithm,&hash,NULL,0,NULL,0,0)>=0&&BCryptHashData(hash,rom,(ULONG)size,0)>=0&&
        BCryptFinishHash(hash,digest,sizeof digest,0)>=0)valid=!memcmp(digest,expected,sizeof expected);
    if(hash)BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);return valid;
}
static int audio_open(void){
    memset(wave,0,sizeof wave);next_wave=0;
    WAVEFORMATEX format={WAVE_FORMAT_PCM,2,48000,48000*4,4,16,0};
    MMRESULT result=waveOutOpen(&device,WAVE_MAPPER,&format,0,0,CALLBACK_NULL);
    if(result!=MMSYSERR_NOERROR){fprintf(stderr,"waveOutOpen failed: %u (use --silent explicitly for video-only diagnostics)\n",result);return 0;}
    for(unsigned i=0;i<8;++i){wave[i].lpData=(LPSTR)pcm[i];wave[i].dwBufferLength=sizeof pcm[i];
        result=waveOutPrepareHeader(device,&wave[i],sizeof wave[i]);if(result!=MMSYSERR_NOERROR)return 0;}
    return 1;
}
static int audio_submit(void){
    while(game.audio.count){
        WAVEHDR*h=&wave[next_wave];
        while((h->dwFlags&WHDR_INQUEUE)&&running){messages();Sleep(1);}
        if(!running)return 1;
        size_t n=ab_audio_read(&game.audio,pcm[next_wave],1024);h->dwBufferLength=(DWORD)(n*4);
        MMRESULT result=waveOutWrite(device,h,sizeof*h);if(result!=MMSYSERR_NOERROR){fprintf(stderr,"waveOutWrite failed: %u\n",result);return 0;}
        next_wave=(next_wave+1)%8;++queued;
    }return !game.audio.dropped;
}
static void audio_close(void){if(device){waveOutReset(device);for(unsigned i=0;i<8;++i)if(wave[i].dwFlags&WHDR_PREPARED)waveOutUnprepareHeader(device,&wave[i],sizeof wave[i]);waveOutClose(device);device=NULL;}}
int task22_right40(const ABGame*g){return g==&game;}
int main(int argc,char**argv){
    const char*rom_path=NULL,*mappings=NULL;int list=0,configure=-1,silent=0;
    for(int i=1;i<argc;++i){
        if(!strcmp(argv[i],"--mappings")&&i+1<argc)mappings=argv[++i];
        else if(!strcmp(argv[i],"--map-controller")&&i+1<argc){char*end;long value=strtol(argv[++i],&end,10);if(*end||value<0||value>65535)return 2;configure=(int)value;}
        else if(!strcmp(argv[i],"--list-controllers"))list=1;
        else if(!strcmp(argv[i],"--controller-smoke"))controller_smoke=1;
        else if(!strcmp(argv[i],"--menu-smoke"))menu_smoke=1;
#ifdef AB_DEBUG_REPORTS
        else if(!strcmp(argv[i],"--report-smoke")){report_smoke=1;smoke=1;}
#endif
        else if(!strcmp(argv[i],"--focus-smoke")){focus_smoke=1;smoke=1;}
        else if(!strcmp(argv[i],"--smoke"))smoke=1;
        else if(!strcmp(argv[i],"--silent"))silent=1;
        else if(argv[i][0]=='-'||rom_path){fprintf(stderr,"Unknown/duplicate argument: %s\n",argv[i]);return 2;}
        else rom_path=argv[i];
    }
    if((list&&configure>=0)||(silent&&(smoke||controller_smoke))||(!list&&configure<0&&!rom_path)||(configure>=0&&!mappings)){
        fprintf(stderr,"usage: ab_windows.exe OWNED_ROM [--silent|--smoke|--controller-smoke] [--mappings FILE]\n       ab_windows.exe --list-controllers [--mappings FILE]\n       ab_windows.exe --map-controller INDEX --mappings FILE\n");return 2;
    }
    smoke=smoke||controller_smoke;audio_enabled=!silent;
    if(list||configure>=0){if(!ab_sdl_controllers_init(&sdl_input,list?mappings:NULL,0)){fprintf(stderr,"SDL controller initialization: %s\n",SDL_GetError());return 1;}
        int ok=list?ab_sdl_controllers_list():ab_sdl_configure_controller((unsigned)configure,mappings);ab_sdl_controllers_close(&sdl_input);return ok?0:1;}
    ab_controller_init(&controllers);
    FILE*f=fopen(rom_path,"rb");if(!f){perror("ROM open");return 2;}
    size_t size=fread(rom,1,sizeof rom,f);int ok=feof(f)&&!ferror(f);fclose(f);
    if(!ok||size!=524288||!verify_rom(size)){fprintf(stderr,"ROM must match owned UE REV02 SHA-1 38945360D824D2FB9535B4FD7F25B9AA9B32F019\n");return 2;}
    if(!ab_sdl_controllers_init(&sdl_input,mappings,smoke||menu_smoke)){fprintf(stderr,"SDL controller initialization: %s\n",SDL_GetError());return 1;}
    if((controller_smoke||menu_smoke)&&(!ab_sdl_virtual_attach(&virtual_pads[0],0,1)||!ab_sdl_virtual_attach(&virtual_pads[1],1,1))){fprintf(stderr,"SDL virtual controller creation: %s\n",SDL_GetError());return 1;}
    if(!ab_game_init(&game,rom,size)){fprintf(stderr,"Machine initialization failed\n");return 1;}
    if(audio_enabled&&!audio_open()){audio_close();ab_game_destroy(&game);return 1;}
    SetProcessDPIAware();WNDCLASSA cls={0};cls.lpfnWndProc=window_proc;cls.hInstance=GetModuleHandleA(NULL);cls.lpszClassName="ABNativeDiagnostic";cls.hCursor=LoadCursor(NULL,IDC_ARROW);
    if(!RegisterClassA(&cls))return 1;
    RECT bounds={0,0,1200,672};AdjustWindowRect(&bounds,WS_OVERLAPPEDWINDOW,FALSE);
    HWND window=CreateWindowA(cls.lpszClassName,"Altered Beast EXPERIMENTAL WIDE 400x224 | Gamepad: D-pad/stick + left/bottom/right face + Start | Keyboard: arrows Z X C Enter",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,bounds.right-bounds.left,bounds.bottom-bounds.top,NULL,NULL,cls.hInstance,NULL);
    if(!window){audio_close();ab_game_destroy(&game);return 1;}if(!smoke&&!menu_smoke)ShowWindow(window,SW_SHOW);
    LARGE_INTEGER start,frequency,now;QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&start);
    int exit_code=0;
    while(running){
        if(smoke){
            uint64_t frame=game.raster.presented;
            if(!controller_smoke){
                if(frame==500)PostMessageA(window,WM_KEYDOWN,VK_RETURN,0);
                if(frame==505)PostMessageA(window,WM_KEYUP,VK_RETURN,0);
                if(frame==1300)PostMessageA(window,WM_KEYDOWN,VK_RIGHT,0);
                if(frame==1340)PostMessageA(window,WM_KEYUP,VK_RIGHT,0);
#ifdef AB_DEBUG_REPORTS
                if(report_smoke&&(frame==700||frame==720))PostMessageA(window,WM_KEYDOWN,VK_F8,0);
#endif
            }
            if(frame>=1400){running=0;break;}
        }
        messages();if(!running)break;
        if(focus_smoke&&((game.raster.presented==700&&focus_tests==0)||(game.raster.presented==900&&focus_tests==1)))focus_test_begin(window);
        if(!focus_wait(window,&start)){exit_code=1;break;}if(!running)break;
        if(menu_requested){
            menu_requested=0;menu_ticks=0;LARGE_INTEGER paused,resumed;QueryPerformanceCounter(&paused);
            uint64_t signature=game_signature();if(device&&waveOutPause(device)!=MMSYSERR_NOERROR){fprintf(stderr,"Could not pause audio for game menu\n");exit_code=1;break;}pad=0;
            unsigned action=ab_host_menu_run(window,menu_poll,NULL,menu_smoke);
            if(game_signature()!=signature){fprintf(stderr,"Game state changed while menu was open\n");exit_code=1;break;}
            pad=0;menu_held=1;input_suppressed=1;menu_release_at=host_steps+2;
            if(action==AB_MENU_ERROR){fprintf(stderr,"Menu/controller failure\n");exit_code=1;break;}
            if(menu_smoke){unsigned expected=menu_smoke_phase==0?AB_MENU_RESUME:menu_smoke_phase==1?AB_MENU_STAGE_BASE+2:menu_smoke_phase==2?AB_MENU_TITLE:menu_smoke_phase==3?AB_MENU_BEAST:AB_MENU_QUIT;if(action!=expected){fprintf(stderr,"Menu smoke action mismatch: %u != %u\n",action,expected);exit_code=1;break;}++menu_smoke_phase;menu_next+=30;}
            if(menu_smoke&&!virtual_pads[0].id&&!ab_sdl_virtual_attach(&virtual_pads[0],0,1)){exit_code=1;break;}
            if(action==AB_MENU_QUIT){running=0;break;}
            if(action==AB_MENU_TITLE||action==AB_MENU_BEAST||action>=AB_MENU_STAGE_BASE){
                audio_close();ABGameControlRequest request={AB_GAME_CONTROL_VERSION,action==AB_MENU_TITLE?AB_GAME_TITLE:AB_GAME_STAGE,action>=AB_MENU_STAGE_BASE?action-AB_MENU_STAGE_BASE:0};
                if(!(action==AB_MENU_BEAST?ab_preview_beast_select(&game):ab_game_apply_control(&game,&request))){fprintf(stderr,"Original title/stage initialization failed: phase=%u PC=%06X\n",game.control_phase,game.cpu.state.regs.PC);exit_code=1;break;}
                if(audio_enabled&&!audio_open()){exit_code=1;break;}
#ifdef AB_DEBUG_REPORTS
                memset(&debug_history,0,sizeof debug_history);
#endif
                QueryPerformanceCounter(&now);start.QuadPart=now.QuadPart-(LONGLONG)((double)game.raster.sound.master_cycles/AB_MASTER_HZ*frequency.QuadPart);
            }else{if(device&&waveOutRestart(device)!=MMSYSERR_NOERROR){fprintf(stderr,"Could not resume audio after game menu\n");exit_code=1;break;}QueryPerformanceCounter(&resumed);start.QuadPart+=resumed.QuadPart-paused.QuadPart;}
        }
#ifdef AB_DEBUG_REPORTS
        if(debug_requested){
            debug_requested=0;LARGE_INTEGER paused,resumed;QueryPerformanceCounter(&paused);
            uint64_t frame_before=game.raster.presented,instructions_before=game.instructions,clock_before=game.raster.sound.master_cycles;
            if(device)waveOutPause(device);pad=0;
            int result=ab_debug_report(window,&game,display,width,height,&debug_history,report_smoke?(frame_before==720?2:1):0);
            pad=0;if(device)waveOutRestart(device);QueryPerformanceCounter(&resumed);start.QuadPart+=resumed.QuadPart-paused.QuadPart;
            if(result>0)++report_saved;else if(!result)++report_cancelled;else if(!report_smoke)MessageBoxW(window,L"Could not capture a diagnostic report. Check the build/debug-reports folder and available disk space.",L"Diagnostic capture failed",MB_OK|MB_ICONERROR);
            if(game.raster.presented!=frame_before||game.instructions!=instructions_before||game.raster.sound.master_cycles!=clock_before){exit_code=1;break;}
            if(report_smoke&&result<0){exit_code=1;break;}
        }
#endif
        if(menu_smoke){if(host_steps>200){exit_code=1;break;}if(host_steps==menu_release_at){for(unsigned p=0;p<2;++p)if(virtual_pads[p].joystick){for(unsigned i=0;i<12;++i)SDL_SetJoystickVirtualButton(virtual_pads[p].joystick,i,false);SDL_SetJoystickVirtualHat(virtual_pads[p].joystick,0,SDL_HAT_CENTERED);}}if(host_steps==menu_next&&!SDL_SetJoystickVirtualButton(virtual_pads[0].joystick,4,true)){exit_code=1;break;}}
        uint8_t input[2];if(!poll_controllers(input,smoke||menu_smoke||GetForegroundWindow()==window)){fprintf(stderr,"SDL polling failed: %s\n",SDL_GetError());exit_code=1;break;}
        if(menu_requested)continue; /* Select freezes before another frame is executed. */
        if(!ab_game_frame(&game,input[0],input[1])){
            fprintf(stderr,"Machine stopped: native PC=%06X result=%u bus=%u/%06X; Z80 PC=%04X address=%04X\n",game.cpu.state.regs.PC,game.result,game.raster.bus.fault,game.raster.bus.fault_address,game.raster.sound.fault_pc,game.raster.sound.fault_address);exit_code=1;break;
        }
        ABRaster*r=&game.raster;width=r->frame_width;height=r->frame_height;
        ++host_steps;
#ifdef AB_DEBUG_REPORTS
        ab_debug_record(&debug_history,&game,input[0],input[1]);
#endif
        seen_start|=r->bus.ram[0xb144]&0x80;seen_right|=r->bus.ram[0xb144]&8;
        seen_p2_b|=r->bus.ram[0xb146]&0x10;
        if(!width||width>512||!height||height>240){exit_code=1;break;}
        const uint8_t*rgb=r->rgb[r->drawing^1];
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){const uint8_t*p=rgb+(y*512+x)*3;display[y*width+x]=((uint32_t)p[0]<<16)|((uint32_t)p[1]<<8)|p[2];}
        InvalidateRect(window,NULL,FALSE);UpdateWindow(window);
        if(smoke&&!paints)SendMessageA(window,WM_PAINT,0,0);
        if(audio_enabled){if(!audio_submit()){exit_code=1;break;}}
        else{int16_t discard[2048];while(ab_audio_read(&game.audio,discard,1024)){} }
        double target=(double)r->sound.master_cycles/AB_MASTER_HZ;
        do{QueryPerformanceCounter(&now);if((double)(now.QuadPart-start.QuadPart)/frequency.QuadPart>=target)break;messages();Sleep(1);}while(running);
    }
    if(focus_worker){WaitForSingleObject(focus_worker,1000);CloseHandle(focus_worker);focus_worker=NULL;}
    audio_close();ab_game_destroy(&game);
    if(smoke&&(!seen_start||!seen_right||!queued||!paints||gdi_fault||game.audio.dropped||game.raster.presented!=1400))exit_code=1;
    if(focus_smoke){printf("Focus smoke: inactive/minimized cycles=%u restored paints=%u; game state frozen; input cleared; pacing rebased\n",focus_tests,focus_repaints);if(focus_tests!=2||focus_repaints!=2)exit_code=1;}
    if(controller_smoke&&(!seen_p2_b||controllers.slot[0]!=AB_CONTROLLER_NONE||controllers.slot[1]!=1))exit_code=1;
    if(menu_smoke){if(menu_smoke_phase!=5||menu_blocked<2||!menu_disconnect_stable)exit_code=1;printf("Menu smoke: %u actions; SDL Select opens, Resume/Stage3/Title/BeastChooser/Quit, controller navigation/cancel; full game struct frozen during every menu; %u held-input frames suppressed; stable P2 after disconnect=%u\n",menu_smoke_phase,menu_blocked,menu_disconnect_stable);}
#ifdef AB_DEBUG_REPORTS
    if(report_smoke){if(report_saved!=1||report_cancelled!=1)exit_code=1;printf("Diagnostic report smoke: saved=%u cancelled=%u; frame/instruction/master clocks frozen during dialogs\n",report_saved,report_cancelled);}
#endif
    if(controller_smoke)printf("Controller-path smoke (real SDL virtual devices, distinct mappings): P1_Start=%u P1_Right=%u P2_B=%u stable_P2_slot=%u\n",seen_start,seen_right,seen_p2_b,controllers.slot[1]);
    printf("Windows presentation: paints=%u GDI_fault=%u raster_periods=%llu\n",paints,gdi_fault,(unsigned long long)game.raster.frames);
    printf("Native diagnostic closed: frames=%llu instructions=%llu PCM=%llu dropped=%u queued=%u Start=%u Right=%u smoke=%u\n",(unsigned long long)game.raster.presented,(unsigned long long)game.instructions,(unsigned long long)game.audio.emitted,game.audio.dropped,queued,seen_start,seen_right,smoke);
    ab_sdl_virtual_detach(&virtual_pads[0]);ab_sdl_virtual_detach(&virtual_pads[1]);ab_sdl_controllers_close(&sdl_input);
    return exit_code;
}
