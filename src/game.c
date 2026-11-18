#include "ab_game.h"
#include "ab_rom.h"
#include "ab_ram.h"
#include "ab_right.h"
int task22_right40(const ABGame *);
#include "ab_refresh.h"
#include "game-control-contract.h"
#include <string.h>
int ab_game_init(ABGame*g,const uint8_t*rom,size_t size){
    if(!g||!rom||size!=ab_rom_module.image_size)return 0;
    memset(g,0,sizeof*g);ab_raster_init(&g->raster,rom,size);
    /* Frame calls begin at VBlank and publish after the following complete
     * active display. This is a deterministic fixture phase, not power-on timing. */
    g->raster.line=224;gvdp_begin_scanline(&g->raster.bus.vdp,224);g->raster.bus.vdp.vint_pending=0;
    g->raster.bus.timed_dma=1;
    ab_bus_enable_startup(&g->raster.bus,AB_START_ALL,0xa1);
    ab_z80_enable_genesis_io(&g->raster.sound);
    ab_psg_attach(&g->psg,&g->raster.bus);
    if(!ab_fm_attach(&g->fm,&g->psg,&g->raster.bus))return 0;
    ab_audio_attach(&g->audio,&g->psg,&g->fm);
    ab_raster_cpu_init(&g->raster,&g->cpu);g->result=m68k_step_reset(&g->cpu);
    g->ready=g->result==M68K_STEP_OK;return g->ready;
}
int ab_game_frame(ABGame*g,uint8_t p1,uint8_t p2){
    if(!g->ready)return 0;
    ABRaster*r=&g->raster;r->bus.startup.pad[0]=p1;r->bus.startup.pad[1]=p2;
    uint64_t frame=r->presented,start=g->instructions;
    while(r->presented==frame&&g->instructions-start<1000000){
        if(g->clock_debt){
            if(!ab_raster_advance_to_frame(r,&g->clock_debt))goto fail;
            if(r->presented!=frame)break;
        }
        if(!ab_raster_interrupt(r,&g->cpu))goto fail;
        if(r->presented!=frame)break;
        uint32_t pc=g->cpu.state.regs.PC;
        if(g->control_phase==1&&pc==AB_CONTROL_TITLE_PC&&(r->bus.ram[AB_CONTROL_SCREEN_STATE+1]&0x80))g->control_phase=2;
        if(g->control_phase==3&&pc==AB_CONTROL_STAGE_LOAD_PC){
            /* Optional stage request changes only the original options value,
             * exactly where original code consumes it. That code copies it to
             * the current stage and performs all original initialization. */
            r->bus.ram[AB_CONTROL_STAGE_CONFIG]=0;r->bus.ram[AB_CONTROL_STAGE_CONFIG+1]=(uint8_t)g->control_stage;
            ++g->control_hook_hits;g->control_phase=4;
        }
        if(g->control_phase==4&&pc==AB_CONTROL_GAMEPLAY_PC&&(r->bus.ram[AB_CONTROL_SCREEN_STATE+1]&0x80))g->control_phase=5;
        uint64_t cycles=g->cpu.state.cycles;
        unsigned refresh=ab_refresh_before_instruction(&g->refresh_clock,r->sound.master_cycles);
        if(refresh){g->cpu.state.cycles+=refresh;++g->refresh_events;}
        g->result=m68k_step_module_execute(&g->cpu,task22_right40(g)&&(pc==0x42d2u||pc==0x42d8u||pc==0x42feu||pc==0x4304u||pc==0x4340u||pc==0x4346u)?&ab_right_module:(pc>=0xff0000u?&ab_ram_module:&ab_rom_module));
        r->bus.trace_count=0;
        if(g->result!=M68K_STEP_OK)goto fail;
        ++g->instructions;
        uint64_t debt=r->bus.stall_debt;r->bus.stall_debt=0;r->dma_cycles+=debt;
        g->clock_debt=g->cpu.state.cycles-cycles+debt;
    }
    if(r->presented!=frame+1)goto fail;
    return 1;
fail:g->ready=0;return 0;
}
void ab_game_destroy(ABGame*g){if(g){ab_fm_detach(&g->fm);g->ready=0;}}
int ab_game_apply_control(ABGame*g,const ABGameControlRequest*request){
    if(!g||!request||request->version!=AB_GAME_CONTROL_VERSION||(request->action!=AB_GAME_TITLE&&request->action!=AB_GAME_STAGE)||(request->action==AB_GAME_STAGE&&request->stage>4)||!g->ready)return 0;
    const uint8_t*rom=g->raster.bus.rom;size_t size=g->raster.bus.rom_size;
    const unsigned pcs[]={AB_CONTROL_TITLE_PC,AB_CONTROL_STAGE_LOAD_PC,AB_CONTROL_GAMEPLAY_PC},ops[]={AB_CONTROL_TITLE_PC_OPCODE,AB_CONTROL_STAGE_LOAD_PC_OPCODE,AB_CONTROL_GAMEPLAY_PC_OPCODE};
    for(unsigned i=0;i<3;++i)if(pcs[i]+1>=size||((rom[pcs[i]]<<8)|rom[pcs[i]+1])!=ops[i])return 0;
    unsigned generation=g->control_generation+1;ab_game_destroy(g);if(!ab_game_init(g,rom,size))return 0;g->control_generation=generation;g->control_phase=1;g->control_stage=request->stage;
    int16_t discard[2048];unsigned frames=0;
    while(g->control_phase!=2&&frames++<1200){if(!ab_game_frame(g,0,0))return 0;while(ab_audio_read(&g->audio,discard,1024)){} }
    if(g->control_phase!=2){g->ready=0;return 0;}
    if(request->action==AB_GAME_TITLE)return 1;
    g->control_phase=3;frames=0;
    while(g->control_phase!=5&&frames++<1200){uint8_t input=g->control_phase==3?0xc0:0;if(!ab_game_frame(g,input,0))return 0;while(ab_audio_read(&g->audio,discard,1024)){} }
    if(g->control_phase!=5){g->ready=0;return 0;}
    return g->control_hook_hits==1&&g->raster.bus.ram[AB_CONTROL_CURRENT_STAGE]==0&&g->raster.bus.ram[AB_CONTROL_CURRENT_STAGE+1]==request->stage;
}
