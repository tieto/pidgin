
/*
 * aim_misc.c
 *
 * TODO: Seperate a lot of this into an aim_bos.c.
 *
 * Other things...
 *
 *   - Idle setting 
 * 
 *
 */

#include <faim/aim.h> 

/*
 * aim_bos_setidle()
 *
 *  Should set your current idle time in seconds.  Idealy, OSCAR should
 *  do this for us.  But, it doesn't.  The client must call this to set idle
 *  time.  
 *
 */
faim_export unsigned long aim_bos_setidle(struct aim_session_t *sess,
					  struct aim_conn_t *conn, 
					  u_long idletime)
{
  return aim_genericreq_l(sess, conn, 0x0001, 0x0011, &idletime);
}


/*
 * aim_bos_changevisibility(conn, changtype, namelist)
 *
 * Changes your visibility depending on changetype:
 *
 *  AIM_VISIBILITYCHANGE_PERMITADD: Lets provided list of names see you
 *  AIM_VISIBILITYCHANGE_PERMIDREMOVE: Removes listed names from permit list
 *  AIM_VISIBILITYCHANGE_DENYADD: Hides you from provided list of names
 *  AIM_VISIBILITYCHANGE_DENYREMOVE: Lets list see you again
 *
 * list should be a list of 
 * screen names in the form "Screen Name One&ScreenNameTwo&" etc.
 *
 * Equivelents to options in WinAIM:
 *   - Allow all users to contact me: Send an AIM_VISIBILITYCHANGE_DENYADD
 *      with only your name on it.
 *   - Allow only users on my Buddy List: Send an 
 *      AIM_VISIBILITYCHANGE_PERMITADD with the list the same as your
 *      buddy list
 *   - Allow only the uesrs below: Send an AIM_VISIBILITYCHANGE_PERMITADD 
 *      with everyone listed that you want to see you.
 *   - Block all users: Send an AIM_VISIBILITYCHANGE_PERMITADD with only 
 *      yourself in the list
 *   - Block the users below: Send an AIM_VISIBILITYCHANGE_DENYADD with
 *      the list of users to be blocked
 *
 *
 */
faim_export unsigned long aim_bos_changevisibility(struct aim_session_t *sess,
						   struct aim_conn_t *conn, 
						   int changetype, 
						   char *denylist)
{
  struct command_tx_struct *newpacket;
  int packlen = 0;
  u_short subtype;

  char *localcpy = NULL;
  char *tmpptr = NULL;
  int i,j;
  int listcount;

  if (!denylist)
    return 0;

  localcpy = (char *) malloc(strlen(denylist)+1);
  memcpy(localcpy, denylist, strlen(denylist)+1);
  
  listcount = aimutil_itemcnt(localcpy, '&');
  packlen = aimutil_tokslen(localcpy, 99, '&') + listcount + 9;

  if (!(newpacket = aim_tx_new(AIM_FRAMETYPE_OSCAR, 0x0002, conn, packlen)))
    return -1;

  newpacket->lock = 1;

  switch(changetype)
    {
    case AIM_VISIBILITYCHANGE_PERMITADD:    subtype = 0x05; break;
    case AIM_VISIBILITYCHANGE_PERMITREMOVE: subtype = 0x06; break;
    case AIM_VISIBILITYCHANGE_DENYADD:      subtype = 0x07; break;
    case AIM_VISIBILITYCHANGE_DENYREMOVE:   subtype = 0x08; break;
    default:
      free(newpacket->data);
      free(newpacket);
      return 0;
    }

  /* We actually DO NOT send a SNAC ID with this one! */
  aim_putsnac(newpacket->data, 0x0009, subtype, 0x00, 0);
 
  j = 10;  /* the next byte */
  
  for (i=0; (i < (listcount - 1)) && (i < 99); i++)
    {
      tmpptr = aimutil_itemidx(localcpy, i, '&');

      newpacket->data[j] = strlen(tmpptr);
      memcpy(&(newpacket->data[j+1]), tmpptr, strlen(tmpptr));
      j += strlen(tmpptr)+1;
      free(tmpptr);
    }
  free(localcpy);

  newpacket->lock = 0;

  aim_tx_enqueue(sess, newpacket);

  return (sess->snac_nextid); /* dont increment */

}



