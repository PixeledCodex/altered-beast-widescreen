#ifndef AB_REFRESH_H
#define AB_REFRESH_H
#include <stdint.h>
/* Genesis Plus GX 5355eae m68kcpu.c:302-307. Called once before an
 * instruction, after outstanding DMA/interrupt/frame debt is consumed.
 * Long stalls skip refresh intervals; they do not accumulate charges. */
static inline unsigned ab_refresh_before_instruction(uint64_t *cursor,uint64_t master_clock){
    if(master_clock-*cursor<896)return 0;
    *cursor=(master_clock/896)*896;
    return 2;
}
#endif
