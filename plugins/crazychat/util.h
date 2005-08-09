#ifndef __UTIL_H__
#define __UTIL_H__

#include <pthread.h>
#include <debug.h>

#define THREAD_CREATE(thread, func, args)				\
	do {								\
		assert(!pthread_create(thread, NULL, func, args));	\
	} while(0)

#define THREAD_JOIN(thread)						\
	do {								\
		assert(!pthread_join(thread, NULL));			\
	} while(0)

#define MUTEX_INIT(mutex)						\
	do {								\
		assert(!pthread_mutex_init(mutex, NULL));		\
	} while(0)

#define MUTEX_DESTROY(mutex)						\
	do {								\
		assert(!pthread_mutex_destroy(mutex));			\
	} while(0)

#define MUTEX_LOCK(mutex)						\
	do {								\
		assert(!pthread_mutex_lock(mutex));			\
	} while(0)

#define MUTEX_UNLOCK(mutex)						\
	do {								\
		assert(!pthread_mutex_unlock(mutex));			\
	} while(0)

#define COND_INIT(cond)							\
	do {								\
		assert(!pthread_cond_init(cond, NULL));			\
	} while(0)

#define COND_SIGNAL(cond, mutex)					\
	do {								\
		MUTEX_LOCK(mutex);					\
		assert(!pthread_cond_signal(cond));			\
		MUTEX_UNLOCK(mutex);					\
	} while(0)

#define COND_WAIT(cond, mutex)						\
	do {								\
		assert(!pthread_cond_wait(cond, mutex));		\
	} while(0)

#define COND_DESTROY(cond)						\
	do {								\
		assert(!pthread_cond_destroy(cond));			\
	} while(0)

#define SET_TIME(x)							\
	do {								\
		assert(!gettimeofday((x), NULL));			\
	} while(0)

#define SET_TIMEOUT(timespec, given_timeout)	/* timeout is in ms */	\
	do {								\
		struct timeval* curr = (struct timeval*)(timespec);	\
		unsigned int tout;					\
		if (given_timeout > 100) {				\
			tout = given_timeout;				\
		} else {						\
			tout = 100;					\
		}							\
		SET_TIME(curr);						\
		curr->tv_sec += (tout / 1000);				\
		curr->tv_usec /= 1000; /* set to ms */			\
		curr->tv_usec += (tout % 1000);				\
		curr->tv_sec += (curr->tv_usec / 1000);			\
		curr->tv_usec = (curr->tv_usec % 1000);			\
		curr->tv_usec *= 1000000;				\
	} while (0)

#endif

/* -- gcc specific vararg macro support ... but its so nice! -- */
#ifdef _DEBUG_
#define Debug(x, args...)						\
	do {								\
		printf(x, ## args);					\
		gaim_debug(GAIM_DEBUG_INFO, "crazychat", x, ## args);	\
	} while (0)
#else
#define Debug(x, args...) do{}while(0)
#endif
