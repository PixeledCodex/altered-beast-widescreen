#include "ab_audio.h"
#include <string.h>
static int16_t clamp(int64_t v){return (int16_t)(v>32767?32767:v< -32768?-32768:v);}
static void psg_sample(void*u,uint64_t time,int16_t value){(void)time;((ABAudio*)u)->psg=value;}
static void fm_sample(void*u,uint64_t time,int32_t left,int32_t right){
    ABAudio*a=u;
    /* Causal sample hold. Host gain is a presentation choice, not chip parity. */
    while(a->next<=time*AB_AUDIO_RATE){
        if(a->count==AB_AUDIO_CAPACITY)++a->dropped;
        else {for(unsigned c=0;c<2;++c)a->pcm[a->write*2+c]=a->last[c];a->write=(a->write+1)%AB_AUDIO_CAPACITY;++a->count;}
        for(unsigned c=0;c<2;++c)for(unsigned b=0;b<2;++b){a->hash^=(uint8_t)((uint16_t)a->last[c]>>(8*b));a->hash*=UINT64_C(1099511628211);}
        ++a->emitted;a->next+=AB_MASTER_HZ;
    }
    a->last[0]=clamp(((int64_t)left*3+a->psg)/4);
    a->last[1]=clamp(((int64_t)right*3+a->psg)/4);
}
void ab_audio_attach(ABAudio*a,ABPsg*p,ABFm*f){
    memset(a,0,sizeof*a);a->next=AB_MASTER_HZ;a->hash=UINT64_C(14695981039346656037);
    p->sample=psg_sample;p->sample_context=a;f->sample=fm_sample;f->sample_context=a;
}
size_t ab_audio_read(ABAudio*a,int16_t*out,size_t frames){
    size_t n=frames<a->count?frames:a->count;
    for(size_t i=0;i<n;++i){out[i*2]=a->pcm[a->read*2];out[i*2+1]=a->pcm[a->read*2+1];a->read=(a->read+1)%AB_AUDIO_CAPACITY;}
    a->count-=(unsigned)n;return n;
}
