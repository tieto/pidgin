
/*
  aim_rxhandlers.c

  This file contains most all of the incoming packet handlers, along
  with aim_rxdispatch(), the Rx dispatcher.  Queue/list management is
  actually done in aim_rxqueue.c.
  
 */


#include "aim.h" /* for most everything */

int bleck(struct command_rx_struct *param, ...)
{
  return 1;
}

/*
 * The callbacks.  Used to pass data up from the wire into the client.
 *
 * TODO: MASSIVE OVERHAUL.  This method of doing it (array of function 
 *       pointers) is ugly.  Overhaul may mean including chained callbacks
 *       for having client features such as run-time loadable modules.
 *
 */
rxcallback_t aim_callbacks[] = {
  bleck, /* incoming IM */
  bleck, /* oncoming buddy */
  bleck, /* offgoing buddy */
  bleck, /* messaging error */
  bleck, /* server missed call */
  bleck, /* login phase 4 packet C command 1*/
  bleck, /* login phase 4 packet C command 2 */
  bleck, /* login phase 2, first resp */
  bleck, /* login phase 2, second resp -- **REQUIRED** */
  bleck, /* login phase 3 packet B */
  bleck, /* login phase 3D packet A */
  bleck, /* login phase 3D packet B */
  bleck, /* login phase 3D packet C */
  bleck, /* login phase 3D packet D */
  bleck, /* login phase 3D packet E */
  bleck, /* redirect -- **REQUIRED** */
  bleck, /* server rate change */
  bleck, /* user location error */
  aim_parse_unknown, /* completely unknown command */
  bleck, /* User Info Response */
  bleck, /* User Search by Address response */
  bleck, /* User Search by Name response */
  bleck, /* user search fail */
  bleck, /* auth error */
  bleck, /* auth success */
  bleck, /* auth server ready */
  bleck, /* auth other */
  bleck, /* info change reply */
  bleck, /* ChatNAV: server ready */
  0x00
};


int aim_register_callbacks(rxcallback_t *newcallbacks)
{
  int i = 0;
  
  for (i = 0; aim_callbacks[i] != 0x00; i++)
    {
      if ( (newcallbacks[i] != NULL) &&
	   (newcallbacks[i] != 0x00) )
	{
#if debug > 3
	  printf("aim_register_callbacks: changed handler %d\n", i);
#endif
	  aim_callbacks[i] = newcallbacks[i];
	}
    }
  
  return 0;
}

/*
  aim_rxdispatch()

  Basically, heres what this should do:
    1) Determine correct packet handler for this packet
    2) Mark the packet handled (so it can be dequeued in purge_queue())
    3) Send the packet to the packet handler
    4) Go to next packet in the queue and start over
    5) When done, run purge_queue() to purge handled commands

  Note that any unhandlable packets should probably be left in the
  queue.  This is the best way to prevent data loss.  This means
  that a single packet may get looked at by this function multiple
  times.  This is more good than bad!  This behavior may change.

  Aren't queue's fun? 

  TODO: Get rid of all the ugly if's.
  TODO: Clean up.
  TODO: More support for mid-level handlers.
  TODO: Allow for NULL handlers.
  
 */
