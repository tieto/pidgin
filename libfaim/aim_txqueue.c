/*
 *  aim_txqueue.c
 *
 * Herein lies all the mangement routines for the transmit (Tx) queue.
 *
 */

#include <aim.h>

/*
 * Allocate a new tx frame.
 *
 * This is more for looks than anything else.
 *
 * Right now, that is.  If/when we implement a pool of transmit
 * frames, this will become the request-an-unused-frame part.
 */
struct command_tx_struct *aim_tx_new(int chan, struct aim_conn_t *conn, int datalen)
{
  struct command_tx_struct *new;

  if (!conn) {
    printf("aim_tx_new: ERROR: no connection specified\n");
    return NULL;
  }

  new = (struct command_tx_struct *)malloc(sizeof(struct command_tx_struct));
  if (!new)
    return NULL;
  memset(new, 0, sizeof(struct command_tx_struct));

  new->conn = conn; 
  new->type = chan;

  if(datalen) {
    new->data = (u_char *)malloc(datalen);
    new->commandlen = datalen;
  }

  return new;
}

/*
 * aim_tx_enqeue__queuebased()
 *
 * The overall purpose here is to enqueue the passed in command struct
 * into the outgoing (tx) queue.  Basically...
 *   1) Make a scope-irrelevent copy of the struct
 *   2) Lock the struct
 *   3) Mark as not-sent-yet
 *   4) Enqueue the struct into the list
 *   5) Unlock the struct once it's linked in
 *   6) Return
 *
 * Note that this is only used when doing queue-based transmitting;
 * that is, when sess->tx_enqueue is set to &aim_tx_enqueue__queuebased.
 *
 */
int aim_tx_enqueue__queuebased(struct aim_session_t *sess,
			       struct command_tx_struct *newpacket)
{
  struct command_tx_struct *cur;

  if (newpacket->conn == NULL) {
      faimdprintf(1, "aim_tx_enqueue: WARNING: enqueueing packet with no connecetion\n");
      newpacket->conn = aim_getconn_type(sess, AIM_CONN_TYPE_BOS);
  }
 
  /* assign seqnum */
  newpacket->seqnum = aim_get_next_txseqnum(newpacket->conn);
  /* set some more fields */
  newpacket->lock = 1; /* lock */
  newpacket->sent = 0; /* not sent yet */
  newpacket->next = NULL; /* always last */

  /* see overhead note in aim_rxqueue counterpart */
  if (sess->queue_outgoing == NULL) {
    sess->queue_outgoing = newpacket;
  } else {
    for (cur = sess->queue_outgoing;
	 cur->next;
	 cur = cur->next)
      ;
    cur->next = newpacket;
  }

  newpacket->lock = 0; /* unlock so it can be sent */

#if debug == 2
  faimdprintf(2, "calling aim_tx_printqueue()\n");
  aim_tx_printqueue(sess);
  faimdprintf(2, "back from aim_tx_printqueue()\n");
#endif

  return 0;
}

/*
 * aim_tx_enqueue__immediate()
 *
 * Parallel to aim_tx_enqueue__queuebased, however, this bypasses
 * the whole queue mess when you want immediate writes to happen.
 *
 * Basically the same as its __queuebased couterpart, however
 * instead of doing a list append, it just calls aim_tx_sendframe()
 * right here. 
 * 
 */
int aim_tx_enqueue__immediate(struct aim_session_t *sess, struct command_tx_struct *newpacket)
{
  if (newpacket->conn == NULL) {
    faimdprintf(1, "aim_tx_enqueue: ERROR: packet has no connection\n");
    if (newpacket->data)
      free(newpacket->data);
    free(newpacket);
    return -1;
  }

  newpacket->seqnum = aim_get_next_txseqnum(newpacket->conn);

  newpacket->lock = 1; /* lock */
  newpacket->sent = 0; /* not sent yet */

  aim_tx_sendframe(newpacket);

  if (newpacket->data)
    free(newpacket->data);
  free(newpacket);

  return 0;
}

/* 
 *  aim_get_next_txseqnum()
 *
 *   This increments the tx command count, and returns the seqnum
 *   that should be stamped on the next FLAP packet sent.  This is
 *   normally called during the final step of packet preparation
 *   before enqueuement (in aim_tx_enqueue()).
 *
 */
u_int aim_get_next_txseqnum(struct aim_conn_t *conn)
{
  u_int ret;
  
  faim_mutex_lock(&conn->seqnum_lock);
  ret = ++conn->seqnum;
  faim_mutex_unlock(&conn->seqnum_lock);
  return ret;
}

/*
 *  aim_tx_printqueue()
 *
 *  This is basically for debuging purposes only.  It dumps all the
 *  records in the tx queue and their current status.  Very helpful
 *  if the queue isn't working quite right.
 *
 */
#if debug == 2
int aim_tx_printqueue(struct aim_session_t *sess)
{
  struct command_tx_struct *cur;

  faimdprintf(2, "\ncurrent aim_queue_outgoing...\n");
  faimdprintf(2, "\ttype seqnum  len  lock sent\n");  

  if (sess->queue_outgoing == NULL)
    faimdprintf(2, "aim_tx_flushqueue(): queue empty");
  else {
      for (cur = sess->queue_outgoing; cur; cur = cur->next) {
	  faimdprintf(2, "\t  %2x   %4x %4x   %1d    %1d\n", 
		      cur->type, cur->seqnum, 
		      cur->commandlen, cur->lock, 
		      cur->sent);
      }
  }

  faimdprintf(2, "\n(done printing queue)\n");
  
  return 0;
}
#endif