/*
 * aim_bos_setbuddylist(buddylist)
 *
 * This just builds the "set buddy list" command then queues it.
 *
 * buddy_list = "Screen Name One&ScreenNameTwo&";
 *
 * TODO: Clean this up.  
 *
 * XXX: I can't stress the TODO enough.
 *
 */
faim_export unsigned long aim_bos_setbuddylist(struct aim_session_t *sess,
					       struct aim_conn_t *conn, 
					       char *buddy_list)
{
  int i, j;

  struct command_tx_struct *newpacket;

  int len = 0;

  char *localcpy = NULL;
  char *tmpptr = NULL;

  len = 10; /* 10B SNAC headers */

  if (!buddy_list || !(localcpy = (char *) malloc(strlen(buddy_list)+1))) 
    return -1;
  strncpy(localcpy, buddy_list, strlen(buddy_list)+1);

  i = 0;
  tmpptr = strtok(localcpy, "&");
  while ((tmpptr != NULL) && (i < 150)) {
#if debug > 0
    printf("---adding %d: %s (%d)\n", i, tmpptr, strlen(tmpptr));
#endif
    len += 1+strlen(tmpptr);
    i++;
    tmpptr = strtok(NULL, "&");
  }
#if debug > 0
  printf("*** send buddy list len: %d (%x)\n", len, len);
#endif

  if (!(newpacket = aim_tx_new(AIM_FRAMETYPE_OSCAR, 0x0002, conn, len)))
    return -1;

  newpacket->lock = 1;
  
  aim_putsnac(newpacket->data, 0x0003, 0x0004, 0x0000, 0);

  j = 10;  /* the next byte */

  strncpy(localcpy, buddy_list, strlen(buddy_list)+1);
  i = 0;
  tmpptr = strtok(localcpy, "&");
  while ((tmpptr != NULL) & (i < 150)) {
#if debug > 0
    printf("---adding %d: %s (%d)\n", i, tmpptr, strlen(tmpptr));
#endif
    newpacket->data[j] = strlen(tmpptr);
    memcpy(&(newpacket->data[j+1]), tmpptr, strlen(tmpptr));
    j += 1+strlen(tmpptr);
    i++;
    tmpptr = strtok(NULL, "&");
  }

  newpacket->lock = 0;

  aim_tx_enqueue(sess, newpacket);

  free(localcpy);

  return (sess->snac_nextid);
}

/* 
 * aim_bos_setprofile(profile)
 *
 * Gives BOS your profile.
 *
 * 
 */
faim_export unsigned long aim_bos_setprofile(struct aim_session_t *sess,
					     struct aim_conn_t *conn, 
					     char *profile,
					     char *awaymsg,
					     unsigned short caps)
{
  struct command_tx_struct *newpacket;
  int i = 0, tmp, caplen;

  if (!(newpacket = aim_tx_new(AIM_FRAMETYPE_OSCAR, 0x0002, conn, 1152+strlen(profile)+1+(awaymsg?strlen(awaymsg):0))))
    return -1;

  i += aim_putsnac(newpacket->data, 0x0002, 0x004, 0x0000, sess->snac_nextid);
  i += aim_puttlv_str(newpacket->data+i, 0x0001, strlen("text/x-aolrtf; charset=\"us-ascii\""), "text/x-aolrtf; charset=\"us-ascii\"");
  i += aim_puttlv_str(newpacket->data+i, 0x0002, strlen(profile), profile);
  /* why do we send this twice?  */
  i += aim_puttlv_str(newpacket->data+i, 0x0003, strlen("text/x-aolrtf; charset=\"us-ascii\""), "text/x-aolrtf; charset=\"us-ascii\"");
  
  /* Away message -- we send this no matter what, even if its blank */
  if (awaymsg)
    i += aim_puttlv_str(newpacket->data+i, 0x0004, strlen(awaymsg), awaymsg);
  else
    i += aim_puttlv_str(newpacket->data+i, 0x0004, 0x0000, NULL);

  /* Capability information. */
 
  tmp = (i += aimutil_put16(newpacket->data+i, 0x0005));
  i += aimutil_put16(newpacket->data+i, 0x0000); /* rewritten later */
  i += (caplen = aim_putcap(newpacket->data+i, 512, caps));
  aimutil_put16(newpacket->data+tmp, caplen); /* rewrite TLV size */

  newpacket->commandlen = i;
  aim_tx_enqueue(sess, newpacket);
  
  return (sess->snac_nextid++);
}

