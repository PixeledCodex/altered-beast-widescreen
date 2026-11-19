#include "ab_z80_runner.h"
#include <string.h>
void ab_z80_core_sample_interrupts(z80 *);
void ab_z80_irq_window(ABZ80Runner*s,uint64_t start,uint64_t end){
    s->irq_window=1;s->irq_start=start;s->irq_end=end;
    s->irq_line=s->master_cycles>=start&&s->master_cycles<end;
    s->cpu.int_pending=0;
}
static void fail(ABZ80Runner*s,uint16_t a,uint32_t mapped,uint8_t value,int write){
    s->fault=1;s->fault_pc=s->instruction_pc;s->fault_address=a;s->fault_main_address=mapped;s->fault_value=value;s->fault_write=(uint8_t)write;
    longjmp(s->escape,1); /* Do not return an invented peripheral read value. */
}
static uint8_t read_byte(void*u,uint16_t a){
    ABZ80Runner*s=u;if(a<0x4000)return s->bus->startup.z80_ram[a&8191u];
    if(a>=0x4000&&a<0x6000&&s->bus->startup.fm_read){uint8_t v;if(s->bus->startup.fm_read(s->bus->startup.sound_context,a&3u,&v,s->master_cycles-(uint64_t)s->credit))return v;}
    if(a>=0x8000){uint32_t mapped=(s->bank<<15)|(a&0x7fffu);uint8_t v;
        if((mapped<s->bus->rom_size||mapped>=0xe00000u)&&ab_machine_bus_callbacks.read8(s->bus,mapped,&v)==M68K_BUS_OK)return v;
        fail(s,a,mapped,0,0);
    }
    fail(s,a,(a>=0x4000&&a<0x6000)?0xa04000u+(a&3u):a==0x7f11?0xc00011u:0,0,0);return 0;
}
static void write_byte(void*u,uint16_t a,uint8_t v){
    ABZ80Runner*s=u;if(a<0x4000){s->bus->startup.z80_ram[a&8191u]=v;return;}
    if(a>=0x6000&&a<0x6100){s->bank=((s->bank>>1)|((v&1u)<<8))&511u;++s->bank_writes;return;}
    if(a>=0x4000&&a<0x6000&&s->bus->startup.fm_write&&s->bus->startup.fm_write(s->bus->startup.sound_context,a&3u,v,s->master_cycles-(uint64_t)s->credit))return;
    if(a>=0x8000){uint32_t mapped=(s->bank<<15)|(a&0x7fffu);
        if(mapped==0xc00011u&&s->bus->startup.psg_write&&s->bus->startup.psg_write(s->bus->startup.sound_context,v,s->master_cycles-(uint64_t)s->credit))return;
        if(mapped>=0xe00000u&&ab_machine_bus_callbacks.write8(s->bus,mapped,v)==M68K_BUS_OK)return;
        fail(s,a,mapped,v,1);
    }
    if(a==0x7f11&&s->bus->startup.psg_write&&s->bus->startup.psg_write(s->bus->startup.sound_context,v,s->master_cycles-(uint64_t)s->credit))return;
    fail(s,a,(a>=0x4000&&a<0x6000)?0xa04000u+(a&3u):a==0x7f11?0xc00011u:0,v,1);
}
static uint8_t port_in(z80*z,uint8_t p){ABZ80Runner*s=z->userdata;if(s->genesis_io){++s->port_reads;return 0xff;}fail(s,(uint16_t)((z->b<<8)|p),0,0,0);return 0;}
static void port_out(z80*z,uint8_t p,uint8_t v){ABZ80Runner*s=z->userdata;if(s->genesis_io){++s->port_writes;return;}fail(s,(uint16_t)((z->b<<8)|p),0,v,1);}
void ab_z80_enable_genesis_io(ABZ80Runner*s){s->genesis_io=1;}
static int control(void*u,unsigned request,unsigned reset_off){
    ABZ80Runner*s=u;if(s->fault||s->executing)return 0;
    /* The coordinator only arbitrates between completed Z80 instructions.
     * Reset changes PC/IFF/IM, preserving RAM and general registers. */
    if(s->bus->startup.z80_reset_off&&!reset_off){
        s->cpu.pc=0;s->cpu.iff1=s->cpu.iff2=0;s->cpu.halted=0;s->cpu.interrupt_mode=0;s->cpu.iff_delay=0;
        s->cpu.int_pending=s->cpu.nmi_pending=0;s->cpu.int_data=0xff;s->cpu.i=s->cpu.r=0;++s->resets;
    }
    if(!reset_off||!s->bus->startup.z80_reset_off)s->credit=0;
    if(reset_off&&!request&&(!s->bus->startup.z80_reset_off||s->bus->startup.z80_busreq)){
        s->upload_hash=UINT64_C(14695981039346656037);
        for(unsigned i=0;i<8192;++i){s->upload_hash^=s->bus->startup.z80_ram[i];s->upload_hash*=UINT64_C(1099511628211);}++s->releases;
    }
    return 1;
}
void ab_z80_attach(ABZ80Runner*s,ABMachineBus*b){
    memset(s,0,sizeof *s);s->bus=b;z80_init(&s->cpu);s->cpu.read_byte=read_byte;s->cpu.write_byte=write_byte;s->cpu.port_in=port_in;s->cpu.port_out=port_out;s->cpu.userdata=s;
    s->pc_hash=UINT64_C(14695981039346656037);b->startup.z80_control=control;b->startup.z80_context=s;
}
int ab_z80_advance_68k(ABZ80Runner*s,uint64_t cycles){
    if(s->fault)return 0;
    if(cycles>UINT64_MAX/7||s->master_cycles>UINT64_MAX-cycles*7||cycles*7>INT64_MAX-(s->credit>0?s->credit:0)){s->fault=2;return 0;}
    s->master_cycles+=cycles*7;
    s->bus->clock_master=s->master_cycles;
    if(s->irq_window){
        s->irq_line=s->master_cycles>=s->irq_start&&s->master_cycles<s->irq_end;
        if(!s->irq_line)s->cpu.int_pending=0;
    }
    if(!s->bus->startup.z80_reset_off||s->bus->startup.z80_busreq){
        s->credit+=(int64_t)(cycles*7);if(s->credit>0)s->credit=0;
        return !s->bus->startup.sound_advance||s->bus->startup.sound_advance(s->bus->startup.sound_context,s->master_cycles);
    }
    s->credit+=(int64_t)(cycles*7);
    while(s->credit>0){
        s->instruction_pc=s->cpu.pc;s->executing=1;++s->visits[s->instruction_pc];
        if(setjmp(s->escape)){s->executing=0;return 0;}
        unsigned long before=s->cpu.cyc;
        if(s->irq_window){
            int blocked=z80_step_no_interrupts(&s->cpu);
            uint64_t boundary=s->master_cycles-(uint64_t)s->credit+(unsigned long)(s->cpu.cyc-before)*15;
            s->cpu.int_pending=boundary>=s->irq_start&&boundary<s->irq_end;
            s->cpu.int_data=0xff;
            if(!blocked){
                unsigned requested=s->cpu.int_pending;
                ab_z80_core_sample_interrupts(&s->cpu);
                if(requested&&!s->cpu.int_pending){++s->irq_accepted;s->last_irq_accept=boundary;}
            }
        }else z80_step(&s->cpu);
        uint64_t spent=(unsigned long)(s->cpu.cyc-before);
        s->executing=0;if(!spent){s->fault=2;return 0;}
        s->credit-=(int64_t)(spent*15);s->tstates+=spent;++s->instructions;
        for(unsigned i=0;i<2;++i){s->pc_hash^=(uint8_t)(s->instruction_pc>>(i*8));s->pc_hash*=UINT64_C(1099511628211);}
    }
    if(s->irq_window&&!s->irq_line)s->cpu.int_pending=0;
    return !s->bus->startup.sound_advance||s->bus->startup.sound_advance(s->bus->startup.sound_context,s->master_cycles);
}
