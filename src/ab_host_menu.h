/* Durable host presentation; no game state or simulation is owned here. */
#ifndef AB_HOST_MENU_H
#define AB_HOST_MENU_H
#include <windows.h>
enum {AB_MENU_ERROR=0,AB_MENU_RESUME=1,AB_MENU_TITLE=2,AB_MENU_QUIT=3,AB_MENU_BEAST=4,AB_MENU_STAGE_BASE=100};
enum {AB_MENU_UP=1,AB_MENU_DOWN=2,AB_MENU_CONFIRM=4,AB_MENU_CANCEL=8,AB_MENU_TOGGLE=16};
typedef int (*ABMenuPoll)(void *,unsigned *);
unsigned ab_host_menu_run(HWND,ABMenuPoll,void *,unsigned automated);
#endif
