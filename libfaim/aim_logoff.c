/*
 * aim_logoff.c
 *
 *
 */

#include "aim.h"

/* 
 * aim_logoff()
 * 
 * Closes -ALL- open connections.
 *
 */
int aim_logoff(void)
{
  int i = AIM_CONN_MAX-1;
  while (i > -1)
    {
      if (aim_conns[i].fd>-1)
	aim_conn_close(&(aim_conns[i]));
      i--;
    }
  aim_connrst();  /* in case we want to connect again */

  return 0;

}