/* 
 * aim_bos_setgroupperm(mask)
 * 
 * Set group permisson mask.  Normally 0x1f (all classes).
 *
 * The group permission mask allows you to keep users of a certain
 * class or classes from talking to you.  The mask should be
 * a bitwise OR of all the user classes you want to see you.
 *
 */
faim_export unsigned long aim_bos_setgroupperm(struct aim_session_t *sess,
					       struct aim_conn_t *conn, 
					       u_long mask)
{
  return aim_genericreq_l(sess, conn, 0x0009, 0x0004, &mask);
}

faim_internal int aim_parse_bosrights(struct aim_session_t *sess,
				      struct command_rx_struct *command, ...)
{
  rxcallback_t userfunc = NULL;
  int ret=1;
  struct aim_tlvlist_t *tlvlist;
  unsigned short maxpermits = 0, maxdenies = 0;

  /* 
   * TLVs follow 
   */
  if (!(tlvlist = aim_readtlvchain(command->data+10, command->commandlen-10)))
    return ret;

  /*
   * TLV type 0x0001: Maximum number of buddies on permit list.
   */
  if (aim_gettlv(tlvlist, 0x0001, 1))
    maxpermits = aim_gettlv16(tlvlist, 0x0001, 1);

  /*
   * TLV type 0x0002: Maximum number of buddies on deny list.
   *
   */
  if (aim_gettlv(tlvlist, 0x0002, 1)) 
    maxdenies = aim_gettlv16(tlvlist, 0x0002, 1);
  
  if ((userfunc = aim_callhandler(command->conn, 0x0009, 0x0003)))
    ret = userfunc(sess, command, maxpermits, maxdenies);

  aim_freetlvchain(&tlvlist);

  return ret;  
}

/*
 * aim_bos_clientready()
 * 
 * Send Client Ready.  
 *
 */
faim_export unsigned long aim_bos_clientready(struct aim_session_t *sess,
					      struct aim_conn_t *conn)
{
#define AIM_TOOL_JAVA   0x0001
#define AIM_TOOL_MAC    0x0002
#define AIM_TOOL_WIN16  0x0003
#define AIM_TOOL_WIN32  0x0004
#define AIM_TOOL_MAC68K 0x0005
#define AIM_TOOL_MACPPC 0x0006
  struct aim_tool_version {
    unsigned short group;
    unsigned short version;
    unsigned short tool;
    unsigned short toolversion;
  } tools[] = {
    {0x0001, 0x0003,    AIM_TOOL_WIN32, 0x0686},
    {0x0002, 0x0001,    AIM_TOOL_WIN32, 0x0001}, 
    {0x0003, 0x0001,    AIM_TOOL_WIN32, 0x0001},
    {0x0004, 0x0001,    AIM_TOOL_WIN32, 0x0001},
    {0x0006, 0x0001,    AIM_TOOL_WIN32, 0x0001}, 
    {0x0008, 0x0001,    AIM_TOOL_WIN32, 0x0001},
    {0x0009, 0x0001,    AIM_TOOL_WIN32, 0x0001}, 
    {0x000a, 0x0001,    AIM_TOOL_WIN32, 0x0001},
    {0x000b, 0x0001,    AIM_TOOL_WIN32, 0x0001}
  };
  int i,j;
  struct command_tx_struct *newpacket;
  int toolcount = sizeof(tools)/sizeof(struct aim_tool_version);

