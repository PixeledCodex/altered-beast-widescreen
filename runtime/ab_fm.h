#ifndef AB_FM_H
#define AB_FM_H
#include "ab_psg.h"
typedef struct {
    void *impl;ABPsg *psg;ABMachineBus *bus;
    uint64_t clock,samples,sample_hash;
    unsigned writes,reads,busy_reads,timer_expiries[2],fault;
    void (*sample)(void *,uint64_t,int32_t,int32_t);void *sample_context;
} ABFm;
int ab_fm_attach(ABFm*,ABPsg*,ABMachineBus*);
int ab_fm_advance(ABFm*,uint64_t);
int ab_fm_write(ABFm*,unsigned,uint8_t,uint64_t);
int ab_fm_read(ABFm*,unsigned,uint8_t*,uint64_t);
void ab_fm_detach(ABFm*);
#endif
