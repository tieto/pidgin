/* This file is part of the Project Athena Zephyr Notification System.
 * It contains the ZCheckIfNotice/select loop used for waiting for
 * a notice, with a timeout.
 *
 *	Created by:	<Joe Random Hacker>
 *
 *	$Source$
 *	$Author: warmenhoven $
 *
 *	Copyright (c) 1991 by the Massachusetts Institute of Technology.
 *	For copying and distribution information, see the file
 *	"mit-copyright.h". 
 */

#include "mit-copyright.h"

#ifndef lint
static char rcsid_ZWaitForNotice_c[] = "$Zephyr$";
#endif

#include <internal.h>
#include <sys/socket.h>

Code_t Z_WaitForNotice (notice, pred, arg, timeout)
     ZNotice_t *notice;
     int (*pred) __P((ZNotice_t *, void *));
     void *arg;
     int timeout;
{
  Code_t retval;
  struct timeval tv, t0;
  fd_set fdmask;
  int i, fd;

  retval = ZCheckIfNotice (notice, (struct sockaddr_in *) 0, pred,
			   (char *) arg);
  if (retval == ZERR_NONE)
    return ZERR_NONE;
  if (retval != ZERR_NONOTICE)
    return retval;

  fd = ZGetFD ();
  FD_ZERO (&fdmask);
  tv.tv_sec = timeout;
  tv.tv_usec = 0;
  gettimeofday (&t0, (struct timezone *) 0);
  t0.tv_sec += timeout;
  while (1) {
    FD_SET (fd, &fdmask);
    i = select (fd + 1, &fdmask, (fd_set *) 0, (fd_set *) 0, &tv);
    if (i == 0)
      return ETIMEDOUT;
    if (i < 0 && errno != EINTR)
      return errno;
    if (i > 0) {
      retval = ZCheckIfNotice (notice, (struct sockaddr_in *) 0, pred,
			       (char *) arg);
      if (retval != ZERR_NONOTICE) /* includes ZERR_NONE */
	return retval;
    }
    gettimeofday (&tv, (struct timezone *) 0);
    tv.tv_usec = t0.tv_usec - tv.tv_usec;
    if (tv.tv_usec < 0) {
      tv.tv_usec += 1000000;
      tv.tv_sec = t0.tv_sec - tv.tv_sec - 1;
    }
    else
      tv.tv_sec = t0.tv_sec - tv.tv_sec;
  }
  /*NOTREACHED*/
}
