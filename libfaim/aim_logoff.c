/*
 * aim_logoff.c
 *
 *
 */

#include <faim/aim.h> 

/* 
 * aim_logoff()
 * 
 * Closes -ALL- open connections.
 *
 */
int aim_logoff(struct aim_session_t *sess)
{
  aim_connrst(sess);  /* in case we want to connect again */

  return 0;

}
