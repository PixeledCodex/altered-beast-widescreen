/* Included only by the private preview host, after its globals. */
static unsigned focus_inactive,focus_minimized,focus_smoke,focus_tests,focus_repaints;
static HANDLE focus_worker;
enum {FOCUS_RESTORE=WM_APP+24};
static DWORD WINAPI focus_restore_thread(void*context){
    Sleep(300);PostMessageA((HWND)context,FOCUS_RESTORE,0,0);return 0;
}
static void focus_test_begin(HWND window){
    /* Exercise the real message handler while keeping smoke windows hidden. */
    pad=8;
    SendMessageA(window,WM_KILLFOCUS,0,0);
    SendMessageA(window,WM_ACTIVATEAPP,FALSE,0);
    SendMessageA(window,WM_SIZE,SIZE_MINIMIZED,0);
    focus_worker=CreateThread(NULL,0,focus_restore_thread,window,0,NULL);
    if(!focus_worker){fprintf(stderr,"Focus restore worker failed\n");running=0;}
}
static int focus_wait(HWND window,LARGE_INTEGER*start){
    if(!focus_inactive&&!focus_minimized)return 1;
    LARGE_INTEGER paused,resumed;QueryPerformanceCounter(&paused);
    uint64_t signature=game_signature();
    if(device&&waveOutPause(device)!=MMSYSERR_NOERROR){fprintf(stderr,"Focus: could not pause audio\n");return 0;}
    pad=0;input_suppressed=1;
    while(running&&(focus_inactive||focus_minimized)){
        messages();
        if(running&&(focus_inactive||focus_minimized))MsgWaitForMultipleObjects(0,NULL,FALSE,50,QS_ALLINPUT);
    }
    QueryPerformanceCounter(&resumed);start->QuadPart+=resumed.QuadPart-paused.QuadPart;
    if(game_signature()!=signature){fprintf(stderr,"Focus: game state changed while inactive\n");return 0;}
    if(!running)return 1;
    if(device&&waveOutRestart(device)!=MMSYSERR_NOERROR){fprintf(stderr,"Focus: could not resume audio\n");return 0;}
    RedrawWindow(window,NULL,NULL,RDW_INVALIDATE|RDW_FRAME|RDW_UPDATENOW);
    if(focus_smoke){
        if(pad||resumed.QuadPart-paused.QuadPart<0){fprintf(stderr,"Focus: input/clock failure\n");return 0;}
        CloseHandle(focus_worker);focus_worker=NULL;++focus_tests;
        unsigned before=paints;SendMessageA(window,WM_PAINT,0,0);
        if(paints!=before+1||gdi_fault){fprintf(stderr,"Focus: repaint failed\n");return 0;}
        ++focus_repaints;
    }
    return 1;
}