int aim_rxdispatch(void)
{
  int i = 0;
  struct command_rx_struct *workingPtr = NULL;
  
  if (aim_queue_incoming == NULL)
    /* this shouldn't really happen, unless the main loop's select is broke  */
    printf("parse_generic: incoming packet queue empty.\n");
  else
    {
      workingPtr = aim_queue_incoming;
      for (i = 0; workingPtr != NULL; i++)
	{
	  switch(workingPtr->conn->type)
	    {
	    case AIM_CONN_TYPE_AUTH:
	      if ( (workingPtr->data[0] == 0x00) && 
		   (workingPtr->data[1] == 0x00) &&
		   (workingPtr->data[2] == 0x00) &&
		   (workingPtr->data[3] == 0x01) )
		{
#if debug > 0
		  fprintf(stderr, "got connection ack on auth line\n");
#endif
		  workingPtr->handled = 1;
		}
	      else
		{
		  /* any user callbacks will be called from here */
		  workingPtr->handled = aim_authparse(workingPtr);
		}
	      break;
	    case AIM_CONN_TYPE_BOS:
	      {
		u_short family;
		u_short subtype;
		family = (workingPtr->data[0] << 8) + workingPtr->data[1];
		subtype = (workingPtr->data[2] << 8) + workingPtr->data[3];
		switch (family)
		  {
		  case 0x0000: /* not really a family, but it works */
		    if (subtype == 0x0001)
		      workingPtr->handled = (aim_callbacks[AIM_CB_LOGIN_P2_1])(workingPtr);
		    else
		      workingPtr->handled = (aim_callbacks[AIM_CB_UNKNOWN])(workingPtr);
		    break;
		  case 0x0001: /* Family: General */
		    switch (subtype)
		      {
		      case 0x0001:
			workingPtr->handled = aim_parse_generalerrs(workingPtr);
			break;
		      case 0x0003:
			workingPtr->handled = (aim_callbacks[AIM_CB_LOGIN_P2_2])(workingPtr);
			break;
		      case 0x0005:
			workingPtr->handled = aim_handleredirect_middle(workingPtr);
			break;
		      case 0x0007:
			workingPtr->handled = (aim_callbacks[AIM_CB_LOGIN_P3_B])(workingPtr);
			break;
		      case 0x000a:
			workingPtr->handled = (aim_callbacks[AIM_CB_RATECHANGE])(workingPtr);
			break;
		      case 0x000f:
			workingPtr->handled = (aim_callbacks[AIM_CB_LOGIN_P3D_A])(workingPtr);
			break;
		      case 0x0013:
			workingPtr->handled = (aim_callbacks[AIM_CB_LOGIN_P4_C2])(workingPtr);
			break;
		      default:
			workingPtr->handled = (aim_callbacks[AIM_CB_UNKNOWN])(workingPtr);
		      }
		    break;
		  case 0x0002: /* Family: Location */
		    switch (subtype)
		      {
		      case 0x0001:
			workingPtr->handled = (aim_callbacks[AIM_CB_MISSED_IM])(workingPtr);
			break;
		      case 0x0003:
			workingPtr->handled = (aim_callbacks[AIM_CB_LOGIN_P3D_D])(workingPtr);
			break;
		      case 0x0006:
			workingPtr->handled = aim_parse_userinfo_middle(workingPtr);
			break;
		      default:
			workingPtr->handled = (aim_callbacks[AIM_CB_UNKNOWN])(workingPtr);
		      }
		    break;
		  case 0x0003: /* Family: Buddy List */
		    switch (subtype)
		      {
		      case 0x0001:
			workingPtr->handled = aim_parse_generalerrs(workingPtr);
			break;
		      case 0x0003:
			workingPtr->handled = (aim_callbacks[AIM_CB_LOGIN_P3D_C])(workingPtr);
			break;
		      case 0x000b:
			workingPtr->handled = (aim_callbacks[AIM_CB_ONCOMING_BUDDY])(workingPtr);
			break;
		      case 0x000c:
			workingPtr->handled = (aim_callbacks[AIM_CB_OFFGOING_BUDDY])(workingPtr);
			break;
		      default:
			workingPtr->handled = (aim_callbacks[AIM_CB_UNKNOWN])(workingPtr);
		      }
		    break;
		  case 0x0004: /* Family: Messeging */
		    switch (subtype)
		      {
		      case 0x0001:
			workingPtr->handled = (aim_callbacks[AIM_CB_USERERROR])(workingPtr);
			break;
		      case 0x0005:
			workingPtr->handled = (aim_callbacks[AIM_CB_LOGIN_P3D_E])(workingPtr);
			break;
		      case 0x0007:
			workingPtr->handled = aim_parse_incoming_im_middle(workingPtr);
			break;
		      case 0x000a:
			workingPtr->handled = (aim_callbacks[AIM_CB_MISSED_CALL])(workingPtr);
			break;
		      default:
			workingPtr->handled = (aim_callbacks[AIM_CB_UNKNOWN])(workingPtr);
		      }
		    break;
		  case 0x0009:
		    if (subtype == 0x0001)
		      workingPtr->handled = aim_parse_generalerrs(workingPtr);
		    else if (subtype == 0x0003)
		      workingPtr->handled = (aim_callbacks[AIM_CB_LOGIN_P3D_B])(workingPtr);
		    else
		      workingPtr->handled = (aim_callbacks[AIM_CB_UNKNOWN])(workingPtr);
		    break;
		  case 0x000a:  /* Family: User lookup */
		    switch (subtype)
		      {
		      case 0x0001:
			workingPtr->handled = (aim_callbacks[AIM_CB_SEARCH_FAIL])(workingPtr);
			break;
		      case 0x0003:
			workingPtr->handled = (aim_callbacks[AIM_CB_SEARCH_ADDRESS])(workingPtr);
			break;
		      default:
			workingPtr->handled = (aim_callbacks[AIM_CB_UNKNOWN])(workingPtr);
		      }
		    break;
		  case 0x000b:
		    if (subtype == 0x0001)
		      workingPtr->handled = aim_parse_generalerrs(workingPtr);
		    else if (subtype == 0x0002)
		      workingPtr->handled = (aim_callbacks[AIM_CB_LOGIN_P4_C1])(workingPtr);
		    else
		      workingPtr->handled = (aim_callbacks[AIM_CB_UNKNOWN])(workingPtr);
		    break;
		  default:
		    workingPtr->handled = (aim_callbacks[AIM_CB_UNKNOWN])(workingPtr);
		    break;
		  }
	      }
	      break;
	    case AIM_CONN_TYPE_CHATNAV:
	      if ( (workingPtr->data[0] == 0x00) &&
		   (workingPtr->data[1] == 0x02) &&
		   (workingPtr->data[2] == 0x00) &&
		   (workingPtr->data[3] == 0x06) )
		{
		  workingPtr->handled = 1;
		  aim_conn_setstatus(workingPtr->conn, AIM_CONN_STATUS_READY);
		}
	      else
		workingPtr->handled = (aim_callbacks[AIM_CB_UNKNOWN])(workingPtr);
	      break;
	    case AIM_CONN_TYPE_CHAT:
	      fprintf(stderr, "\nAHH! Dont know what to do with CHAT stuff yet!\n");
	      workingPtr->handled = (aim_callbacks[AIM_CB_UNKNOWN])(workingPtr);
	      break;
	    default:
	      fprintf(stderr, "\nAHHHHH! UNKNOWN CONNECTION TYPE!\n\n");
	      workingPtr->handled = (aim_callbacks[AIM_CB_UNKNOWN])(workingPtr);
	      break;
	    }
	      /* move to next command */
	  workingPtr = workingPtr->next;
	}
    }

  aim_queue_incoming = aim_purge_rxqueue(aim_queue_incoming);
  
  return 0;
}

