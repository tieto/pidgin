/*
 * aim_rxhandlers.c
 *
 * This file contains most all of the incoming packet handlers, along
 * with aim_rxdispatch(), the Rx dispatcher.  Queue/list management is
 * actually done in aim_rxqueue.c.
 *
 */

#define FAIM_INTERNAL
#include <aim.h>

/*
 * Bleck functions get called when there's no non-bleck functions
 * around to cleanup the mess...
 */
faim_internal int bleck(struct aim_session_t *sess,struct command_rx_struct *workingPtr, ...)
{
  u_short family;
  u_short subtype;

  u_short maxf;
  u_short maxs;

  /* XXX: this is ugly. and big just for debugging. */
  char *literals[14][25] = {
    {"Invalid", 
     NULL
    },
    {"General", 
     "Invalid",
     "Error",
     "Client Ready",
     "Server Ready",
     "Service Request",
     "Redirect",
     "Rate Information Request",
     "Rate Information",
     "Rate Information Ack",
     NULL,
     "Rate Information Change",
     "Server Pause",
     NULL,
     "Server Resume",
     "Request Personal User Information",
     "Personal User Information",
     "Evil Notification",
     NULL,
     "Migration notice",
     "Message of the Day",
     "Set Privacy Flags",
     "Well Known URL",
     "NOP"
    },
    {"Location", 
      "Invalid",
      "Error",
      "Request Rights",
      "Rights Information", 
      "Set user information", 
      "Request User Information", 
      "User Information", 
      "Watcher Sub Request",
      "Watcher Notification"
    },
    {"Buddy List Management", 
      "Invalid", 
      "Error", 
      "Request Rights",
      "Rights Information",
      "Add Buddy", 
      "Remove Buddy", 
      "Watcher List Query", 
      "Watcher List Response", 
      "Watcher SubRequest", 
      "Watcher Notification", 
      "Reject Notification", 
      "Oncoming Buddy", 
      "Offgoing Buddy"
    },
    {"Messeging", 
      "Invalid",
      "Error", 
      "Add ICBM Parameter",
      "Remove ICBM Parameter", 
      "Request Parameter Information",
      "Parameter Information",
      "Outgoing Message", 
      "Incoming Message",
      "Evil Request",
      "Evil Reply", 
      "Missed Calls",
      "Message Error", 
      "Host Ack"
    },
    {"Advertisements", 
      "Invalid", 
      "Error", 
      "Request Ad",
      "Ad Data (GIFs)"
    },
    {"Invitation / Client-to-Client", 
     "Invalid",
     "Error",
     "Invite a Friend",
     "Invitation Ack"
    },
    {"Administrative", 
      "Invalid",
      "Error",
      "Information Request",
      "Information Reply",
      "Information Change Request",
      "Information Chat Reply",
      "Account Confirm Request",
      "Account Confirm Reply",
      "Account Delete Request",
      "Account Delete Reply"
    },
    {"Popups", 
      "Invalid",
      "Error",
      "Display Popup"
    },
    {"BOS", 
      "Invalid",
      "Error",
      "Request Rights",
      "Rights Response",
      "Set group permission mask",
      "Add permission list entries",
      "Delete permission list entries",
      "Add deny list entries",
      "Delete deny list entries",
      "Server Error"
    },
    {"User Lookup", 
      "Invalid",
      "Error",
      "Search Request",
      "Search Response"
    },
    {"Stats", 
      "Invalid",
      "Error",
      "Set minimum report interval",
      "Report Events"
    },
    {"Translate", 
      "Invalid",
      "Error",
      "Translate Request",
      "Translate Reply",
    },
    {"Chat Navigation", 
      "Invalid",
      "Error",
      "Request rights",
      "Request Exchange Information",
      "Request Room Information",
      "Request Occupant List",
      "Search for Room",
      "Outgoing Message", 
      "Incoming Message",
      "Evil Request", 
      "Evil Reply", 
      "Chat Error",
    }
  };

  maxf = sizeof(literals) / sizeof(literals[0]);
  maxs = sizeof(literals[0]) / sizeof(literals[0][0]);

  family = aimutil_get16(workingPtr->data+0);
  subtype= aimutil_get16(workingPtr->data+2);

  if((family < maxf) && (subtype+1 < maxs) && (literals[family][subtype] != NULL))
    faimdprintf(sess, 0, "bleck: null handler for %04x/%04x (%s)\n", family, subtype, literals[family][subtype+1]);
  else
    faimdprintf(sess, 0, "bleck: null handler for %04x/%04x (no literal)\n",family,subtype);

  return 1;
}

