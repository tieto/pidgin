/*
 *  aim_rxqueue.c
 *
 * This file contains the management routines for the receive
 * (incoming packet) queue.  The actual packet handlers are in
 * aim_rxhandlers.c.
 */

#include <faim/aim.h> 

#ifndef _WIN32
#include <sys/socket.h>
#endif

/*
 * Since not all implementations support MSG_WAITALL, define
 * an alternate guarenteed read function...
 *
 * We keep recv() for systems that can do it because it means
 * a single system call for the entire packet, where read may
 * take more for a badly fragmented packet.
 *
 */
faim_internal int aim_recv(int fd, void *buf, size_t count)
{
#ifdef MSG_WAITALL
  return recv(fd, buf, count, MSG_WAITALL);
#else
  int left, ret, cur = 0; 

  left = count;

  while (left) {
    ret = recv(fd, ((unsigned char *)buf)+cur, left, 0);
    if (ret == -1)
      return -1;
    if (ret == 0)
      return cur;
    
    cur += ret;
    left -= ret;
  }

  return cur;
#endif
}

/*
 * Grab a single command sequence off the socket, and enqueue
 * it in the incoming event queue in a seperate struct.
 */
faim_export int aim_get_command(struct aim_session_t *sess, struct aim_conn_t *conn)
{
  unsigned char generic[6]; 
  struct command_rx_struct *newrx = NULL;

  if (!sess || !conn)
    return 0;

  if (conn->fd == -1)
    return -1; /* its a aim_conn_close()'d connection */

  if (conn->fd < 3)  /* can happen when people abuse the interface */
    return 0;

  if (conn->status & AIM_CONN_STATUS_INPROGRESS)
    return aim_conn_completeconnect(sess, conn);

  /*
   * Rendezvous (client-client) connections do not speak
   * FLAP, so this function will break on them.
   */
  if (conn->type == AIM_CONN_TYPE_RENDEZVOUS) 
    return aim_get_command_rendezvous(sess, conn);
  if (conn->type == AIM_CONN_TYPE_RENDEZVOUS_OUT) 
    return 0; 

  /*
   * Read FLAP header.  Six bytes:
   *    
   *   0 char  -- Always 0x2a
   *   1 char  -- Channel ID.  Usually 2 -- 1 and 4 are used during login.
   *   2 short -- Sequence number 
   *   4 short -- Number of data bytes that follow.
   */
  faim_mutex_lock(&conn->active);
  if (aim_recv(conn->fd, generic, 6) < 6){
    aim_conn_close(conn);
    faim_mutex_unlock(&conn->active);
    return -1;
  }

  /*
   * This shouldn't happen unless the socket breaks, the server breaks,
   * or we break.  We must handle it just in case.
   */
  if (generic[0] != 0x2a) {
    faimdprintf(1, "Bad incoming data!");
    aim_conn_close(conn);
    faim_mutex_unlock(&conn->active);
    return -1;
  }	

  /* allocate a new struct */
  if (!(newrx = (struct command_rx_struct *)malloc(sizeof(struct command_rx_struct)))) {
    faim_mutex_unlock(&conn->active);
    return -1;
  }
  memset(newrx, 0x00, sizeof(struct command_rx_struct));

  newrx->lock = 1;  /* lock the struct */

  /* we're doing OSCAR if we're here */
  newrx->hdrtype = AIM_FRAMETYPE_OSCAR;

  /* store channel -- byte 2 */
  newrx->hdr.oscar.type = (char) generic[1];

  /* store seqnum -- bytes 3 and 4 */
  newrx->hdr.oscar.seqnum = aimutil_get16(generic+2);

  /* store commandlen -- bytes 5 and 6 */
  newrx->commandlen = aimutil_get16(generic+4);

  newrx->nofree = 0; /* free by default */

  /* malloc for data portion */
  if (!(newrx->data = (u_char *) malloc(newrx->commandlen))) {
    free(newrx);
    faim_mutex_unlock(&conn->active);
    return -1;
  }

  /* read the data portion of the packet */
  if (aim_recv(conn->fd, newrx->data, newrx->commandlen) < newrx->commandlen){
    free(newrx->data);
    free(newrx);
    aim_conn_close(conn);
    faim_mutex_unlock(&conn->active);
    return -1;
  }
  faim_mutex_unlock(&conn->active);

  newrx->conn = conn;

  newrx->next = NULL;  /* this will always be at the bottom */
  newrx->lock = 0; /* unlock */

  /* enqueue this packet */
  if (sess->queue_incoming == NULL) {
    sess->queue_incoming = newrx;
  } else {
    struct command_rx_struct *cur;

    /*
     * This append operation takes a while.  It might be faster
     * if we maintain a pointer to the last entry in the queue
     * and just update that.  Need to determine if the overhead
     * to maintain that is lower than the overhead for this loop.
     */
    for (cur = sess->queue_incoming; cur->next; cur = cur->next)
      ;
    cur->next = newrx;
  }
  
  newrx->conn->lastactivity = time(NULL);

  return 0;  
}

/*
 * Purge recieve queue of all handled commands (->handled==1).  Also
 * allows for selective freeing using ->nofree so that the client can
 * keep the data for various purposes.  
 *
 * If ->nofree is nonzero, the frame will be delinked from the global list, 
 * but will not be free'ed.  The client _must_ keep a pointer to the
 * data -- libfaim will not!  If the client marks ->nofree but
 * does not keep a pointer, it's lost forever.
 *
 */
faim_export void aim_purge_rxqueue(struct aim_session_t *sess)
{
  struct command_rx_struct *cur = NULL;
  struct command_rx_struct *tmp;

  if (sess->queue_incoming == NULL)
    return;
  
  if (sess->queue_incoming->next == NULL) {
    if (sess->queue_incoming->handled) {
      tmp = sess->queue_incoming;
      sess->queue_incoming = NULL;

      if (!tmp->nofree) {
	if (tmp->hdrtype == AIM_FRAMETYPE_OFT)
	  free(tmp->hdr.oft.hdr2);
	free(tmp->data);
	free(tmp);
      } else
	tmp->next = NULL;
    }
    return;
  }

  for(cur = sess->queue_incoming; cur->next != NULL; ) {
    if (cur->next->handled) {
      tmp = cur->next;
      cur->next = tmp->next;
      if (!tmp->nofree) {
	if (tmp->hdrtype == AIM_FRAMETYPE_OFT)
	  free(tmp->hdr.oft.hdr2);
	free(tmp->data);
	free(tmp);
      } else
	tmp->next = NULL;
    }	
    cur = cur->next;

    /* 
     * Be careful here.  Because of the way we just
     * manipulated the pointer, cur may be NULL and 
     * the for() will segfault doing the check unless
     * we find this case first.
     */
    if (cur == NULL)	
      break;
  }

  return;
}

/*
 * Since aim_get_command will aim_conn_kill dead connections, we need
 * to clean up the rxqueue of unprocessed connections on that socket.
 *
 * XXX: this is something that was handled better in the old connection
 * handling method, but eh.
 */
faim_internal void aim_rxqueue_cleanbyconn(struct aim_session_t *sess, struct aim_conn_t *conn)
{
  struct command_rx_struct *currx;

  for (currx = sess->queue_incoming; currx; currx = currx->next) {
    if ((!currx->handled) && (currx->conn == conn))
      currx->handled = 1;
  }	
  return;
}
