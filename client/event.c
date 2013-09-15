
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/time.h>

#include "event.h"
	
static struct sockaddr_in sa;
static int fd;

extern double clock_offset;

void event_init(const char *ip)
{
	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(9339);
	sa.sin_addr.s_addr = inet_addr(ip);

	fd = socket(AF_INET, SOCK_DGRAM, 0);
}


void event_send(const char *fmt, ...)
{
	char buf[1024];
	int r = 0;
	va_list va;
	double t;
	struct timeval tv;

	gettimeofday(&tv, NULL);
	t = tv.tv_sec + (tv.tv_usec) / 1.0E6;
	t += clock_offset;

	va_start(va, fmt);
	r += snprintf(buf + r, sizeof buf - r, "t:%f ", t);
	r += vsnprintf(buf + r, sizeof buf - r, fmt, va);
	r += snprintf(buf + r, sizeof buf - r, "\n");
	va_end(va);

	r = sendto(fd, buf, r, 0, (struct sockaddr *)&sa, sizeof sa);
}

/*
 * End
 */
