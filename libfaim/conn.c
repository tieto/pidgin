
/*
 *  aim_conn.c
 *
 * Does all this gloriously nifty connection handling stuff...
 *
 */

#define FAIM_INTERNAL
#include <aim.h> 

#ifndef _WIN32
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

/**
 * aim_connrst - Clears out connection list, killing remaining connections.
 * @sess: Session to be cleared
 *
 * Clears out the connection list and kills any connections left.
 *
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

/**
 * aim_conn_init - Reset a connection to default values.
 * @deadconn: Connection to be reset
 *
 * Initializes and/or resets a connection structure.
 *
 */
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

/**
 * aim_conn_getnext - Gets a new connection structure.
 * @sess: Session
 *
 * Allocate a new empty connection structure.
 *
 */
faim_internal struct aim_conn_t *aim_conn_getnext(struct aim_session_t *sess)
{
  struct aim_conn_t *newconn, *cur;

  if (!(newconn = malloc(sizeof(struct aim_conn_t)))) 	
    return NULL;

  memset(newconn, 0, sizeof(struct aim_conn_t));
  aim_conn_init(newconn);
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

/**
 * aim_conn_kill - Close and free a connection.
 * @sess: Session for the connection
 * @deadconn: Connection to be freed
 *
 * Close, clear, and free a connection structure. Should never be
 * called from within libfaim.
 *
 */
faim_export void aim_conn_kill(struct aim_session_t *sess, struct aim_conn_t **deadconn)
{
  struct aim_conn_t *cur;

  if (!deadconn || !*deadconn)	
    return;

  aim_tx_cleanqueue(sess, *deadconn);

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

  if ((*deadconn)->fd != -1) 
    aim_conn_close(*deadconn);
  if ((*deadconn)->priv)
    free((*deadconn)->priv);
  free(*deadconn);
  deadconn = NULL;

  return;
}

/**
 * aim_conn_close - Close a connection
 * @deadconn: Connection to close
 *
 * Close (but not free) a connection.
 *
 * This leaves everything untouched except for clearing the 
 * handler list and setting the fd to -1 (used to recognize
 * dead connections).
 *
 */
faim_export void aim_conn_close(struct aim_conn_t *deadconn)
{

  faim_mutex_destroy(&deadconn->active);
  faim_mutex_destroy(&deadconn->seqnum_lock);
  if (deadconn->fd >= 3)
    close(deadconn->fd);
  deadconn->fd = -1;
  if (deadconn->handlerlist)
    aim_clearhandlers(deadconn);

  return;
}

/**
 * aim_getconn_type - Find a connection of a specific type
 * @sess: Session to search
 * @type: Type of connection to look for
 *
 * Searches for a connection of the specified type in the 
 * specified session.  Returns the first connection of that
 * type found.
 *
 */
faim_export struct aim_conn_t *aim_getconn_type(struct aim_session_t *sess,
						int type)
{
  struct aim_conn_t *cur;

  faim_mutex_lock(&sess->connlistlock);
  for (cur = sess->connlist; cur; cur = cur->next) {
    if ((cur->type == type) && !(cur->status & AIM_CONN_STATUS_INPROGRESS))
      break;
  }
  faim_mutex_unlock(&sess->connlistlock);
  return cur;
}

/**
 * aim_proxyconnect - An extrememly quick and dirty SOCKS5 interface. 
 * @sess: Session to connect
 * @host: Host to connect to
 * @port: Port to connect to
 * @statusret: Return value of the connection
 *
 * Attempts to connect to the specified host via the configured
 * proxy settings, if present.  If no proxy is configured for
 * this session, the connection is done directly.
 *
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
      faimdprintf(sess, 0, "proxyconnect: unable to resolve proxy name\n");
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
      faimdprintf(sess, 0, "proxyconnect: unable to connect to proxy\n");
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

    memset(&sa, 0, sizeof(struct sockaddr_in));
    sa.sin_port = htons(port);
    memcpy(&sa.sin_addr, hp->h_addr, hp->h_length);
    sa.sin_family = hp->h_addrtype;
  
    fd = socket(hp->h_addrtype, SOCK_STREAM, 0);

    if (sess->flags & AIM_SESS_FLAGS_NONBLOCKCONNECT)
      fcntl(fd, F_SETFL, O_NONBLOCK); /* XXX save flags */

    if (connect(fd, (struct sockaddr *)&sa, sizeof(struct sockaddr_in)) < 0) {
      if (sess->flags & AIM_SESS_FLAGS_NONBLOCKCONNECT) {
	if ((errno == EINPROGRESS) || (errno == EINTR)) {
	  if (statusret)
	    *statusret |= AIM_CONN_STATUS_INPROGRESS;
	  return fd;
	}
      }
      close(fd);
      fd = -1;
    }
  }
  return fd;
}

