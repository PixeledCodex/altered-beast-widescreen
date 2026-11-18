/* Repeat existing foreground edge tiles on a VDP copy only. */
#ifndef AB_PLANE_EDGE_H
#define AB_PLANE_EDGE_H
#include "ab_raster.h"
static unsigned ab_plane_word(const GVDP*v,unsigned a){return (v->vram[a&65535]<<8)|v->vram[(a+1)&65535];}
static unsigned ab_plane_blank(const GVDP*v,unsigned tile){
    for(unsigned i=0;i<32;++i)if(v->vram[(tile*32+i)&65535])return 0;return 1;
}
static unsigned ab_plane_extend_edges(const ABRaster*r,GVDP*copy,unsigned line){
    const uint8_t*m=r->bus.ram;
    /* Normal Stages 1,2,3,5. Stage 4 and demos deliberately remain unchanged. */
    unsigned stage=(m[0xfe14]<<8)|m[0xfe15];
    if((m[0xb141]&127)!=8||stage>4||stage==3||!(copy->reg[1]&64)||
       !(copy->reg[12]&1)||(copy->reg[12]&6)==6||(copy->reg[16]&3)!=1)return 0;
    unsigned start=(m[0xb206]<<8)|m[0xb207],current=(m[0xb214]<<8)|m[0xb215];
    int distance=(int16_t)(uint16_t)(start-current);if(distance>=40)return 0;
    unsigned hmode=copy->reg[11]&3,hidx=hmode==0?0:hmode==2?line&~7u:line;
    unsigned hs=ab_plane_word(copy,((copy->reg[13]&63)<<10)+hidx*4)&1023;
    unsigned height=(copy->reg[16]>>4&3)==1?64:(copy->reg[16]>>4&3)==3?128:32;
    unsigned py=(line+(copy->vsram[0]&1023))&(height*8-1);
    unsigned base=((copy->reg[2]&56)<<10)+(py>>3)*128;
    unsigned boundary=((0u-start)&511)>>3,last=65536,changed=0;
    if(start&15)return 0;
    for(int x=-40;x<0;++x){
        unsigned col=((unsigned)(x-(int)hs)&511)>>3;
        unsigned relative=(col-boundary)&63;
        unsigned dest=(base+col*2)&65535;
        if(dest==last||relative<59)continue;last=dest;
        unsigned old=ab_plane_word(copy,dest);
        if(!ab_plane_blank(copy,old&2047))continue;
        unsigned source=(base+((boundary+(relative&1))&63)*2)&65535;
        unsigned value=ab_plane_word(copy,source);
        if(ab_plane_blank(copy,value&2047))continue;
        copy->vram[dest]=(uint8_t)(value>>8);copy->vram[(dest+1)&65535]=(uint8_t)value;++changed;
    }
    return changed;
}
#endif
