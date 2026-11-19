/* Windows diagnostic host only; excluded when AB_DEBUG_REPORTS is off. */
#ifndef AB_DEBUG_REPORT_H
#define AB_DEBUG_REPORT_H
#include <windows.h>
#include "ab_game.h"
typedef struct {uint64_t frame,master;uint8_t p1,p2;} ABDebugInput;
typedef struct {ABDebugInput inputs[600];unsigned count,next;} ABDebugHistory;
void ab_debug_record(ABDebugHistory *,const ABGame *,uint8_t,uint8_t);
int ab_debug_report(HWND,const ABGame *,const uint32_t *,unsigned,unsigned,const ABDebugHistory *,unsigned);
#endif
