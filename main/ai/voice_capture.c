#include "voice_capture.h"
#include <string.h>
static const int steps[]={7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767};
static const int changes[]={-1,-1,-1,-1,2,4,6,8};
int16_t voice_capture_decode(voice_adpcm_t *s,uint8_t n){
 int step=steps[s->index],delta=step/8;if(n&1)delta+=step/4;if(n&2)delta+=step/2;if(n&4)delta+=step;
 s->predictor+=(n&8)?-delta:delta;if(s->predictor>32767)s->predictor=32767;if(s->predictor< -32768)s->predictor=-32768;
 s->index+=changes[n&7];if(s->index<0)s->index=0;if(s->index>88)s->index=88;++s->samples;return (int16_t)s->predictor;
}
void voice_capture_encode(voice_adpcm_t *s,uint8_t *out,const int16_t *pcm,size_t count){
 for(size_t i=0;i+1<count&&s->samples<VOICE_RATE*VOICE_SECONDS;i+=2){
  int sample=((int)pcm[i]+pcm[i+1])/2,diff=sample-s->predictor,step=steps[s->index];uint8_t n=0;
  if(diff<0){n=8;diff=-diff;}if(diff>=step){n|=4;diff-=step;}if(diff>=step/2){n|=2;diff-=step/2;}if(diff>=step/4)n|=1;
  unsigned at=s->samples;if(!(at&1))out[at/2]=n;else out[at/2]|=(uint8_t)(n<<4);
  voice_capture_decode(s,n);
 }
}
bool voice_capture_audible(const int16_t *pcm,size_t count){
 uint64_t sum=0;for(size_t i=0;i<count;++i){int v=pcm[i];sum+=(unsigned)(v<0?-v:v);}return count&&sum/count>180;
}
static void put(uint8_t *p,uint32_t n,unsigned bytes){for(unsigned i=0;i<bytes;++i)p[i]=(uint8_t)(n>>(8*i));}
void voice_wav_header(uint8_t h[44],unsigned samples){memset(h,0,44);memcpy(h,"RIFF",4);put(h+4,36+samples*2,4);memcpy(h+8,"WAVEfmt ",8);put(h+16,16,4);put(h+20,1,2);put(h+22,1,2);put(h+24,VOICE_RATE,4);put(h+28,VOICE_RATE*2,4);put(h+32,2,2);put(h+34,16,2);memcpy(h+36,"data",4);put(h+40,samples*2,4);}
