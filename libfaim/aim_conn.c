
/*
 *  aim_conn.c
 *
 * Does all this gloriously nifty connection handling stuff...
 *
 */

#include "aim.h"

void aim_connrst(void)
{
  int i;
  for (i = 0; i < AIM_CONN_MAX; i++)
    {
      aim_conns[i].fd = -1;
      aim_conns[i].type = -1;
      aim_conns[i].status = 0;
    }

}

struct aim_conn_t *aim_conn_getnext(void)
{
  int i;
  for (i=0;i<AIM_CONN_MAX;i++)
    if (aim_conns[i].fd == -1)
      return &(aim_conns[i]);
  return NULL;
}

void aim_conn_close(struct aim_conn_t *deadconn)
{
  if (deadconn->fd >= 3)
    close(deadconn->fd);
  deadconn->fd = -1;
  deadconn->type = -1;
}

struct aim_conn_t *aim_getconn_type(int type)
{
  int i;
  for (i=0; i<AIM_CONN_MAX; i++)
    if (aim_conns[i].type == type)
      return &(aim_conns[i]);
  return NULL;
}

/*
 * aim_newconn(type, dest)
 *
 * Opens a new connection to the specified dest host of type type.
 *
 * TODO: fix for proxies
 * FIXME: Return errors in a more sane way.
 *
 */
struct aim_conn_t *aim_newconn(int type, char *dest)
{
  struct aim_conn_t *connstruct;
  int ret;
  struct sockaddr_in sa;
  struct hostent *hp;
  int port = FAIM_LOGIN_PORT;
  int i=0;
  
  if (!dest || ((connstruct=aim_conn_getnext())==NULL))
    return NULL;

  connstruct->type = type;

  /* 
   * As of 23 Jul 1999, AOL now sends the port number, preceded by a 
   * colon, in the BOS redirect.  This fatally breaks all previous 
   * libfaims.  Bad, bad AOL.
   *
   * We put this here to catch every case. 
   *
   */
  for(i=0;(i<strlen(dest));i++)
    if (dest[i] == ':') break; 
  if (i<strlen(dest))
      {
	port = atoi(dest+i);
	dest[i] = '\0';
      }

  hp = gethostbyname2(dest, AF_INET);
  if (hp == NULL)
    {
      connstruct->status = (h_errno | AIM_CONN_STATUS_RESOLVERR);
      return connstruct;
    }

  memset(&sa.sin_zero, 0, 8);
  sa.sin_port = htons(port);
  memcpy(&sa.sin_addr, hp->h_addr, hp->h_length);
  sa.sin_family = hp->h_addrtype;
  
  connstruct->fd = socket(hp->h_addrtype, SOCK_STREAM, 0);
  ret = connect(connstruct->fd, (struct sockaddr *)&sa, sizeof(struct sockaddr_in));
  if( ret < 0)
    {
      connstruct->fd = -1;
      connstruct->status = (errno | AIM_CONN_STATUS_CONNERR);
      return connstruct;
    }

  return connstruct;
}

int aim_conngetmaxfd(void)
{
  int i,j;
  j=0;
  for (i=0;i<AIM_CONN_MAX;i++)
    if(aim_conns[i].fd > j)
      j = aim_conns[i].fd;
  return j;
}

int aim_countconn(void)
{
  int i,cnt;
  cnt = 0;
  for (i=0;i<AIM_CONN_MAX;i++)
    if (aim_conns[i].fd > -1)
      cnt++;
  return cnt;
}

/*
 * aim_select(timeout)
 *
 * Waits for a socket with data or for timeout, whichever comes first.
 * See select(2).
 * 
 */ 
struct aim_conn_t *aim_select(struct timeval *timeout)
{
  fd_set fds;
  fd_set errfds;
  int i;

  if (aim_countconn() <= 0)
    return 0;
  
  FD_ZERO(&fds);
  FD_ZERO(&errfds);
  
  for(i=0;i<AIM_CONN_MAX;i++)
    if (aim_conns[i].fd>-1)
      {
	FD_SET(aim_conns[i].fd, &fds);
	FD_SET(aim_conns[i].fd, &errfds);
      }
  
  i = select(aim_conngetmaxfd()+1, &fds, NULL, &errfds, timeout);
  if (i>=1)
    {
      int j;
      for (j=0;j<AIM_CONN_MAX;j++)
	{
	  if ((FD_ISSET(aim_conns[j].fd, &errfds)))
	    {
	      /* got an exception; close whats left of it up */
	      aim_conn_close(&(aim_conns[j]));
	      return (struct aim_conn_t *)-1;
	    }
	  else if ((FD_ISSET(aim_conns[j].fd, &fds)))
	    return &(aim_conns[j]);  /* return the first waiting struct */
	}
      /* should never get here */
    }
  else
    return (struct aim_conn_t *)i;  /* no waiting or error, return -- FIXME: return type funnies */
  return NULL; /* NO REACH */
}

int aim_conn_isready(struct aim_conn_t *conn)
{
  if (conn)
    return (conn->status & 0x0001);
  else
    return -1;
}

int aim_conn_setstatus(struct aim_conn_t *conn, int status)
{
  if (conn)
    return (conn->status ^= status);
  else
    return -1;
}