  if (!(newpacket = aim_tx_new(AIM_FRAMETYPE_OSCAR, 0x0002, conn, 1152)))
    return -1;

  newpacket->lock = 1;

  i = aim_putsnac(newpacket->data, 0x0001, 0x0002, 0x0000, sess->snac_nextid);
  aim_cachesnac(sess, 0x0001, 0x0002, 0x0000, NULL, 0);

  for (j = 0; j < toolcount; j++) {
    i += aimutil_put16(newpacket->data+i, tools[j].group);
    i += aimutil_put16(newpacket->data+i, tools[j].version);
    i += aimutil_put16(newpacket->data+i, tools[j].tool);
    i += aimutil_put16(newpacket->data+i, tools[j].toolversion);
  }

  newpacket->commandlen = i;
  newpacket->lock = 0;

  aim_tx_enqueue(sess, newpacket);

  return sess->snac_nextid;
}

/* 
 *  Request Rate Information.
 * 
 */
faim_export unsigned long aim_bos_reqrate(struct aim_session_t *sess,
					  struct aim_conn_t *conn)
{
  return aim_genericreq_n(sess, conn, 0x0001, 0x0006);
}

/* 
 *  Rate Information Response Acknowledge.
 *
 */
faim_export unsigned long aim_bos_ackrateresp(struct aim_session_t *sess,
					      struct aim_conn_t *conn)
{
  struct command_tx_struct *newpacket;
  int packlen = 20, i=0;

  if(!(newpacket = aim_tx_new(AIM_FRAMETYPE_OSCAR, 0x0002, conn, packlen)))
    return (sess->snac_nextid);
  
  newpacket->lock = 1;

  i = aim_putsnac(newpacket->data, 0x0001, 0x0008, 0x0000, 0);
  i += aimutil_put16(newpacket->data+i, 0x0001); 
  i += aimutil_put16(newpacket->data+i, 0x0002);
  i += aimutil_put16(newpacket->data+i, 0x0003);
  i += aimutil_put16(newpacket->data+i, 0x0004);
  i += aimutil_put16(newpacket->data+i, 0x0005);

  newpacket->commandlen = i;
  newpacket->lock = 0;

  aim_tx_enqueue(sess, newpacket);

  return (sess->snac_nextid);
}

/* 
 * aim_bos_setprivacyflags()
 *
 * Sets privacy flags. Normally 0x03.
 *
 *  Bit 1:  Allows other AIM users to see how long you've been idle.
 *  Bit 2:  Allows other AIM users to see how long you've been a member.
 *
 */
faim_export unsigned long aim_bos_setprivacyflags(struct aim_session_t *sess,
						  struct aim_conn_t *conn, 
						  u_long flags)
{
  return aim_genericreq_l(sess, conn, 0x0001, 0x0014, &flags);
}

/*
 * aim_bos_reqpersonalinfo()
 *
 * Requests the current user's information. Can't go generic on this one
 * because aparently it uses SNAC flags.
 *
 */
faim_export unsigned long aim_bos_reqpersonalinfo(struct aim_session_t *sess,
						  struct aim_conn_t *conn)
{
  return aim_genericreq_n(sess, conn, 0x0001, 0x000e);
}

faim_export unsigned long aim_setversions(struct aim_session_t *sess,
					  struct aim_conn_t *conn)
{
  struct command_tx_struct *newpacket;
  int i;

  if (!(newpacket = aim_tx_new(AIM_FRAMETYPE_OSCAR, 0x0002, conn, 10 + (4*12))))
    return -1;

  newpacket->lock = 1;

  i = aim_putsnac(newpacket->data, 0x0001, 0x0017, 0x0000, sess->snac_nextid);
  aim_cachesnac(sess, 0x0001, 0x0017, 0x0000, NULL, 0);