faim_export int aim_conn_addhandler(struct aim_session_t *sess,
			struct aim_conn_t *conn,
			u_short family,
			u_short type,
			rxcallback_t newhandler,
			u_short flags)
{
  struct aim_rxcblist_t *newcb;

  if (!conn)
    return -1;

  faimdprintf(sess, 1, "aim_conn_addhandler: adding for %04x/%04x\n", family, type);

  if (!(newcb = (struct aim_rxcblist_t *)calloc(1, sizeof(struct aim_rxcblist_t))))
    return -1;
  newcb->family = family;
  newcb->type = type;
  newcb->flags = flags;
  if (!newhandler)
    newcb->handler = &bleck;
  else
    newcb->handler = newhandler;
  newcb->next = NULL;
  
  if (!conn->handlerlist)
    conn->handlerlist = newcb;
  else {
    struct aim_rxcblist_t *cur;

    cur = conn->handlerlist;

    while (cur->next)
      cur = cur->next;
    cur->next = newcb;
  }

  return 0;
}

faim_export int aim_clearhandlers(struct aim_conn_t *conn)
{
 struct aim_rxcblist_t *cur;

 if (!conn)
   return -1;

 for (cur = conn->handlerlist; cur; ) {
   struct aim_rxcblist_t *tmp;

   tmp = cur->next;
   free(cur);
   cur = tmp;
 }
 conn->handlerlist = NULL;

 return 0;
}

faim_internal rxcallback_t aim_callhandler(struct aim_session_t *sess,
					   struct aim_conn_t *conn,
					   unsigned short family,
					   unsigned short type)
{
  struct aim_rxcblist_t *cur;

  if (!conn)
    return NULL;

  faimdprintf(sess, 1, "aim_callhandler: calling for %04x/%04x\n", family, type);
  
  for (cur = conn->handlerlist; cur; cur = cur->next) {
    if ((cur->family == family) && (cur->type == type))
      return cur->handler;
  }

  if (type == AIM_CB_SPECIAL_DEFAULT) {
    faimdprintf(sess, 1, "aim_callhandler: no default handler for family 0x%04x\n", family);
    return NULL; /* prevent infinite recursion */
  }

  faimdprintf(sess, 1, "aim_callhandler: no handler for  0x%04x/0x%04x\n", family, type);

  return aim_callhandler(sess, conn, family, AIM_CB_SPECIAL_DEFAULT);
}

