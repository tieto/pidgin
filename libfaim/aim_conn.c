
/*
 *  aim_conn.c
 *
 * Does all this gloriously nifty connection handling stuff...
 *
 */

#include <faim/aim.h> 

/*
 * Clears out connection list, killing remaining connections.
 */
void aim_connrst(struct aim_session_t *sess)
{
  faim_mutex_init(&sess->connlistlock, NULL);
  if (sess->connlist) {
    struct aim_conn_t *cur = sess->connlist, *tmp;

    while(cur) {
      tmp = cur->next;
      aim_conn_close(cur);
      free(cur);
      cur = tmp;
    }
  }
  sess->connlist = NULL;
  return;
}

/*
 * Gets a new connection structure.
 */
struct aim_conn_t *aim_conn_getnext(struct aim_session_t *sess)
{
  struct aim_conn_t *newconn, *cur;

  if (!(newconn = malloc(sizeof(struct aim_conn_t)))) 	
    return NULL;

  memset(newconn, 0, sizeof(struct aim_conn_t));
  aim_conn_close(newconn);
  newconn->next = NULL;

  faim_mutex_lock(&sess->connlistlock);
  if (sess->connlist == NULL)
    sess->connlist = newconn;
  else {
    for (cur = sess->connlist; cur->next; cur = cur->next)
      ;
    cur->next = newconn;
  }
  faim_mutex_unlock(&sess->connlistlock);

  return newconn;
}

static void aim_conn_init(struct aim_conn_t *deadconn)
{
  if (!deadconn)
    return;

  deadconn->fd = -1;
  deadconn->subtype = -1;
  deadconn->type = -1;
  deadconn->seqnum = 0;
  deadconn->lastactivity = 0;
  deadconn->forcedlatency = 0;
  deadconn->handlerlist = NULL;
  deadconn->priv = NULL;
  faim_mutex_init(&deadconn->active, NULL);
  faim_mutex_init(&deadconn->seqnum_lock, NULL);
  
  return;
}

void aim_conn_kill(struct aim_session_t *sess, struct aim_conn_t **deadconn)
{
  struct aim_conn_t *cur;

  if (!deadconn || !*deadconn)	
    return;

  faim_mutex_lock(&sess->connlistlock);
  if (sess->connlist == NULL)
    ;
  else if (sess->connlist->next == NULL) {
    if (sess->connlist == *deadconn)
      sess->connlist = NULL;
  } else {
    cur = sess->connlist;
    while (cur->next) {
      if (cur->next == *deadconn) {
	cur->next = cur->next->next;
	break;
      }
      cur = cur->next;
    }
  }
  faim_mutex_unlock(&sess->connlistlock);

  /* XXX: do we need this for txqueue too? */
  aim_rxqueue_cleanbyconn(sess, *deadconn);

  aim_conn_close(*deadconn);
  if ((*deadconn)->priv)
    free((*deadconn)->priv);
  free(*deadconn);
  deadconn = NULL;

  return;
}

void aim_conn_close(struct aim_conn_t *deadconn)
{
  int typesav = -1, subtypesav = -1;
  void *privsav = NULL;

  faim_mutex_destroy(&deadconn->active);
  faim_mutex_destroy(&deadconn->seqnum_lock);
  if (deadconn->fd >= 3)
    close(deadconn->fd);
  if (deadconn->handlerlist)
    aim_clearhandlers(deadconn);

  typesav = deadconn->type;
  subtypesav = deadconn->subtype;

  if (deadconn->priv && (deadconn->type != AIM_CONN_TYPE_RENDEZVOUS)) {
    free(deadconn->priv);
    deadconn->priv = NULL;
  }
  privsav = deadconn->priv;

  aim_conn_init(deadconn);

  deadconn->type = typesav;
  deadconn->subtype = subtypesav;
  deadconn->priv = privsav;

  return;
}

