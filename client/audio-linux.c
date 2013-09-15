
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <linux/soundcard.h>
#include <sys/ioctl.h>

#include "audio.h"
#include "mainloop.h"


int on_fd_audio(int fd, void *data)
{
	int16_t buf[16];
	int r = read(fd, buf, sizeof(buf));
	audio_handle(buf, r/2);
	return 0;
}


void audio_linux_init(void)
{
	int fd = open("/dev/dsp", O_RDONLY);
	int format = AFMT_S16_LE;
	ioctl(fd, SOUND_PCM_SETFMT, &format);

	mainloop_fd_add(fd, FD_READ, on_fd_audio, NULL);
}



/*
 * End
 */