faim_internal int aim_callhandler_noparam(struct aim_session_t *sess,
					  struct aim_conn_t *conn,
					  u_short family,
					  u_short type,
					  struct command_rx_struct *ptr)
{
  rxcallback_t userfunc = NULL;
  userfunc = aim_callhandler(sess, conn, family, type);
  if (userfunc)
    return userfunc(sess, ptr);
  return 1; /* XXX */
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
faim_export int aim_rxdispatch(struct aim_session_t *sess)
{
  int i = 0;
  struct command_rx_struct *workingPtr = NULL;
  
  if (sess->queue_incoming == NULL) {
    faimdprintf(sess, 1, "parse_generic: incoming packet queue empty.\n");
    return 0;
  } else {
    workingPtr = sess->queue_incoming;
    for (i = 0; workingPtr != NULL; workingPtr = workingPtr->next, i++) {
      unsigned short family,subtype;

      /*
       * XXX: This is still fairly ugly.
       */
      if (workingPtr->handled)
	continue;

      /*
       * This is a debugging/sanity check only and probably could/should be removed
       * for stable code.
       */
      if (((workingPtr->hdrtype == AIM_FRAMETYPE_OFT) && 
	   (workingPtr->conn->type != AIM_CONN_TYPE_RENDEZVOUS)) || 
	  ((workingPtr->hdrtype == AIM_FRAMETYPE_OSCAR) && 
	   (workingPtr->conn->type == AIM_CONN_TYPE_RENDEZVOUS))) {
	faimdprintf(sess, 0, "rxhandlers: incompatible frame type %d on connection type 0x%04x\n", workingPtr->hdrtype, workingPtr->conn->type);
	workingPtr->handled = 1;
	continue;
      }

      if (workingPtr->conn->type == AIM_CONN_TYPE_RENDEZVOUS) {
	/* make sure that we only get OFT frames on these connections */
	if (workingPtr->hdrtype != AIM_FRAMETYPE_OFT) {
	  faimdprintf(sess, 0, "internal error: non-OFT frames on OFT connection\n");
	  workingPtr->handled = 1; /* get rid of it */
	} else {
	  /* XXX: implement this */
	  faimdprintf(sess, 0, "faim: OFT frame!\n");
	  workingPtr->handled = 1; /* get rid of it */
	}
	continue;
      }

      if (workingPtr->conn->type == AIM_CONN_TYPE_RENDEZVOUS_OUT) {
	/* not possible */
	faimdprintf(sess, 0, "rxdispatch called on RENDEZVOUS_OUT connection!\n");
	workingPtr->handled = 1;
	continue;
      }

      if ((workingPtr->commandlen == 4) && 
	  (aimutil_get32(workingPtr->data) == 0x00000001)) {
	workingPtr->handled = aim_callhandler_noparam(sess, workingPtr->conn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_FLAPVER, workingPtr);
	continue;
      } 

      if (workingPtr->hdr.oscar.type == 0x04) {
	workingPtr->handled = aim_negchan_middle(sess, workingPtr);
	continue;
      }
	  
      family = aimutil_get16(workingPtr->data);
      subtype = aimutil_get16(workingPtr->data+2);
	  
      if (family == 0x0001) {

	if (subtype == 0x0001)
	  workingPtr->handled = aim_parse_generalerrs(sess, workingPtr);
	else if (subtype == 0x0003)
	  workingPtr->handled = aim_parse_hostonline(sess, workingPtr);
	else if (subtype == 0x0005)
	  workingPtr->handled = aim_handleredirect_middle(sess, workingPtr);
	else if (subtype == 0x0007)
	  workingPtr->handled = aim_callhandler_noparam(sess, workingPtr->conn, 0x0001, 0x0007, workingPtr);
	else if (subtype == 0x000a)
	  workingPtr->handled = aim_parse_ratechange_middle(sess, workingPtr);
	else if (subtype == 0x000f)
	  workingPtr->handled = aim_callhandler_noparam(sess, workingPtr->conn, 0x0001, 0x000f, workingPtr);
	else if (subtype == 0x0010)
	  workingPtr->handled = aim_parse_evilnotify_middle(sess, workingPtr);
	else if (subtype == 0x0013)
	  workingPtr->handled = aim_parsemotd_middle(sess, workingPtr);
	else if (subtype == 0x0018)
	  workingPtr->handled = aim_parse_hostversions(sess, workingPtr);
	else
	  workingPtr->handled = aim_callhandler_noparam(sess, workingPtr->conn, 0x0001, 0xffff, workingPtr);

      } else if (family == 0x0002) {

	if (subtype == 0x0001)
	    workingPtr->handled = aim_parse_locateerr(sess, workingPtr);
	else if (subtype == 0x0003)
	  workingPtr->handled = aim_callhandler_noparam(sess, workingPtr->conn, 0x0002, 0x0003, workingPtr);
	else if (subtype == 0x0006)
	  workingPtr->handled = aim_parse_userinfo_middle(sess, workingPtr);
	else
	  workingPtr->handled = aim_callhandler_noparam(sess, workingPtr->conn, AIM_CB_FAM_LOC, AIM_CB_LOC_DEFAULT, workingPtr);

      } else if (family == 0x0003) {

	if (subtype == 0x0001)
	  workingPtr->handled = aim_parse_generalerrs(sess, workingPtr);
	else if (subtype == 0x0003)
	  workingPtr->handled = aim_parse_buddyrights(sess, workingPtr);
	else if (subtype == 0x000b)
	  workingPtr->handled = aim_parse_oncoming_middle(sess, workingPtr);
	else if (subtype == 0x000c)
	  workingPtr->handled = aim_parse_offgoing_middle(sess, workingPtr);
	else
	  workingPtr->handled = aim_callhandler_noparam(sess, workingPtr->conn, AIM_CB_FAM_BUD, AIM_CB_BUD_DEFAULT, workingPtr);

      } else if (family == 0x0004) {

	if (subtype == 0x0001)
	  workingPtr->handled = aim_parse_msgerror_middle(sess, workingPtr);
	else if (subtype == 0x0005)
	  workingPtr->handled = aim_callhandler_noparam(sess, workingPtr->conn, 0x0004, 0x0005, workingPtr);
	else if (subtype == 0x0006)
	  workingPtr->handled = aim_parse_outgoing_im_middle(sess, workingPtr);
	else if (subtype == 0x0007)
	  workingPtr->handled = aim_parse_incoming_im_middle(sess, workingPtr);
	else if (subtype == 0x000a)
	  workingPtr->handled = aim_parse_missedcall(sess, workingPtr);
	else if (subtype == 0x000c)
	  workingPtr->handled = aim_parse_msgack_middle(sess, workingPtr);
	else
	  workingPtr->handled = aim_callhandler_noparam(sess, workingPtr->conn, AIM_CB_FAM_MSG, AIM_CB_MSG_DEFAULT, workingPtr);

      } else if (family == 0x0007) {

	if (subtype == 0x0003)
	  workingPtr->handled = aim_parse_infochange(sess, workingPtr);
	else if (subtype == 0x0005)
	  workingPtr->handled = aim_parse_infochange(sess, workingPtr);
	else if (subtype == 0x0007)
	  workingPtr->handled = aim_parse_accountconfirm(sess, workingPtr);
	break;

      } else if (family == 0x0009) {

	if (subtype == 0x0001)
	  workingPtr->handled = aim_parse_generalerrs(sess, workingPtr);
	else if (subtype == 0x0003)
	  workingPtr->handled = aim_parse_bosrights(sess, workingPtr);
	else
	  workingPtr->handled = aim_callhandler_noparam(sess, workingPtr->conn, AIM_CB_FAM_BOS, AIM_CB_BOS_DEFAULT, workingPtr);

      } else if (family == 0x000a) {

	if (subtype == 0x0001)
	  workingPtr->handled = aim_parse_searcherror(sess, workingPtr);
	else if (subtype == 0x0003)
	  workingPtr->handled = aim_parse_searchreply(sess, workingPtr);
	else
	  workingPtr->handled = aim_callhandler_noparam(sess, workingPtr->conn, AIM_CB_FAM_LOK, AIM_CB_LOK_DEFAULT, workingPtr);

      } else if (family == 0x000b) {

	if (subtype == 0x0001)
	  workingPtr->handled = aim_parse_generalerrs(sess, workingPtr);
	else if (subtype == 0x0002)
	  workingPtr->handled = aim_callhandler_noparam(sess, workingPtr->conn, 0x000b, 0x0002, workingPtr);
	else
	  workingPtr->handled = aim_callhandler_noparam(sess, workingPtr->conn, AIM_CB_FAM_STS, AIM_CB_STS_DEFAULT, workingPtr);

      } else if (family == 0x000d) {

	if (subtype == 0x0009)
	  workingPtr->handled = aim_chatnav_parse_info(sess, workingPtr);

      } else if (family == 0x000e) {

	if (subtype == 0x0002)
	  workingPtr->handled = aim_chat_parse_infoupdate(sess, workingPtr);
	else if (subtype == 0x0003)
	  workingPtr->handled = aim_chat_parse_joined(sess, workingPtr);
	else if (subtype == 0x0004)
	  workingPtr->handled = aim_chat_parse_leave(sess, workingPtr);
	else if (subtype == 0x0006)
	  workingPtr->handled = aim_chat_parse_incoming(sess, workingPtr);

      } else if (family == 0x0013) {

	faimdprintf(sess, 0, "lalala: 0x%04x/0x%04x\n", family, subtype);

      } else if (family == 0x0017) {	/* New login protocol */

	if (subtype == 0x0001)
	  workingPtr->handled = aim_callhandler_noparam(sess, workingPtr->conn, 0x0017, 0x0001, workingPtr);
	else if (subtype == 0x0003)
	  workingPtr->handled = aim_authparse(sess, workingPtr);
	else if (subtype == 0x0007)
	  workingPtr->handled = aim_authkeyparse(sess, workingPtr);
	else
	  workingPtr->handled = aim_callhandler_noparam(sess, workingPtr->conn, 0x0017, 0xffff, workingPtr);

      } else if (family == AIM_CB_FAM_SPECIAL) {

	workingPtr->handled = aim_callhandler_noparam(sess, workingPtr->conn, family, subtype, workingPtr);

      }

      /* Try it raw and see if we can get it to happen... */
      if (!workingPtr->handled) /* XXX this is probably bad. */
	workingPtr->handled = aim_callhandler_noparam(sess, workingPtr->conn, family, subtype, workingPtr);

    }
  }

  /* 
   * This doesn't have to be called here.  It could easily be done
   * by a seperate thread or something. It's an administrative operation,
   * and can take a while. Though the less you call it the less memory
   * you'll have :)
   */
  aim_purge_rxqueue(sess);
  
  return 0;
}

faim_internal int aim_parse_msgack_middle(struct aim_session_t *sess, struct command_rx_struct *command)
{
  rxcallback_t userfunc = NULL;
  char sn[MAXSNLEN];
  unsigned short type;
  int i = 10+8; /* skip SNAC and cookie */
  int ret = 1;
  unsigned char snlen;

  type = aimutil_get16(command->data+i);
  i += 2;

  snlen = aimutil_get8(command->data+i);
  i++;

  memset(sn, 0, sizeof(sn));
  strncpy(sn, (char *)command->data+i, snlen);

  if ((userfunc = aim_callhandler(sess, command->conn, 0x0004, 0x000c)))
    ret =  userfunc(sess, command, type, sn);

  return ret;
}

/*
 * The Rate Limiting System, An Abridged Guide to Nonsense.
 *
 * OSCAR defines several 'rate classes'.  Each class has seperate
 * rate limiting properties (limit level, alert level, disconnect
 * level, etc), and a set of SNAC family/type pairs associated with
 * it.  The rate classes, their limiting properties, and the definitions
 * of which SNACs are belong to which class, are defined in the
 * Rate Response packet at login to each host.  
 *
 * Logically, all rate offenses within one class count against further
 * offenses for other SNACs in the same class (ie, sending messages
 * too fast will limit the number of user info requests you can send,
 * since those two SNACs are in the same rate class).
 *
 * Since the rate classes are defined dynamically at login, the values
 * below may change. But they seem to be fairly constant.
 *
 * Currently, BOS defines five rate classes, with the commonly used
 * members as follows...
 *
 *  Rate class 0x0001:
 *  	- Everything thats not in any of the other classes
 *
 *  Rate class 0x0002:
 * 	- Buddy list add/remove
 *	- Permit list add/remove
 *	- Deny list add/remove
 *
 *  Rate class 0x0003:
 *	- User information requests
 *	- Outgoing ICBMs
 *
 *  Rate class 0x0004:
 *	- A few unknowns: 2/9, 2/b, and f/2
 *
 *  Rate class 0x0005:
 *	- Chat room create
 *	- Outgoing chat ICBMs
 *
 * The only other thing of note is that class 5 (chat) has slightly looser
 * limiting properties than class 3 (normal messages).  But thats just a 
 * small bit of trivia for you.
 *
 * The last thing that needs to be learned about the rate limiting
 * system is how the actual numbers relate to the passing of time.  This
 * seems to be a big mystery.
 * 
 */
faim_internal int aim_parse_ratechange_middle(struct aim_session_t *sess, struct command_rx_struct *command)
{
  rxcallback_t userfunc = NULL;
  int ret = 1;
  int i;
  int code;
  unsigned long rateclass, windowsize, clear, alert, limit, disconnect;
  unsigned long currentavg, maxavg;

  i = 10;

  code = aimutil_get16(command->data+i);
  i += 2;

  rateclass = aimutil_get16(command->data+i);
  i += 2;

  windowsize = aimutil_get32(command->data+i);
  i += 4;
  clear = aimutil_get32(command->data+i);
  i += 4;
  alert = aimutil_get32(command->data+i);
  i += 4;
  limit = aimutil_get32(command->data+i);
  i += 4;
  disconnect = aimutil_get32(command->data+i);
  i += 4;
  currentavg = aimutil_get32(command->data+i);
  i += 4;
  maxavg = aimutil_get32(command->data+i);
  i += 4;

  if ((userfunc = aim_callhandler(sess, command->conn, 0x0001, 0x000a)))
    ret =  userfunc(sess, command, code, rateclass, windowsize, clear, alert, limit, disconnect, currentavg, maxavg);

  return ret;
}

faim_internal int aim_parse_evilnotify_middle(struct aim_session_t *sess, struct command_rx_struct *command)
{
  rxcallback_t userfunc = NULL;
  int ret = 1;
  int i;
  unsigned short newevil;
  struct aim_userinfo_s userinfo;

  i = 10;
  newevil = aimutil_get16(command->data+10);
  i += 2;

  memset(&userinfo, 0, sizeof(struct aim_userinfo_s));
  if (command->commandlen-i)
    i += aim_extractuserinfo(sess, command->data+i, &userinfo);

  if ((userfunc = aim_callhandler(sess, command->conn, 0x0001, 0x0010)))
    ret = userfunc(sess, command, newevil, &userinfo);
  
  return ret;
}

faim_internal int aim_parsemotd_middle(struct aim_session_t *sess,
				       struct command_rx_struct *command, ...)
{
  rxcallback_t userfunc = NULL;
  char *msg;
  int ret=1;
  struct aim_tlvlist_t *tlvlist;
  u_short id;

