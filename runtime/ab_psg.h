#ifndef AB_PSG_H
#define AB_PSG_H
#include "ab_machine_bus.h"
typedef struct {uint64_t clock,samples,sample_hash,write_hash;unsigned writes,fault;
    void (*sample)(void *,uint64_t,int16_t);void *sample_context;} ABPsg;
void ab_psg_attach(ABPsg*,ABMachineBus*);
int ab_psg_advance(ABPsg*,uint64_t);
int ab_psg_write_timed(ABPsg*,uint8_t,uint64_t);
#endif
