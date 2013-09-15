
#ifndef ntp_h
#define ntp_h

struct ntp;

struct ntp *ntp_init(const char *addr, void (*offset_cb)(double offset));
double ntp_get_offset(struct ntp *ntp);

#endif
