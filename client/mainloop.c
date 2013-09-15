
 
#include <stdio.h>
#include <sys/select.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "mainloop.h"
#include "list.h"


struct mainloop_fd_t {
	int fd;
	enum fd_type type;
	int (*handler)(int fd, void *user);
	void *user;
	int remove;
	struct mainloop_fd_t *prev;
	struct mainloop_fd_t *next;
};

struct mainloop_timer_t {
	struct timeval when;
	struct timeval interval;
	int (*handler)(void *user);
	void *user;
	int remove;
	struct mainloop_timer_t *prev;
	struct mainloop_timer_t *next;
};

struct mainloop_signal_t {
	int signum;
	int (*handler)(int signum, void *user);
	void *user;
	int signaled;
	int remove;
	struct mainloop_signal_t *prev;
	struct mainloop_signal_t *next;
};


struct mainloop_fd_t     *mainloop_fd_list     = NULL;
struct mainloop_timer_t  *mainloop_timer_list  = NULL;
struct mainloop_signal_t *mainloop_signal_list = NULL;
int mainloop_running = 1;



int mainloop_fd_add(int fd, enum fd_type type, int (*handler)(int fd, void *user), void *user)
{
	struct mainloop_fd_t *mf, *mf_next;

	/*
 	 * Make sure the same fd is not twice in the list
	 */
	
	LIST_FOREACH(mainloop_fd_list, mf, mf_next) {
		if(mf->fd == fd && !mf->remove) return(-1);
	}

	mf = calloc(sizeof *mf, 1);
	if(mf == NULL) return(-1);
	mf->fd      = fd;
	mf->type    = type;
	mf->handler = handler;
	mf->user    = user;
	
	LIST_ADD_ITEM(mainloop_fd_list, mf);
	
	return(0);	
}


/*
 * Mark this fd for removeal
 */
 
int mainloop_fd_del(int fd, enum fd_type type, int (*handler)(int fd, void *user), void *user)
{
	struct mainloop_fd_t *mf, *mf_next;
	int found = 0;
	
	LIST_FOREACH(mainloop_fd_list, mf, mf_next) {
		if( (mf->fd == fd) && (mf->type == type) && (mf->handler == handler) && (mf->user == user)) {
			mf->remove = 1;
			found = 1;
		}
	}	
	
	if(found == 0) return(-1);
	return(0);
}


int mainloop_timer_add(int sec, int msec, int (*handler)(void *user), void *user)
{
	struct timeval interval;
	struct timeval when;
	struct timeval now;
	struct mainloop_timer_t *mt;
	struct mainloop_timer_t *later;
	struct mainloop_timer_t *sooner;

	/*
	 * Remove this timer if already pending
	 */

	mainloop_timer_del(handler, user);

	/*
	 * Calculate timestamp when this timer will expire
	 */
	
	gettimeofday(&now, NULL);
	interval.tv_sec  = sec;
	interval.tv_usec = msec * 1000;
	timeradd(&now, &interval, &when);
	
	/* 
	 * Create new instance
	 */
	 	
	mt = calloc(sizeof *mt, 1);
	if(mt == NULL) return(-1);
	mt->interval = interval;
	mt->when     = when;
	mt->handler  = handler;
	mt->user     = user;

	/*
	 * Add it to the list of timers, sorted
	 */
	 
	if(mainloop_timer_list == NULL) {

		mainloop_timer_list = mt;

	} else {
	
		/* 
		 * Find the nodes in the list that come sooner and later then 'mt' 
		 */ 

		sooner = NULL;
		later  = mainloop_timer_list;

		while(later && timercmp(&later->when, &when, < )) {
			sooner = later;
			later = later->next;
		}		

		/* 
		 * Insert 'mt' between 'sooner' and 'later' 
		 */
				
		mt->prev = sooner;
		mt->next = later;
		if(sooner) sooner->next = mt;
		if(later)  later->prev = mt;
		if(sooner == NULL) mainloop_timer_list = mt;
	}
		
	return(0);	
};
	