  /*
   * Code.
   *
   * Valid values:
   *   1 Mandatory upgrade
   *   2 Advisory upgrade
   *   3 System bulletin
   *   4 Nothing's wrong ("top o the world" -- normal)
   *
   */
  id = aimutil_get16(command->data+10);

  /* 
   * TLVs follow 
   */
  if (!(tlvlist = aim_readtlvchain(command->data+12, command->commandlen-12)))
    return ret;

  if (!(msg = aim_gettlv_str(tlvlist, 0x000b, 1))) {
    aim_freetlvchain(&tlvlist);
    return ret;
  }
  
  userfunc = aim_callhandler(sess, command->conn, 0x0001, 0x0013);
  if (userfunc)
    ret =  userfunc(sess, command, id, msg);

  aim_freetlvchain(&tlvlist);
  free(msg);

  return ret;  
}

faim_internal int aim_parse_hostonline(struct aim_session_t *sess,
				       struct command_rx_struct *command, ...)
{
  rxcallback_t userfunc = NULL;
  int ret = 1;
  unsigned short *families = NULL;
  int famcount = 0, i;

  famcount = (command->commandlen-10)/2;
  if (!(families = malloc(command->commandlen-10)))
    return ret;

  for (i = 0; i < famcount; i++)
    families[i] = aimutil_get16(command->data+((i*2)+10));

