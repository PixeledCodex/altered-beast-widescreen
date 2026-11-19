#include "ab_machine_bus.h"
#include <string.h>
static M68kStepBusResult failed(ABMachineBus*b,uint32_t a){b->fault=1;b->fault_address=a;return M68K_BUS_FAULT;}
static void begin_dma(ABMachineBus*b,unsigned mode,unsigned old_length,unsigned old_source){
    if(!b->timed_dma)return;
    /* The backend transfers data eagerly. Keep completion-register effects
     * deferred until the existing timed fill/copy busy window actually ends. */
    b->dma_finish_source=b->vdp.reg[21]|(b->vdp.reg[22]<<8);
    b->vdp.reg[19]=(uint8_t)old_length;b->vdp.reg[20]=(uint8_t)(old_length>>8);
    b->vdp.reg[21]=(uint8_t)old_source;b->vdp.reg[22]=(uint8_t)(old_source>>8);
    unsigned length=old_length?old_length:65536;
    b->dma_work=(uint64_t)length*3420;b->dma_mode=mode;++b->dma_jobs;b->vdp.dma_active=1;
    unsigned slots=b->vdp.in_vblank||!(b->vdp.reg[1]&0x40)?((b->vdp.reg[12]&1)?204u:166u):((b->vdp.reg[12]&1)?18u:16u);
    /* Reference vdp_dma_update: initial fill write costs two slots once. */
    b->dma_startup_clocks=mode==2?2u*(3420u/slots):0;
}
void ab_bus_advance_dma(ABMachineBus*b,uint64_t clocks){
    if(!b->dma_work)return;
    if(clocks<b->dma_startup_clocks){b->dma_startup_clocks-=clocks;return;}
    clocks-=b->dma_startup_clocks;b->dma_startup_clocks=0;
    GVDP*v=&b->vdp;unsigned wide=v->reg[12]&1,blank=v->in_vblank||!(v->reg[1]&0x40);
    unsigned slots=blank?(wide?204u:166u):(wide?18u:16u);
    /* Fill uses every access slot; copy needs a read plus a write. */
    unsigned rate=b->dma_mode==2?slots:slots/2;
    if(clocks>=(b->dma_work+rate-1)/rate){b->dma_work=0;v->dma_active=0;v->reg[19]=v->reg[20]=0;v->reg[21]=(uint8_t)b->dma_finish_source;v->reg[22]=(uint8_t)(b->dma_finish_source>>8);}
    else b->dma_work-=clocks*rate;
}
static M68kStepBusResult record(ABMachineBus*b,uint32_t a,uint16_t v,unsigned kind,unsigned width){
    if(b->trace_count==AB_BUS_TRACE_CAP)return failed(b,a);
    ABBusTrace *t=&b->trace[b->trace_count++];t->address=a;t->value=v;t->kind=(uint8_t)kind;t->width=(uint8_t)width;return M68K_BUS_OK;
}
static int memory_word(ABMachineBus*b,uint32_t a,uint16_t*v){
    if(a&1)return 0;
    if(a<b->rom_size&&b->rom_size-a>=2){*v=(uint16_t)((b->rom[a]<<8)|b->rom[a+1]);return 1;}
    if(a>=0xe00000u){unsigned n=a&65535u;*v=(uint16_t)((b->ram[n]<<8)|b->ram[n+1]);return 1;}
    return 0;
}
static M68kStepBusResult read8(void*u,uint32_t a,uint8_t*v){
    ABMachineBus*b=u;a&=0xffffffu;if(a<b->rom_size)*v=b->rom[a];else if(a>=0xe00000u)*v=b->ram[a&65535u];else{
        uint16_t value;
        if((b->startup.enabled&AB_START_VDP_BYTE)&&a>=0xc00004u&&a<0xc00008u){value=gvdp_read_control(&b->vdp);*v=(uint8_t)(a&1?value:value>>8);}
        else{int handled=ab_startup_read(b,a,1,&value);if(handled!=1)return failed(b,a);*v=(uint8_t)value;}
    }
    return record(b,a,*v,'R',1);
}
static M68kStepBusResult read16(void*u,uint32_t a,uint16_t*v){
    ABMachineBus*b=u;a&=0xffffffu;if(a&1)return failed(b,a);
    if(!memory_word(b,a,v)){
        if(a>=0xc00000u&&a<0xc00004u)*v=gvdp_read_data(&b->vdp);
        else if(a>=0xc00004u&&a<0xc00008u)*v=gvdp_read_control(&b->vdp);
        else if(a>=0xc00008u&&a<0xc00010u)*v=gvdp_read_hv_counter(&b->vdp);
        else if(ab_startup_read(b,a,2,v)!=1)return failed(b,a);
    }
    return record(b,a,*v,'R',2);
}
static M68kStepBusResult fetch16(void*u,uint32_t a,uint16_t*v){
    ABMachineBus*b=u;a&=0xffffffu;
    return memory_word(b,a,v)?M68K_BUS_OK:failed(b,a);
}
static M68kStepBusResult write8(void*u,uint32_t a,uint8_t v){
    ABMachineBus*b=u;a&=0xffffffu;
    if(a<0xe00000u){
        if(b->trace_count==AB_BUS_TRACE_CAP)return failed(b,a);
        if(a==0xc00011u&&b->startup.psg_write&&b->startup.psg_write(b->startup.sound_context,v,b->clock_master))return record(b,a,v,'W',1);
        if((b->startup.enabled&AB_START_VDP_BYTE)&&a>=0xc00000u&&a<0xc00004u){
            if(b->dma_work)return failed(b,a);
            unsigned fill=b->vdp.dma_fill_pending;
            unsigned length=b->vdp.reg[19]|(b->vdp.reg[20]<<8),source=b->vdp.reg[21]|(b->vdp.reg[22]<<8);
            if(record(b,a,v,'W',1)!=M68K_BUS_OK)return M68K_BUS_FAULT;gvdp_write_data(&b->vdp,(uint16_t)(v*0x101u));
            if(fill)begin_dma(b,2,length,source);
            b->stall_debt+=gvdp_consume_68k_stall(&b->vdp);return M68K_BUS_OK;
        }
        if(ab_startup_write(b,a,1,v)!=1)return failed(b,a);
        return record(b,a,v,'W',1);
    }
    if(record(b,a,v,'W',1)!=M68K_BUS_OK)return M68K_BUS_FAULT;
    b->ram[a&65535u]=v;return M68K_BUS_OK;
}
/* Reject unsupported DMA source mappings before completing the command. The
 * upstream DMA callback has no error result; permitting it would silently write
 * invented source data. Preflight performs no bus access or device side effect. */
