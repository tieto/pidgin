/*
 *  aim_rxqueue.c
 *
 * This file contains the management routines for the receive
 * (incoming packet) queue.  The actual packet handlers are in
 * aim_rxhandlers.c.
 */

#include <faim/aim.h> 

/*
 * Grab a single command sequence off the socket, and enqueue
 * it in the incoming event queue in a seperate struct.
 */
int aim_get_command(struct aim_session_t *sess, struct aim_conn_t *conn)
{
  unsigned char generic[6]; 
  struct command_rx_struct *newrx = NULL;

  if (!sess || !conn)
    return 0;

  if (conn->fd < 3)  /* can happen when people abuse the interface */
    return 0;

  /*
   * Rendezvous (client-client) connections do not speak
   * FLAP, so this function will break on them.
   */
  if (conn->type == AIM_CONN_TYPE_RENDEZVOUS) 
    return aim_get_command_rendezvous(sess, conn);

  /*
   * Read FLAP header.  Six bytes:
   *    
   *   0 char  -- Always 0x2a
   *   1 char  -- Channel ID.  Usually 2 -- 1 and 4 are used during login.
   *   2 short -- Sequence number 
   *   4 short -- Number of data bytes that follow.
   */
  faim_mutex_lock(&conn->active);
  if (read(conn->fd, generic, 6) < 6){
    aim_conn_kill(sess, &conn);
    faim_mutex_unlock(&conn->active);
    return -1;
  }

  /*
   * This shouldn't happen unless the socket breaks, the server breaks,
   * or we break.  We must handle it just in case.
   */
  if (generic[0] != 0x2a) {
    faimdprintf(1, "Bad incoming data!");
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
  if (read(conn->fd, newrx->data, newrx->commandlen) < newrx->commandlen){
    free(newrx->data);
    free(newrx);
    aim_conn_kill(sess, &conn);
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

int aim_get_command_rendezvous(struct aim_session_t *sess, struct aim_conn_t *conn)
{
  unsigned char hdrbuf1[6];
  unsigned char *hdr = NULL;
  int hdrlen, hdrtype;
  int payloadlength = 0;
  int flags = 0;
  char *snptr = NULL;

  if (read(conn->fd, hdrbuf1, 6) < 6) {
    perror("read");
    printf("faim: rend: read error\n");
    aim_conn_kill(sess, &conn);
    return -1;
  }

  hdrlen = aimutil_get16(hdrbuf1+4);

  hdrlen -= 6;
  hdr = malloc(hdrlen);

  faim_mutex_lock(&conn->active);
  if (read(conn->fd, hdr, hdrlen) < hdrlen) {
    perror("read");
    printf("faim: rend: read2 error\n");
    free(hdr);
    faim_mutex_unlock(&conn->active);
    aim_conn_kill(sess, &conn);
    return -1;
  }
  
  hdrtype = aimutil_get16(hdr);  

  switch (hdrtype) {
  case 0x0001: {
    payloadlength = aimutil_get32(hdr+22);
    flags = aimutil_get16(hdr+32);
    snptr = hdr+38;

    printf("OFT frame: %04x / %04x / %04x / %s\n", hdrtype, payloadlength, flags, snptr);

    if (flags == 0x000e) {
      printf("directim: %s has started typing\n", snptr);
    } else if ((flags == 0x0000) && payloadlength) {
      unsigned char *buf;
      buf = malloc(payloadlength+1);

      /* XXX theres got to be a better way */
      faim_mutex_lock(&conn->active);
      if (recv(conn->fd, buf, payloadlength, MSG_WAITALL) < payloadlength) {
	perror("read");
	printf("faim: rend: read3 error\n");
	free(buf);
	faim_mutex_unlock(&conn->active);
	aim_conn_kill(sess, &conn);
	return -1;
      }
      faim_mutex_unlock(&conn->active);
      buf[payloadlength] = '\0';
      printf("directim: %s/%04x/%04x/%s\n", snptr, payloadlength, flags, buf);
      aim_send_im_direct(sess, conn, buf);
      free(buf);
    }
    break;
  }
  default:
    printf("OFT frame: type %04x\n", hdrtype);  
    /* data connection may be unreliable here */
    break;
  } /* switch */

  free(hdr);
  
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
void aim_purge_rxqueue(struct aim_session_t *sess)
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
void aim_rxqueue_cleanbyconn(struct aim_session_t *sess, struct aim_conn_t *conn)
{
  struct command_rx_struct *currx;

  for (currx = sess->queue_incoming; currx; currx = currx->next) {
    if ((!currx->handled) && (currx->conn == conn))
      currx->handled = 1;
  }	
  return;
}
