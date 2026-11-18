#include "plane-edge.h"
#include <stdio.h>
#include <string.h>
static ABRaster r;static GVDP copy;static uint8_t original[65536];
#define CHECK(c) do{if(!(c)){fprintf(stderr,"Plane edge failure at %d: %s\n",__LINE__,#c);return 1;}}while(0)
static void setup(void){
    memset(&r,0,sizeof r);r.bus.ram[0xb141]=0x88;r.bus.ram[0xb206]=r.bus.ram[0xb214]=10;
    GVDP*v=&r.bus.vdp;v->reg[1]=64;v->reg[12]=1;v->reg[16]=0x11;v->reg[2]=0x30;
    v->vram[32]=0x11;v->vram[64]=0x22;
    for(unsigned row=0;row<28;++row){v->vram[0xc001+row*128]=1;v->vram[0xc003+row*128]=2;}
    copy=*v;memcpy(original,v->vram,sizeof original);
}
int main(void){
    setup();CHECK(ab_plane_extend_edges(&r,&copy,0)==5);
    for(unsigned col=59;col<64;++col)CHECK(ab_plane_word(&copy,0xc000+col*2)==(col&1?2:1));
    CHECK(!memcmp(original,r.bus.vdp.vram,sizeof original));
    CHECK(!memcmp(copy.vram+0xc000,r.bus.vdp.vram+0xc000,59*2));
    CHECK(!memcmp(copy.vram+0xe000,r.bus.vdp.vram+0xe000,8192));
    for(unsigned stage=0;stage<5;++stage){setup();r.bus.ram[0xfe15]=(uint8_t)stage;CHECK(ab_plane_extend_edges(&r,&copy,0)==(stage==3?0:5));if(stage==3)CHECK(!memcmp(original,copy.vram,sizeof original));}
    setup();r.bus.ram[0xfe15]=5;CHECK(!ab_plane_extend_edges(&r,&copy,0));
    setup();r.bus.ram[0xfe14]=1;CHECK(!ab_plane_extend_edges(&r,&copy,0));
    setup();r.bus.ram[0xb141]=0xa0;CHECK(!ab_plane_extend_edges(&r,&copy,0));
    setup();r.bus.ram[0xb214]=9;r.bus.ram[0xb215]=0xd8;CHECK(!ab_plane_extend_edges(&r,&copy,0));
    setup();copy.vram[0xc000+59*2+1]=2;CHECK(ab_plane_extend_edges(&r,&copy,0)==4);CHECK(ab_plane_word(&copy,0xc000+59*2)==2);
    puts("PASS: Stages 1/2/3/5 enabled; Stage 4/demo/unknown/loaded cells unchanged; center/background/live VRAM intact");return 0;
}
