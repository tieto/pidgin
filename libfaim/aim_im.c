/*
 *  aim_im.c
 *
 *  The routines for sending/receiving Instant Messages.
 *
 */

#include "aim.h"

/*
 * Send an ICBM (instant message).  
 *
 *
 * Possible flags:
 *   AIM_IMFLAGS_AWAY  -- Marks the message as an autoresponse
 *   AIM_IMFLAGS_ACK   -- Requests that the server send an ack
 *                        when the message is received (of type 0x0004/0x000c)
 *
 *
 * TODO: Update to new standard form 
 *
 *
 */

u_long aim_send_im(struct aim_conn_t *conn, char *destsn, int flags, char *msg)
{   

  int curbyte;
  struct command_tx_struct newpacket;
  
  newpacket.lock = 1; /* lock struct */
  newpacket.type = 0x02; /* IMs are always family 0x02 */
  if (conn)
    newpacket.conn = conn;
  else
    newpacket.conn = aim_getconn_type(AIM_CONN_TYPE_BOS);

  newpacket.commandlen = 20+1+strlen(destsn)+1+1+2+7+2+4+strlen(msg)+2;

  if (flags & AIM_IMFLAGS_ACK)
    newpacket.commandlen += 4;
  if (flags & AIM_IMFLAGS_AWAY)
    newpacket.commandlen += 4;

  newpacket.data = (char *) calloc(1, newpacket.commandlen);

  curbyte  = 0;
  curbyte += aim_putsnac(newpacket.data+curbyte, 0x0004, 0x0006, 0x0000, aim_snac_nextid);
  curbyte += aimutil_put16(newpacket.data+curbyte,0x0000);
  curbyte += aimutil_put16(newpacket.data+curbyte,0x0000);
  curbyte += aimutil_put16(newpacket.data+curbyte,0x0000);
  curbyte += aimutil_put16(newpacket.data+curbyte,0x0000);
  curbyte += aimutil_put16(newpacket.data+curbyte,0x0001);
  curbyte += aimutil_put8(newpacket.data+curbyte,strlen(destsn));
  curbyte += aimutil_putstr(newpacket.data+curbyte, destsn, strlen(destsn));

  if (flags & AIM_IMFLAGS_ACK)
    {
      curbyte += aimutil_put16(newpacket.data+curbyte,0x0003);
      curbyte += aimutil_put16(newpacket.data+curbyte,0x0000);
    }

  if (flags & AIM_IMFLAGS_AWAY)
    {
      curbyte += aimutil_put16(newpacket.data+curbyte,0x0004);
      curbyte += aimutil_put16(newpacket.data+curbyte,0x0000);
    }

  curbyte += aimutil_put16(newpacket.data+curbyte,0x0002);
  curbyte += aimutil_put16(newpacket.data+curbyte,strlen(msg)+0xf);
  curbyte += aimutil_put16(newpacket.data+curbyte,0x0501);
  curbyte += aimutil_put16(newpacket.data+curbyte,0x0001);
  curbyte += aimutil_put16(newpacket.data+curbyte,0x0101);
  curbyte += aimutil_put16(newpacket.data+curbyte,0x0101);
  curbyte += aimutil_put8(newpacket.data+curbyte,0x01);
  curbyte += aimutil_put16(newpacket.data+curbyte,strlen(msg)+4);
  curbyte += aimutil_put16(newpacket.data+curbyte,0x0000);
  curbyte += aimutil_put16(newpacket.data+curbyte,0x0000);
  curbyte += aimutil_putstr(newpacket.data+curbyte, msg, strlen(msg));

  aim_tx_enqueue(&newpacket);

#ifdef USE_SNAC_FOR_IMS
 {
    struct aim_snac_t snac;

    snac.id = aim_snac_nextid;
    snac.family = 0x0004;
    snac.type = 0x0006;
    snac.flags = 0x0000;

    snac.data = malloc(strlen(destsn)+1);
    memcpy(snac.data, destsn, strlen(destsn)+1);

    aim_newsnac(&snac);
  }

 aim_cleansnacs(60); /* clean out all SNACs over 60sec old */
#endif

  return (aim_snac_nextid++);
}

