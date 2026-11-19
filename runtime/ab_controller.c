#include "ab_controller.h"
void ab_controller_init(ABControllerRouting*r){r->slot[0]=r->slot[1]=AB_CONTROLLER_NONE;}
void ab_controller_assign(ABControllerRouting*r,const ABControllerState*s){
    for(unsigned p=0;p<2;++p)if(r->slot[p]>=AB_CONTROLLER_SLOTS||!s[r->slot[p]].connected)r->slot[p]=AB_CONTROLLER_NONE;
    for(unsigned p=0;p<2;++p)if(r->slot[p]==AB_CONTROLLER_NONE)
        for(unsigned i=0;i<AB_CONTROLLER_SLOTS;++i)if(s[i].connected&&i!=r->slot[1-p]){r->slot[p]=i;break;}
}
static uint8_t directions(uint8_t mask){
    if((mask&3)==3)mask&=(uint8_t)~3u;
    if((mask&12)==12)mask&=(uint8_t)~12u;
    return mask;
}
uint8_t ab_controller_mask(const ABControllerState*s){
    if(!s||!s->connected)return 0;
    uint8_t mask=(uint8_t)(s->buttons&15);
    if(s->left_x>AB_CONTROLLER_DEADZONE)mask|=8;
    if(s->left_x< -AB_CONTROLLER_DEADZONE)mask|=4;
    if(s->left_y>AB_CONTROLLER_DEADZONE)mask|=1;
    if(s->left_y< -AB_CONTROLLER_DEADZONE)mask|=2;
    if(s->buttons&AB_PAD_X)mask|=0x40; /* Left face -> Genesis A. */
    if(s->buttons&AB_PAD_A)mask|=0x10; /* Bottom face -> Genesis B. */
    if(s->buttons&AB_PAD_B)mask|=0x20; /* Right face -> Genesis C. */
    if(s->buttons&AB_PAD_START)mask|=0x80;
    return directions(mask);
}
uint8_t ab_controller_merge(uint8_t keyboard,uint8_t controller,unsigned focused){return focused?directions((uint8_t)(keyboard|controller)):0;}