/**
 * aim_cloneconn - clone an aim_conn_t
 * @sess: session containing parent
 * @src: connection to clone
 *
 * A new connection is allocated, and the values are filled in
 * appropriately. Note that this function sets the new connnection's
 * ->priv pointer to be equal to that of its parent: only the pointer
 * is copied, not the data it points to.
 *
 * This function returns a pointer to the new aim_conn_t, or %NULL on
 * error
 */
faim_internal struct aim_conn_t *aim_cloneconn(struct aim_session_t *sess,
					       struct aim_conn_t *src)
{
  struct aim_conn_t *conn;
    struct aim_rxcblist_t *cur;

  if (!(conn = aim_conn_getnext(sess)))
    return NULL;

  faim_mutex_lock(&conn->active);

  conn->fd = src->fd;
  conn->type = src->type;
  conn->subtype = src->subtype;
  conn->seqnum = src->seqnum;
  conn->priv = src->priv;
  conn->lastactivity = src->lastactivity;
  conn->forcedlatency = src->forcedlatency;

  /* clone handler list */
  for (cur = src->handlerlist; cur; cur = cur->next) {
    aim_conn_addhandler(sess, conn, cur->family, cur->type, 
			cur->handler, cur->flags);
  }

  faim_mutex_unlock(&conn->active);

  return conn;
}

/**
 * aim_newconn - Open a new connection
 * @sess: Session to create connection in
 * @type: Type of connection to create
 * @dest: Host to connect to (in "host:port" syntax)
 *
 * Opens a new connection to the specified dest host of specified
 * type, using the proxy settings if available.  If @host is %NULL,
 * the connection is allocated and returned, but no connection 
 * is made.
 *
 * FIXME: Return errors in a more sane way.
 *
 */
