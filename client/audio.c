#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <stdio.h>

#include "audio.h"
#include "event.h"

void audio_linux_init(void);
void audio_android_init(void);

static int backoff = 0;
static uint8_t logtab[8192];


void audio_init(void)
{
	int i;

	for(i=0; i<8192; i++) {
		logtab[i] = log(i) * 28;
	}

#ifdef __ANDROID_API__
	audio_android_init();
#else
	audio_linux_init();
#endif

}


void audio_handle(int16_t *buf, int samples)
{
	int i, v;
	static double vavg = 0;

	for(i=0; i<samples; i++) {
		v = buf[i];
		v = (v < 0) ? -v : v;
		v = logtab[v >> 2];

		vavg = vavg * 0.999 + v * 0.001;

		if(!backoff && v - vavg > 80) {
			event_send("e:boom v=%d a=%.0f", v, vavg);
			backoff = 4000;
		}

		if(backoff) backoff--;
	}

}


/*
 * End
 */
