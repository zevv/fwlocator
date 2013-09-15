#ifndef mainloop_h
#define mainloop_h

enum fd_type {
	FD_READ,
	FD_WRITE,
	FD_ERR
};

int mainloop_fd_add(int fd, enum fd_type type, int (*handler)(int fd, void *user), void *user);
int mainloop_fd_del(int fd, enum fd_type type, int (*handle)(int fd, void *user), void *user);

int mainloop_timer_add(int sec, int msec, int (*handler)(void *user), void *user);
int mainloop_timer_del(int (*handler)(void *user), void *user);

int mainloop_signal_add(int signum, int (*handler)(int signum, void *user), void *user);
int mainloop_signal_del(int signum, int (*handler)(int signum, void *user));

void mainloop_start(void);
void mainloop_stop(void);
int  mainloop_poll(void);
void mainloop_run(void);
void mainloop_cleanup(void);

#endif
