#ifndef event_h
#define event_h

void event_init(const char *ip);
void event_send(const char *fmt, ...);

#endif

