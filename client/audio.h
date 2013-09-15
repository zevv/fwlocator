
#ifndef audio_h
#define audio_h

struct audio;

struct audio *audio_open(void);
void audio_close(struct audio *audio);

#endif