  if ((userfunc = aim_callhandler(sess, command->conn, 0x0001, 0x0003)))
    ret = userfunc(sess, command, famcount, families);

  free(families);

  return ret;  
}

faim_internal int aim_parse_accountconfirm(struct aim_session_t *sess,
					   struct command_rx_struct *command)
{
  rxcallback_t userfunc = NULL;
  int ret = 1;
  int status = -1;

  status = aimutil_get16(command->data+10);

  if ((userfunc = aim_callhandler(sess, command->conn, 0x0007, 0x0007)))
    ret = userfunc(sess, command, status);

  return ret;  
}

faim_internal int aim_parse_infochange(struct aim_session_t *sess,
				       struct command_rx_struct *command)
{
  unsigned short subtype; /* called for both reply and change-reply */
  int i;

  subtype = aimutil_get16(command->data+2);

  /*
   * struct {
   *    unsigned short perms;
   *    unsigned short tlvcount;
   *    aim_tlv_t tlvs[tlvcount];
   *  } admin_info[n];
   */
  for (i = 10; i < command->commandlen; ) {
    int perms, tlvcount;

    perms = aimutil_get16(command->data+i);
    i += 2;

    tlvcount = aimutil_get16(command->data+i);
    i += 2;

    while (tlvcount) {
      rxcallback_t userfunc;
      struct aim_tlv_t *tlv;
      int str = 0;

      if ((aimutil_get16(command->data+i) == 0x0011) ||
	  (aimutil_get16(command->data+i) == 0x0004))
	str = 1;

      if (str)
	tlv = aim_grabtlvstr(command->data+i);
      else
	tlv = aim_grabtlv(command->data+i);

      /* XXX fix so its only called once for the entire packet */
      if ((userfunc = aim_callhandler(sess, command->conn, 0x0007, subtype)))
	userfunc(sess, command, perms, tlv->type, tlv->length, tlv->value, str);

      if (tlv)
	i += 2+2+tlv->length;

      if (tlv && tlv->value)
	free(tlv->value);
      if (tlv)
	free(tlv);

      tlvcount--;
    }
  }