  i += aimutil_put16(newpacket->data+i, 0x0001);
  i += aimutil_put16(newpacket->data+i, 0x0003);

  i += aimutil_put16(newpacket->data+i, 0x0013);
  i += aimutil_put16(newpacket->data+i, 0x0001);

  i += aimutil_put16(newpacket->data+i, 0x0002);
  i += aimutil_put16(newpacket->data+i, 0x0001);

  i += aimutil_put16(newpacket->data+i, 0x0003);
  i += aimutil_put16(newpacket->data+i, 0x0001);

  i += aimutil_put16(newpacket->data+i, 0x0004);
  i += aimutil_put16(newpacket->data+i, 0x0001);

  i += aimutil_put16(newpacket->data+i, 0x0006);
  i += aimutil_put16(newpacket->data+i, 0x0001);

  i += aimutil_put16(newpacket->data+i, 0x0008);
  i += aimutil_put16(newpacket->data+i, 0x0001);

  i += aimutil_put16(newpacket->data+i, 0x0009);
  i += aimutil_put16(newpacket->data+i, 0x0001);

  i += aimutil_put16(newpacket->data+i, 0x000a);
  i += aimutil_put16(newpacket->data+i, 0x0001);

  i += aimutil_put16(newpacket->data+i, 0x000b);
  i += aimutil_put16(newpacket->data+i, 0x0001);

  i += aimutil_put16(newpacket->data+i, 0x000c);
  i += aimutil_put16(newpacket->data+i, 0x0001);

  newpacket->commandlen = i;
  newpacket->lock = 0;
  aim_tx_enqueue(sess, newpacket);

  return sess->snac_nextid;
}


/*
 * aim_bos_reqservice(serviceid)
 *
 * Service request. 
 *
 */
faim_export unsigned long aim_bos_reqservice(struct aim_session_t *sess,
			  struct aim_conn_t *conn, 
			  u_short serviceid)
{
  return aim_genericreq_s(sess, conn, 0x0001, 0x0004, &serviceid);
}

/*
 * aim_bos_nop()
 *
 * No-op.  WinAIM sends these every 4min or so to keep
 * the connection alive.  Its not real necessary.
 *
 */
faim_export unsigned long aim_bos_nop(struct aim_session_t *sess,
				      struct aim_conn_t *conn)
{
  return aim_genericreq_n(sess, conn, 0x0001, 0x0016);
}

/*
 * aim_flap_nop()
 *
 * No-op.  WinAIM 4.x sends these _every minute_ to keep
 * the connection alive.  
 */
faim_export unsigned long aim_flap_nop(struct aim_session_t *sess,
				       struct aim_conn_t *conn)
{
  struct command_tx_struct *newpacket;

  if (!(newpacket = aim_tx_new(AIM_FRAMETYPE_OSCAR, 0x0005, conn, 0)))
    return sess->snac_nextid;

  newpacket->lock = 1;
  newpacket->commandlen = 0;
  newpacket->lock = 0;

  aim_tx_enqueue(sess, newpacket);

  return (sess->snac_nextid);
}

/*
 * aim_bos_reqrights()
 *
 * Request BOS rights.
 *
 */
faim_export unsigned long aim_bos_reqrights(struct aim_session_t *sess,
					    struct aim_conn_t *conn)
{
  return aim_genericreq_n(sess, conn, 0x0009, 0x0002);
}

/*
 * aim_bos_reqbuddyrights()
 *
 * Request Buddy List rights.
 *
 */
faim_export unsigned long aim_bos_reqbuddyrights(struct aim_session_t *sess,
						 struct aim_conn_t *conn)
{
  return aim_genericreq_n(sess, conn, 0x0003, 0x0002);
}

/*
 * aim_send_warning(struct aim_session_t *sess, 
 *                  struct aim_conn_t *conn, char *destsn, int anon)
 * send a warning to destsn.
 * anon is anonymous or not;
 *  AIM_WARN_ANON anonymous
 *
 * returns -1 on error (couldn't alloc packet), next snacid on success.
 *
 */
