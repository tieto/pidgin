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

static aim_module_t *findmodule(struct aim_session_t *sess, const char *name)
{
  aim_module_t *cur;

  for (cur = (aim_module_t *)sess->modlistv; cur; cur = cur->next) {
    if (strcmp(name, cur->name) == 0)
      return cur;
  }

  return NULL;
}

faim_internal int aim__registermodule(struct aim_session_t *sess, int (*modfirst)(struct aim_session_t *, aim_module_t *))
{
  aim_module_t *mod;

  if (!sess || !modfirst)
    return -1;

  if (!(mod = malloc(sizeof(aim_module_t))))
    return -1;
  memset(mod, 0, sizeof(aim_module_t));

  if (modfirst(sess, mod) == -1) {
    free(mod);
    return -1;
  }

  if (findmodule(sess, mod->name)) {
    if (mod->shutdown)
      mod->shutdown(sess, mod);
    free(mod);
    return -1;
  }

  mod->next = (aim_module_t *)sess->modlistv;
  (aim_module_t *)sess->modlistv = mod;

  faimdprintf(sess, 1, "registered module %s (family 0x%04x)\n", mod->name, mod->family);

  return 0;
}

faim_internal void aim__shutdownmodules(struct aim_session_t *sess)
{
  aim_module_t *cur;

  for (cur = (aim_module_t *)sess->modlistv; cur; ) {
    aim_module_t *tmp;

    tmp = cur->next;

    if (cur->shutdown)
      cur->shutdown(sess, cur);

    free(cur);

    cur = tmp;
  }

  sess->modlistv = NULL;

  return;
}

static int consumesnac(struct aim_session_t *sess, struct command_rx_struct *rx)
{
  aim_module_t *cur;
  aim_modsnac_t snac;

  snac.family = aimutil_get16(rx->data+0);
  snac.subtype = aimutil_get16(rx->data+2);
  snac.flags = aimutil_get16(rx->data+4);
  snac.id = aimutil_get32(rx->data+6);

  for (cur = (aim_module_t *)sess->modlistv; cur; cur = cur->next) {

    if (!(cur->flags & AIM_MODFLAG_MULTIFAMILY) && 
	(cur->family != snac.family))
      continue;

    if (cur->snachandler(sess, cur, rx, &snac, rx->data+10, rx->commandlen-10))
      return 1;

  }

  return 0;
}

static int consumenonsnac(struct aim_session_t *sess, struct command_rx_struct *rx, unsigned short family, unsigned short subtype)
{
  aim_module_t *cur;
  aim_modsnac_t snac;

  snac.family = family;
  snac.subtype = subtype;
  snac.flags = snac.id = 0;

  for (cur = (aim_module_t *)sess->modlistv; cur; cur = cur->next) {

    if (!(cur->flags & AIM_MODFLAG_MULTIFAMILY) && 
	(cur->family != snac.family))
      continue;

    if (cur->snachandler(sess, cur, rx, &snac, rx->data, rx->commandlen))
      return 1;

  }

  return 0;
}

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
				    aim_rxcallback_t newhandler,
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

faim_internal aim_rxcallback_t aim_callhandler(struct aim_session_t *sess,
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
  aim_rxcallback_t userfunc = NULL;
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
  static int critical = 0;
  
  if (critical)
    return 0; /* don't call recursively! */

  critical = 1;

  if (sess->queue_incoming == NULL) {
    faimdprintf(sess, 1, "parse_generic: incoming packet queue empty.\n");
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

      if ((workingPtr->handled = consumesnac(sess, workingPtr)))
	continue;
	  
      if (!workingPtr->handled) {
	family = aimutil_get16(workingPtr->data);
	subtype = aimutil_get16(workingPtr->data+2);

	faimdprintf(sess, 1, "warning: unhandled packet %04x/%04x\n", family, subtype);
	consumenonsnac(sess, workingPtr, 0xffff, 0xffff); /* last chance! */
	workingPtr->handled = 1;
      }
    }
  }

  /* 
   * This doesn't have to be called here.  It could easily be done
   * by a seperate thread or something. It's an administrative operation,
   * and can take a while. Though the less you call it the less memory
   * you'll have :)
   */
  aim_purge_rxqueue(sess);

  critical = 0;
  
  return 0;
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
  aim_rxcallback_t userfunc = NULL;
  int ret = 1;

  /* Used only by the older login protocol */
  /* XXX remove this special case? */
  if (command->conn->type == AIM_CONN_TYPE_AUTH)
    return consumenonsnac(sess, command, 0x0017, 0x0003);

  tlvlist = aim_readtlvchain(command->data, command->commandlen);

  if (aim_gettlv(tlvlist, 0x0009, 1))
    code = aim_gettlv16(tlvlist, 0x0009, 1);

  if (aim_gettlv(tlvlist, 0x000b, 1))
    msg = aim_gettlv_str(tlvlist, 0x000b, 1);

  if ((userfunc = aim_callhandler(sess, command->conn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNERR))) 
    ret = userfunc(sess, command, code, msg);

  aim_freetlvchain(&tlvlist);

  if (msg)
    free(msg);

  return ret;
}

