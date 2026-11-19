/* Internal qualification bus. This is not part of the public game API. */
#ifndef AB_MACHINE_BUS_H
#define AB_MACHINE_BUS_H
#include "m68k_step.h"
#include "genesis_vdp.h"
enum { AB_BUS_TRACE_CAP=131100 };
typedef struct {uint32_t address;uint16_t value;uint8_t kind,width;} ABBusTrace;
enum {AB_START_VERSION=1,AB_START_TMSS=2,AB_START_VDP_BYTE=4,AB_START_PARALLEL=8,
      AB_START_Z80_CONTROL=16,AB_START_Z80_RAM=32,AB_START_ALL=63};
typedef struct {
    unsigned enabled;uint8_t version,data[3],control[3],pad[2];
    uint8_t z80_ram[8192],z80_busreq,z80_reset_off,tmss_written;
    uint16_t tmss[2];unsigned execution_required;
    int (*z80_control)(void *,unsigned,unsigned);
    void *z80_context;
    int (*psg_write)(void *,uint8_t,uint64_t);
    int (*sound_advance)(void *,uint64_t);
    int (*fm_write)(void *,unsigned,uint8_t,uint64_t);
    int (*fm_read)(void *,unsigned,uint8_t *,uint64_t);
    void *sound_context;
} ABStartupDevices;
typedef struct {
    const uint8_t *rom;size_t rom_size;
    uint8_t ram[65536];GVDP vdp;
    ABBusTrace trace[AB_BUS_TRACE_CAP];size_t trace_count;
    uint32_t fault_address;unsigned fault;
    uint64_t stall_debt;unsigned dma_reads;
    uint64_t clock_master;
    unsigned timed_dma,dma_mode,dma_jobs;
    uint64_t dma_work;
    uint64_t dma_startup_clocks;
    ABStartupDevices startup;
    unsigned dma_finish_source;
} ABMachineBus;
void ab_bus_init(ABMachineBus *,const uint8_t *,size_t);
void ab_bus_clear_trace(ABMachineBus *);
/* Coarse asynchronous fill/copy busy duration. Transfer data is still eager. */
void ab_bus_advance_dma(ABMachineBus *,uint64_t master_cycles);
void ab_bus_enable_startup(ABMachineBus *,unsigned,uint8_t);
/* Internal device dispatch: 1 handled, 0 unmapped, -1 requires absent Z80. */
int ab_startup_read(ABMachineBus *,uint32_t,unsigned,uint16_t *);
int ab_startup_write(ABMachineBus *,uint32_t,unsigned,uint16_t);
extern const M68kStepBus ab_machine_bus_callbacks;
#endif
