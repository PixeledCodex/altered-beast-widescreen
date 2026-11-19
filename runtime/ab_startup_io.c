/* Bounded Genesis startup endpoints. No Z80 instructions, clock progression,
 * audio or serial/6-button protocol is fabricated by this device model. */
#include "ab_machine_bus.h"
static uint8_t pins(const ABStartupDevices*s,unsigned port){
    if(port==2)return 0x7f; /* Unplugged expansion port, input pull-ups. */
    unsigned p=s->pad[port];int th=(s->control[port]&0x40)?!!(s->data[port]&0x40):1;
    return (uint8_t)(th?(0x40|((~p)&0x3f)):
        (((p&0x80)?0:0x20)|((p&0x40)?0:0x10)|((~p)&3u)));
}
static int io_value(ABStartupDevices*s,uint32_t a,uint8_t*v){
    unsigned offset=a&0x1e;
    if(offset==0&&(s->enabled&AB_START_VERSION)){*v=s->version;return 1;}
    if(!(s->enabled&AB_START_PARALLEL))return 0;
    if(offset>=2&&offset<=6){unsigned p=offset/2-1;*v=(uint8_t)((pins(s,p)&~s->control[p])|(s->data[p]&s->control[p]));return 1;}
    if(offset>=8&&offset<=12){*v=s->control[offset/2-4];return 1;}
    return 0;
}
int ab_startup_read(ABMachineBus*b,uint32_t a,unsigned width,uint16_t*out){
    ABStartupDevices*s=&b->startup;uint8_t value;
    if(a>=0xa04000u&&a<=0xa04003u&&s->fm_read){
        if(s->z80_reset_off&&!s->z80_busreq)return -1;
        if(!s->fm_read(s->sound_context,a&3u,&value,b->clock_master))return -1;
        *out=width==1?value:(uint16_t)(value*0x101u);return 1;
    }
    if(a>=0xa10000u&&a<0xa10020u&&io_value(s,a,&value)){*out=(uint16_t)(width==2||(a&1)?value:0);return 1;}
    if((s->enabled&AB_START_Z80_CONTROL)&&((a&~1u)==0xa11100u||(a&~1u)==0xa11200u)){
        unsigned bit=(a&~1u)==0xa11100u?!s->z80_busreq:s->z80_reset_off;
        *out=(uint16_t)(width==2?bit<<8:(a&1)?0:bit);return 1;
    }
    if((s->enabled&AB_START_Z80_RAM)&&a>=0xa00000u&&a+width<=0xa02000u){
        if(s->z80_reset_off&&!s->z80_busreq){s->execution_required=1;return -1;}
        unsigned n=a-0xa00000u;*out=width==1?s->z80_ram[n]:(uint16_t)((s->z80_ram[n]<<8)|s->z80_ram[n+1]);return 1;
    }
    return 0;
}
int ab_startup_write(ABMachineBus*b,uint32_t a,unsigned width,uint16_t value){
    ABStartupDevices*s=&b->startup;
    if(a>=0xa04000u&&a<=0xa04003u&&s->fm_write){
        if(s->z80_reset_off&&!s->z80_busreq)return -1;
        return s->fm_write(s->sound_context,a&3u,(uint8_t)value,b->clock_master)?1:-1;
    }
    if((s->enabled&AB_START_TMSS)&&width==2&&(a==0xa14000u||a==0xa14002u)){
        unsigned n=(a-0xa14000u)/2;s->tmss[n]=value;s->tmss_written|=(uint8_t)(1u<<n);return 1;
    }
    if((s->enabled&AB_START_PARALLEL)&&a>=0xa10002u&&a<=0xa1000du&&(width==2||(a&1u))){
        unsigned offset=a&0x1e;
        if(offset>=2&&offset<=6){s->data[offset/2-1]=(uint8_t)(value&0x7f);return 1;}
        if(offset>=8&&offset<=12){s->control[offset/2-4]=(uint8_t)(value&0x7f);return 1;}
    }
    if((s->enabled&AB_START_Z80_CONTROL)&&((a&~1u)==0xa11100u||(a&~1u)==0xa11200u)){
        if(width==1&&(a&1))return 0;
        unsigned bit=(width==2?value>>8:value)&1u;
        unsigned request=(a&~1u)==0xa11100u?bit:s->z80_busreq;
        unsigned running=(a&~1u)==0xa11200u?bit:s->z80_reset_off;
        if(s->z80_control){if(!s->z80_control(s->z80_context,request,running)){s->execution_required=1;return -1;}}
        else if(running&&!request){s->execution_required=1;return -1;}
        s->z80_busreq=(uint8_t)request;s->z80_reset_off=(uint8_t)running;return 1;
    }
    if((s->enabled&AB_START_Z80_RAM)&&a>=0xa00000u&&a+width<=0xa02000u){
        if(s->z80_reset_off&&!s->z80_busreq){s->execution_required=1;return -1;}
        unsigned n=a-0xa00000u;if(width==1)s->z80_ram[n]=(uint8_t)value;
        else{s->z80_ram[n]=(uint8_t)(value>>8);s->z80_ram[n+1]=(uint8_t)value;}return 1;
    }
    return 0;
}
void ab_bus_enable_startup(ABMachineBus*b,unsigned features,uint8_t version){b->startup.enabled=features&AB_START_ALL;b->startup.version=version;}
