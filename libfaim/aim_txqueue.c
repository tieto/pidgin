/*
  aim_txqueue.c

  Herein lies all the mangement routines for the transmit (Tx) queue.

 */

#include "aim.h"

/*
  aim_tx_enqeue()

  The overall purpose here is to enqueue the passed in command struct
  into the outgoing (tx) queue.  Basically...
    1) Make a scope-irrelevent copy of the struct
    2) Lock the struct
    3) Mark as not-sent-yet
    4) Enqueue the struct into the list
    5) Unlock the struct once it's linked in
    6) Return

 */

int aim_tx_enqueue(struct command_tx_struct *newpacket)
{
  struct command_tx_struct *workingPtr = NULL;
  struct command_tx_struct *newpacket_copy = NULL;

  if (newpacket->conn == NULL)
    {
      printf("aim_tx_enqueue: WARNING: enqueueing packet with no connecetion,  defaulting to BOS\n");
      newpacket->conn = aim_getconn_type(AIM_CONN_TYPE_BOS);
    }
 
  newpacket_copy = (struct command_tx_struct *) malloc (sizeof(struct command_tx_struct));
  memcpy(newpacket_copy, newpacket, sizeof(struct command_tx_struct));

  /* assign seqnum */
  newpacket_copy->seqnum = aim_get_next_txseqnum(newpacket_copy->conn);
  /* set some more fields */
  newpacket_copy->lock = 1; /* lock */
  newpacket_copy->sent = 0; /* not sent yet */
  newpacket_copy->next = NULL; /* always last */

  if (aim_queue_outgoing == NULL)
    {
      aim_queue_outgoing = newpacket_copy;
    }
  else
    {
      workingPtr = aim_queue_outgoing;
      while (workingPtr->next != NULL)
	workingPtr = workingPtr->next;
      workingPtr->next = newpacket_copy;
    }

  newpacket_copy->lock = 0; /* unlock so it can be sent */

#if debug > 2
  printf("calling aim_tx_printqueue()\n");
  aim_tx_printqueue();
  printf("back from aim_tx_printqueue()\n");
#endif

  /* we'll force a flush for now -- this behavior probably will change */
#if debug > 1
  printf("calling aim_tx_flushqueue()\n");
#endif
  aim_tx_flushqueue();
#if debug > 1
  printf("back from aim_tx_flushqueue()\n");
#endif

  return 0;
}

/* 
   aim_get_next_txseqnum()

   This increments the tx command count, and returns the seqnum
   that should be stamped on the next FLAP packet sent.  This is
   normally called during the final step of packet preparation
   before enqueuement (in aim_tx_enqueue()).

 */
unsigned int aim_get_next_txseqnum(struct aim_conn_t *conn)
{
  return ( ++conn->seqnum );
}

/*
  aim_tx_printqueue()

  This is basically for debuging purposes only.  It dumps all the
  records in the tx queue and their current status.  Very helpful
  if the queue isn't working quite right.

 */
#if debug > 2
int aim_tx_printqueue(void)
{
  struct command_tx_struct *workingPtr = NULL;

  workingPtr = aim_queue_outgoing;
#if debug > 2
  printf("\ncurrent aim_queue_outgoing...\n");
  printf("\ttype seqnum  len  lock sent\n");  
#endif
  if (workingPtr == NULL)
    printf("aim_tx_flushqueue(): queue empty");
  else
    {
      while (workingPtr != NULL)
	{
	  printf("\t  %2x   %4x %4x   %1d    %1d\n", workingPtr->type, workingPtr->seqnum, workingPtr->commandlen, workingPtr->lock, workingPtr->sent);
	  
	  workingPtr = workingPtr->next;
	}
    }

  printf("\n(done printing queue)\n");
  
  return 0;
}
#endif

/*
  aim_tx_flushqueue()

  This the function is responsable for putting the queued commands
  onto the wire.  This function is critical to the operation of 
  the queue and therefore is the most prone to brokenness.  It
  seems to be working quite well at this point.

  Procedure:
    1) Traverse the list, only operate on commands that are unlocked
       and haven't been sent yet.
    2) Lock the struct
    3) Allocate a temporary buffer to store the finished, fully
       processed packet in.
    4) Build the packet from the command_tx_struct data.
    5) Write the packet to the socket.
    6) If success, mark the packet sent, if fail report failure, do NOT
       mark the packet sent (so it will not get purged and therefore
       be attempted again on next call).
    7) Unlock the struct.
    8) Free the temp buffer
    9) Step to next struct in list and go back to 1.

 */
