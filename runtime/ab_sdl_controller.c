#include "ab_sdl_controller.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#ifdef _WIN32
#include <conio.h>
#endif
int ab_sdl_controllers_init(ABSDLControllers*s,const char*file,unsigned virtual_only){
    memset(s,0,sizeof*s);s->virtual_only=virtual_only;
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1"); /* Host applies focus gating itself. */
    if(!SDL_Init(SDL_INIT_GAMEPAD))return 0;s->initialized=1;
    SDL_SetGamepadEventsEnabled(false);SDL_SetJoystickEventsEnabled(false);
    if(file&&SDL_AddGamepadMappingsFromFile(file)<0){ab_sdl_controllers_close(s);return 0;}
    return 1;
}
void ab_sdl_controllers_close(ABSDLControllers*s){
    for(unsigned i=0;i<AB_CONTROLLER_SLOTS;++i)if(s->pad[i]){SDL_CloseGamepad(s->pad[i]);s->pad[i]=NULL;s->id[i]=0;}
    if(s->initialized){SDL_Quit();s->initialized=0;}
}
int ab_sdl_controllers_poll(ABSDLControllers*s,ABControllerState*out){
    memset(out,0,sizeof(*out)*AB_CONTROLLER_SLOTS);SDL_PumpEvents();SDL_UpdateGamepads();
    for(unsigned i=0;i<AB_CONTROLLER_SLOTS;++i)if(s->pad[i]&&!SDL_GamepadConnected(s->pad[i])){SDL_CloseGamepad(s->pad[i]);s->pad[i]=NULL;s->id[i]=0;}
    int count=0;SDL_JoystickID*ids=SDL_GetGamepads(&count);if(!ids)return 0;
    for(int n=0;n<count;++n){
        if(s->virtual_only&&!SDL_IsJoystickVirtual(ids[n]))continue;
        unsigned i;for(i=0;i<AB_CONTROLLER_SLOTS;++i)if(s->id[i]==ids[n])break;
        if(i<AB_CONTROLLER_SLOTS)continue;
        for(i=0;i<AB_CONTROLLER_SLOTS;++i)if(!s->pad[i])break;
        if(i==AB_CONTROLLER_SLOTS)break;
        SDL_Gamepad*pad=SDL_OpenGamepad(ids[n]);if(pad){s->pad[i]=pad;s->id[i]=ids[n];}
    }SDL_free(ids);
    for(unsigned i=0;i<AB_CONTROLLER_SLOTS;++i)if(s->pad[i]){
        SDL_Gamepad*p=s->pad[i];ABControllerState*v=&out[i];v->connected=1;
        if(SDL_GetGamepadButton(p,SDL_GAMEPAD_BUTTON_DPAD_UP))v->buttons|=AB_PAD_UP;
        if(SDL_GetGamepadButton(p,SDL_GAMEPAD_BUTTON_DPAD_DOWN))v->buttons|=AB_PAD_DOWN;
        if(SDL_GetGamepadButton(p,SDL_GAMEPAD_BUTTON_DPAD_LEFT))v->buttons|=AB_PAD_LEFT;
        if(SDL_GetGamepadButton(p,SDL_GAMEPAD_BUTTON_DPAD_RIGHT))v->buttons|=AB_PAD_RIGHT;
        if(SDL_GetGamepadButton(p,SDL_GAMEPAD_BUTTON_WEST))v->buttons|=AB_PAD_X;
        if(SDL_GetGamepadButton(p,SDL_GAMEPAD_BUTTON_SOUTH))v->buttons|=AB_PAD_A;
        if(SDL_GetGamepadButton(p,SDL_GAMEPAD_BUTTON_EAST))v->buttons|=AB_PAD_B;
        if(SDL_GetGamepadButton(p,SDL_GAMEPAD_BUTTON_START))v->buttons|=AB_PAD_START;
        if(SDL_GetGamepadButton(p,SDL_GAMEPAD_BUTTON_BACK))v->buttons|=AB_PAD_BACK;
        v->left_x=SDL_GetGamepadAxis(p,SDL_GAMEPAD_AXIS_LEFTX);
        Sint16 y=SDL_GetGamepadAxis(p,SDL_GAMEPAD_AXIS_LEFTY);v->left_y=y==SDL_MIN_SINT16?SDL_MAX_SINT16:(Sint16)-y;
    }return 1;
}
int ab_sdl_controllers_list(void){
    int count=0;SDL_JoystickID*ids=SDL_GetJoysticks(&count);if(!ids)return 0;
    for(int i=0;i<count;++i){char guid[33];SDL_GUIDToString(SDL_GetJoystickGUIDForID(ids[i]),guid,sizeof guid);
        printf("Controller %d: %s | %s | GUID %s\n",i,SDL_GetJoystickNameForID(ids[i]),SDL_IsGamepad(ids[i])?"mapped":"UNMAPPED: use -ConfigureController",guid);
    }printf("SDL controllers detected: %d\n",count);SDL_free(ids);return 1;
}
static int binding_valid(const char*b){
    if(!b||!*b)return 0;const char*p=b;
    if(*p=='+'||*p=='-'){++p;if(*p++!='a')return 0;}
    else if(*p!='b'&&*p!='h')return 0;else ++p;
    if(!isdigit((unsigned char)*p))return 0;while(isdigit((unsigned char)*p))++p;
    if(b[0]=='h'){if(*p++!='.'||(*p!='1'&&*p!='2'&&*p!='4'&&*p!='8'))return 0;++p;}
    return !*p;
}
int ab_sdl_mapping_line(char*out,size_t size,const char*guid,const char*name,const char*bindings[8]){
    static const char*keys[]={"dpup","dpdown","dpleft","dpright","x","a","b","start"};
    if(!out||!guid||strlen(guid)!=32||!name)return 0;
    for(unsigned i=0;i<32;++i)if(!isxdigit((unsigned char)guid[i]))return 0;
    char clean[81];unsigned n=0;for(;name[n]&&n<80;++n)clean[n]=name[n]==','||name[n]=='\n'||name[n]=='\r'?' ':name[n];clean[n]=0;
    int used=snprintf(out,size,"%s,%s,",guid,clean);if(used<0||(size_t)used>=size)return 0;
    for(unsigned i=0;i<8;++i){if(!binding_valid(bindings[i]))return 0;int added=snprintf(out+used,size-used,"%s:%s,",keys[i],bindings[i]);if(added<0||(size_t)added>=size-used)return 0;used+=added;}
    int added=snprintf(out+used,size-used,"platform:Windows,");return added>=0&&(size_t)added<size-used;
}
#ifdef _WIN32
static int capture_binding(SDL_Joystick*j,const Sint16*baseline,int axes,char*out,size_t size,unsigned optional){
    unsigned quiet=0;Uint64 deadline=SDL_GetTicks()+120000;
    while(SDL_GetTicks()<deadline){
        if(_kbhit()){int key=_getch();if(key==27)return 0;if(optional&&(key=='s'||key=='S'))return 2;}
        SDL_PumpEvents();SDL_UpdateJoysticks();if(!SDL_JoystickConnected(j))return 0;
        int active=0;
        for(int i=0;i<SDL_GetNumJoystickButtons(j);++i)if(SDL_GetJoystickButton(j,i)){if(quiet>=10){snprintf(out,size,"b%d",i);return 1;}active=1;}
        for(int i=0;i<SDL_GetNumJoystickHats(j);++i){Uint8 h=SDL_GetJoystickHat(j,i);if(h){if(quiet>=10){unsigned bit=h&1?1:h&2?2:h&4?4:8;snprintf(out,size,"h%d.%u",i,bit);return 1;}active=1;}}
        for(int i=0;i<axes;++i){int delta=(int)SDL_GetJoystickAxis(j,i)-baseline[i];if(abs(delta)>16000){if(quiet>=10){snprintf(out,size,"%ca%d",delta<0?'-':'+',i);return 1;}active=1;}}
        if(active)quiet=0;else ++quiet;SDL_Delay(10);
    }return 0;
}
#endif
int ab_sdl_configure_controller(unsigned index,const char*file){
#ifdef _WIN32
    int count=0;SDL_JoystickID*ids=SDL_GetJoysticks(&count);if(!ids||index>=(unsigned)count){SDL_free(ids);fprintf(stderr,"Controller index unavailable; run -ListControllers first.\n");return 0;}
    SDL_Joystick*j=SDL_OpenJoystick(ids[index]);SDL_free(ids);if(!j)return 0;
    char guid[33],line[1024],storage[8][32];const char*bindings[8];
    static const char*prompts[]={"UP (D-pad or stick)","DOWN","LEFT","RIGHT","LEFT face button -> Genesis A","BOTTOM face button -> Genesis B","RIGHT face button -> Genesis C","START / Menu / Options / +"};
    SDL_GUIDToString(SDL_GetJoystickGUID(j),guid,sizeof guid);
    printf("Mapping %s. Center sticks and release controls, then press Enter on the KEYBOARD. Esc cancels without saving.\n",SDL_GetJoystickName(j));fflush(stdout);
    int key;do{key=_getch();if(key==27){SDL_CloseJoystick(j);return 0;}}while(key!=13);
    SDL_PumpEvents();SDL_UpdateJoysticks();int axes=SDL_GetNumJoystickAxes(j);if(axes>64)axes=64;
    Sint16 baseline[64];for(int i=0;i<axes;++i)baseline[i]=SDL_GetJoystickAxis(j,i);
    for(unsigned i=0;i<8;++i){printf("Release controls, then press %s...\n",prompts[i]);fflush(stdout);if(!capture_binding(j,baseline,axes,storage[i],sizeof storage[i],0)){SDL_CloseJoystick(j);fprintf(stderr,"Mapping cancelled, timed out or controller disconnected; nothing saved.\n");return 0;}bindings[i]=storage[i];printf("  captured %s\n",storage[i]);}
    printf("Release controls, then press SELECT / Back / View / Share / Minus for the game menu. Keyboard S skips; Esc cancels.\n");fflush(stdout);
    char back[32];int captured=capture_binding(j,baseline,axes,back,sizeof back,1);if(!captured){SDL_CloseJoystick(j);return 0;}
    if(!ab_sdl_mapping_line(line,sizeof line,guid,SDL_GetJoystickName(j),bindings)){SDL_CloseJoystick(j);return 0;}
    if(captured==1){size_t used=strlen(line);int added=snprintf(line+used,sizeof line-used,"back:%s,",back);if(added<0||(size_t)added>=sizeof line-used){SDL_CloseJoystick(j);return 0;}}
    if(SDL_AddGamepadMapping(line)<0){SDL_CloseJoystick(j);return 0;}
    FILE*out=fopen(file,"ab");if(!out){SDL_CloseJoystick(j);perror("Mapping file");return 0;}
    int ok=fprintf(out,"\n%s\n",line)>0;if(fclose(out))ok=0;SDL_CloseJoystick(j);
    if(ok)printf("Saved mapping to %s. Reconnect and launch normally.\n",file);return ok;
#else
    (void)index;(void)file;return 0;
#endif
}
