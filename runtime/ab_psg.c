#include "ab_psg.h"
#include "sn76489.h"
#include <string.h>
int ab_psg_advance(ABPsg*p,uint64_t target){
    if(target<p->clock){p->fault=1;return 0;}
    int16_t buffer[4096];
    while(p->clock<target){uint64_t left=target-p->clock;uint32_t chunk=(uint32_t)(left>4096u*240u?4096u*240u:left);
        psg_advance(chunk);p->clock+=chunk;
        size_t n;while((n=psg_render(buffer,4096))!=0){
            for(size_t i=0;i<n;++i){
                for(unsigned b=0;b<2;++b){p->sample_hash^=(uint8_t)((uint16_t)buffer[i]>>(b*8));p->sample_hash*=UINT64_C(1099511628211);}
                ++p->samples;if(p->sample)p->sample(p->sample_context,p->samples*240,buffer[i]);
            }
        }
    }
    return 1;
}
int ab_psg_write_timed(ABPsg*p,uint8_t value,uint64_t stamp){if(!ab_psg_advance(p,stamp))return 0;psg_write(value);++p->writes;p->write_hash^=value;p->write_hash*=UINT64_C(1099511628211);return 1;}
static int write(void*u,uint8_t value,uint64_t stamp){return ab_psg_write_timed(u,value,stamp);}
static int advance(void*u,uint64_t stamp){return ab_psg_advance(u,stamp);}
void ab_psg_attach(ABPsg*p,ABMachineBus*b){memset(p,0,sizeof*p);psg_init();p->sample_hash=p->write_hash=UINT64_C(14695981039346656037);b->startup.psg_write=write;b->startup.sound_advance=advance;b->startup.sound_context=p;}
