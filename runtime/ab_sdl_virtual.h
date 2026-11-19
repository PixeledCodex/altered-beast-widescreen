/* Process-local SDL virtual devices for deterministic qualification only. */
#ifndef AB_SDL_VIRTUAL_H
#define AB_SDL_VIRTUAL_H
#include "ab_sdl_controller.h"
typedef struct {SDL_JoystickID id;SDL_Joystick*joystick;} ABSDLVirtual;
int ab_sdl_virtual_attach(ABSDLVirtual *,unsigned profile,unsigned mapped);
int ab_sdl_virtual_mapping(ABSDLVirtual *,char *,size_t);
void ab_sdl_virtual_detach(ABSDLVirtual *);
int ab_sdl_virtual_set(ABSDLVirtual *,unsigned profile,uint8_t genesis_mask);
#endif
