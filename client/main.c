#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>

#include "mainloop.h"
#include "event.h"
#include "ntp.h"
#include "audio.h"
#include "log.h"

#define SERVER "82.94.235.106"


double clock_offset = 0;

int on_fd_ntp(int fd, void *data);
int on_tick(void *data);
int on_sigint(int signum, void *data);


static void on_ntp_offset(double offset, double deviation)
{
	clock_offset = offset;
	event_send("e:ntp o:%f d:%f", offset, deviation);
}

void bye(void)
{
	event_send("e:bye");
}


int main(int argc, char **argv)
{
	int c;
	int log_level = LG_INF;
	char *host = SERVER;
	char device_id[32];

	while((c = getopt(argc, argv, "a:l:")) != EOF) {
		switch(c) {
			case 'a':
				host = optarg;
				break;
			case 'l':
				log_level = atoi(optarg);
				break;
		}
	}

	log_init("fwloc", LOG_TO_STDERR, log_level, 1);

	gethostname(device_id, sizeof device_id);
	event_init(host, device_id);
	ntp_init(host, on_ntp_offset);
	audio_init();
	
	mainloop_timer_add(60, 0, on_tick, NULL);
	mainloop_signal_add(SIGINT, on_sigint, NULL);
	mainloop_signal_add(SIGTERM, on_sigint, NULL);

	event_send("e:hello");
	logf(LG_INF, "Start");

	mainloop_run();
	
	event_send("e:bye");
	logf(LG_INF, "Stop");

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