  return 1;
}

faim_internal int aim_parse_hostversions(struct aim_session_t *sess,
					 struct command_rx_struct *command, ...)
{
  rxcallback_t userfunc = NULL;
  int ret = 1;
  int vercount;

  vercount = (command->commandlen-10)/4;
  
  if ((userfunc = aim_callhandler(sess, command->conn, 0x0001, 0x0018)))
    ret = userfunc(sess, command, vercount, command->data+10);

  return ret;  
}

faim_internal int aim_handleredirect_middle(struct aim_session_t *sess,
					    struct command_rx_struct *command, ...)
{
  int serviceid = 0;
  unsigned char *cookie = NULL;
  char *ip = NULL;
  rxcallback_t userfunc = NULL;
  struct aim_tlvlist_t *tlvlist;
  int ret = 1;
  
  tlvlist = aim_readtlvchain(command->data+10, command->commandlen-10);

  if (aim_gettlv(tlvlist, 0x000d, 1))
    serviceid = aim_gettlv16(tlvlist, 0x000d, 1);
  if (aim_gettlv(tlvlist, 0x0005, 1))
    ip = aim_gettlv_str(tlvlist, 0x0005, 1);
  if (aim_gettlv(tlvlist, 0x0006, 1))
    cookie = aim_gettlv_str(tlvlist, 0x0006, 1);

