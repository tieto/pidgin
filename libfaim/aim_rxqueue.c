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
  u_char generic[6]; 
  struct command_rx_struct *newrx = NULL;

  if (!sess || !conn)
    return 0;

  if (conn->fd < 3)  /* can happen when people abuse the interface */
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
  if (read(conn->fd, generic, 6) < 6){
    aim_conn_close(conn);
    faim_mutex_unlock(&conn->active);
    return -1;
  }
  faim_mutex_unlock(&conn->active);

  /*
   * This shouldn't happen unless the socket breaks, the server breaks,
   * or we break.  We must handle it just in case.
   */
  if (generic[0] != 0x2a) {
    faimdprintf(1, "Bad incoming data!");
    return -1;
  }	

  /* allocate a new struct */
  newrx = (struct command_rx_struct *)malloc(sizeof(struct command_rx_struct));
  if (!newrx)
    return -1;
  memset(newrx, 0x00, sizeof(struct command_rx_struct));

  newrx->lock = 1;  /* lock the struct */

  /* store channel -- byte 2 */
  newrx->type = (char) generic[1];

  /* store seqnum -- bytes 3 and 4 */
  newrx->seqnum = aimutil_get16(generic+2);

  /* store commandlen -- bytes 5 and 6 */
  newrx->commandlen = aimutil_get16(generic+4);

  newrx->nofree = 0; /* free by default */

  /* malloc for data portion */
  newrx->data = (u_char *) malloc(newrx->commandlen);
  if (!newrx->data) {
    free(newrx);
    return -1;
  }

  /* read the data portion of the packet */
  faim_mutex_lock(&conn->active);
  if (read(conn->fd, newrx->data, newrx->commandlen) < newrx->commandlen){
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
