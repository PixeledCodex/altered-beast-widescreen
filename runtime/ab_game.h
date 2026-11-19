/* Internal standalone machine, not a change to the public ABI. */
#ifndef AB_GAME_H
#define AB_GAME_H
#include "ab_raster.h"
#include "ab_audio.h"
typedef struct {
    ABRaster raster;ABPsg psg;ABFm fm;ABAudio audio;M68kStepContext cpu;
    uint64_t instructions,clock_debt;M68kStepResult result;unsigned ready;
    uint64_t refresh_clock,refresh_events;
    unsigned control_phase,control_stage,control_hook_hits,control_generation;
} ABGame;
enum {AB_GAME_CONTROL_VERSION=1,AB_GAME_TITLE=1,AB_GAME_STAGE=2};
typedef struct {unsigned version,action,stage;} ABGameControlRequest;
/* Typed optional host action. Stage is 0..4; initialization executes original
 * compiled code. Does not expose arbitrary memory access or instruction jumps. */
int ab_game_apply_control(ABGame *,const ABGameControlRequest *);
int ab_game_init(ABGame *,const uint8_t *,size_t);
int ab_game_frame(ABGame *,uint8_t,uint8_t);
void ab_game_destroy(ABGame *);
#endif
