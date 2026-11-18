#include "ab_raster.h"
#include "genesis_dac.h"
#include "plane-edge.h"
#include <string.h>
static unsigned acknowledge(void*u,unsigned level){
    ABRaster*r=u;r->pending&=~(1u<<level);++r->accepted[level];
    if(level==6)r->bus.vdp.vint_pending=0;
    return M68K_STEP_AUTOVECTOR;
}
void ab_raster_init(ABRaster*r,const uint8_t*rom,size_t size){
    memset(r,0,sizeof *r);ab_bus_init(&r->bus,rom,size);ab_z80_attach(&r->sound,&r->bus);
    r->next_line=3420;r->next_vint=UINT64_MAX;r->pixel_hash=UINT64_C(14695981039346656037);
    gvdp_begin_scanline(&r->bus.vdp,0);
}
void ab_raster_cpu_init(ABRaster*r,M68kStepContext*c){
    M68kStepBus callbacks=ab_machine_bus_callbacks;callbacks.ack_interrupt=acknowledge;
    m68k_step_init(c,&callbacks,r);
}
static void line_event(ABRaster*r){
    GVDP*v=&r->bus.vdp;
    if(r->line<(unsigned)gvdp_screen_height(v)&&r->line<240){
        uint8_t*row=r->pixels+r->line*512;
        int width=gvdp_render_scanline(v,(int)r->line,row);
        /* Keep original hardware-visible status; reveal only on a private copy.
         * This single-process preview owns the optional renderer globals. */
        GVDP reveal=*v;reveal.bus_read=NULL;reveal.colour_updated=NULL;
        ab_plane_extend_edges(r,&reveal,r->line);
        gvdp_set_ws_extra(40);
        width=gvdp_render_scanline(&reveal,(int)r->line,row);
        gvdp_set_ws_extra(0);
        r->current_width=(unsigned)width;r->current_height=(unsigned)gvdp_screen_height(v);
        for(int x=0;x<width&&x<512;++x){
            r->pixel_hash^=row[x];r->pixel_hash*=UINT64_C(1099511628211);
            unsigned index=row[x],shade=index<64?GENESIS_DAC_NORMAL:index<128?GENESIS_DAC_SHADOW:GENESIS_DAC_HIGHLIGHT;
            uint32_t color=index<192?genesis_dac_cram_to_argb(v->cram[index&63],shade):0;
            uint8_t*rgb=r->rgb[r->drawing]+(r->line*512+x)*3;
            rgb[0]=(uint8_t)(color>>16);rgb[1]=(uint8_t)(color>>8);rgb[2]=(uint8_t)color;
        }
        ++r->rendered_lines;
    }
    r->line=(r->line+1)%262;++r->lines;if(!r->line)++r->frames;
    if(r->line==(unsigned)gvdp_screen_height(v)){
        ++r->presented;r->drawing^=1;r->frame_width=r->current_width;r->frame_height=r->current_height;
    }
    unsigned old_flag=v->vint_pending;
    unsigned irq=gvdp_begin_scanline(v,(int)r->line);
    if(irq&GVDP_IRQ_HBLANK)r->pending|=1u<<4;
    /* VBlank starts at the line edge. VINT status/IRQ are separate, delayed
     * events. Timings are audited against reference revision 5355eae, not an
     * image-alignment offset. The backend's immediate VINT is suppressed. */
    if(r->line==(unsigned)gvdp_screen_height(v)){
        v->vint_pending=(uint8_t)old_flag;
        r->next_vint=r->next_line+((v->reg[12]&1)?788:770);
        ab_z80_irq_window(&r->sound,r->next_vint,r->next_line+3420);
    }
}
int ab_raster_advance(ABRaster*r,uint64_t cycles){
    while(cycles){
        uint64_t edge=r->next_vint<r->next_line?r->next_vint:r->next_line;
        uint64_t until=(edge-r->sound.master_cycles+6)/7;
        uint64_t part=cycles<until?cycles:until;
        if(!ab_z80_advance_68k(&r->sound,part))return 0;
        ab_bus_advance_dma(&r->bus,part*7);
        cycles-=part;
        if(r->sound.master_cycles>=r->next_vint){
            r->bus.vdp.vint_pending=1;
            if(r->bus.vdp.reg[1]&0x20)r->pending|=1u<<6;
            ++r->z80_irqs;r->next_vint=UINT64_MAX;
        }
        if(r->sound.master_cycles>=r->next_line){line_event(r);r->next_line+=3420;}
    }
    return 1;
}
int ab_raster_interrupt(ABRaster*r,M68kStepContext*c){
    unsigned level=r->pending&(1u<<6)?6:r->pending&(1u<<4)?4:0;
    /* Genesis Plus GX 5355eae m68kcpu.h: accepted 68000 entry includes
     * phase-dependent IACK waits in addition to the generic core's 44 cycles.
     * Its CPU clock rebases each NTSC frame. Keep our absolute master clock,
     * flooring the frame-local /7 phase (including any subcycle remainder).
     * Init resets master_cycles to zero; presentation/VBlank do not reset it. */
    static const unsigned iack_wait[10]={6,15,14,13,12,11,10,9,8,7};
    unsigned phase=(unsigned)(((r->sound.master_cycles%(UINT64_C(262)*3420))/7)%10);
    uint64_t before=c->state.cycles;
    if(level&&m68k_step_interrupt(c,level)){
        c->state.cycles+=iack_wait[phase];
        uint64_t spent=c->state.cycles-before;r->irq_cycles+=spent;
        return ab_raster_advance(r,spent);
    }
    return !r->bus.fault;
}
int ab_raster_advance_to_frame(ABRaster*r,uint64_t*cycles){
    uint64_t frame=r->presented;
    while(*cycles&&r->presented==frame){
        uint64_t until=(r->next_line-r->sound.master_cycles+6)/7;
        uint64_t part=*cycles<until?*cycles:until;
        if(!ab_raster_advance(r,part))return 0;
        *cycles-=part;
    }
    return 1;
}
