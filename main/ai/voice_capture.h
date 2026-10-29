#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* 16 kHz microphone input, pair-average to 8 kHz, IMA ADPCM in RAM only.
 * Decode sequentially to ordinary 8 kHz/16-bit mono WAV for model upload. */
enum { VOICE_SECONDS=10,VOICE_RATE=8000,VOICE_CAPACITY=VOICE_RATE*VOICE_SECONDS/2 };
typedef struct { int predictor,index; unsigned samples; } voice_adpcm_t;
void voice_capture_encode(voice_adpcm_t *,uint8_t *,const int16_t *,size_t samples);
int16_t voice_capture_decode(voice_adpcm_t *,uint8_t nibble);
void voice_wav_header(uint8_t header[44],unsigned samples);
bool voice_capture_audible(const int16_t *,size_t);
