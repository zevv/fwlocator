

#include <stdio.h>
#include <limits.h>
#include <stdint.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>

#include "ntp.h"
#include "mainloop.h"


#define	LI_NOWARNING	(0 << 6)	/* no warning */
#define	LI_PLUSSEC	(1 << 6)	/* add a second (61 seconds) */
#define	LI_MINUSSEC	(2 << 6)	/* minus a second (59 seconds) */
#define	LI_ALARM	(3 << 6)	/* alarm condition */

#define	MODEMASK	(7 << 0)
#define	VERSIONMASK	(7 << 3)
#define LIMASK		(3 << 6)

#define	MODE_RES0	0	/* reserved */
#define	MODE_SYM_ACT	1	/* symmetric active */
#define	MODE_SYM_PAS	2	/* symmetric passive */
#define	MODE_CLIENT	3	/* client */
#define	MODE_SERVER	4	/* server */
#define	MODE_BROADCAST	5	/* broadcast */
#define	MODE_RES1	6	/* reserved for NTP control message */
#define	MODE_RES2	7	/* reserved for private use */

#define	JAN_1970	2208988800UL	/* 1970 - 1900 in seconds */
#define	JAN_2030	1893456000UL + JAN_1970	/* 1. 1. 2030 00:00:00 */

#define	NTP_VERSION	4
#define	NTP_MAXSTRATUM	15

struct l_fixedpt {
	uint32_t i;
	uint32_t f;
};


struct s_fixedpt {
	uint16_t i;
	uint16_t f;
};


struct ntp_msg {
        uint8_t status;        /* status of local clock and leap info */
        uint8_t stratum;       /* Stratum level */
        uint8_t ppoll;         /* poll value */
        int8_t precision;
        struct s_fixedpt rootdelay;
        struct s_fixedpt dispersion;
        uint32_t refid;
        struct l_fixedpt reftime;
        struct l_fixedpt orgtime;
        struct l_fixedpt rectime;
        struct l_fixedpt xmttime;
} __attribute__ (( __packed__ ));


struct ntp {
	int fd;
	int n;
	int when;
	struct sockaddr_in sa;
	struct ntp_msg req;
	struct ntp_msg rsp;
	double t_send;
	double t_recv;
	double delay;
	double offset;
	void (*offset_cb)(double offset, double delay);
};



void ntp_sync(struct ntp *ntp)
{
	struct ntp_msg *req = &ntp->req;
	struct timeval tv;

	memset(req, 0, sizeof *req);

	req->status = MODE_CLIENT | (NTP_VERSION << 3);
	req->precision = 0xfa;
	req->xmttime.i = rand();
	req->xmttime.f = rand();

	sendto(ntp->fd, req, sizeof *req, 0, (struct sockaddr *)&ntp->sa, sizeof(ntp->sa));

	gettimeofday(&tv, NULL);
	ntp->t_send = tv.tv_sec + JAN_1970 + 1.0e-6 * tv.tv_usec;
}


static double lfp_to_d(struct l_fixedpt lfp)
{       
	lfp.i = ntohl(lfp.i);
	lfp.f = ntohl(lfp.f);
	return (double)(lfp.i) + ((double)lfp.f/ UINT_MAX);
}


static int on_fd_ntp(int fd, void *data)
{
	struct ntp *ntp = data;
	int r;
	struct ntp_msg *rsp = &ntp->rsp;
	struct timeval tv;
	
	gettimeofday(&tv, NULL);
	ntp->t_recv = tv.tv_sec + JAN_1970 + 1.0e-6 * tv.tv_usec;

	r = recv(ntp->fd, rsp, sizeof *rsp, 0);
	if(r != sizeof *rsp) return 0;

	/*
	 * From RFC 2030 (with a correction to the delay math):
	 *
	 *     Timestamp Name          ID   When Generated
	 *     ------------------------------------------------------------
	 *     Originate Timestamp     T1   time request sent by client
	 *     Receive Timestamp       T2   time request received by server
	 *     Transmit Timestamp      T3   time reply sent by server
	 *     Destination Timestamp   T4   time reply received by client
	 *
	 *  The roundtrip delay d and local clock offset t are defined as
	 *
	 *    d = (T4 - T1) - (T3 - T2)     t = ((T2 - T1) + (T3 - T4)) / 2.
	 */

	double T1 = ntp->t_send;
	double T2 = lfp_to_d(rsp->rectime); 
	double T3 = lfp_to_d(rsp->xmttime); 
	double T4 = ntp->t_recv;

	if(ntp->n > 0) {
		ntp->n --;
		ntp_sync(ntp);
	} else {
		ntp->offset = ((T2 - T1) + (T3 - T4)) / 2.0;
		ntp->delay  = (T4 - T1) - (T3 - T2);
		if(ntp->offset_cb) ntp->offset_cb(ntp->offset, ntp->delay);
		ntp->when = 60 * 60;
	}

	return 0;
}


double ntp_get_offset(struct ntp *ntp)
{
	return ntp->offset;
}


static int on_ntp_timer(void *data)
{
	struct ntp *ntp = data;

	if(ntp->when == 0) {
		ntp->n = 3;
		ntp_sync(ntp);
	}

	return 1;
}


struct ntp *ntp_init(const char *addr, void (*offset_cb)(double offset, double delay))
{
	struct ntp *ntp = calloc(1, sizeof *ntp);

	ntp->offset_cb = offset_cb;
	ntp->fd = socket(AF_INET, SOCK_DGRAM, 0);
	memset(&ntp->sa, 0, sizeof ntp->sa);

	ntp->sa.sin_family = AF_INET;
	ntp->sa.sin_port = htons(123);
	ntp->sa.sin_addr.s_addr = inet_addr(addr);
	
	mainloop_fd_add(ntp->fd, FD_READ, on_fd_ntp, ntp);
	mainloop_timer_add(5, 0, on_ntp_timer, ntp);

	ntp->when = 0;

	on_ntp_timer(ntp);

	return ntp;
}


/*
 * End
 */
 