faim_export int aim_send_warning(struct aim_session_t *sess, struct aim_conn_t *conn, char *destsn, int anon)
{
  struct command_tx_struct *newpacket;
  int curbyte;

  if (!(newpacket = aim_tx_new(AIM_FRAMETYPE_OSCAR, 0x0002, conn, strlen(destsn)+13)))
    return -1;

  newpacket->lock = 1;

  curbyte  = 0;
  curbyte += aim_putsnac(newpacket->data+curbyte,
                        0x0004, 0x0008, 0x0000, sess->snac_nextid);

  curbyte += aimutil_put16(newpacket->data+curbyte, (anon & AIM_WARN_ANON)?1:0);

  curbyte += aimutil_put8(newpacket->data+curbyte, strlen(destsn));

  curbyte += aimutil_putstr(newpacket->data+curbyte, destsn, strlen(destsn));

  newpacket->commandlen = curbyte;
  newpacket->lock = 0;

  aim_tx_enqueue(sess, newpacket);

  return (sess->snac_nextid++);
}

/*
 * aim_debugconn_sendconnect()
 *
 * For aimdebugd.  If you don't know what it is, you don't want to.
 */
faim_export unsigned long aim_debugconn_sendconnect(struct aim_session_t *sess,
						    struct aim_conn_t *conn)
{
  return aim_genericreq_n(sess, conn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_DEBUGCONN_CONNECT);
}

/*
 * Generic routine for sending commands.
 *
 *
 * I know I can do this in a smarter way...but I'm not thinking straight
 * right now...
 *
 * I had one big function that handled all three cases, but then it broke
 * and I split it up into three.  But then I fixed it.  I just never went
 * back to the single.  I don't see any advantage to doing it either way.
 *
 */
faim_internal unsigned long aim_genericreq_n(struct aim_session_t *sess,
					     struct aim_conn_t *conn, 
					     u_short family, u_short subtype)
{
  struct command_tx_struct *newpacket;

  if (!(newpacket = aim_tx_new(AIM_FRAMETYPE_OSCAR, 0x0002, conn, 10)))
    return 0;

  newpacket->lock = 1;

  aim_putsnac(newpacket->data, family, subtype, 0x0000, sess->snac_nextid);

  aim_cachesnac(sess, family, subtype, 0x0000, NULL, 0);

  aim_tx_enqueue(sess, newpacket);
  return sess->snac_nextid;
}

/*
 *
 *
 */
faim_internal unsigned long aim_genericreq_l(struct aim_session_t *sess,
					     struct aim_conn_t *conn, 
					     u_short family, u_short subtype, 
					     u_long *longdata)
{
  struct command_tx_struct *newpacket;
  u_long newlong;

  /* If we don't have data, there's no reason to use this function */
  if (!longdata)
    return aim_genericreq_n(sess, conn, family, subtype);

  if (!(newpacket = aim_tx_new(AIM_FRAMETYPE_OSCAR, 0x0002, conn, 10+sizeof(u_long))))
    return -1;

  newpacket->lock = 1;

  aim_putsnac(newpacket->data, family, subtype, 0x0000, sess->snac_nextid);
  aim_cachesnac(sess, family, subtype, 0x0000, NULL, 0);

  /* copy in data */
  newlong = htonl(*longdata);
  memcpy(&(newpacket->data[10]), &newlong, sizeof(u_long));

  aim_tx_enqueue(sess, newpacket);
  return sess->snac_nextid;
}

