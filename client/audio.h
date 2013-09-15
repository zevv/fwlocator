
#ifndef audio_h
#define audio_h

struct audio;

void audio_init(void);
void audio_handle(int16_t *data, int samples);

#endif