int aim_tx_flushqueue(void)
{
  struct command_tx_struct *workingPtr = NULL;
  unsigned char *curPacket = NULL;
#if debug > 1
  int i = 0;
#endif

  workingPtr = aim_queue_outgoing;
#if debug > 1
  printf("beginning txflush...\n");
#endif
  while (workingPtr != NULL)
    {
      /* only process if its unlocked and unsent */
      if ( (workingPtr->lock == 0) &&
	   (workingPtr->sent == 0) )
	{
	  workingPtr->lock = 1; /* lock the struct */
	  
	  /* allocate full-packet buffer */
	  curPacket = (char *) malloc(workingPtr->commandlen + 6);
	  
	  /* command byte */
	  curPacket[0] = 0x2a;
	  /* type/family byte */
	  curPacket[1] = workingPtr->type;
	  /* bytes 3+4: word: FLAP sequence number */
	  curPacket[2] = (char) ( (workingPtr->seqnum) >> 8);
	  curPacket[3] = (char) ( (workingPtr->seqnum) & 0xFF);
	  /* bytes 5+6: word: SNAC len */
	  curPacket[4] = (char) ( (workingPtr->commandlen) >> 8);
	  curPacket[5] = (char) ( (workingPtr->commandlen) & 0xFF);
	  /* bytes 7 and on: raw: SNAC data */
	  memcpy(&(curPacket[6]), workingPtr->data, workingPtr->commandlen);
	  
	  /* full image of raw packet data now in curPacket */

	  if ( write(workingPtr->conn->fd, curPacket, (workingPtr->commandlen + 6)) != (workingPtr->commandlen + 6))
	    {
	      perror("write");
	      printf("\nWARNING: Error in sending packet 0x%4x -- will try again next time\n\n", workingPtr->seqnum);
	      workingPtr->sent = 0; /* mark it unsent */
	      return -1;  /* bail out */
	    }
	  else
	    {
#if debug > 2
	      printf("\nSENT 0x%4x\n\n", workingPtr->seqnum);
#endif
	      workingPtr->sent = 1; /* mark the struct as sent */
	    }
#if debug > 2
	  printf("\nPacket:");
	  for (i = 0; i < (workingPtr->commandlen + 6); i++)
	    {
	      if ((i % 8) == 0)
		printf("\n\t");
	      if (curPacket[i] >= ' ' && curPacket[i]<127)
		 printf("%c=%02x ",curPacket[i], curPacket[i]);
	      else
		 printf("0x%2x ", curPacket[i]);
	    }
	  printf("\n");
#endif
	  workingPtr->lock = 0; /* unlock the struct */
	  free(curPacket); /* free up full-packet buffer */
	}
      workingPtr = workingPtr->next;
    }

  /* purge sent commands from queue */
  /*   this may not always occur explicitly--i may put this on a timer later */
#if debug > 1
  printf("calling aim_tx_purgequeue()\n");
#endif
  aim_tx_purgequeue();
#if debug > 1
  printf("back from aim_tx_purgequeu() [you must be a lucky one]\n");
#endif

  return 0;
}

/*
  aim_tx_purgequeue()
  
  This is responsable for removing sent commands from the transmit 
  queue. This is not a required operation, but it of course helps
  reduce memory footprint at run time!  

 */
int aim_tx_purgequeue(void)
{
  struct command_tx_struct *workingPtr = NULL;
  struct command_tx_struct *workingPtr2 = NULL;
#if debug > 1
  printf("purgequeue(): starting purge\n");
#endif
  /* Empty queue: nothing to do */
  if (aim_queue_outgoing == NULL)
    {
#if debug > 1
      printf("purgequeue(): purge done (len=0)\n");
#endif
      return 0;
    }
  /* One Node queue: free node and return */
  else if (aim_queue_outgoing->next == NULL)
    {
#if debug > 1
      printf("purgequeue(): entered case len=1\n");
#endif
      /* only free if sent AND unlocked -- dont assume sent structs are done */
      if ( (aim_queue_outgoing->lock == 0) &&
	   (aim_queue_outgoing->sent == 1) )
	{
#if debug > 1
	  printf("purgequeue(): purging seqnum 0x%04x\n", aim_queue_outgoing->seqnum);
#endif
	  workingPtr2 = aim_queue_outgoing;
	  aim_queue_outgoing = NULL;
	  free(workingPtr2->data);
	  free(workingPtr2);
	}
#if debug > 1
      printf("purgequeue(): purge done (len=1)\n");
#endif
      return 0;
    }
  else
    {
#if debug > 1
      printf("purgequeue(): entering case len>1\n");
#endif
      while(workingPtr->next != NULL)
	{
	  if ( (workingPtr->next->lock == 0) &&
	       (workingPtr->next->sent == 1) )
	    {
#if debug > 1
	      printf("purgequeue(): purging seqnum 0x%04x\n", workingPtr->next->seqnum);
#endif
	      workingPtr2 = workingPtr->next;
	      workingPtr->next = workingPtr2->next;
	      free(workingPtr2->data);
	      free(workingPtr2);
	    }
	}
#if debug > 1
      printf("purgequeue(): purge done (len>1)\n");
#endif
      return 0;
    }

  /* no reach */
}