int aim_parse_incoming_im_middle(struct command_rx_struct *command)
{
  int i = 0;
  char *srcsn = NULL;
  char *msg = NULL;
  unsigned int msglen = 0;
  int warninglevel = 0;
  int tlvcnt = 0;
  int class = 0;
  ulong membersince = 0;
  ulong onsince = 0;
  int idletime = 0;
  int isautoreply = 0;

  i = 20;
  
  srcsn = malloc(command->data[i] + 1);
  memcpy(srcsn, &(command->data[i+1]), command->data[i]);
  srcsn[(int)command->data[i]] = '\0';
  
  i += (int) command->data[i] + 1; /* add SN len */
  
  /* warning level */
  warninglevel = (command->data[i] << 8);
  warninglevel += (command->data[i+1]);
  i += 2;
  
  tlvcnt = ((command->data[i++]) << 8) & 0xFF00;
  tlvcnt += (command->data[i++]) & 0x00FF;
  
  /* a mini TLV parser */
  {
    int curtlv = 0;
    int tlv1 = 0;

    while (curtlv < tlvcnt)
      {
	if ((command->data[i] == 0x00) &&
	    (command->data[i+1] == 0x01) )
	  {
	    if (tlv1)
	      break;
	    /* t(0001) = class */
	    if (command->data[i+3] != 0x02)
	      printf("faim: userinfo: **warning: strange v(%x) for t(1)\n", command->data[i+3]);
	    class = ((command->data[i+4]) << 8) & 0xFF00;
	    class += (command->data[i+5]) & 0x00FF;
	    i += (2 + 2 + command->data[i+3]);
	    tlv1++;
	  }
	else if ((command->data[i] == 0x00) &&
		 (command->data[i+1] == 0x02))
	  {
	    /* t(0002) = member since date  */
	    if (command->data[i+3] != 0x04)
	      printf("faim: userinfo: **warning: strange v(%x) for t(2)\n", command->data[i+3]);

	    membersince = ((command->data[i+4]) << 24) &  0xFF000000;
	    membersince += ((command->data[i+5]) << 16) & 0x00FF0000;
	    membersince += ((command->data[i+6]) << 8) &  0x0000FF00;
	    membersince += ((command->data[i+7]) ) &      0x000000FF;
	    i += (2 + 2 + command->data[i+3]);
	  }
	else if ((command->data[i] == 0x00) &&
		 (command->data[i+1] == 0x03))
	  {
	    /* t(0003) = on since date  */
	    if (command->data[i+3] != 0x04)
	      printf("faim: userinfo: **warning: strange v(%x) for t(3)\n", command->data[i+3]);

	    onsince = ((command->data[i+4]) << 24) &  0xFF000000;
	    onsince += ((command->data[i+5]) << 16) & 0x00FF0000;
	    onsince += ((command->data[i+6]) << 8) &  0x0000FF00;
	    onsince += ((command->data[i+7]) ) &      0x000000FF;
	    i += (2 + 2 + command->data[i+3]);
	  }
	else if ((command->data[i] == 0x00) &&
		 (command->data[i+1] == 0x04) )
	  {
	    /* t(0004) = idle time */
	    if (command->data[i+3] != 0x02)
	      printf("faim: userinfo: **warning: strange v(%x) for t(4)\n", command->data[i+3]);
	    idletime = ((command->data[i+4]) << 8) & 0xFF00;
	    idletime += (command->data[i+5]) & 0x00FF;
	    i += (2 + 2 + command->data[i+3]);
	  }  
	else
	  {
	    printf("faim: userinfo: **warning: unexpected TLV t(%02x%02x) l(%02x%02x)\n", command->data[i], command->data[i+1], command->data[i+2], command->data[i+3]);
	    i += (2 + 2 + command->data[i+3]);
	  }
	curtlv++;
      }
  }

  {
    /* detect if this is an auto-response or not */
    /*   auto-responses can be detected by the presence of a *second* TLV with
	 t(0004), but of zero length (and therefore no value portion) */
    struct aim_tlv_t *tsttlv = NULL;
    tsttlv = aim_grabtlv((u_char *) &(command->data[i]));
    if (tsttlv->type == 0x04)
      isautoreply = 1;
    aim_freetlv(&tsttlv);
  }
  
  i += 2;
  
  i += 2; /* skip first msglen */
  i += 7; /* skip garbage */
  i -= 4;

  /* oh boy is this terrible...  this comes from a specific of the spec */
  while(1)
    {
      if ( ( (command->data[i] == 0x00) &&
	     (command->data[i+1] == 0x00) &&
	     (command->data[i+2] == 0x00) &&
	     (command->data[i+3] == 0x00) ) &&
	   (i < command->commandlen) ) /* prevent infinity */
	break;
      else
	i++;
    }

  i -= 2;
  
  if ( (command->data[i] == 0x00) &&
       (command->data[i+1] == 0x00) )
    i += 2;

  msglen = ( (( (unsigned int) command->data[i]) & 0xFF ) << 8);
  msglen += ( (unsigned int) command->data[i+1]) & 0xFF; /* mask off garbage */
  i += 2;

  msglen -= 4; /* skip four 0x00s */
  i += 4;
  
  msg = malloc(msglen +1);
  
  memcpy(msg, &(command->data[i]), msglen);
  msg[msglen] = '\0'; 

  i = (aim_callbacks[AIM_CB_INCOMING_IM])(command, srcsn, msg, warninglevel, class, membersince, onsince, idletime, isautoreply);

  free(srcsn);
  free(msg);

  return i;
}