/*
 * Mark the given timer for removal
 */
 
int mainloop_timer_del(int (*handler)(void *user), void *user)
{
	struct mainloop_timer_t *mt, *mt_next;
	int found = 0;

	LIST_FOREACH(mainloop_timer_list, mt, mt_next) {
		if( (mt->handler == handler) && (mt->user == user) ) {
			mt->remove = 1;
			found = 1;
		}
			
	}
	
	if(found == 0) return(-1);
	return(0);
}	



/*
 * Signal handler, called for all registered signals
 */

static void mainloop_signal_handler(int signum)
{
	struct mainloop_signal_t *ms, *ms_next;;
	
	LIST_FOREACH(mainloop_signal_list, ms, ms_next) {
		if(ms->signum == signum) ms->signaled = 1;
	}
}



int mainloop_signal_add(int signum, int (*handler)(int signum, void *user), void *user)
{
	struct mainloop_signal_t *ms;
	
	ms = calloc(sizeof *ms, 1);
	if(ms == NULL) return(-1);
	ms->signum  = signum;
	ms->handler = handler;
	ms->user    = user;

	LIST_ADD_ITEM(mainloop_signal_list, ms);
	
	signal(signum, mainloop_signal_handler);
	
	return(0);	
}


/*
 * Mark the given signal for removal
 */
 
int mainloop_signal_del(int signum, int (*handler)(int signum, void *user))
{
	struct mainloop_signal_t *ms, *ms_next;
	int signum_refcount = 0;
	int found = 0;

	LIST_FOREACH(mainloop_signal_list, ms, ms_next) {
		if( ms->signum == signum ) {
			signum_refcount ++;
			if(ms->handler == handler) {
				found = 1;
				ms->remove = 1;
				signum_refcount --;
			}
		}
	}
	
	if(found == 0) return(-1);
	
	/*
	 * If no handlers are registerd for this signal anymore, reset
	 * the signal to default behaviour 
	 */
	 
	if(signum_refcount == 0) {
		signal(signum, SIG_DFL);
	}
	
	return(0);
}


/*
 * Start mainloop
 */
 
void mainloop_start(void)
{
	mainloop_running = 1;
}


/*
 * Request the mainloop to stop on the next iteration
 */
 
void mainloop_stop(void)
{
	mainloop_running = 0;
}

					
/*
 * Run the mainloop. Stops when mainloop_stop() is called
 */