struct aim_conn_t *aim_getconn_type(struct aim_session_t *sess,
				    int type)
{
  struct aim_conn_t *cur;

  faim_mutex_lock(&sess->connlistlock);
  for (cur = sess->connlist; cur; cur = cur->next) {
    if (cur->type == type)
      break;
  }
  faim_mutex_unlock(&sess->connlistlock);
  return cur;
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

  faim_mutex_lock(&connstruct->active);
  
  connstruct->type = type;

  if (!dest) { /* just allocate a struct */
    connstruct->fd = -1;
    connstruct->status = 0;
    faim_mutex_unlock(&connstruct->active);
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

  for(i=0;i<strlen(dest);i++) {
    if (dest[i] == ':') {
      port = atoi(&(dest[i+1]));
      break;
    }
  }
  host = (char *)malloc(i+1);
  strncpy(host, dest, i);
  host[i] = '\0';

  hp = gethostbyname(host);
  free(host);

  if (hp == NULL) {
    connstruct->status = (h_errno | AIM_CONN_STATUS_RESOLVERR);
    faim_mutex_unlock(&connstruct->active);
    return connstruct;
  }

  memset(&sa.sin_zero, 0, 8);
  sa.sin_port = htons(port);
  memcpy(&sa.sin_addr, hp->h_addr, hp->h_length);
  sa.sin_family = hp->h_addrtype;
  
  connstruct->fd = socket(hp->h_addrtype, SOCK_STREAM, 0);
  ret = connect(connstruct->fd, (struct sockaddr *)&sa, sizeof(struct sockaddr_in));
  if(ret < 0) {
    connstruct->fd = -1;
    connstruct->status = (errno | AIM_CONN_STATUS_CONNERR);
    faim_mutex_unlock(&connstruct->active);
    return connstruct;
  }
  
  faim_mutex_unlock(&connstruct->active);

  return connstruct;
}

int aim_conngetmaxfd(struct aim_session_t *sess)
{
  int j = 0;
  struct aim_conn_t *cur;

  faim_mutex_lock(&sess->connlistlock);
  for (cur = sess->connlist; cur; cur = cur->next) {
    if (cur->fd > j)
      j = cur->fd;
  }
  faim_mutex_unlock(&sess->connlistlock);

  return j;
}

int aim_countconn(struct aim_session_t *sess)
{
  int cnt = 0;
  struct aim_conn_t *cur;

  faim_mutex_lock(&sess->connlistlock);
  for (cur = sess->connlist; cur; cur = cur->next)
    cnt++;
  faim_mutex_unlock(&sess->connlistlock);

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
 * XXX: we could probably stand to do a little courser locking here.
 *
 */ 
struct aim_conn_t *aim_select(struct aim_session_t *sess,
			      struct timeval *timeout, int *status)
{
  struct aim_conn_t *cur;
  fd_set fds;
  int maxfd = 0;
  int i;

  faim_mutex_lock(&sess->connlistlock);
  if (sess->connlist == NULL) {
    faim_mutex_unlock(&sess->connlistlock);
    *status = -1;
    return NULL;
  }
  faim_mutex_unlock(&sess->connlistlock);

  /* 
   * If we have data waiting to be sent, return immediatly
   */
  if (sess->queue_outgoing != NULL) {
    *status = 1;
    return NULL;
  } 

  FD_ZERO(&fds);
  maxfd = 0;

  faim_mutex_lock(&sess->connlistlock);
  for (cur = sess->connlist; cur; cur = cur->next) {
    FD_SET(cur->fd, &fds);
    if (cur->fd > maxfd)
      maxfd = cur->fd;
  }
  faim_mutex_unlock(&sess->connlistlock);

  if ((i = select(maxfd+1, &fds, NULL, NULL, timeout))>=1) {
    faim_mutex_lock(&sess->connlistlock);
    for (cur = sess->connlist; cur; cur = cur->next) {
      if (FD_ISSET(cur->fd, &fds)) {
	*status = 2;
	faim_mutex_unlock(&sess->connlistlock);
	return cur;
      }
    }
  }

  faim_mutex_unlock(&sess->connlistlock);
  *status = i; /* may be 0 or -1 */
  return NULL;  /* no waiting or error, return */
}

int aim_conn_isready(struct aim_conn_t *conn)
{
  if (conn)
    return (conn->status & 0x0001);
  return -1;
}

int aim_conn_setstatus(struct aim_conn_t *conn, int status)
{
  int val;

  if (!conn)
    return -1;
  
  faim_mutex_lock(&conn->active);
  val = conn->status ^= status;
  faim_mutex_unlock(&conn->active);
  return val;
}

int aim_conn_setlatency(struct aim_conn_t *conn, int newval)
{
  if (!conn)
    return -1;

  faim_mutex_lock(&conn->active);
  conn->forcedlatency = newval;
  conn->lastactivity = 0; /* reset this just to make sure */
  faim_mutex_unlock(&conn->active);

  return 0;
}

void aim_session_init(struct aim_session_t *sess)
{
  if (!sess)
    return;

  memset(sess, 0, sizeof(struct aim_session_t));
  aim_connrst(sess);
  sess->queue_outgoing = NULL;
  sess->queue_incoming = NULL;
  sess->pendingjoin = NULL;
  sess->outstanding_snacs = NULL;
  sess->snac_nextid = 0x00000001;

  /*
   * This must always be set.  Default to the queue-based
   * version for back-compatibility.  
   */
  sess->tx_enqueue = &aim_tx_enqueue__queuebased;

  return;
}
