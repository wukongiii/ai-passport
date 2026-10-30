#pragma once
#include "voice_capture.h"
#include <stdlib.h>
#include <string.h>

/* Worker-owned ADPCM blocks. No contiguous recording or realloc/copy peak.
 * Transport consumes the buffer and frees blocks after successful writes. */
enum { VOICE_BLOCK_BYTES=2048, VOICE_BLOCK_COUNT=(VOICE_CAPACITY+VOICE_BLOCK_BYTES-1)/VOICE_BLOCK_BYTES };
typedef struct {uint8_t *blocks[VOICE_BLOCK_COUNT];size_t size;} voice_buffer_t;
static inline void voice_buffer_release(voice_buffer_t *b){
 if(!b)return;
 for(unsigned i=0;i<VOICE_BLOCK_COUNT;++i){free(b->blocks[i]);b->blocks[i]=NULL;}
 b->size=0;
}
static inline bool voice_buffer_append(voice_buffer_t *b,const uint8_t *data,size_t size){
 if(size>VOICE_CAPACITY-b->size)return false;
 while(size){size_t block=b->size/VOICE_BLOCK_BYTES,at=b->size%VOICE_BLOCK_BYTES;
  if(!b->blocks[block]){b->blocks[block]=malloc(VOICE_BLOCK_BYTES);if(!b->blocks[block])return false;}
  size_t n=VOICE_BLOCK_BYTES-at;if(n>size)n=size;
  memcpy(b->blocks[block]+at,data,n);b->size+=n;data+=n;size-=n;
 }return true;
}
static inline uint8_t voice_buffer_byte(const voice_buffer_t *b,size_t at){return b->blocks[at/VOICE_BLOCK_BYTES][at%VOICE_BLOCK_BYTES];}
static inline void voice_buffer_discard(voice_buffer_t *b,size_t consumed){
 for(size_t i=0;i<consumed/VOICE_BLOCK_BYTES&&i<VOICE_BLOCK_COUNT;++i){free(b->blocks[i]);b->blocks[i]=NULL;}
}
