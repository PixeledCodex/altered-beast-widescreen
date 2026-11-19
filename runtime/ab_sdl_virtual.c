#include "ab_sdl_virtual.h"
#include <string.h>
int ab_sdl_virtual_mapping(ABSDLVirtual*v,char*out,size_t size){
    char guid[33];SDL_GUIDToString(SDL_GetJoystickGUID(v->joystick),guid,sizeof guid);
    /* Two distinct physical layouts exercise SDL mapping, not raw button assumptions. */
    const char*first[]={"h0.1","h0.4","h0.8","h0.2","b0","b1","b2","b3"};
    const char*second[]={"h0.1","h0.4","h0.8","h0.2","b7","b5","b3","b9"};
    const char**bindings=SDL_GetJoystickProduct(v->joystick)==2?second:first;
    if(!ab_sdl_mapping_line(out,size,guid,SDL_GetJoystickName(v->joystick),bindings))return 0;
    size_t used=strlen(out);return SDL_snprintf(out+used,size-used,"leftx:a0,lefty:a1,back:b%u,",SDL_GetJoystickProduct(v->joystick)==2?11u:4u)<(int)(size-used);
}
int ab_sdl_virtual_attach(ABSDLVirtual*v,unsigned profile,unsigned mapped){
    SDL_VirtualJoystickDesc desc;SDL_INIT_INTERFACE(&desc);desc.type=SDL_JOYSTICK_TYPE_UNKNOWN;
    desc.vendor_id=0xf00d;desc.product_id=(Uint16)(profile+1);desc.naxes=2;desc.nbuttons=12;desc.nhats=1;
    desc.name=profile?"AB generic remapped virtual pad":"AB standard virtual pad";
    memset(v,0,sizeof*v);v->id=SDL_AttachVirtualJoystick(&desc);if(!v->id)return 0;
    v->joystick=SDL_OpenJoystick(v->id);if(!v->joystick){ab_sdl_virtual_detach(v);return 0;}
    if(mapped){char line[1024];if(!ab_sdl_virtual_mapping(v,line,sizeof line)||SDL_AddGamepadMapping(line)<0){ab_sdl_virtual_detach(v);return 0;}}
    return 1;
}
void ab_sdl_virtual_detach(ABSDLVirtual*v){if(v->id){SDL_DetachVirtualJoystick(v->id);v->id=0;}if(v->joystick){SDL_CloseJoystick(v->joystick);v->joystick=NULL;}}
int ab_sdl_virtual_set(ABSDLVirtual*v,unsigned profile,uint8_t mask){
    if(!v->joystick)return 0;const unsigned first[]={0,1,2,3},second[]={7,5,3,9};const unsigned*buttons=profile?second:first;
    for(unsigned i=0;i<12;++i)if(!SDL_SetJoystickVirtualButton(v->joystick,i,false))return 0;
    const unsigned bits[]={0x40,0x10,0x20,0x80};for(unsigned i=0;i<4;++i)if(mask&bits[i])if(!SDL_SetJoystickVirtualButton(v->joystick,buttons[i],true))return 0;
    Uint8 hat=0;if(mask&1)hat|=SDL_HAT_UP;if(mask&2)hat|=SDL_HAT_DOWN;if(mask&4)hat|=SDL_HAT_LEFT;
    /* Right uses the stick, so the host smoke covers both normalized paths. */
    return SDL_SetJoystickVirtualHat(v->joystick,0,hat)&&SDL_SetJoystickVirtualAxis(v->joystick,0,mask&8?32767:0)&&SDL_SetJoystickVirtualAxis(v->joystick,1,0);
}
