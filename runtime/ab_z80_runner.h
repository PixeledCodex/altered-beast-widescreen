/* Internal ancillary sound-CPU runner. The 68000 path remains native AOT. */
#ifndef AB_Z80_RUNNER_H
#define AB_Z80_RUNNER_H
#include "ab_machine_bus.h"
#include "z80.h"
#include <setjmp.h>
typedef struct {
    z80 cpu;ABMachineBus *bus;
    uint64_t master_cycles,tstates,instructions,pc_hash;
    uint64_t upload_hash;unsigned releases,visits[65536];
    uint64_t irq_start,irq_end,last_irq_accept;
    unsigned irq_window,irq_line,irq_accepted;
    int64_t credit;
    unsigned bank,bank_writes,resets,executing,fault;
    unsigned genesis_io,port_reads,port_writes;
    uint16_t instruction_pc,fault_pc,fault_address;
    uint32_t fault_main_address;uint8_t fault_value,fault_write;
    jmp_buf escape;
} ABZ80Runner;
void ab_z80_attach(ABZ80Runner *,ABMachineBus *);
/* Genesis separate I/O space: undriven FF reads, discarded writes. Default
 * remains strict for the earlier unsupported-device diagnostic. */
void ab_z80_enable_genesis_io(ABZ80Runner *);
int ab_z80_advance_68k(ABZ80Runner *,uint64_t);
/* Internal level window [start,end), in absolute master clocks. Reset and bus
 * ownership do not extend it. Future instruction boundaries sample this window. */
void ab_z80_irq_window(ABZ80Runner *,uint64_t,uint64_t);
#endif
