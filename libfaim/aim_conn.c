
/*
 *  aim_conn.c
 *
 * Does all this gloriously nifty connection handling stuff...
 *
 */

#include <aim.h> 

void aim_connrst(struct aim_session_t *sess)
{
  int i;
  for (i = 0; i < AIM_CONN_MAX; i++)
    {
      sess->conns[i].fd = -1;
      sess->conns[i].type = -1;
      sess->conns[i].status = 0;
      sess->conns[i].seqnum = 0;
      sess->conns[i].lastactivity = 0;
      sess->conns[i].forcedlatency = 0;
      aim_clearhandlers(&(sess->conns[i]));
      sess->conns[i].handlerlist = NULL;
    }

}

struct aim_conn_t *aim_conn_getnext(struct aim_session_t *sess)
{
  int i;
  for (i=0;i<AIM_CONN_MAX;i++)
    if (sess->conns[i].fd == -1)
      return &(sess->conns[i]);
  return NULL;
}

void aim_conn_close(struct aim_conn_t *deadconn)
{
  if (deadconn->fd >= 3)
    close(deadconn->fd);
  deadconn->fd = -1;
  deadconn->type = -1;
  deadconn->seqnum = 0;
  deadconn->lastactivity = 0;
  deadconn->forcedlatency = 0;
  aim_clearhandlers(deadconn);
  deadconn->handlerlist = NULL;
  if (deadconn->priv)
    free(deadconn->priv);
  deadconn->priv = NULL;
}

struct aim_conn_t *aim_getconn_type(struct aim_session_t *sess,
				    int type)
{
  int i;
  for (i=0; i<AIM_CONN_MAX; i++)
    if (sess->conns[i].type == type)
      return &(sess->conns[i]);
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
struct aim_conn_t *aim_newconn(struct aim_session_t *sess,
			       int type, char *dest)
{
  struct aim_conn_t *connstruct;
  int ret;
  struct sockaddr_in sa;
  struct hostent *hp;
  u_short port = FAIM_LOGIN_PORT;
  char *host = NULL;
  int i=0;
  
  if ((connstruct=aim_conn_getnext(sess))==NULL)
    return NULL;

  connstruct->type = type;

  if (!dest) { /* just allocate a struct */
    connstruct->fd = -1;
    connstruct->status = 0;
    return connstruct;
  }

  /* 
   * As of 23 Jul 1999, AOL now sends the port number, preceded by a 
   * colon, in the BOS redirect.  This fatally breaks all previous 
   * libfaims.  Bad, bad AOL.
   *
   * We put this here to catch every case. 
   *
   */

  for(i=0;i<strlen(dest);i++)
    {
      if (dest[i] == ':') {
	port = atoi(&(dest[i+1]));
	break;
      }
    }
  host = (char *)malloc(i+1);
  strncpy(host, dest, i);
  host[i] = '\0';

  hp = gethostbyname2(host, AF_INET);
  free(host);

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

int aim_conngetmaxfd(struct aim_session_t *sess)
{
  int i,j;
  j=0;
  for (i=0;i<AIM_CONN_MAX;i++)
    if(sess->conns[i].fd > j)
      j = sess->conns[i].fd;
  return j;
}

int aim_countconn(struct aim_session_t *sess)
{
  int i,cnt;
  cnt = 0;
  for (i=0;i<AIM_CONN_MAX;i++)
    if (sess->conns[i].fd > -1)
      cnt++;
  return cnt;
}

/*
 * aim_select(timeout)
 *
 * Waits for a socket with data or for timeout, whichever comes first.
 * See select(2).
 * 
 * Return codes in *status:
 *   -1  error in select() (NULL returned)
 *    0  no events pending (NULL returned)
 *    1  outgoing data pending (NULL returned)
 *    2  incoming data pending (connection with pending data returned)
 *
 */ 
struct aim_conn_t *aim_select(struct aim_session_t *sess,
			      struct timeval *timeout, int *status)
{
  fd_set fds;
  int i;

  if (aim_countconn(sess) <= 0)
    return 0;

  /* 
   * If we have data waiting to be sent, return immediatly
   */
  if (sess->queue_outgoing != NULL) {
    *status = 1;
    return NULL;
  } 

  FD_ZERO(&fds);
  
  for(i=0;i<AIM_CONN_MAX;i++)
    if (sess->conns[i].fd>-1)
      FD_SET(sess->conns[i].fd, &fds);
  
  if ((i = select(aim_conngetmaxfd(sess)+1, &fds, NULL, NULL, timeout))>=1) {
    int j;
    for (j=0;j<AIM_CONN_MAX;j++) {
	if (sess->conns[j].fd > -1) {
	    if ((FD_ISSET(sess->conns[j].fd, &fds))) {
	      *status = 2;
	      return &(sess->conns[j]);  /* return the first waiting struct */
	    }
	  }	
	}	
    /* should never get here */
  }

  *status = i; /* may be 0 or -1 */
  return NULL;  /* no waiting or error, return */
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

int aim_conn_setlatency(struct aim_conn_t *conn, int newval)
{
  if (!conn)
    return -1;
  
  conn->forcedlatency = newval;
  conn->lastactivity = 0; /* reset this just to make sure */

  return 0;
}

void aim_session_init(struct aim_session_t *sess)
{
  int i;

  if (!sess)
    return;

  memset(sess->logininfo.screen_name, 0x00, MAXSNLEN);
  sess->logininfo.BOSIP = NULL;
  memset(sess->logininfo.cookie, 0x00, AIM_COOKIELEN);
  sess->logininfo.email = NULL;
  sess->logininfo.regstatus = 0x00;

  for (i = 0; i < AIM_CONN_MAX; i++)
    {
      sess->conns[i].fd = -1;
      sess->conns[i].type = -1;
      sess->conns[i].status = 0;
      sess->conns[i].seqnum = 0;
      sess->conns[i].lastactivity = 0;
      sess->conns[i].forcedlatency = 0;
      sess->conns[i].handlerlist = NULL;
      sess->conns[i].priv = NULL;
    }
  
  sess->queue_outgoing = NULL;
  sess->queue_incoming = NULL;
  sess->pendingjoin = NULL;
  sess->outstanding_snacs = NULL;
  sess->snac_nextid = 0x00000001;

  return;
}
