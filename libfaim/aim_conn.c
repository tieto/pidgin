
/*
 *  aim_conn.c
 *
 * Does all this gloriously nifty connection handling stuff...
 *
 */

#include <faim/aim.h> 

#ifndef _WIN32
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

/*
 * Clears out connection list, killing remaining connections.
 */
faim_internal void aim_connrst(struct aim_session_t *sess)
{
  faim_mutex_init(&sess->connlistlock);
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
faim_internal struct aim_conn_t *aim_conn_getnext(struct aim_session_t *sess)
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
  faim_mutex_init(&deadconn->active);
  faim_mutex_init(&deadconn->seqnum_lock);
  
  return;
}

faim_export void aim_conn_kill(struct aim_session_t *sess, struct aim_conn_t **deadconn)
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

faim_export void aim_conn_close(struct aim_conn_t *deadconn)
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

faim_internal struct aim_conn_t *aim_getconn_type(struct aim_session_t *sess,
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
 * An extrememly quick and dirty SOCKS5 interface. 
 */
static int aim_proxyconnect(struct aim_session_t *sess, 
			    char *host, unsigned short port,
			    int *statusret)
{
  int fd = -1;

  if (strlen(sess->socksproxy.server)) { /* connecting via proxy */
    int i;
    unsigned char buf[512];
    struct sockaddr_in sa;
    struct hostent *hp;
    char *proxy;
    unsigned short proxyport = 1080;

    for(i=0;i<(int)strlen(sess->socksproxy.server);i++) {
      if (sess->socksproxy.server[i] == ':') {
	proxyport = atoi(&(sess->socksproxy.server[i+1]));
	break;
      }
    }
    proxy = (char *)malloc(i+1);
    strncpy(proxy, sess->socksproxy.server, i);
    proxy[i] = '\0';

    if (!(hp = gethostbyname(proxy))) {
      printf("proxyconnect: unable to resolve proxy name\n");
      *statusret = (h_errno | AIM_CONN_STATUS_RESOLVERR);
      return -1;
    }
    free(proxy);

    memset(&sa.sin_zero, 0, 8);
    sa.sin_port = htons(proxyport);
    memcpy(&sa.sin_addr, hp->h_addr, hp->h_length);
    sa.sin_family = hp->h_addrtype;
  
    fd = socket(hp->h_addrtype, SOCK_STREAM, 0);
    if (connect(fd, (struct sockaddr *)&sa, sizeof(struct sockaddr_in)) < 0) {
      printf("proxyconnect: unable to connect to proxy\n");
      close(fd);
      return -1;
    }

    i = 0;
    buf[0] = 0x05; /* SOCKS version 5 */
    if (strlen(sess->socksproxy.username)) {
      buf[1] = 0x02; /* two methods */
      buf[2] = 0x00; /* no authentication */
      buf[3] = 0x02; /* username/password authentication */
      i = 4;
    } else {
      buf[1] = 0x01;
      buf[2] = 0x00;
      i = 3;
    }

    if (write(fd, buf, i) < i) {
      *statusret = errno;
      close(fd);
      return -1;
    }

    if (read(fd, buf, 2) < 2) {
      *statusret = errno;
      close(fd);
      return -1;
    }

    if ((buf[0] != 0x05) || (buf[1] == 0xff)) {
      *statusret = EINVAL;
      close(fd);
      return -1;
    }

    /* check if we're doing username authentication */
    if (buf[1] == 0x02) {
      i  = aimutil_put8(buf, 0x01); /* version 1 */
      i += aimutil_put8(buf+i, strlen(sess->socksproxy.username));
      i += aimutil_putstr(buf+i, sess->socksproxy.username, strlen(sess->socksproxy.username));
      i += aimutil_put8(buf+i, strlen(sess->socksproxy.password));
      i += aimutil_putstr(buf+i, sess->socksproxy.password, strlen(sess->socksproxy.password));
      if (write(fd, buf, i) < i) {
	*statusret = errno;
	close(fd);
	return -1;
      }
      if (read(fd, buf, 2) < 2) {
	*statusret = errno;
	close(fd);
	return -1;
      }
      if ((buf[0] != 0x01) || (buf[1] != 0x00)) {
	*statusret = EINVAL;
	close(fd);
	return -1;
      }
    }

    i  = aimutil_put8(buf, 0x05);
    i += aimutil_put8(buf+i, 0x01); /* CONNECT */
    i += aimutil_put8(buf+i, 0x00); /* reserved */
    i += aimutil_put8(buf+i, 0x03); /* address type: host name */
    i += aimutil_put8(buf+i, strlen(host));
    i += aimutil_putstr(buf+i, host, strlen(host));
    i += aimutil_put16(buf+i, port);

    if (write(fd, buf, i) < i) {
      *statusret = errno;
      close(fd);
      return -1;
    }
    if (read(fd, buf, 10) < 10) {
      *statusret = errno;
      close(fd);
      return -1;
    }
    if ((buf[0] != 0x05) || (buf[1] != 0x00)) {
      *statusret = EINVAL;
      close(fd);
      return -1;
    }

  } else { /* connecting directly */
    struct sockaddr_in sa;
    struct hostent *hp;

    if (!(hp = gethostbyname(host))) {
      *statusret = (h_errno | AIM_CONN_STATUS_RESOLVERR);
      return -1;
    }

    memset(&sa.sin_zero, 0, 8);
    sa.sin_port = htons(port);
    memcpy(&sa.sin_addr, hp->h_addr, hp->h_length);
    sa.sin_family = hp->h_addrtype;
  
    fd = socket(hp->h_addrtype, SOCK_STREAM, 0);
    if (connect(fd, (struct sockaddr *)&sa, sizeof(struct sockaddr_in)) < 0) {
      close(fd);
      fd = -1;
    }
  }
  return fd;
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
faim_export struct aim_conn_t *aim_newconn(struct aim_session_t *sess,
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

  for(i=0;i<(int)strlen(dest);i++) {
    if (dest[i] == ':') {
      port = atoi(&(dest[i+1]));
      break;
    }
  }
  host = (char *)malloc(i+1);
  strncpy(host, dest, i);
  host[i] = '\0';

  if ((ret = aim_proxyconnect(sess, host, port, &connstruct->status)) < 0) {
    connstruct->fd = -1;
    connstruct->status = (errno | AIM_CONN_STATUS_CONNERR);
    free(host);
    faim_mutex_unlock(&connstruct->active);
    return connstruct;
  } else
    connstruct->fd = ret;
  
  faim_mutex_unlock(&connstruct->active);

  free(host);

  return connstruct;
}

faim_export int aim_conngetmaxfd(struct aim_session_t *sess)
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

static int aim_countconn(struct aim_session_t *sess)
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
faim_export struct aim_conn_t *aim_select(struct aim_session_t *sess,
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
    *status = 0; /* shouldn't happen */
  } else if ((i == -1) && (errno == EINTR)) /* treat interrupts as a timeout */
    *status = 0;
  else
    *status = i; /* can be 0 or -1 */

  faim_mutex_unlock(&sess->connlistlock);
  return NULL;  /* no waiting or error, return */
}

faim_export int aim_conn_isready(struct aim_conn_t *conn)
{
  if (conn)
    return (conn->status & 0x0001);
  return -1;
}

faim_export int aim_conn_setstatus(struct aim_conn_t *conn, int status)
{
  int val;

  if (!conn)
    return -1;
  
  faim_mutex_lock(&conn->active);
  val = conn->status ^= status;
  faim_mutex_unlock(&conn->active);
  return val;
}

faim_export int aim_conn_setlatency(struct aim_conn_t *conn, int newval)
{
  if (!conn)
    return -1;

  faim_mutex_lock(&conn->active);
  conn->forcedlatency = newval;
  conn->lastactivity = 0; /* reset this just to make sure */
  faim_mutex_unlock(&conn->active);

  return 0;
}

/*
 * Call this with your SOCKS5 proxy server parameters before
 * the first call to aim_newconn().  If called with all NULL
 * args, it will clear out a previously set proxy.  
 *
 * Set username and password to NULL if not applicable.
 *
 */
faim_export void aim_setupproxy(struct aim_session_t *sess, char *server, char *username, char *password)
{
  /* clear out the proxy info */
  if (!server || !strlen(server)) {
    memset(sess->socksproxy.server, 0, sizeof(sess->socksproxy.server));
    memset(sess->socksproxy.username, 0, sizeof(sess->socksproxy.username));
    memset(sess->socksproxy.password, 0, sizeof(sess->socksproxy.password));
    return;
  }

  strncpy(sess->socksproxy.server, server, sizeof(sess->socksproxy.server));
  if (username && strlen(username)) 
    strncpy(sess->socksproxy.username, username, sizeof(sess->socksproxy.username));
  if (password && strlen(password))
    strncpy(sess->socksproxy.password, password, sizeof(sess->socksproxy.password));
  return;
}

faim_export void aim_session_init(struct aim_session_t *sess)
{
  if (!sess)
    return;

  memset(sess, 0, sizeof(struct aim_session_t));
  aim_connrst(sess);
  sess->queue_outgoing = NULL;
  sess->queue_incoming = NULL;
  sess->pendingjoin = NULL;
  aim_initsnachash(sess);
  sess->snac_nextid = 0x00000001;

  /*
   * This must always be set.  Default to the queue-based
   * version for back-compatibility.  
   */
  sess->tx_enqueue = &aim_tx_enqueue__queuebased;

  return;
}
