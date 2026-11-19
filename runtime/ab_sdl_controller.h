#ifndef AB_SDL_CONTROLLER_H
#define AB_SDL_CONTROLLER_H
#include "ab_controller.h"
#include <SDL3/SDL.h>
typedef struct {
    SDL_Gamepad *pad[AB_CONTROLLER_SLOTS];
    SDL_JoystickID id[AB_CONTROLLER_SLOTS];
    unsigned initialized,virtual_only;
} ABSDLControllers;
int ab_sdl_controllers_init(ABSDLControllers *,const char *mapping_file,unsigned virtual_only);
void ab_sdl_controllers_close(ABSDLControllers *);
int ab_sdl_controllers_poll(ABSDLControllers *,ABControllerState[AB_CONTROLLER_SLOTS]);
int ab_sdl_controllers_list(void);
/* Mapping file syntax is SDL's public GUID/name/binding format. */
int ab_sdl_mapping_line(char *,size_t,const char *guid,const char *name,const char *bindings[8]);
int ab_sdl_configure_controller(unsigned index,const char *mapping_file);
#endif