/*
 * TODO: check and cure memory leakage in this function.
 */
int aim_authparse(struct command_rx_struct *command)
{
  int iserror = 0;
  struct aim_tlv_t *tlv = NULL;
  char *errorurl = NULL;
  short errorcode;
  int z = 0;

  if ( (command->data[0] == 0x00) &&
       (command->data[1] == 0x01) &&
       (command->data[2] == 0x00) &&
       (command->data[3] == 0x03) )
    {
      /* "server ready"  -- can be ignored */
      return (aim_callbacks[AIM_CB_AUTH_SVRREADY])(command);
    }
  else if ( (command->data[0] == 0x00) &&
	    (command->data[1] == 0x07) &&
	    (command->data[2] == 0x00) &&
	    (command->data[3] == 0x05) )
    {
      /* "information change reply" */
      return (aim_callbacks[AIM_CB_AUTH_INFOCHNG_REPLY])(command);
    }
  else
    {
      /* anything else -- usually used for login; just parse as pure TLVs */


      /* all this block does is figure out if it's an
	 error or a success, nothing more */
      while (z < command->commandlen)
	{
	  tlv = aim_grabtlvstr(&(command->data[z]));
	  switch(tlv->type) 
	    {
	    case 0x0001: /* screen name */
	      aim_logininfo.screen_name = tlv->value;
	      z += 2 + 2 + tlv->length;
	      free(tlv);
	      tlv = NULL;
	      break;
	    case 0x0004: /* error URL */
	      errorurl = tlv->value;
	      z += 2 + 2 + tlv->length;
	      free(tlv);
	      tlv = NULL;
	      break;
	    case 0x0005: /* BOS IP */
	      aim_logininfo.BOSIP = tlv->value;
	      z += 2 + 2 + tlv->length;
	      free(tlv);
	      tlv = NULL;
	      break;
	    case 0x0006: /* auth cookie */
	      aim_logininfo.cookie = tlv->value;
	      z += 2 + 2 + tlv->length;
	      free(tlv);
	      tlv=NULL;
	      break;
	    case 0x0011: /* email addy */
	      aim_logininfo.email = tlv->value;
	      z += 2 + 2 + tlv->length;
	      free(tlv);
	      tlv = NULL;
	      break;
	    case 0x0013: /* registration status */
	      aim_logininfo.regstatus = *(tlv->value);
	      z += 2 + 2 + tlv->length;
	      aim_freetlv(&tlv);
	      break;
	    case 0x0008: /* error code */
	      errorcode = *(tlv->value);
	      z += 2 + 2 + tlv->length;
	      aim_freetlv(&tlv);
	      iserror = 1;
	      break;
	    default:
	  z += 2 + 2 + tlv->length;
	  aim_freetlv(&tlv);
	  /* dunno */
	    }
	}

      if (iserror && 
	  errorurl && 
	  errorcode)
	return (aim_callbacks[AIM_CB_AUTH_ERROR])(command, &aim_logininfo, errorurl, errorcode);
      else if (aim_logininfo.screen_name && 
	       aim_logininfo.cookie && aim_logininfo.BOSIP)
	return (aim_callbacks[AIM_CB_AUTH_SUCCESS])(command, &aim_logininfo);
      else
	return (aim_callbacks[AIM_CB_AUTH_OTHER])(command);
    }
}

