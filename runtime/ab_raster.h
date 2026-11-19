/* Internal NTSC qualification scheduler; instruction-boundary timing. */
#ifndef AB_RASTER_H
#define AB_RASTER_H
#include "ab_z80_runner.h"
typedef struct {
    /* First member: CPU bus callbacks receive this same address. */
    ABMachineBus bus;
    ABZ80Runner sound;
    uint64_t next_line,next_vint,frames,presented,lines,dma_cycles,irq_cycles,pixel_hash;
    unsigned line,pending,accepted[8],z80_irqs,rendered_lines;
    uint8_t pixels[512*240];
    uint8_t rgb[2][512*240*3];
    unsigned drawing,current_width,current_height,frame_width,frame_height;
} ABRaster;
void ab_raster_init(ABRaster *,const uint8_t *,size_t);
void ab_raster_cpu_init(ABRaster *,M68kStepContext *);
int ab_raster_advance(ABRaster *,uint64_t);
/* Consume at most one completed-display boundary, retaining unspent clocks. */
int ab_raster_advance_to_frame(ABRaster *,uint64_t *);
int ab_raster_interrupt(ABRaster *,M68kStepContext *);
#endif