int mainloop_poll(void)
{
	int r;
	int maxfd;
	fd_set fds_read;
	fd_set fds_write;
	fd_set fds_err;
	struct timeval tv;
	struct timeval now;
	struct mainloop_fd_t     *mf, *mf_next;
	struct mainloop_timer_t  *mt, *mt_next;
	struct mainloop_signal_t *ms, *ms_next;

	/*
	 * Add fd's to fd_set
	 */

	FD_ZERO(&fds_read);
	FD_ZERO(&fds_write);
	FD_ZERO(&fds_err);
	maxfd = 0;	
	LIST_FOREACH(mainloop_fd_list, mf, mf_next) {
		if(!mf->remove) {
			if(mf->type == FD_READ) FD_SET(mf->fd, &fds_read);
			if(mf->type == FD_WRITE) FD_SET(mf->fd, &fds_write);
			if(mf->type == FD_ERR) FD_SET(mf->fd, &fds_err);
			if(mf->fd > maxfd) maxfd = mf->fd;
		}
	}

	/*
	 * See if there are timers to expire, and set the select()'s
	 * timeout value accordingly
	 */
	 
	if(mainloop_timer_list) {
		gettimeofday(&now, NULL);
		if(timercmp(&now, &mainloop_timer_list->when, >)) {
			tv.tv_sec = 0;
			tv.tv_usec = 0;
		} else {
			timersub(&mainloop_timer_list->when, &now, &tv);
		}
		
	} else {
		tv.tv_sec = 10000;
		tv.tv_usec = 0;
	}
	

	/* 
	 * The select call waits on fd's or timeouts. Here the
	 * actual work is done ...
	 */

	if(mainloop_running == 0) return(0);
	r = select(maxfd+1, &fds_read, &fds_write, &fds_err, &tv);
	if((r < 0) && (errno != EINTR)) return(-1);


	/*
	 * Call all registerd read fd's that have data
	 */

	if(r > 0) {		
		LIST_FOREACH(mainloop_fd_list, mf, mf_next) {
			if(!mf->remove && mf->handler) {
				if((mf->type == FD_READ) && FD_ISSET(mf->fd, &fds_read)) mf->handler(mf->fd, mf->user);
				if((mf->type == FD_WRITE) && FD_ISSET(mf->fd, &fds_write)) mf->handler(mf->fd, mf->user);
				if((mf->type == FD_ERR) && FD_ISSET(mf->fd, &fds_err)) mf->handler(mf->fd, mf->user);
			}
		}
	}


	/*
	 * Call signal handlers, if signaled
	 */
	 
	LIST_FOREACH(mainloop_signal_list, ms, ms_next) {
		if(ms->signaled) {
			ms->signaled = 0;
			ms->handler(ms->signum, ms->user);
		}
	}	

		
	/*
	 * Call all timers that have expired
	 */

	gettimeofday(&now, NULL);

	while(mainloop_timer_list && timercmp(&mainloop_timer_list->when, &now, <)) {

		r = 0;
		if(!mainloop_timer_list->remove) {
			r = mainloop_timer_list->handler(mainloop_timer_list->user);
		}
		
		/*
		 * If the function returned nonzero, reschedule a timer on the
		 * same handler and userdata
		 */
		 
		if(r != 0) {
			mainloop_timer_add(	
					mainloop_timer_list->interval.tv_sec,
					mainloop_timer_list->interval.tv_usec / 1000,
					mainloop_timer_list->handler, 
					mainloop_timer_list->user);
		}
		
		/*
		 * Remove and free the expired timer
		 */
		 
		mt_next = mainloop_timer_list->next;
		free(mainloop_timer_list);
		mainloop_timer_list = mt_next;
		if(mainloop_timer_list) mainloop_timer_list->prev = NULL;

		gettimeofday(&now, NULL);
	}
	

	/*
	 * Cleanup all fds, signals and timers that have their 'remove' flag set. 
	 */

	LIST_FOREACH(mainloop_fd_list, mf, mf_next) {
		if(mf->remove) {
			LIST_REMOVE_ITEM(mainloop_fd_list, mf);
			free(mf);
		}
	}

	LIST_FOREACH(mainloop_signal_list, ms, ms_next) {
		if(ms->remove) {
			LIST_REMOVE_ITEM(mainloop_signal_list, ms);
			free(ms);
		}
	}

	LIST_FOREACH(mainloop_timer_list, mt, mt_next) {	
		if(mt->remove) {
			LIST_REMOVE_ITEM(mainloop_timer_list, mt);
			free(mt);
		}
	}

	return(0);
}


static int on_sigquit(int signo, void *data)
{
	return 0;
}


/*
 * Run the mainloop. Stops when mainloop_stop() is called
 */

void mainloop_run(void)
{
	mainloop_running  = 1;

	mainloop_signal_add(SIGQUIT, on_sigquit, NULL);

	while(mainloop_running) {
		mainloop_poll();
	}

}


void mainloop_cleanup(void)
{
	struct mainloop_fd_t     *mf, *mf_next;
	struct mainloop_timer_t  *mt, *mt_next;
	struct mainloop_signal_t *ms, *ms_next;

	if(mainloop_running) return;

	/*
	 * Free all used stuff
	 */

	LIST_FOREACH(mainloop_fd_list,     mf, mf_next) free(mf);
	LIST_FOREACH(mainloop_timer_list,  mt, mt_next) free(mt);
	LIST_FOREACH(mainloop_signal_list, ms, ms_next) free(ms);
	
	mainloop_fd_list     = NULL;
	mainloop_timer_list  = NULL;
	mainloop_signal_list = NULL;

}


/* end */

