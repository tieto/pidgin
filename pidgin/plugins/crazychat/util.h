#ifndef __UTIL_H__
#define __UTIL_H__

#include <debug.h>

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
		purple_debug(PURPLE_DEBUG_INFO, "crazychat", x, ## args);	\
	} while (0)
#else
#define Debug(x, args...) do{}while(0)
#endif