faim_internal unsigned long aim_genericreq_s(struct aim_session_t *sess,
					     struct aim_conn_t *conn, 
					     u_short family, u_short subtype, 
					     u_short *shortdata)
{
  struct command_tx_struct *newpacket;
  u_short newshort;

  /* If we don't have data, there's no reason to use this function */
  if (!shortdata)
    return aim_genericreq_n(sess, conn, family, subtype);

  if (!(newpacket = aim_tx_new(AIM_FRAMETYPE_OSCAR, 0x0002, conn, 10+sizeof(u_short))))
    return -1;

  newpacket->lock = 1;

  aim_putsnac(newpacket->data, family, subtype, 0x0000, sess->snac_nextid);
  aim_cachesnac(sess, family, subtype, 0x0000, NULL, 0);

  /* copy in data */
  newshort = htons(*shortdata);
  memcpy(&(newpacket->data[10]), &newshort, sizeof(u_short));

  aim_tx_enqueue(sess, newpacket);
  return sess->snac_nextid;
}

/*
 * aim_bos_reqlocaterights()
 *
 * Request Location services rights.
 *
 */
faim_export unsigned long aim_bos_reqlocaterights(struct aim_session_t *sess,
						  struct aim_conn_t *conn)
{
  return aim_genericreq_n(sess, conn, 0x0002, 0x0002);
}

/*
* aim_bos_reqicbmparaminfo()
 *
 * Request ICBM parameter information.
 *
 */
faim_export unsigned long aim_bos_reqicbmparaminfo(struct aim_session_t *sess,
						   struct aim_conn_t *conn)
{
  return aim_genericreq_n(sess, conn, 0x0004, 0x0004);
}

/*
 * Add ICBM parameter? Huh?
 */
faim_export unsigned long aim_addicbmparam(struct aim_session_t *sess,
					   struct aim_conn_t *conn)
{
  struct command_tx_struct *newpacket;
  int packlen = 10+16, i=0;

  if(!(newpacket = aim_tx_new(AIM_FRAMETYPE_OSCAR, 0x0002, conn, packlen)))
    return (sess->snac_nextid);
  
  newpacket->lock = 1;

  i = aim_putsnac(newpacket->data, 0x0004, 0x0002, 0x0000, sess->snac_nextid);
  aim_cachesnac(sess, 0x0004, 0x0002, 0x0000, NULL, 0);

  i += aimutil_put16(newpacket->data+i, 0x0000); 
  i += aimutil_put16(newpacket->data+i, 0x0000);
  i += aimutil_put16(newpacket->data+i, 0x0003);
  i += aimutil_put16(newpacket->data+i, 0x1f40);
  i += aimutil_put16(newpacket->data+i, 0x03e7);
  i += aimutil_put16(newpacket->data+i, 0x03e7);
  i += aimutil_put16(newpacket->data+i, 0x0000); 
  i += aimutil_put16(newpacket->data+i, 0x0000); 
  
  aim_tx_enqueue(sess, newpacket);

  return sess->snac_nextid;
}

/* 
 * Set directory profile data (not the same as aim_bos_setprofile!)
 */
faim_export unsigned long aim_setdirectoryinfo(struct aim_session_t *sess, struct aim_conn_t *conn, char *first, char *middle, char *last, char *maiden, char *nickname, char *street, char *city, char *state, char *zip, int country, unsigned short privacy) 
{
  struct command_tx_struct *newpacket;
  int packlen = 0, i = 0;

  packlen += 2+2+2;

  if(first) /* TLV 0001 */
    packlen += (strlen(first) + 4);
  if(middle) 
    packlen += (strlen(middle) + 4);
  if(last)
    packlen += (strlen(last) + 4);
  if(maiden)
    packlen += (strlen(maiden) + 4);
  if(nickname)
    packlen += (strlen(nickname) + 4);
  if(street)
    packlen += (strlen(street) + 4);
  if(state)
    packlen += (strlen(state) + 4);
  if(city)
    packlen += (strlen(city) + 4);
  if(zip)
    packlen += (strlen(zip) + 4);
    
  if(!(newpacket = aim_tx_new(AIM_FRAMETYPE_OSCAR, 0x0002, conn, packlen+10)))
    return -1;

  newpacket->lock = 1;

  i = aim_putsnac(newpacket->data, 0x0002, 0x0009, 0x0000, 0);

  /* 000a/0002: privacy: 1 to allow search/disp, 0 to disallow */
  i += aim_puttlv_16(newpacket->data+i, 0x000a, privacy);


  if (first)
    i += aim_puttlv_str(newpacket->data+i, 0x0001, strlen(first), first);
  if (middle)
    i += aim_puttlv_str(newpacket->data+i, 0x0003, strlen(middle), middle);
  if (last)
    i += aim_puttlv_str(newpacket->data+i, 0x0002, strlen(last), last);
  if (maiden)
    i += aim_puttlv_str(newpacket->data+i, 0x0004, strlen(maiden), maiden);
  if (nickname)
    i += aim_puttlv_str(newpacket->data+i, 0x000c, strlen(nickname), nickname);
  if (street)
    i += aim_puttlv_str(newpacket->data+i, 0x0021, strlen(street), street);
  if (city)
    i += aim_puttlv_str(newpacket->data+i, 0x0008, strlen(city), city);
  if (state)
    i += aim_puttlv_str(newpacket->data+i, 0x0007, strlen(state), state);
  if (zip)
    i += aim_puttlv_str(newpacket->data+i, 0x000d, strlen(zip), zip);

  newpacket->commandlen = i;
  newpacket->lock = 0;

  aim_tx_enqueue(sess, newpacket);
   
  return(sess->snac_nextid);
}