faim_export struct aim_conn_t *aim_newconn(struct aim_session_t *sess,
					   int type, char *dest)
{
  struct aim_conn_t *connstruct;
  int ret;
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

/**
 * aim_conngetmaxfd - Return the highest valued file discriptor in session
 * @sess: Session to search
 *
 * Returns the highest valued filed descriptor of all open 
 * connections in @sess.
 *
 */
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

/**
 * aim_conn_in_sess - Predicate to test the precense of a connection in a sess
 * @sess: Session to look in
 * @conn: Connection to look for
 *
 * Searches @sess for the passed connection.  Returns 1 if its present,
 * zero otherwise.
 *
 */
faim_export int aim_conn_in_sess(struct aim_session_t *sess, struct aim_conn_t *conn)
{
  struct aim_conn_t *cur;

  faim_mutex_lock(&sess->connlistlock);
  for(cur = sess->connlist; cur; cur = cur->next)
    if(cur == conn) {
      faim_mutex_unlock(&sess->connlistlock);
      return 1;
    }
  faim_mutex_unlock(&sess->connlistlock);
  return 0;
}

/**
 * aim_select - Wait for a socket with data or timeout
 * @sess: Session to wait on
 * @timeout: How long to wait
 * @status: Return status
 *
 * Waits for a socket with data or for timeout, whichever comes first.
 * See select(2).
 * 
 * Return codes in *status:
 *   -1  error in select() (%NULL returned)
 *    0  no events pending (%NULL returned)
 *    1  outgoing data pending (%NULL returned)
 *    2  incoming data pending (connection with pending data returned)
 *
 * XXX: we could probably stand to do a little courser locking here.
 *
 */ 
faim_export struct aim_conn_t *aim_select(struct aim_session_t *sess,
					  struct timeval *timeout, 
					  int *status)
{
  struct aim_conn_t *cur;
  fd_set fds, wfds;
  int maxfd = 0;
  int i, haveconnecting = 0;

  faim_mutex_lock(&sess->connlistlock);
  if (sess->connlist == NULL) {
    faim_mutex_unlock(&sess->connlistlock);
    *status = -1;
    return NULL;
  }
  faim_mutex_unlock(&sess->connlistlock);

  FD_ZERO(&fds);
  FD_ZERO(&wfds);
  maxfd = 0;

  faim_mutex_lock(&sess->connlistlock);
  for (cur = sess->connlist; cur; cur = cur->next) {
    if (cur->fd == -1) {
      /* don't let invalid/dead connections sit around */
      *status = 2;
      faim_mutex_unlock(&sess->connlistlock);
      return cur;
    } else if (cur->status & AIM_CONN_STATUS_INPROGRESS) {
      FD_SET(cur->fd, &wfds);
      
      haveconnecting++;
    }
    FD_SET(cur->fd, &fds);
    if (cur->fd > maxfd)
      maxfd = cur->fd;
  }
  faim_mutex_unlock(&sess->connlistlock);

  /* 
   * If we have data waiting to be sent, return
   *
   * We have to not do this if theres at least one
   * connection thats still connecting, since that connection
   * may have queued data and this return would prevent
   * the connection from ever completing!  This is a major
   * inadequacy of the libfaim way of doing things.  It means
   * that nothing can transmit as long as there's connecting
   * sockets. Evil.
   *
   * But its still better than having blocking connects.
   *
   */
  if (!haveconnecting && (sess->queue_outgoing != NULL)) {
    *status = 1;
    return NULL;
  } 

  if ((i = select(maxfd+1, &fds, &wfds, NULL, timeout))>=1) {
    faim_mutex_lock(&sess->connlistlock);
    for (cur = sess->connlist; cur; cur = cur->next) {
      if ((FD_ISSET(cur->fd, &fds)) || 
	  ((cur->status & AIM_CONN_STATUS_INPROGRESS) && 
	   FD_ISSET(cur->fd, &wfds))) {
	*status = 2;
	faim_mutex_unlock(&sess->connlistlock);
	return cur; /* XXX race condition here -- shouldnt unlock connlist */
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

/**
 * aim_conn_isready - Test if a connection is marked ready
 * @conn: Connection to test
 *
 * Returns true if the connection is ready, false otherwise.
 * Returns -1 if the connection is invalid.
 *
 * XXX: This is deprecated.
 *
 */
faim_export int aim_conn_isready(struct aim_conn_t *conn)
{
  if (conn)
    return (conn->status & 0x0001);
  return -1;
}

/**
 * aim_conn_setstatus - Set the status of a connection
 * @conn: Connection
 * @status: New status
 *
 * @newstatus is %XOR'd with the previous value of the connection
 * status and returned.  Returns -1 if the connection is invalid.
 *
 * This isn't real useful.
 *
 */
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

/**
 * aim_conn_setlatency - Set a forced latency value for connection
 * @conn: Conn to set latency for
 * @newval: Number of seconds to force between transmits
 *
 * Causes @newval seconds to be spent between transmits on a connection.
 *
 * This is my lame attempt at overcoming not understanding the rate
 * limiting. 
 *
 * XXX: This should really be replaced with something that scales and
 * backs off like the real rate limiting does.
 *
 */
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

/**
 * aim_setupproxy - Configure a proxy for this session
 * @sess: Session to set proxy for
 * @server: SOCKS server
 * @username: SOCKS username
 * @password: SOCKS password
 *
 * Call this with your SOCKS5 proxy server parameters before
 * the first call to aim_newconn().  If called with all %NULL
 * args, it will clear out a previously set proxy.  
 *
 * Set username and password to %NULL if not applicable.
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

static void defaultdebugcb(struct aim_session_t *sess, int level, const char *format, va_list va)
{
  vfprintf(stderr, format, va);
}

/**
 * aim_session_init - Initializes a session structure
 * @sess: Session to initialize
 * @flags: Flags to use. Any of %AIM_SESS_FLAGS %OR'd together.
 * @debuglevel: Level of debugging output (zero is least)
 *
 * Sets up the initial values for a session.
 *
 */
faim_export void aim_session_init(struct aim_session_t *sess, unsigned long flags, int debuglevel)
{
  if (!sess)
    return;

  memset(sess, 0, sizeof(struct aim_session_t));
  aim_connrst(sess);
  sess->queue_outgoing = NULL;
  sess->queue_incoming = NULL;
  sess->pendingjoin = NULL;
  sess->pendingjoinexchange = 0;
  aim_initsnachash(sess);
  sess->msgcookies = NULL;
  sess->snac_nextid = 0x00000001;

  sess->flags = 0;
  sess->debug = debuglevel;
  sess->debugcb = defaultdebugcb;

  sess->modlistv = NULL;

  /*
   * Default to SNAC login unless XORLOGIN is explicitly set.
   */
  if (!(flags & AIM_SESS_FLAGS_XORLOGIN))
    sess->flags |= AIM_SESS_FLAGS_SNACLOGIN;
  sess->flags |= flags;

  /*
   * This must always be set.  Default to the queue-based
   * version for back-compatibility.  
   */
  aim_tx_setenqueue(sess, AIM_TX_QUEUED, NULL);


  /*
   * Register all the modules for this session...
   */
  aim__registermodule(sess, misc_modfirst); /* load the catch-all first */
  aim__registermodule(sess, buddylist_modfirst);
  aim__registermodule(sess, admin_modfirst);
  aim__registermodule(sess, bos_modfirst);
  aim__registermodule(sess, search_modfirst);
  aim__registermodule(sess, stats_modfirst);
  aim__registermodule(sess, auth_modfirst);
  aim__registermodule(sess, msg_modfirst);
  aim__registermodule(sess, chatnav_modfirst);
  aim__registermodule(sess, chat_modfirst);
  aim__registermodule(sess, locate_modfirst);
  aim__registermodule(sess, general_modfirst);

  return;
}

/**
 * aim_session_kill - Deallocate a session
 * @sess: Session to kill
 *
 *
 */
faim_export void aim_session_kill(struct aim_session_t *sess)
{

  aim_logoff(sess);

  aim__shutdownmodules(sess);

  return;
}

/**
 * aim_setdebuggingcb - Set the function to call when outputting debugging info
 * @sess: Session to change
 * @cb: Function to call
 *
 * The function specified is called whenever faimdprintf() is used within
 * libfaim, and the session's debugging level is greater tha nor equal to
 * the value faimdprintf was called with.
 *
 */
faim_export int aim_setdebuggingcb(struct aim_session_t *sess, faim_debugging_callback_t cb)
{

  if (!sess)
    return -1;

  sess->debugcb = cb;

  return 0;
}

/**
 * aim_conn_isconnecting - Determine if a connection is connecting
 * @conn: Connection to examine
 *
 * Returns nonzero if the connection is in the process of
 * connecting (or if it just completed and aim_conn_completeconnect()
 * has yet to be called on it).
 *
 */
faim_export int aim_conn_isconnecting(struct aim_conn_t *conn)
{
  if (!conn)
    return 0;
  return (conn->status & AIM_CONN_STATUS_INPROGRESS)?1:0;
}

faim_export int aim_conn_completeconnect(struct aim_session_t *sess, struct aim_conn_t *conn)
{
  fd_set fds, wfds;
  struct timeval tv;
  int res, error = ETIMEDOUT;
  rxcallback_t userfunc;

  if (!conn || (conn->fd == -1))
    return -1;

  if (!(conn->status & AIM_CONN_STATUS_INPROGRESS))
    return -1;

  FD_ZERO(&fds);
  FD_SET(conn->fd, &fds);
  FD_ZERO(&wfds);
  FD_SET(conn->fd, &wfds);
  tv.tv_sec = 0;
  tv.tv_usec = 0;

  if ((res = select(conn->fd+1, &fds, &wfds, NULL, &tv)) == -1) {
    error = errno;
    aim_conn_close(conn);
    errno = error;
    return -1;
  } else if (res == 0) {
    faimdprintf(sess, 0, "aim_conn_completeconnect: false alarm on %d\n", conn->fd);
    return 0; /* hasn't really completed yet... */
  } 

  if (FD_ISSET(conn->fd, &fds) || FD_ISSET(conn->fd, &wfds)) {
    int len = sizeof(error);

    if (getsockopt(conn->fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0)
      error = errno;
  }

  if (error) {
    aim_conn_close(conn);
    errno = error;
    return -1;
  }

  fcntl(conn->fd, F_SETFL, 0); /* XXX should restore original flags */

  conn->status &= ~AIM_CONN_STATUS_INPROGRESS;

  if ((userfunc = aim_callhandler(sess, conn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNCOMPLETE)))
    userfunc(sess, NULL, conn);

  /* Flush out the queues if there was something waiting for this conn  */
  aim_tx_flushqueue(sess);

  return 0;
}

/*
 * aim_logoff()
 *
 * Closes -ALL- open connections.
 *
 */
faim_export int aim_logoff(struct aim_session_t *sess)
{

  aim_connrst(sess);  /* in case we want to connect again */

  return 0;

}

