
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

#include "aim.h"

/*
 * aim_bos_setidle()
 *
 *  Should set your current idle time in seconds.  Idealy, OSCAR should
 *  do this for us.  But, it doesn't.  The client must call this to set idle
 *  time.  
 *
 */
u_long aim_bos_setidle(struct aim_conn_t *conn, u_long idletime)
{
  return aim_genericreq_l(conn, 0x0001, 0x0011, &idletime);
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
u_long aim_bos_changevisibility(struct aim_conn_t *conn, int changetype, char *denylist)
{
  struct command_tx_struct newpacket;

  char *localcpy = NULL;
  char *tmpptr = NULL;
  char *tmpptr2 = NULL;
  int i,j;

  if (!denylist)
    return 0;

  newpacket.lock = 1;

  if (conn)
    newpacket.conn = conn;
  else
    newpacket.conn = aim_getconn_type(AIM_CONN_TYPE_BOS);

  newpacket.type = 0x02;
  newpacket.commandlen = 10;

  localcpy = (char *) malloc(strlen(denylist)+1);
  memcpy(localcpy, denylist, strlen(denylist)+1);
  tmpptr2 = localcpy; /* save this for the free() */

  i = 0;
  tmpptr = strsep(&localcpy, "&");
  while (strlen(tmpptr) && (i < 100))
    {
      newpacket.commandlen += strlen(tmpptr)+1;
      i++;
      tmpptr = strsep(&localcpy, "&");
    }
  free(tmpptr2);
  tmpptr2 = NULL;

  newpacket.data = (char *) malloc(newpacket.commandlen);
  memset(newpacket.data, 0x00, newpacket.commandlen);

  newpacket.data[0] = 0x00;
  newpacket.data[1] = 0x09;
  newpacket.data[2] = 0x00;
  switch(changetype)
    {
    case AIM_VISIBILITYCHANGE_PERMITADD:
      newpacket.data[3] = 0x05; break;
    case AIM_VISIBILITYCHANGE_PERMITREMOVE:
      newpacket.data[3] = 0x06; break;
    case AIM_VISIBILITYCHANGE_DENYADD:
      newpacket.data[3] = 0x07; break;
    case AIM_VISIBILITYCHANGE_DENYREMOVE:
      newpacket.data[4] = 0x08; break;
    default:
      return 0;
    }
  /* SNAC reqid -- we actually DO NOT send a SNAC ID with this one! */
  newpacket.data[6] = 0;
  newpacket.data[7] = 0;
  newpacket.data[8] = 0;
  newpacket.data[9] = 0;
 
  j = 10;  /* the next byte */

  localcpy = (char *) malloc(strlen(denylist)+1);
  memcpy(localcpy, denylist, strlen(denylist)+1);
  tmpptr2 = localcpy; /* save this for the free() */

  i = 0;
  tmpptr = strsep(&localcpy, "&");
  while (strlen(tmpptr) && (i < 100))
    {
      newpacket.data[j] = strlen(tmpptr);
      memcpy(&(newpacket.data[j+1]), tmpptr, strlen(tmpptr));
      j += strlen(tmpptr)+1;
      i++;
      tmpptr = strsep(&localcpy, "&");
    }
  free(tmpptr2);

  newpacket.lock = 0;

  aim_tx_enqueue(&newpacket);

  return (aim_snac_nextid); /* dont increment */

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
 */
u_long aim_bos_setbuddylist(struct aim_conn_t *conn, char *buddy_list)
{
  int i, j;

  struct command_tx_struct newpacket;

  int packet_login_phase3c_hi_b_len = 0;

  char *localcpy = NULL;
  char *tmpptr = NULL;

  packet_login_phase3c_hi_b_len = 16; /* 16b for FLAP and SNAC headers */

  /* bail out if we can't make the packet */
  if (buddy_list == NULL)
    {
      printf("\nNO BUDDIES!  ARE YOU THAT LONELY???\n");
      return 0;
    }
#if debug > 0
  printf("****buddy list: %s\n", buddy_list);
  printf("****buddy list len: %d (%x)\n", strlen(buddy_list), strlen(buddy_list));
#endif

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

  newpacket.type = 0x02;
  if (conn)
    newpacket.conn = conn;
  else
    newpacket.conn = aim_getconn_type(AIM_CONN_TYPE_BOS);
  newpacket.commandlen = packet_login_phase3c_hi_b_len - 6;
  newpacket.lock = 1;
  
  newpacket.data = (char *) malloc(newpacket.commandlen);

  newpacket.data[0] = 0x00;
  newpacket.data[1] = 0x03;
  newpacket.data[2] = 0x00;
  newpacket.data[3] = 0x04;
  newpacket.data[4] = 0x00;
  newpacket.data[5] = 0x00;
  /* SNAC reqid */
  newpacket.data[6] = (aim_snac_nextid >> 24) & 0xFF;
  newpacket.data[7] = (aim_snac_nextid >> 16) & 0xFF;
  newpacket.data[8] = (aim_snac_nextid >>  8) & 0xFF;
  newpacket.data[9] = (aim_snac_nextid) & 0xFF;

  j = 10;  /* the next byte */

  i = 0;
  tmpptr = strtok(buddy_list, "&");
  while ((tmpptr != NULL) & (i < 100))
    {
#if debug > 0
      printf("---adding %s (%d)\n", tmpptr, strlen(tmpptr));
#endif
      newpacket.data[j] = strlen(tmpptr);
      memcpy(&(newpacket.data[j+1]), tmpptr, strlen(tmpptr));
      j += strlen(tmpptr)+1;
      i++;
      tmpptr = strtok(NULL, "&");
    }

  newpacket.lock = 0;

  aim_tx_enqueue(&newpacket);

  return (aim_snac_nextid++);
}

/* 
 * aim_bos_setprofile(profile)
 *
 * Gives BOS your profile.
 *
 */
u_long aim_bos_setprofile(struct aim_conn_t *conn, char *profile)
{
  int packet_profile_len = 0;
  struct command_tx_struct newpacket;
  int i = 0;

  /* len: SNAC */
  packet_profile_len = 10;
  /* len: T+L (where t(0001)) */
  packet_profile_len += 2 + 2;
  /* len: V (where t(0001)) */
  packet_profile_len += strlen("text/x-aolrtf");
  /* len: T+L (where t(0002)) */
  packet_profile_len += 2 + 2;
  /* len: V (where t(0002)) */
  packet_profile_len += strlen(profile);

  newpacket.type = 0x02;
  if (conn)
    newpacket.conn = conn;
  else
    newpacket.conn = aim_getconn_type(AIM_CONN_TYPE_BOS);
  newpacket.commandlen = packet_profile_len;
  newpacket.data = (char *) malloc(packet_profile_len);

  i = 0;
  /* SNAC: family */
  newpacket.data[i++] = 0x00;
  newpacket.data[i++] = 0x02;
  /* SNAC: subtype */
  newpacket.data[i++] = 0x00;
  newpacket.data[i++] = 0x04;
  /* SNAC: flags */
  newpacket.data[i++] = 0x00;
  newpacket.data[i++] = 0x00;
  /* SNAC: id */
  /* SNAC reqid */
  newpacket.data[i++] = (aim_snac_nextid >> 24) & 0xFF;
  newpacket.data[i++] = (aim_snac_nextid >> 16) & 0xFF;
  newpacket.data[i++] = (aim_snac_nextid >>  8) & 0xFF;
  newpacket.data[i++] = (aim_snac_nextid) & 0xFF;
  /* TLV t(0001) */
  newpacket.data[i++] = 0x00;
  newpacket.data[i++] = 0x01;
  /* TLV l(000d) */
  newpacket.data[i++] = 0x00;
  newpacket.data[i++] = 0x0d;
  /* TLV v(text/x-aolrtf) */
  memcpy(&(newpacket.data[i]), "text/x-aolrtf", 0x000d);
  i += 0x000d;
  
  /* TLV t(0002) */
  newpacket.data[i++] = 0x00;
  newpacket.data[i++] = 0x02;
  /* TLV l() */
  newpacket.data[i++] = (strlen(profile) >> 8) & 0xFF;
  newpacket.data[i++] = (strlen(profile) & 0xFF);
  /* TLV v(profile) */
  memcpy(&(newpacket.data[i]), profile, strlen(profile));

  aim_tx_enqueue(&newpacket);
  
  return (aim_snac_nextid++);
}

/* 
 * aim_bos_setgroupperm(mask)
 * 
 * Set group permisson mask.  Normally 0x1f.
 *
 */
u_long aim_bos_setgroupperm(struct aim_conn_t *conn, u_long mask)
{
  return aim_genericreq_l(conn, 0x0009, 0x0004, &mask);
}

/*
 * aim_bos_clientready()
 * 
 * Send Client Ready.  
 *
 * TODO: Dynamisize.
 *
 */
u_long aim_bos_clientready(struct aim_conn_t *conn)
{
  char command_2[] = {
     0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x7a, 0x8c,
     0x11, 0xab, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01,
     0x00, 0x13, 0x00, 0x09, 0x00, 0x01, 0x00, 0x01,
     0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x01,
     0x00, 0x01, 0x00, 0x04, 0x00, 0x01, 0x00, 0x01,
     0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x01,
     0x00, 0x01, 0x00, 0x08, 0x00, 0x01, 0x00, 0x01,
     0x00, 0x01, 0x00, 0x06, 0x00, 0x01, 0x00, 0x01,
     0x00, 0x01, 0x00, 0x0a, 0x00, 0x01, 0x00, 0x01,
     0x00, 0x01, 0x00, 0x0b, 0x00, 0x01, 0x00, 0x01,
     0x00, 0x01
  };
  int command_2_len = 0x52;
  struct command_tx_struct newpacket;
  
  newpacket.lock = 1;
  if (conn)
    newpacket.conn = conn;
  else
    newpacket.conn = aim_getconn_type(AIM_CONN_TYPE_BOS);
  newpacket.type = 0x02;
  newpacket.commandlen = command_2_len;
  newpacket.data = (char *) malloc (newpacket.commandlen);
  memcpy(newpacket.data, command_2, newpacket.commandlen);
  
  /* SNAC reqid */
  newpacket.data[6] = (aim_snac_nextid >> 24) & 0xFF;
  newpacket.data[7] = (aim_snac_nextid >> 16) & 0xFF;
  newpacket.data[8] = (aim_snac_nextid >>  8) & 0xFF;
  newpacket.data[9] = (aim_snac_nextid) & 0xFF;

  aim_tx_enqueue(&newpacket);

  return (aim_snac_nextid++);
}

/* 
 *  send_login_phase3(int socket)   
 *
 *  Request Rate Information.
 * 
 *  TODO: Move to aim_conn.
 *  TODO: Move to SNAC interface.
 */
u_long aim_bos_reqrate(struct aim_conn_t *conn)
{
  return aim_genericreq_n(conn, 0x0001, 0x0006);
}

/* 
 *  send_login_phase3b(int socket)   
 *
 *  Rate Information Response Acknowledge.
 *
 */
u_long aim_bos_ackrateresp(struct aim_conn_t *conn)
{
  struct command_tx_struct newpacket;

  newpacket.lock = 1;
  if (conn)
    newpacket.conn = conn;
  else
    newpacket.conn = aim_getconn_type(AIM_CONN_TYPE_BOS);
  newpacket.type = 0x02;
  newpacket.commandlen = 18;

  newpacket.data = (char *) malloc(newpacket.commandlen);
  newpacket.data[0] = 0x00;
  newpacket.data[1] = 0x01;
  newpacket.data[2] = 0x00;
  newpacket.data[3] = 0x08;
  newpacket.data[4] = 0x00;
  newpacket.data[5] = 0x00;
  /* SNAC reqid */
  newpacket.data[6] = (aim_snac_nextid >> 24) & 0xFF;
  newpacket.data[7] = (aim_snac_nextid >> 16) & 0xFF;
  newpacket.data[8] = (aim_snac_nextid >>  8) & 0xFF;
  newpacket.data[9] = (aim_snac_nextid) & 0xFF;

  newpacket.data[10] = 0x00;
  newpacket.data[11] = 0x01;
  newpacket.data[12] = 0x00;
  newpacket.data[13] = 0x02;
  newpacket.data[14] = 0x00;
  newpacket.data[15] = 0x03;
  newpacket.data[16] = 0x00;
  newpacket.data[17] = 0x04;

  aim_tx_enqueue(&newpacket);

  return (aim_snac_nextid++);
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
u_long aim_bos_setprivacyflags(struct aim_conn_t *conn, u_long flags)
{
  return aim_genericreq_l(conn, 0x0001, 0x0014, &flags);
}

/*
 * aim_bos_reqpersonalinfo()
 *
 * Requests the current user's information. Can't go generic on this one
 * because aparently it uses SNAC flags.
 *
 */
u_long aim_bos_reqpersonalinfo(struct aim_conn_t *conn)
{
  struct command_tx_struct newpacket;
  
  newpacket.lock = 1;
  if (conn)
    newpacket.conn = conn;
  else
    newpacket.conn = aim_getconn_type(AIM_CONN_TYPE_BOS);
  newpacket.type = 0x02;
  newpacket.commandlen = 12;

  newpacket.data = (char *) malloc(newpacket.commandlen);
  
  newpacket.data[0] = 0x00;
  newpacket.data[1] = 0x0a;
  newpacket.data[2] = 0x00;
  newpacket.data[3] = 0x01;
  newpacket.data[4] = 0x00;
  newpacket.data[5] = 0x0e; /* huh? */

  /* SNAC reqid */
  newpacket.data[6] = (aim_snac_nextid >> 24) & 0xFF;
  newpacket.data[7] = (aim_snac_nextid >> 16) & 0xFF;
  newpacket.data[8] = (aim_snac_nextid >>  8) & 0xFF;
  newpacket.data[9] = (aim_snac_nextid) & 0xFF;

  newpacket.data[10] = 0x0d;
  newpacket.data[11] = 0xda;

  aim_tx_enqueue(&newpacket);

  return (aim_snac_nextid++);
}

/*
 * aim_bos_reqservice(serviceid)
 *
 * Service request. 
 *
 */
u_long aim_bos_reqservice(struct aim_conn_t *conn, u_short serviceid)
{
  return aim_genericreq_s(conn, 0x0001, 0x0004, &serviceid);
}

/*
 * aim_bos_reqrights()
 *
 * Request BOS rights.
 *
 */
u_long aim_bos_reqrights(struct aim_conn_t *conn)
{
  return aim_genericreq_n(conn, 0x0009, 0x0002);
}

/*
 * aim_bos_reqbuddyrights()
 *
 * Request Buddy List rights.
 *
 */
u_long aim_bos_reqbuddyrights(struct aim_conn_t *conn)
{
  return aim_genericreq_n(conn, 0x0003, 0x0002);
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
u_long aim_genericreq_n(struct aim_conn_t *conn, u_short family, u_short subtype)
{
  struct command_tx_struct newpacket;

  newpacket.lock = 1;

  if (conn)
    newpacket.conn = conn;
  else
    newpacket.conn = aim_getconn_type(AIM_CONN_TYPE_BOS);
  newpacket.type = 0x02;

  newpacket.commandlen = 10;

  newpacket.data = (char *) malloc(newpacket.commandlen);
  memset(newpacket.data, 0x00, newpacket.commandlen);
  newpacket.data[0] = (family & 0xff00)>>8;
  newpacket.data[1] = family & 0xff;
  newpacket.data[2] = (subtype & 0xff00)>>8;
  newpacket.data[3] = subtype & 0xff;
  newpacket.data[4] = 0x00;
  newpacket.data[5] = 0x00;
  /* SNAC reqid */
  newpacket.data[6] = (aim_snac_nextid >> 24) & 0xFF;
  newpacket.data[7] = (aim_snac_nextid >> 16) & 0xFF;
  newpacket.data[8] = (aim_snac_nextid >>  8) & 0xFF;
  newpacket.data[9] = (aim_snac_nextid) & 0xFF;

  aim_tx_enqueue(&newpacket);
  return (aim_snac_nextid++);
}

/*
 *
 *
 */
u_long aim_genericreq_l(struct aim_conn_t *conn, u_short family, u_short subtype, u_long *longdata)
{
  struct command_tx_struct newpacket;
  u_long newlong;

  /* If we don't have data, there's no reason to use this function */
  if (!longdata)
    return aim_genericreq_n(conn, family, subtype);

  newpacket.lock = 1;

  if (conn)
    newpacket.conn = conn;
  else
    newpacket.conn = aim_getconn_type(AIM_CONN_TYPE_BOS);
  newpacket.type = 0x02;

  newpacket.commandlen = 10+sizeof(u_long);

  newpacket.data = (char *) malloc(newpacket.commandlen);
  memset(newpacket.data, 0x00, newpacket.commandlen);

  newpacket.data[0] = (family & 0xff00)>>8;
  newpacket.data[1] = family & 0xff;
  newpacket.data[2] = (subtype & 0xff00)>>8;
  newpacket.data[3] = subtype & 0xff;
  newpacket.data[4] = 0x00;
  newpacket.data[5] = 0x00;
  /* SNAC reqid */
  newpacket.data[6] = (aim_snac_nextid >> 24) & 0xFF;
  newpacket.data[7] = (aim_snac_nextid >> 16) & 0xFF;
  newpacket.data[8] = (aim_snac_nextid >>  8) & 0xFF;
  newpacket.data[9] = (aim_snac_nextid) & 0xFF;

  /* copy in data */
  newlong = htonl(*longdata);
  memcpy(&(newpacket.data[10]), &newlong, sizeof(u_long));

  aim_tx_enqueue(&newpacket);
  return (aim_snac_nextid++);
}

u_long aim_genericreq_s(struct aim_conn_t *conn, u_short family, u_short subtype, u_short *shortdata)
{
  struct command_tx_struct newpacket;
  u_short newshort;

  /* If we don't have data, there's no reason to use this function */
  if (!shortdata)
    return aim_genericreq_n(conn, family, subtype);

  newpacket.lock = 1;

  if (conn)
    newpacket.conn = conn;
  else
    newpacket.conn = aim_getconn_type(AIM_CONN_TYPE_BOS);
  newpacket.type = 0x02;

  newpacket.commandlen = 10+sizeof(u_short);

  newpacket.data = (char *) malloc(newpacket.commandlen);
  memset(newpacket.data, 0x00, newpacket.commandlen);

  newpacket.data[0] = (family & 0xff00)>>8;
  newpacket.data[1] = family & 0xff;
  newpacket.data[2] = (subtype & 0xff00)>>8;
  newpacket.data[3] = subtype & 0xff;
  newpacket.data[4] = 0x00;
  newpacket.data[5] = 0x00;
  /* SNAC reqid */
  newpacket.data[6] = (aim_snac_nextid >> 24) & 0xFF;
  newpacket.data[7] = (aim_snac_nextid >> 16) & 0xFF;
  newpacket.data[8] = (aim_snac_nextid >>  8) & 0xFF;
  newpacket.data[9] = (aim_snac_nextid) & 0xFF;

  /* copy in data */
  newshort = htons(*shortdata);
  memcpy(&(newpacket.data[10]), &newshort, sizeof(u_short));

  aim_tx_enqueue(&newpacket);
  return (aim_snac_nextid++);
}

/*
 * aim_bos_reqlocaterights()
 *
 * Request Location services rights.
 *
 */
u_long aim_bos_reqlocaterights(struct aim_conn_t *conn)
{
  return aim_genericreq_n(conn, 0x0002, 0x0002);
}

/*
 * aim_bos_reqicbmparaminfo()
 *
 * Request ICBM parameter information.
 *
 */
u_long aim_bos_reqicbmparaminfo(struct aim_conn_t *conn)
{
  return aim_genericreq_n(conn, 0x0004, 0x0004);
}
