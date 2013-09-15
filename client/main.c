#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include "mainloop.h"
#include "event.h"
#include "ntp.h"
#include "audio.h"

#define SERVER "82.94.235.106"


double clock_offset = 0;

int on_fd_ntp(int fd, void *data);
int on_tick(void *data);
int on_sigint(int signum, void *data);


static void on_ntp_offset(double offset, double delay)
{
	clock_offset = offset;
	event_send("e:ntp o:%f d:%f", offset, delay);
}

void bye(void)
{
	event_send("e:bye");
}


int main(int argc, char **argv)
{
	event_init(SERVER, HOST_ID);
	ntp_init(SERVER, on_ntp_offset);
	audio_init();
	
	mainloop_timer_add(60, 0, on_tick, NULL);
	mainloop_signal_add(SIGINT, on_sigint, NULL);
	mainloop_signal_add(SIGTERM, on_sigint, NULL);

	event_send("e:hello");

	mainloop_run();
	
	event_send("e:bye");

	atexit(bye);

	return 0;
}


int on_tick(void *data)
{
	event_send("e:ping");
	return 1;
}


int on_sigint(int signum, void *data)
{
	mainloop_stop();
	return 1;
}


/*
 * End
 */