  if ((serviceid == AIM_CONN_TYPE_CHAT) && sess->pendingjoin) {

    /*
     * Chat hack.
     *
     */
    if ((userfunc = aim_callhandler(sess, command->conn, 0x0001, 0x0005)))
      ret =  userfunc(sess, command, serviceid, ip, cookie, sess->pendingjoin, (int)sess->pendingjoinexchange);
      free(sess->pendingjoin);
      sess->pendingjoin = NULL;
      sess->pendingjoinexchange = 0;
  } else if (!serviceid || !ip || !cookie) { /* yeep! */
    ret = 1;
  } else {
    if ((userfunc = aim_callhandler(sess, command->conn, 0x0001, 0x0005)))
      ret =  userfunc(sess, command, serviceid, ip, cookie);
  }

  if (ip)
    free(ip);
  if (cookie)
    free(cookie);

  aim_freetlvchain(&tlvlist);

  return ret;
}

faim_internal int aim_parse_unknown(struct aim_session_t *sess,
					  struct command_rx_struct *command, ...)
{
  u_int i = 0;

  if (!sess || !command)
    return 1;

  faimdprintf(sess, 1, "\nRecieved unknown packet:");

  for (i = 0; i < command->commandlen; i++)
    {
      if ((i % 8) == 0)
	faimdprintf(sess, 1, "\n\t");

      faimdprintf(sess, 1, "0x%2x ", command->data[i]);
    }
  
  faimdprintf(sess, 1, "\n\n");

  return 1;
}


faim_internal int aim_negchan_middle(struct aim_session_t *sess,
				     struct command_rx_struct *command)
{
  struct aim_tlvlist_t *tlvlist;
  char *msg = NULL;
  unsigned short code = 0;
  rxcallback_t userfunc = NULL;
  int ret = 1;

  /* Used only by the older login protocol */
  /* XXX remove this special case? */
  if (command->conn->type == AIM_CONN_TYPE_AUTH)
    return aim_authparse(sess, command);

  tlvlist = aim_readtlvchain(command->data, command->commandlen);

  if (aim_gettlv(tlvlist, 0x0009, 1))
    code = aim_gettlv16(tlvlist, 0x0009, 1);

  if (aim_gettlv(tlvlist, 0x000b, 1))
    msg = aim_gettlv_str(tlvlist, 0x000b, 1);

  if ((userfunc = aim_callhandler(sess, command->conn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNERR))) 
    ret =  userfunc(sess, command, code, msg);

  aim_freetlvchain(&tlvlist);

  if (msg)
    free(msg);

  return ret;
}

/*
 * aim_parse_generalerrs()
 *
 * Middle handler for 0x0001 snac of each family.
 *
 */
faim_internal int aim_parse_generalerrs(struct aim_session_t *sess,
					struct command_rx_struct *command, ...)
{
  unsigned short family;
  unsigned short subtype;
  int ret = 1;
  int error = 0;
  rxcallback_t userfunc = NULL;
  
  family = aimutil_get16(command->data+0);
  subtype= aimutil_get16(command->data+2);
  
  if (command->commandlen > 10)
    error = aimutil_get16(command->data+10);

  if ((userfunc = aim_callhandler(sess, command->conn, family, subtype))) 
    ret = userfunc(sess, command, error);

  return ret;
}