/*
 *  aim_tx_flushqueue()
 *
 *  This the function is responsable for putting the queued commands
 *  onto the wire.  This function is critical to the operation of 
 *  the queue and therefore is the most prone to brokenness.  It
 *  seems to be working quite well at this point.
 *
 *  Procedure:
 *    1) Traverse the list, only operate on commands that are unlocked
 *       and haven't been sent yet.
 *    2) Lock the struct
 *    3) Allocate a temporary buffer to store the finished, fully
 *       processed packet in.
 *    4) Build the packet from the command_tx_struct data.
 *    5) Write the packet to the socket.
 *    6) If success, mark the packet sent, if fail report failure, do NOT
 *       mark the packet sent (so it will not get purged and therefore
 *       be attempted again on next call).
 *    7) Unlock the struct.
 *    8) Free the temp buffer
 *    9) Step to next struct in list and go back to 1.
 *
 */
int aim_tx_sendframe(struct command_tx_struct *cur)
{
  u_char *curPacket;

  if (!cur)
    return -1; /* fatal */

  cur->lock = 1; /* lock the struct */

  /* allocate full-packet buffer */
  curPacket = (char *) malloc(cur->commandlen + 6);
      
  /* command byte */
  curPacket[0] = 0x2a;
      
  /* type/family byte */
  curPacket[1] = cur->type;
      
  /* bytes 3+4: word: FLAP sequence number */
  aimutil_put16(curPacket+2, cur->seqnum);

  /* bytes 5+6: word: SNAC len */
  aimutil_put16(curPacket+4, cur->commandlen);
      
  /* bytes 7 and on: raw: SNAC data */  /* XXX: ye gods! get rid of this! */
  memcpy(&(curPacket[6]), cur->data, cur->commandlen);
      
  /* full image of raw packet data now in curPacket */
  faim_mutex_lock(&cur->conn->active);
  if ( (u_int)write(cur->conn->fd, curPacket, (cur->commandlen + 6)) != (cur->commandlen + 6)) {
    faim_mutex_unlock(&cur->conn->active);
    printf("\nWARNING: Error in sending packet 0x%4x -- will try again next time\n\n", cur->seqnum);
    cur->sent = 0; /* mark it unsent */
    return 0; /* bail out -- continuable error */
  } else {
    faimdprintf(2, "\nSENT 0x%4x\n\n", cur->seqnum);
    
    cur->sent = 1; /* mark the struct as sent */
    cur->conn->lastactivity = time(NULL);
  }
  faim_mutex_unlock(&cur->conn->active);

#if debug > 2
  faimdprintf(2, "\nPacket:");
  for (i = 0; i < (cur->commandlen + 6); i++) {
    if ((i % 8) == 0) {
      faimdprintf(2, "\n\t");
    }
    if (curPacket[i] >= ' ' && curPacket[i]<127) {
      faimdprintf(2, "%c=%02x ", curPacket[i], curPacket[i]);
    } else {
      faimdprintf(2, "0x%2x ", curPacket[i]);
    }
  }
  faimdprintf(2, "\n");
#endif
  cur->lock = 0; /* unlock the struct */
  free(curPacket); /* free up full-packet buffer */

  return 1; /* success */
}

int aim_tx_flushqueue(struct aim_session_t *sess)
{
  struct command_tx_struct *cur;
   
#if debug > 1
  int i = 0;
#endif

  if (sess->queue_outgoing == NULL)
    return 0;

  faimdprintf(2, "beginning txflush...\n");
  for (cur = sess->queue_outgoing; cur; cur = cur->next) {
    /* only process if its unlocked and unsent */
    if (!cur->lock && !cur->sent) {

      /*
       * And now for the meager attempt to force transmit
       * latency and avoid missed messages.
       */
      if ((cur->conn->lastactivity + cur->conn->forcedlatency) >= time(NULL)) {
	/* FIXME FIXME -- should be a break! we dont want to block the upper layers */
	sleep((cur->conn->lastactivity + cur->conn->forcedlatency) - time(NULL));
      }

      if (aim_tx_sendframe(cur) == -1)
	break;
    }
  }

  /* purge sent commands from queue */
  aim_tx_purgequeue(sess);

  return 0;
}

/*
 *  aim_tx_purgequeue()
 *  
 *  This is responsable for removing sent commands from the transmit 
 *  queue. This is not a required operation, but it of course helps
 *  reduce memory footprint at run time!  
 *
 */
void aim_tx_purgequeue(struct aim_session_t *sess)
{
  struct command_tx_struct *cur = NULL;
  struct command_tx_struct *tmp;

  if (sess->queue_outgoing == NULL)
    return;
  
  if (sess->queue_outgoing->next == NULL) {
    if (!sess->queue_outgoing->lock && sess->queue_outgoing->sent) {
      tmp = sess->queue_outgoing;
      sess->queue_outgoing = NULL;
      free(tmp->data);
      free(tmp);
    }
    return;
  }

  for(cur = sess->queue_outgoing; cur->next != NULL; ) {
    if (!cur->next->lock && cur->next->sent) {
      tmp = cur->next;
      cur->next = tmp->next;
      free(tmp->data);
      free(tmp);
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
