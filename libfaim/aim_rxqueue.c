/*
  aim_rxqueue.c

  This file contains the management routines for the receive
  (incoming packet) queue.  The actual packet handlers are in
  aim_rxhandlers.c.

 */

#include "aim.h"

/*
  This is a modified read() to make SURE we get the number
  of bytes we are told to, otherwise block.
 */
int Read(int fd, u_char *buf, int len)
{
  int i = 0;
  int j = 0;

  while ((i < len) && (!(i < 0)))
    {
      j = read(fd, &(buf[i]), len-i);
      if ( (j < 0) && (errno != EAGAIN))
	return -errno; /* fail */
      else
	i += j; /* success, continue */
    }
#if 0
  printf("\nRead Block: (%d/%04x)\n", len, len);
  printf("\t");
  for (j = 0; j < len; j++)
    {
      if (j % 8 == 0)
	printf("\n\t");
      if (buf[j] >= ' ' && buf[j] < 127)
	 printf("%c=%02x ",buf[j], buf[j]);
      else
	 printf("0x%02x ", buf[j]);
    }
  printf("\n\n");
#endif
  return i;
}

/*
  struct command_struct *
                         get_generic(
                                     struct connection_info struct *,
				     struct command_struct * 
				     )
  
  Grab as many command sequences as we can off the socket, and enqueue
  each command in the incoming event queue in a seperate struct.

*/
int aim_get_command(void)
{
  int i, readgood, j, isav, err;
  int s;
  fd_set fds;
  struct timeval tv;
  char generic[6]; 
  struct command_rx_struct *workingStruct = NULL;
  struct command_rx_struct *workingPtr = NULL;
  struct aim_conn_t *conn = NULL;
#if debug > 0
  printf("Reading generic/unknown response...");
#endif
  
  
  /* dont wait at all (ie, never call this unless something is there) */
  tv.tv_sec = 0; 
  tv.tv_usec = 0;
  conn = aim_select(&tv);

  if (conn==NULL)
    return 0;  /* nothing waiting */

  s = conn->fd;

  FD_ZERO(&fds);
  FD_SET(s, &fds);
  tv.tv_sec = 0;  /* wait, but only for 10us */
  tv.tv_usec = 10;
  
  generic[0] = 0x00;  

  readgood = 0;
  i = 0;
  j = 0;
  /* read first 6 bytes (the FLAP header only) off the socket */
  while ( (select(s+1, &fds, NULL, NULL, &tv) == 1) && (i < 6))
    {
      if ((err = Read(s, &(generic[i]), 1)) < 0)
	{
	  /* error is probably not recoverable...(must be a pessimistic day) */
	  aim_conn_close(conn);
	  return err;
   	}

      if (readgood == 0)
	{
	  if (generic[i] == 0x2a)
	  {
	    readgood = 1;
#if debug > 1
	    printf("%x ", generic[i]);
	    fflush(stdout);
#endif
	    i++;
	  }
	  else
	    {
#if debug > 1
	      printf("skipping 0x%d ", generic[i]);
	      fflush(stdout);
#endif
	      j++;
	    }
	}
      else
	{
#if debug > 1
	  printf("%x ", generic[i]);
#endif
	  i++;
	}
      FD_ZERO(&fds);
      FD_SET(s, &fds);
      tv.tv_sec= 2;
      tv.tv_usec= 2;
    }

  if (generic[0] != 0x2a)
    {
      /* this really shouldn't happen, since the main loop
	 select() should protect us from entering this function
	 without data waiting  */
      printf("Bad incoming data!");
      return -1;
    }

  isav = i;

  /* allocate a new struct */
  workingStruct = (struct command_rx_struct *) malloc(sizeof(struct command_rx_struct));
  workingStruct->lock = 1;  /* lock the struct */

  /* store type -- byte 2 */
  workingStruct->type = (char) generic[1];

  /* store seqnum -- bytes 3 and 4 */
  workingStruct->seqnum = ( (( (unsigned int) generic[2]) & 0xFF) << 8);
  workingStruct->seqnum += ( (unsigned int) generic[3]) & 0xFF;

  /* store commandlen -- bytes 5 and 6 */
  workingStruct->commandlen = ( (( (unsigned int) generic[4]) & 0xFF ) << 8);
  workingStruct->commandlen += ( (unsigned int) generic[5]) & 0xFF;
  
  printf("%d\n", workingStruct->commandlen);

  /* malloc for data portion */
  workingStruct->data = (char *) malloc(workingStruct->commandlen);

  /* read the data portion of the packet */
  i = Read(s, workingStruct->data, workingStruct->commandlen);
  if (i < 0)
    {
      aim_conn_close(conn);
      return i;
    }

#if debug > 0
  printf(" done. (%db+%db read, %db skipped)\n", isav, i, j);
#endif

  workingStruct->conn = conn;

  workingStruct->next = NULL;  /* this will always be at the bottom */
  workingStruct->lock = 0; /* unlock */

  /* enqueue this packet */
  if (aim_queue_incoming == NULL)
    aim_queue_incoming = workingStruct;
  else
    {
      workingPtr = aim_queue_incoming;
      while (workingPtr->next != NULL)
	workingPtr = workingPtr->next;
      workingPtr->next = workingStruct;
    }

  return 0;  
}

/*
  purge_rxqueue()

  This is just what it sounds.  It purges the receive (rx) queue of
  all handled commands.  This is normally called from inside 
  aim_rxdispatch() after it's processed all the commands in the queue.
  
 */
struct command_rx_struct *aim_purge_rxqueue(struct command_rx_struct *queue)
{
  int i = 0;
  struct command_rx_struct *workingPtr = NULL;
  struct command_rx_struct *workingPtr2 = NULL;

  workingPtr = queue;
  if (queue == NULL)
    {
      return queue;
    }
  else if (queue->next == NULL)
    {
      if (queue->handled == 1)
	{
	  workingPtr2 = queue;
	  queue = NULL;
	  free(workingPtr2->data);
	  free(workingPtr2);
	}
      return queue;
    }
  else
    {
      for (i = 0; workingPtr != NULL; i++)
	{
	  if (workingPtr->next->handled == 1)
	    {
	      /* save struct */
	      workingPtr2 = workingPtr->next;
	      /* dequeue */
	      workingPtr->next = workingPtr2->next;
	      /* free */
	      free(workingPtr2->data);
	      free(workingPtr2);
	    }

	  workingPtr = workingPtr->next;  
	}
    }

  return queue;
}
