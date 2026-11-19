/* Internal 48 kHz stereo host sink, separate from chip/reference hashes. */
#ifndef AB_AUDIO_H
#define AB_AUDIO_H
#include "ab_fm.h"
enum {AB_AUDIO_CAPACITY=8192,AB_AUDIO_RATE=48000,AB_MASTER_HZ=53693175};
typedef struct {
    int16_t pcm[AB_AUDIO_CAPACITY*2],psg,last[2];
    unsigned read,write,count,dropped;
    uint64_t next,emitted,hash;
} ABAudio;
void ab_audio_attach(ABAudio *,ABPsg *,ABFm *);
size_t ab_audio_read(ABAudio *,int16_t *,size_t);
#endif