faim_export unsigned long aim_setuserinterests(struct aim_session_t *sess, struct aim_conn_t *conn, char *interest1, char *interest2, char *interest3, char *interest4, char *interest5, unsigned short privacy)
{
  struct command_tx_struct *newpacket;
  int packlen = 0, i = 0;

  packlen += 2+2+2;

  if(interest1)
    packlen += (strlen(interest1) + 4);
  if(interest2)
    packlen += (strlen(interest2) + 4);
  if(interest3)
    packlen += (strlen(interest3) + 4);
  if(interest4)
    packlen += (strlen(interest4) + 4);
  if(interest5)
    packlen += (strlen(interest5) + 4) ;

    
  if(!(newpacket = aim_tx_new(AIM_FRAMETYPE_OSCAR, 0x0002, conn, packlen+10)))
    return -1;

  newpacket->lock = 1;

  i = aim_putsnac(newpacket->data, 0x0002, 0x000f, 0x0000, 0);

  /* 000a/0002: 0000 ?? ?privacy? */
  i += aim_puttlv_16(newpacket->data+i, 0x000a, privacy); 

  if(interest1) 
    i += aim_puttlv_str(newpacket->data+i, 0x000b, strlen(interest1), interest1);
  if(interest2) 
    i += aim_puttlv_str(newpacket->data+i, 0x000b, strlen(interest2), interest2);
  if(interest3) 
    i += aim_puttlv_str(newpacket->data+i, 0x000b, strlen(interest3), interest3);
  if(interest4) 
    i += aim_puttlv_str(newpacket->data+i, 0x000b, strlen(interest4), interest4);
  if(interest5) 
    i += aim_puttlv_str(newpacket->data+i, 0x000b, strlen(interest1), interest5);

  newpacket->commandlen = i;
  newpacket->lock = 0;
    
  aim_tx_enqueue(sess, newpacket);
    
  return(sess->snac_nextid);
}

faim_export unsigned long aim_icq_setstatus(struct aim_session_t *sess,
					    struct aim_conn_t *conn, 
					    unsigned long status)
{
  struct command_tx_struct *newpacket;
  int i;
  unsigned long data;
  
  data = 0x00030000 | status; /* yay for error checking ;^) */

  if(!(newpacket = aim_tx_new(AIM_FRAMETYPE_OSCAR, 0x0002, conn, 10 + 4)))
    return -1;

  newpacket->lock = 1;

  i = aim_putsnac(newpacket->data, 0x0001, 0x001e, 0x0000, 0x0000001e);
  i += aim_puttlv_32(newpacket->data+i, 0x0006, data);

  newpacket->commandlen = i;
  newpacket->lock = 0;

  aim_tx_enqueue(sess, newpacket);

  return(sess->snac_nextid);
}
