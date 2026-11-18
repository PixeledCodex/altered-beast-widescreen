#include "preview-beast.h"
#include "beast-contract.h"
static void drain(ABGame*g){int16_t pcm[2048];while(ab_audio_read(&g->audio,pcm,1024)){} }
int ab_preview_beast_select(ABGame*g){
    if(!g||!g->ready||g->raster.bus.rom_size<=AB_BEAST_CHEAT_PC+3)return 0;
    const uint8_t*rom=g->raster.bus.rom;
    if(((rom[AB_BEAST_CHEAT_PC]<<8)|rom[AB_BEAST_CHEAT_PC+1])!=AB_BEAST_CHEAT_OPCODE||
       ((rom[AB_BEAST_CHEAT_PC+2]<<8)|rom[AB_BEAST_CHEAT_PC+3])!=AB_BEAST_CHEAT_MASK)return 0;
    ABGameControlRequest request={AB_GAME_CONTROL_VERSION,AB_GAME_TITLE,0};
    if(!ab_game_apply_control(g,&request))return 0;
    unsigned entered=0;
    for(unsigned frame=0;frame<180;++frame){
        if(!ab_game_frame(g,AB_BEAST_CHEAT_MASK,0))return 0;drain(g);
        if((g->raster.bus.ram[0xb141]&127)==AB_BEAST_CHEAT_STATE){entered=1;break;}
    }
    if(!entered)return 0;
    /* Release the title chord before original menu navigation resumes. */
    for(unsigned frame=0;frame<6;++frame){if(!ab_game_frame(g,0,0))return 0;drain(g);}
    return (g->raster.bus.ram[0xb141]&127)==AB_BEAST_CHEAT_STATE;
}
