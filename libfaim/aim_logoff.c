/*
 * aim_logoff.c
 *
 *
 */

#include <aim.h> 

/* 
 * aim_logoff()
 * 
 * Closes -ALL- open connections.
 *
 */
int aim_logoff(struct aim_session_t *sess)
{
  int i = AIM_CONN_MAX-1;
  while (i > -1)
    {
      if (sess->conns[i].fd>-1)
	aim_conn_close(&(sess->conns[i]));
      i--;
    }
  aim_connrst(sess);  /* in case we want to connect again */

  return 0;

}