static int dma_sources_mapped(ABMachineBus*b,uint16_t second){
    GVDP*v=&b->vdp;
    unsigned code=(v->code&3u)|((second>>2)&0x3cu);
    if(!v->control_pending||!(code&0x20)||!(v->reg[1]&0x10)||(v->reg[23]&0x80))return 1;
    unsigned length=v->reg[19]|(v->reg[20]<<8);if(!length)length=65536;
    if(b->trace_count+1u+length>AB_BUS_TRACE_CAP){failed(b,0xc00004u);return 0;}
    uint32_t source=(v->reg[21]|(v->reg[22]<<8)|((v->reg[23]&127u)<<16))<<1;
    for(unsigned i=0;i<length;++i){uint16_t ignored;if(!memory_word(b,source,&ignored)){failed(b,source);return 0;}source=(source&0xfe0000u)|((source+2)&0x1ffffu);}
    return 1;
}
static M68kStepBusResult write16(void*u,uint32_t a,uint16_t v){
    ABMachineBus*b=u;a&=0xffffffu;if(a&1)return failed(b,a);
    int ram=a>=0xe00000u,data=a>=0xc00000u&&a<0xc00004u,control=a>=0xc00004u&&a<0xc00008u;
    if(b->dma_work&&(data||control))return failed(b,a); /* Unqualified overlapping VDP writes. */
    if(!ram&&!data&&!control){
        if(b->trace_count==AB_BUS_TRACE_CAP)return failed(b,a);
        if(ab_startup_write(b,a,2,v)!=1)return failed(b,a);return record(b,a,v,'W',2);
    }
    if(control&&!dma_sources_mapped(b,v))return M68K_BUS_FAULT;
    if(record(b,a,v,'W',2)!=M68K_BUS_OK)return M68K_BUS_FAULT;
    unsigned fill=data&&b->vdp.dma_fill_pending;
    unsigned length=b->vdp.reg[19]|(b->vdp.reg[20]<<8),source=b->vdp.reg[21]|(b->vdp.reg[22]<<8);
    unsigned copy=control&&b->vdp.control_pending&&(b->vdp.reg[1]&0x10)&&((b->vdp.reg[23]&0xc0)==0xc0)&&(((b->vdp.code&3u)|((v>>2)&0x3cu))&0x20);
    if(ram){unsigned n=a&65535u;b->ram[n]=(uint8_t)(v>>8);b->ram[n+1]=(uint8_t)v;}
    else if(data)gvdp_write_data(&b->vdp,v);
    else gvdp_write_control(&b->vdp,v);
    if(fill)begin_dma(b,2,length,source);if(copy)begin_dma(b,3,length,source);
    b->stall_debt+=gvdp_consume_68k_stall(&b->vdp);
    return M68K_BUS_OK;
}
static uint16_t dma_read(void*u,uint32_t a){
    ABMachineBus*b=u;uint16_t value=0;a&=0xffffffu;
    if(!memory_word(b,a,&value)){failed(b,a);return 0xffff;}
    ++b->dma_reads;record(b,a,value,'D',2);return value;
}
const M68kStepBus ab_machine_bus_callbacks={read8,read16,write8,write16,fetch16,NULL,NULL,NULL};
void ab_bus_init(ABMachineBus*b,const uint8_t*rom,size_t size){
    memset(b,0,sizeof *b);b->rom=rom;b->rom_size=size;gvdp_init(&b->vdp);b->vdp.bus_read=dma_read;b->vdp.bus_read_user=b;
}
void ab_bus_clear_trace(ABMachineBus*b){b->trace_count=0;b->dma_reads=0;b->fault=0;b->fault_address=0;b->stall_debt=0;}
