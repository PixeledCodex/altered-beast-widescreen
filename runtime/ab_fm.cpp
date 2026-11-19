/* Own interface to the pinned BSD ymfm core. Unlike the legacy shadow wrapper,
 * busy and timer reads are driven by explicit master-clock deadlines. */
extern "C" {
#include "ab_fm.h"
}
#include "ymfm_opn.h"
#include <new>
#include <cstring>
#include <algorithm>
struct FmInterface:ymfm::ymfm_interface {
    ABFm *owner;uint64_t now=0,busy=0,timers[2]={UINT64_MAX,UINT64_MAX};
    explicit FmInterface(ABFm *p):owner(p){}
    void ymfm_set_timer(uint32_t n,int32_t clocks) override {if(n<2)timers[n]=clocks<0?UINT64_MAX:now+uint64_t(clocks)*7;}
    void ymfm_set_busy_end(uint32_t clocks) override {busy=now+uint64_t(clocks)*7;}
    bool ymfm_is_busy() override {return now<busy;}
    void expire(unsigned n){timers[n]=UINT64_MAX;++owner->timer_expiries[n];m_engine->engine_timer_expired(n);}
};
struct FmEngine {
    FmInterface iface;ymfm::ym2612 chip;uint64_t next_sample=1008;
    explicit FmEngine(ABFm *p):iface(p),chip(iface){chip.reset();}
};
extern "C" int ab_fm_advance(ABFm*p,uint64_t target){
    if(!p->impl||p->fault||target<p->clock){p->fault=1;return 0;}
    auto &e=*static_cast<FmEngine*>(p->impl);
    while(true){uint64_t next=std::min(e.next_sample,std::min(e.iface.timers[0],e.iface.timers[1]));if(next>target)break;
        e.iface.now=next;
        for(unsigned n=0;n<2;++n)if(e.iface.timers[n]==next)e.iface.expire(n);
        if(e.next_sample==next){ymfm::ym2612::output_data sample;e.chip.generate(&sample);++p->samples;
            for(unsigned c=0;c<2;++c)for(unsigned b=0;b<4;++b){p->sample_hash^=uint8_t(uint32_t(sample.data[c])>>(b*8));p->sample_hash*=UINT64_C(1099511628211);}e.next_sample+=1008;
            if(p->sample){
                if(p->psg&&!ab_psg_advance(p->psg,next))return 0;
                p->sample(p->sample_context,next,sample.data[0],sample.data[1]);
            }
        }
    }
    e.iface.now=target;p->clock=target;
    return !p->psg||ab_psg_advance(p->psg,target);
}
extern "C" int ab_fm_write(ABFm*p,unsigned port,uint8_t value,uint64_t stamp){if(port>3||!ab_fm_advance(p,stamp))return 0;static_cast<FmEngine*>(p->impl)->chip.write(port,value);++p->writes;return 1;}
extern "C" int ab_fm_read(ABFm*p,unsigned port,uint8_t*value,uint64_t stamp){if(port>3||!ab_fm_advance(p,stamp))return 0;*value=static_cast<FmEngine*>(p->impl)->chip.read_status();++p->reads;if(*value&0x80)++p->busy_reads;return 1;}
static int fm_write(void*u,unsigned port,uint8_t value,uint64_t stamp){return ab_fm_write(static_cast<ABFm*>(u),port,value,stamp);}
static int fm_read(void*u,unsigned port,uint8_t*value,uint64_t stamp){return ab_fm_read(static_cast<ABFm*>(u),port,value,stamp);}
static int psg_write(void*u,uint8_t value,uint64_t stamp){auto*p=static_cast<ABFm*>(u);return ab_fm_advance(p,stamp)&&p->psg&&ab_psg_write_timed(p->psg,value,stamp);}
static int advance(void*u,uint64_t stamp){return ab_fm_advance(static_cast<ABFm*>(u),stamp);}
extern "C" int ab_fm_attach(ABFm*p,ABPsg*psg,ABMachineBus*b){
    std::memset(p,0,sizeof*p);p->psg=psg;p->bus=b;p->sample_hash=UINT64_C(14695981039346656037);p->impl=new(std::nothrow) FmEngine(p);if(!p->impl)return 0;
    b->startup.fm_write=fm_write;b->startup.fm_read=fm_read;b->startup.psg_write=psg_write;b->startup.sound_advance=advance;b->startup.sound_context=p;return 1;
}
extern "C" void ab_fm_detach(ABFm*p){delete static_cast<FmEngine*>(p->impl);p->impl=nullptr;if(p->bus&&p->bus->startup.sound_context==p){p->bus->startup.fm_write=nullptr;p->bus->startup.fm_read=nullptr;p->bus->startup.psg_write=nullptr;p->bus->startup.sound_advance=nullptr;p->bus->startup.sound_context=nullptr;}}
