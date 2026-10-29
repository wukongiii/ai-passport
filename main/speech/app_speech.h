#pragma once
#include "speech_model.h"
typedef enum { SPEECH_IDLE,SPEECH_WAITING,SPEECH_PLAYING,SPEECH_DONE,SPEECH_FAILED,SPEECH_STOPPING } speech_state_t;
typedef struct {speech_state_t state;unsigned revision;int error;bool pending;unsigned volume;} app_speech_snapshot_t;
/* App-lifetime service. Nonblocking requests copy the text; never retain UI
 * pointers. A new request replaces the pending request, not the active phrase.
 * Stop invalidates pending/active work at the next PCM boundary. */
bool app_speech_start(void);
bool app_speech_say(const char *text);
void app_speech_stop(void);
void app_speech_snapshot(app_speech_snapshot_t *);

/* Worker-side codec lease. Acquire only after playback has stopped; while held,
 * new playback is refused. Caller stops reads/sleeps codec before release. */
bool app_speech_capture_acquire(void);
void app_speech_capture_release(void);
