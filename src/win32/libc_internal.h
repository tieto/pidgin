/*
 *  libc_internal.h
 */

#ifndef _LIBC_INTERNAL_
#define _LIBC_INTERNAL_

/* fcntl.h */
#define F_SETFL 1
#define O_NONBLOCK 1

/* sys/time.h */
struct timezone {
	int tz_minuteswest;
	int tz_dsttime;
};


#endif /* _LIBC_INTERNAL_ */
