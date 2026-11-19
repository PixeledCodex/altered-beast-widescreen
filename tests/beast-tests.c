#include "preview-beast.h"
#include <stdio.h>
#include <string.h>
static ABGame game;static uint8_t rom[524289];
int task22_right40(const ABGame*g){return g==&game;}
#define CHECK(c) do{if(!(c)){fprintf(stderr,"Beast cheat test failed at %d: %s\n",__LINE__,#c);return 1;}}while(0)
static unsigned word(unsigned a){return (game.raster.bus.ram[a]<<8)|game.raster.bus.ram[a+1];}
static int frames(unsigned pad,unsigned count){for(unsigned i=0;i<count;++i){if(!ab_game_frame(&game,(uint8_t)pad,0))return 0;int16_t pcm[2048];while(ab_audio_read(&game.audio,pcm,1024)){} }return 1;}
static int pulse(unsigned pad){return frames(pad,4)&&frames(0,4);}
int main(int argc,char**argv){
    CHECK(argc==3);CHECK(!ab_preview_beast_select(NULL));CHECK(!ab_preview_beast_select(&game));
    FILE*f=fopen(argv[1],"rb");CHECK(f);size_t n=fread(rom,1,sizeof rom,f);fclose(f);
    CHECK(ab_game_init(&game,rom,n));ABGameControlRequest stage={1,AB_GAME_STAGE,0};CHECK(ab_game_apply_control(&game,&stage));CHECK(frames(0,90));
    CHECK(game.raster.frame_width==400&&game.raster.frame_height==224);
    unsigned generation=game.control_generation;CHECK(ab_preview_beast_select(&game));CHECK(game.control_generation==generation+1);
    CHECK((game.raster.bus.ram[0xb141]&127)==0x2c);
    unsigned initial[5];for(unsigned i=0;i<5;++i)initial[i]=word(0xfe08+i*2);
    CHECK(pulse(8));CHECK(word(0xfe08)==((initial[0]+1)&3));
    for(unsigned i=1;i<5;++i)CHECK(word(0xfe08+i*2)==initial[i]);
    CHECK(pulse(2));CHECK(word(0xb192)==1);CHECK(pulse(8));CHECK(word(0xfe0a)==((initial[1]+1)&3));
    for(unsigned i=2;i<5;++i)CHECK(word(0xfe08+i*2)==initial[i]);
    char path[4096];snprintf(path,sizeof path,"%s/chooser.ppm",argv[2]);f=fopen(path,"wb");CHECK(f);
    ABRaster*r=&game.raster;fprintf(f,"P6\n%u %u\n255\n",r->frame_width,r->frame_height);
    for(unsigned y=0;y<r->frame_height;++y)CHECK(fwrite(r->rgb[r->drawing^1]+y*512*3,3,r->frame_width,f)==r->frame_width);CHECK(!fclose(f));
    CHECK(pulse(128));CHECK((game.raster.bus.ram[0xb141]&127)==4);CHECK(word(0xfe08)==((initial[0]+1)&3));
    CHECK(word(0xfe0a)==((initial[1]+1)&3));CHECK(!game.audio.dropped);ab_game_destroy(&game);
    puts("PASS: active-stage restart to original beast chooser; native per-round input edits and Start-to-title retain choices");return 0;
}
