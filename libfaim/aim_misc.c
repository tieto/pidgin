
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
u_long aim_bos_setidle(struct aim_session_t *sess,
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
u_long aim_bos_changevisibility(struct aim_session_t *sess,
				struct aim_conn_t *conn, 
				int changetype, char *denylist)
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

  if (!(newpacket = aim_tx_new(0x0002, conn, packlen)))
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
u_long aim_bos_setbuddylist(struct aim_session_t *sess,
			    struct aim_conn_t *conn, 
			    char *buddy_list)
{
  int i, j;

  struct command_tx_struct *newpacket;

  int packet_login_phase3c_hi_b_len = 0;

  char *localcpy = NULL;
  char *tmpptr = NULL;

  packet_login_phase3c_hi_b_len = 16; /* 16b for FLAP and SNAC headers */

  /* bail out if we can't make the packet */
  if (!buddy_list) {
    return -1;
  }

  localcpy = (char *) malloc(strlen(buddy_list)+1);
  memcpy(localcpy, buddy_list, strlen(buddy_list)+1);

  i = 0;
  tmpptr = strtok(localcpy, "&");
  while ((tmpptr != NULL) && (i < 100))
    {
#if debug > 0
      printf("---adding %s (%d)\n", tmpptr, strlen(tmpptr));
#endif
      packet_login_phase3c_hi_b_len += strlen(tmpptr)+1;
      i++;
      tmpptr = strtok(NULL, "&");
    }
#if debug > 0
  printf("*** send buddy list len: %d (%x)\n", packet_login_phase3c_hi_b_len, packet_login_phase3c_hi_b_len);
#endif
  free(localcpy);

  if (!(newpacket = aim_tx_new(0x0002, conn, packet_login_phase3c_hi_b_len - 6)))
    return -1;

  newpacket->lock = 1;
  
  aim_putsnac(newpacket->data, 0x0003, 0x0004, 0x0000, sess->snac_nextid);

  j = 10;  /* the next byte */

  i = 0;
  tmpptr = strtok(buddy_list, "&");
  while ((tmpptr != NULL) & (i < 100))
    {
#if debug > 0
      printf("---adding %s (%d)\n", tmpptr, strlen(tmpptr));
#endif
      newpacket->data[j] = strlen(tmpptr);
      memcpy(&(newpacket->data[j+1]), tmpptr, strlen(tmpptr));
      j += strlen(tmpptr)+1;
      i++;
      tmpptr = strtok(NULL, "&");
    }

  newpacket->lock = 0;

  aim_tx_enqueue(sess, newpacket);

  return (sess->snac_nextid++);
}

/* 
 * aim_bos_setprofile(profile)
 *
 * Gives BOS your profile.
 *
 * 
 * The large data chunk given here is of unknown decoding.
 * What I do know is that each 0x20 byte repetition 
 * represents a capability.  People with only the 
 * first two reptitions can support normal messaging
 * and chat (client version 2.0 or 3.0).  People with 
 * the third as well can also support voice chat (client
 * version 3.5 or higher).  IOW, if we don't send this,
 * we won't get chat invitations (get "software doesn't
 * support chat" error).
 *
 * This data is broadcast along with your oncoming
 * buddy command to everyone who has you on their
 * buddy list, as a type 0x0002 TLV.
 * 
 */
u_long aim_bos_setprofile(struct aim_session_t *sess,
			  struct aim_conn_t *conn, 
			  char *profile,
			  char *awaymsg,
			  unsigned int caps)
{
  struct command_tx_struct *newpacket;
  int i = 0;

  if (!(newpacket = aim_tx_new(0x0002, conn, 1152+strlen(profile)+1+(awaymsg?strlen(awaymsg):0))))
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
  {
    int isave;
    i += aimutil_put16(newpacket->data+i, 0x0005);
    isave = i;
    i += aimutil_put16(newpacket->data+i, 0);
    if (caps & AIM_CAPS_BUDDYICON)
      i += aimutil_putstr(newpacket->data+i, aim_caps[0], 0x10);
    if (caps & AIM_CAPS_VOICE)
      i += aimutil_putstr(newpacket->data+i, aim_caps[1], 0x10);
    if (caps & AIM_CAPS_IMIMAGE)
      i += aimutil_putstr(newpacket->data+i, aim_caps[2], 0x10);
    if (caps & AIM_CAPS_CHAT)
      i += aimutil_putstr(newpacket->data+i, aim_caps[3], 0x10);
    if (caps & AIM_CAPS_GETFILE)
      i += aimutil_putstr(newpacket->data+i, aim_caps[4], 0x10);
    if (caps & AIM_CAPS_SENDFILE)
      i += aimutil_putstr(newpacket->data+i, aim_caps[5], 0x10);
    aimutil_put16(newpacket->data+isave, i-isave-2);
  }
  newpacket->commandlen = i;
  aim_tx_enqueue(sess, newpacket);
  
  return (sess->snac_nextid++);
}

/* 
 * aim_bos_setgroupperm(mask)
 * 
 * Set group permisson mask.  Normally 0x1f.
 *
 */
u_long aim_bos_setgroupperm(struct aim_session_t *sess,
			    struct aim_conn_t *conn, 
			    u_long mask)
{
  return aim_genericreq_l(sess, conn, 0x0009, 0x0004, &mask);
}

/*
 * aim_bos_clientready()
 * 
 * Send Client Ready.  
 *
 * TODO: Dynamisize.
 *
 */
u_long aim_bos_clientready(struct aim_session_t *sess,
			   struct aim_conn_t *conn)
{
  u_char command_2[] = {
     /* placeholders for dynamic data */
     0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
     0xff, 0xff, 
     /* real data */
     0x00, 0x01,   
     0x00, 0x03, 
     0x00, 0x04, 
     0x06, 0x86,  
     0x00, 0x02, 
     0x00, 0x01,  
     0x00, 0x04, 
     0x00, 0x01, 
 
     0x00, 0x03, 
     0x00, 0x01,  
     0x00, 0x04, 
     0x00, 0x01, 
     0x00, 0x04, 
     0x00, 0x01, 
     0x00, 0x04,
     0x00, 0x01,
 
     0x00, 0x06, 
     0x00, 0x01, 
     0x00, 0x04,  
     0x00, 0x01, 
     0x00, 0x08, 
     0x00, 0x01, 
     0x00, 0x04,
     0x00, 0x01,
 
     0x00, 0x09, 
     0x00, 0x01, 
     0x00, 0x04,
     0x00, 0x01, 
     0x00, 0x0a, 
     0x00, 0x01, 
     0x00, 0x04,
     0x00, 0x01,
 
     0x00, 0x0b,
     0x00, 0x01, 
     0x00, 0x04,
     0x00, 0x01
  };
  int command_2_len = 0x52;
  struct command_tx_struct *newpacket;
  
  if (!(newpacket = aim_tx_new(0x0002, conn, command_2_len)))
    return -1;

  newpacket->lock = 1;

  memcpy(newpacket->data, command_2, command_2_len);
  
  /* This write over the dynamic parts of the byte block */
  aim_putsnac(newpacket->data, 0x0001, 0x0002, 0x0000, sess->snac_nextid);

  aim_tx_enqueue(sess, newpacket);

  return (sess->snac_nextid++);
}

/* 
 *  send_login_phase3(int socket)   
 *
 *  Request Rate Information.
 * 
 *  TODO: Move to aim_conn.
 *  TODO: Move to SNAC interface.
 */
u_long aim_bos_reqrate(struct aim_session_t *sess,
		       struct aim_conn_t *conn)
{
  return aim_genericreq_n(sess, conn, 0x0001, 0x0006);
}

/* 
 *  send_login_phase3b(int socket)   
 *
 *  Rate Information Response Acknowledge.
 *
 */
u_long aim_bos_ackrateresp(struct aim_session_t *sess,
			   struct aim_conn_t *conn)
{
  struct command_tx_struct *newpacket;
  int packlen = 18, i=0;

  if (conn->type != AIM_CONN_TYPE_BOS)
    packlen += 2;

  if(!(newpacket = aim_tx_new(0x0002, conn, packlen)));
  
  newpacket->lock = 1;

  i = aim_putsnac(newpacket->data, 0x0001, 0x0008, 0x0000, sess->snac_nextid);
  i += aimutil_put16(newpacket->data+i, 0x0001); 
  i += aimutil_put16(newpacket->data+i, 0x0002);
  i += aimutil_put16(newpacket->data+i, 0x0003);
  i += aimutil_put16(newpacket->data+i, 0x0004);
  
  if (conn->type != AIM_CONN_TYPE_BOS) {
    i += aimutil_put16(newpacket->data+i, 0x0005);
  }

  aim_tx_enqueue(sess, newpacket);

  return (sess->snac_nextid++);
}

/* 
 * aim_bos_setprivacyflags()
 *
 * Sets privacy flags. Normally 0x03.
 *
 *  Bit 1:  Allows other AIM users to see how long you've been idle.
 *
 *
 */
u_long aim_bos_setprivacyflags(struct aim_session_t *sess,
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
u_long aim_bos_reqpersonalinfo(struct aim_session_t *sess,
			       struct aim_conn_t *conn)
{
  struct command_tx_struct *newpacket;
  
  if (!(newpacket = aim_tx_new(0x0002, conn, 12)))
    return -1;

  newpacket->lock = 1;

  aim_putsnac(newpacket->data, 0x000a, 0x0001, 0x000e /* huh? */, sess->snac_nextid);
  
  newpacket->data[10] = 0x0d;
  newpacket->data[11] = 0xda;

  newpacket->lock = 0;
  aim_tx_enqueue(sess, newpacket);

  return (sess->snac_nextid++);
}

u_long aim_setversions(struct aim_session_t *sess,
                               struct aim_conn_t *conn)
{
  struct command_tx_struct *newpacket;
  int i;

  if (!(newpacket = aim_tx_new(0x0002, conn, 10 + (4*11))))
    return -1;

  newpacket->lock = 1;

  i = aim_putsnac(newpacket->data, 0x0001, 0x0017, 0x0000, sess->snac_nextid);

  i += aimutil_put16(newpacket->data+i, 0x0001);
  i += aimutil_put16(newpacket->data+i, 0x0003);

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
  i += aimutil_put16(newpacket->data+i, 0x0002);

  i += aimutil_put16(newpacket->data+i, 0x000c);
  i += aimutil_put16(newpacket->data+i, 0x0001);

  i += aimutil_put16(newpacket->data+i, 0x0015);
  i += aimutil_put16(newpacket->data+i, 0x0001);

#if 0
  for (j = 0; j < 0x10; j++) {
    i += aimutil_put16(newpacket->data+i, j); /* family */
    i += aimutil_put16(newpacket->data+i, 0x0003); /* version */
  }
#endif
  newpacket->lock = 0;
  aim_tx_enqueue(sess, newpacket);

  return (sess->snac_nextid++);
}


/*
 * aim_bos_reqservice(serviceid)
 *
 * Service request. 
 *
 */
u_long aim_bos_reqservice(struct aim_session_t *sess,
			  struct aim_conn_t *conn, 
			  u_short serviceid)
{
  return aim_genericreq_s(sess, conn, 0x0001, 0x0004, &serviceid);
}

/*
 * aim_bos_reqrights()
 *
 * Request BOS rights.
 *
 */
u_long aim_bos_reqrights(struct aim_session_t *sess,
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
u_long aim_bos_reqbuddyrights(struct aim_session_t *sess,
			      struct aim_conn_t *conn)
{
  return aim_genericreq_n(sess, conn, 0x0003, 0x0002);
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
u_long aim_genericreq_n(struct aim_session_t *sess,
			struct aim_conn_t *conn, 
			u_short family, u_short subtype)
{
  struct command_tx_struct *newpacket;

  if (!(newpacket = aim_tx_new(0x0002, conn, 10)))
    return 0;

  newpacket->lock = 1;

  aim_putsnac(newpacket->data, family, subtype, 0x0000, sess->snac_nextid);
 
  aim_tx_enqueue(sess, newpacket);
  return (sess->snac_nextid++);
}

/*
 *
 *
 */
u_long aim_genericreq_l(struct aim_session_t *sess,
			struct aim_conn_t *conn, 
			u_short family, u_short subtype, u_long *longdata)
{
  struct command_tx_struct *newpacket;
  u_long newlong;

  /* If we don't have data, there's no reason to use this function */
  if (!longdata)
    return aim_genericreq_n(sess, conn, family, subtype);

  if (!(newpacket = aim_tx_new(0x0002, conn, 10+sizeof(u_long))))
    return -1;

  newpacket->lock = 1;

  aim_putsnac(newpacket->data, family, subtype, 0x0000, sess->snac_nextid);

  /* copy in data */
  newlong = htonl(*longdata);
  memcpy(&(newpacket->data[10]), &newlong, sizeof(u_long));

  aim_tx_enqueue(sess, newpacket);
  return (sess->snac_nextid++);
}

u_long aim_genericreq_s(struct aim_session_t *sess,
			struct aim_conn_t *conn, 
			u_short family, u_short subtype, u_short *shortdata)
{
  struct command_tx_struct *newpacket;
  u_short newshort;

  /* If we don't have data, there's no reason to use this function */
  if (!shortdata)
    return aim_genericreq_n(sess, conn, family, subtype);

  if (!(newpacket = aim_tx_new(0x0002, conn, 10+sizeof(u_short))))
    return -1;

  newpacket->lock = 1;

  aim_putsnac(newpacket->data, family, subtype, 0x0000, sess->snac_nextid);

  /* copy in data */
  newshort = htons(*shortdata);
  memcpy(&(newpacket->data[10]), &newshort, sizeof(u_short));

  aim_tx_enqueue(sess, newpacket);
  return (sess->snac_nextid++);
}

/*
 * aim_bos_reqlocaterights()
 *
 * Request Location services rights.
 *
 */
u_long aim_bos_reqlocaterights(struct aim_session_t *sess,
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
u_long aim_bos_reqicbmparaminfo(struct aim_session_t *sess,
				struct aim_conn_t *conn)
{
  return aim_genericreq_n(sess, conn, 0x0004, 0x0004);
}

