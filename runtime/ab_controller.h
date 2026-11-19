/* Internal normalized host input. Numeric button bits preserve the tested adapter. */
#ifndef AB_CONTROLLER_H
#define AB_CONTROLLER_H
#include <stdint.h>
enum {AB_CONTROLLER_SLOTS=16,AB_CONTROLLER_NONE=16,AB_CONTROLLER_DEADZONE=7849};
enum {AB_PAD_UP=0x0001,AB_PAD_DOWN=0x0002,AB_PAD_LEFT=0x0004,AB_PAD_RIGHT=0x0008,
      AB_PAD_START=0x0010,AB_PAD_BACK=0x0020,AB_PAD_A=0x1000,AB_PAD_B=0x2000,AB_PAD_X=0x4000};
typedef struct {unsigned connected;uint16_t buttons;int16_t left_x,left_y;} ABControllerState;
typedef struct {unsigned slot[2];} ABControllerRouting;
void ab_controller_init(ABControllerRouting *);
void ab_controller_assign(ABControllerRouting *,const ABControllerState[AB_CONTROLLER_SLOTS]);
uint8_t ab_controller_mask(const ABControllerState *);
uint8_t ab_controller_merge(uint8_t keyboard,uint8_t controller,unsigned focused);
#endif