/*
 * TODO: check for and cure any memory leaks here.
 */
int aim_handleredirect_middle(struct command_rx_struct *command, ...)
{
  struct aim_tlv_t *tlv = NULL;
  int z = 10;
  int serviceid;
  char *cookie;
  char *ip;

  while (z < command->commandlen)
    {
      tlv = aim_grabtlvstr(&(command->data[z]));
      switch(tlv->type)
	{
	case 0x000d:  /* service id */
	  aim_freetlv(&tlv);
	  /* regrab as an int */
	  tlv = aim_grabtlv(&(command->data[z]));
	  serviceid = (tlv->value[0] << 8) + tlv->value[1]; /* hehe */
	  z += 2 + 2 + tlv->length;
	  aim_freetlv(&tlv);
	  break;
	case 0x0005:  /* service server IP */
	  ip = tlv->value;
	  z += 2 + 2 + tlv->length;
	  free(tlv);
	  tlv = NULL;
	  break;
	case 0x0006: /* auth cookie */
	  cookie = tlv->value;
	  z += 2 + 2 + tlv->length;
	  free(tlv);
	  tlv = NULL;
	  break;
	default:
	  /* dunno */
	  z += 2 + 2 + tlv->length;
	  aim_freetlv(&tlv);
	}
    }
  return (aim_callbacks[AIM_CB_LOGIN_P3D_F])(command, serviceid, ip, cookie);
}

int aim_parse_unknown(struct command_rx_struct *command, ...)
{
  int i = 0;

  printf("\nRecieved unknown packet:");

  for (i = 0; i < command->commandlen; i++)
    {
      if ((i % 8) == 0)
	printf("\n\t");

      printf("0x%2x ", command->data[i]);
    }
  
  printf("\n\n");

  return 1;
}


/*
 * aim_parse_generalerrs()
 *
 * Middle handler for 0x0001 snac of each family.
 *
 */
int aim_parse_generalerrs(struct command_rx_struct *command, ...)
{
  u_short family;
  u_short subtype;
  family = (command->data[0] << 8) + command->data[1];
  subtype = (command->data[2] << 8) + command->data[3];
  
  switch(family)
    {
    default:
      /* Unknown family */
      return (aim_callbacks[AIM_CB_UNKNOWN])(command);
    }

  return 1;
}



