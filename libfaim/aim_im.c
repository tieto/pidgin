/*
 *  aim_im.c
 *
 *  The routines for sending/receiving Instant Messages.
 *
 */

#include <faim/aim.h>

/*
 * Send an ICBM (instant message).  
 *
 *
 * Possible flags:
 *   AIM_IMFLAGS_AWAY  -- Marks the message as an autoresponse
 *   AIM_IMFLAGS_ACK   -- Requests that the server send an ack
 *                        when the message is received (of type 0x0004/0x000c)
 *
 */
u_long aim_send_im(struct aim_session_t *sess,
		   struct aim_conn_t *conn, 
		   char *destsn, u_int flags, char *msg)
{   

  int curbyte,i;
  struct command_tx_struct *newpacket;
  
  if (strlen(msg) >= MAXMSGLEN)
    return -1;

  if (!(newpacket = aim_tx_new(AIM_FRAMETYPE_OSCAR, 0x0002, conn, strlen(msg)+256)))
    return -1;

  newpacket->lock = 1; /* lock struct */

  curbyte  = 0;
  curbyte += aim_putsnac(newpacket->data+curbyte, 
			 0x0004, 0x0006, 0x0000, sess->snac_nextid);

  /* 
   * Generate a random message cookie 
   *
   * We could cache these like we do SNAC IDs.  (In fact, it 
   * might be a good idea.)  In the message error functions, 
   * the 8byte message cookie is returned as well as the 
   * SNAC ID.
   *
   */
  for (i=0;i<8;i++)
    curbyte += aimutil_put8(newpacket->data+curbyte, (u_char) random());

  /*
   * Channel ID
   */
  curbyte += aimutil_put16(newpacket->data+curbyte,0x0001);

  /* 
   * Destination SN (prepended with byte length)
   */
  curbyte += aimutil_put8(newpacket->data+curbyte,strlen(destsn));
  curbyte += aimutil_putstr(newpacket->data+curbyte, destsn, strlen(destsn));

  /*
   * metaTLV start.
   */
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0002);
  curbyte += aimutil_put16(newpacket->data+curbyte, strlen(msg) + 0x10);

  /*
   * Flag data / ICBM Parameters?
   */
  curbyte += aimutil_put8(newpacket->data+curbyte, 0x05);
  curbyte += aimutil_put8(newpacket->data+curbyte, 0x01);

  /* number of bytes to follow */
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0004);
  curbyte += aimutil_put8(newpacket->data+curbyte, 0x01);
  curbyte += aimutil_put8(newpacket->data+curbyte, 0x01);
  curbyte += aimutil_put8(newpacket->data+curbyte, 0x01);
  curbyte += aimutil_put8(newpacket->data+curbyte, 0x02);

  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0101);

  /* 
   * Message block length.
   */
  curbyte += aimutil_put16(newpacket->data+curbyte, strlen(msg) + 0x04);

  /*
   * Character set data? 
   */
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0000);
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0000);

  /*
   * Message.  Not terminated.
   */
  curbyte += aimutil_putstr(newpacket->data+curbyte,msg, strlen(msg));

  /*
   * Set the Request Acknowledge flag.  
   */
  if (flags & AIM_IMFLAGS_ACK) {
    curbyte += aimutil_put16(newpacket->data+curbyte,0x0003);
    curbyte += aimutil_put16(newpacket->data+curbyte,0x0000);
  }
  
  /*
   * Set the Autoresponse flag.
   */
  if (flags & AIM_IMFLAGS_AWAY) {
    curbyte += aimutil_put16(newpacket->data+curbyte,0x0004);
    curbyte += aimutil_put16(newpacket->data+curbyte,0x0000);
  }
  
  newpacket->commandlen = curbyte;
  newpacket->lock = 0;

  aim_tx_enqueue(sess, newpacket);

#ifdef USE_SNAC_FOR_IMS
 {
    struct aim_snac_t snac;

    snac.id = sess->snac_nextid;
    snac.family = 0x0004;
    snac.type = 0x0006;
    snac.flags = 0x0000;

    snac.data = malloc(strlen(destsn)+1);
    memcpy(snac.data, destsn, strlen(destsn)+1);

    aim_newsnac(sess, &snac);
  }

 aim_cleansnacs(sess, 60); /* clean out all SNACs over 60sec old */
#endif

  return (sess->snac_nextid++);
}

struct aim_directim_priv {
  unsigned char cookie[8];
  char sn[MAXSNLEN+1];
};

int aim_send_im_direct(struct aim_session_t *sess, 
		       struct aim_conn_t *conn,
		       char *msg)
{
  struct command_tx_struct *newpacket;
  struct aim_directim_priv *priv = NULL;
  int i;

  if (strlen(msg) >= MAXMSGLEN)
    return -1;

  if (!sess || !conn || (conn->type != AIM_CONN_TYPE_RENDEZVOUS) || !conn->priv) {
    printf("faim: directim: invalid arguments\n");
    return -1;
  }

  if (!(newpacket = aim_tx_new(AIM_FRAMETYPE_OFT, 0x0001, conn, strlen(msg)))) {
    printf("faim: directim: tx_new failed\n");
    return -1;
  }

  newpacket->lock = 1; /* lock struct */

  priv = (struct aim_directim_priv *)conn->priv;

  newpacket->hdr.oft.hdr2len = 0x44;

  if (!(newpacket->hdr.oft.hdr2 = malloc(newpacket->hdr.oft.hdr2len))) {
    free(newpacket);
    return -1;
  }

  i = 0;
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0006);
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0000);

  i += aimutil_putstr(newpacket->hdr.oft.hdr2+i, priv->cookie, 8);

  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0000);
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0000);
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0000);
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0000);

  i += aimutil_put32(newpacket->hdr.oft.hdr2+i, strlen(msg));

  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0000);
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0000);
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0000);
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0000);
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0000);
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0000);

  i += aimutil_putstr(newpacket->hdr.oft.hdr2+i, sess->logininfo.screen_name, strlen(sess->logininfo.screen_name));
  
  i = 52;
  i += aimutil_put8(newpacket->hdr.oft.hdr2+i, 0x00);
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0000);
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0000);
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0000);
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0000);
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0000);
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0000);
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0000);
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0000);
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0000);
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0000);
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0000);

  memcpy(newpacket->data, msg, strlen(msg));

  newpacket->lock = 0;

  aim_tx_enqueue(sess, newpacket);

  return 0;
}

int aim_parse_outgoing_im_middle(struct aim_session_t *sess,
				 struct command_rx_struct *command)
{
  unsigned int i = 0, z;
  rxcallback_t userfunc = NULL;
  unsigned char cookie[8];
  int channel;
  struct aim_tlvlist_t *tlvlist;
  char sn[MAXSNLEN];
  unsigned short icbmflags = 0;
  unsigned char flag1 = 0, flag2 = 0;
  unsigned char *msgblock = NULL, *msg = NULL;

  i = 10;
  
  /* ICBM Cookie. */
  for (z=0; z<8; z++,i++)
    cookie[z] = command->data[i];

  /* Channel ID */
  channel = aimutil_get16(command->data+i);
  i += 2;

  if (channel != 0x01) {
    printf("faim: icbm: ICBM recieved on unsupported channel.  Ignoring. (chan = %04x)\n", channel);
    return 1;
  }

  strncpy(sn, command->data+i+1, (int) *(command->data+i));
  i += 1 + (int) *(command->data+i);

  tlvlist = aim_readtlvchain(command->data+i, command->commandlen-i);

  if (aim_gettlv(tlvlist, 0x0003, 1))
    icbmflags |= AIM_IMFLAGS_ACK;
  if (aim_gettlv(tlvlist, 0x0004, 1))
    icbmflags |= AIM_IMFLAGS_AWAY;

  if (aim_gettlv(tlvlist, 0x0002, 1)) {
    int j = 0;

    msgblock = aim_gettlv_str(tlvlist, 0x0002, 1);

    /* no, this really is correct.  I'm not high or anything either. */
    j += 2;
    j += 2 + aimutil_get16(msgblock+j);
    j += 2;
    
    j += 2; /* final block length */

    flag1 = aimutil_get16(msgblock);
    j += 2;
    flag2 = aimutil_get16(msgblock);
    j += 2;
    
    msg = msgblock+j;
  }

  if ((userfunc = aim_callhandler(command->conn, 0x0004, 0x0006)) || (i = 0))
    i = userfunc(sess, command, channel, sn, msg, icbmflags, flag1, flag2);
  
  if (msgblock)
    free(msgblock);
  aim_freetlvchain(&tlvlist);

  return 0;
}

/*
 * It can easily be said that parsing ICBMs is THE single
 * most difficult thing to do in the in AIM protocol.  In
 * fact, I think I just did say that.
 *
 * Below is the best damned solution I've come up with
 * over the past sixteen months of battling with it. This
 * can parse both away and normal messages from every client
 * I have access to.  Its not fast, its not clean.  But it works.
 *
 * We should also support at least minimal parsing of 
 * Channel 2, so that we can at least know the name of the
 * room we're invited to, but obviously can't attend...
 *
 */
int aim_parse_incoming_im_middle(struct aim_session_t *sess,
				 struct command_rx_struct *command)
{
  u_int i = 0,z;
  rxcallback_t userfunc = NULL;
  u_char cookie[8];
  int channel;
  struct aim_tlvlist_t *tlvlist;
  struct aim_userinfo_s userinfo;
  u_short wastebits;

  memset(&userinfo, 0x00, sizeof(struct aim_userinfo_s));
 
  i = 10; /* Skip SNAC header */

  /*
   * Read ICBM Cookie.  And throw away.
   */
  for (z=0; z<8; z++,i++)
    cookie[z] = command->data[i];
  
  /*
   * Channel ID.
   *
   * Channel 0x0001 is the message channel.  There are 
   * other channels for things called "rendevous"
   * which represent chat and some of the other new
   * features of AIM2/3/3.5. 
   *
   * Channel 0x0002 is the Rendevous channel, which
   * is where Chat Invitiations and various client-client
   * connection negotiations come from.
   * 
   */
  channel = aimutil_get16(command->data+i);
  i += 2;
  
  /*
   *
   */
  if ((channel != 0x01) && (channel != 0x02))
    {
      printf("faim: icbm: ICBM received on an unsupported channel.  Ignoring.\n (chan = %04x)", channel);
      return 1;
    }

  /*
   * Extract the standard user info block.
   *
   * Note that although this contains TLVs that appear contiguous
   * with the TLVs read below, they are two different pieces.  The
   * userinfo block contains the number of TLVs that contain user
   * information, the rest are not even though there is no seperation.
   * aim_extractuserinfo() returns the number of bytes used by the
   * userinfo tlvs, so you can start reading the rest of them right
   * afterward.  
   *
   * That also means that TLV types can be duplicated between the
   * userinfo block and the rest of the message, however there should
   * never be two TLVs of the same type in one block.
   * 
   */
  i += aim_extractuserinfo(command->data+i, &userinfo);
  
  /*
   * Read block of TLVs (not including the userinfo data).  All 
   * further data is derived from what is parsed here.
   */
  tlvlist = aim_readtlvchain(command->data+i, command->commandlen-i);

  /*
   * From here on, its depends on what channel we're on.
   */
  if (channel == 1)
    {
     u_int j = 0, y = 0, z = 0;
      char *msg = NULL;
      u_int icbmflags = 0;
      struct aim_tlv_t *msgblocktlv, *tmptlv;
      u_char *msgblock;
      u_short flag1,flag2;
           
      /*
       * Check Autoresponse status.  If it is an autoresponse,
       * it will contain a type 0x0004 TLV, with zero length.
       */
      if (aim_gettlv(tlvlist, 0x0004, 1))
	icbmflags |= AIM_IMFLAGS_AWAY;
      
      /*
       * Check Ack Request status.
       */
      if (aim_gettlv(tlvlist, 0x0003, 1))
	icbmflags |= AIM_IMFLAGS_ACK;
      
      /*
       * Message block.
       */
      msgblocktlv = aim_gettlv(tlvlist, 0x0002, 1);
      if (!msgblocktlv || !msgblocktlv->value) {
	printf("faim: icbm: major error! no message block TLV found!\n");
	aim_freetlvchain(&tlvlist);
	return 1;
      }
      
      /*
       * Extracting the message from the unknown cruft.
       * 
       * This is a bit messy, and I'm not really qualified,
       * even as the author, to comment on it.  At least
       * its not as bad as a while loop shooting into infinity.
       *
       * "Do you believe in magic?"
       *
       */
      msgblock = msgblocktlv->value;
      j = 0;
      
      wastebits = aimutil_get8(msgblock+j++);
      wastebits = aimutil_get8(msgblock+j++);
      
      y = aimutil_get16(msgblock+j);
      j += 2;
      for (z = 0; z < y; z++)
	wastebits = aimutil_get8(msgblock+j++);
      wastebits = aimutil_get8(msgblock+j++);
      wastebits = aimutil_get8(msgblock+j++);
      
      /* 
       * Message string length, including flag words.
       */
      i = aimutil_get16(msgblock+j);
      j += 2;
      
      /*
       * Flag words.
       *
       * Its rumored that these can kick in some funky
       * 16bit-wide char stuff that used to really kill
       * libfaim.  Hopefully the latter is no longer true.
       *
       * Though someone should investiagte the former.
       *
       */
      flag1 = aimutil_get16(msgblock+j);
      j += 2;
      flag2 = aimutil_get16(msgblock+j);
      j += 2;
      
      if (flag1 || flag2)
	printf("faim: icbm: **warning: encoding flags are being used! {%04x, %04x}\n", flag1, flag2);
      
      /* 
       * Message string. 
       */
      i -= 4;
      msg = (char *)malloc(i+1);
      memcpy(msg, msgblock+j, i);
      msg[i] = '\0';
      
      /*
       * Call client.
       */
      userfunc = aim_callhandler(command->conn, 0x0004, 0x0007);
      if (userfunc)
	i = userfunc(sess, command, channel, &userinfo, msg, icbmflags, flag1, flag2);
      else 
	i = 0;
      
      free(msg);
    }
  else if (channel == 0x0002)
    {	
      struct aim_tlv_t *block1;
      struct aim_tlvlist_t *list2;
      struct aim_tlv_t *tmptlv;
      unsigned short reqclass = 0;
      unsigned short status = 0;
      
      /*
       * There's another block of TLVs embedded in the type 5 here. 
       */
      block1 = aim_gettlv(tlvlist, 0x0005, 1);
      if (!block1) {
	printf("faim: no tlv 0x0005 in rendezvous transaction!\n");
	aim_freetlvchain(&tlvlist);
	return 1; /* major problem */
      }

      /*
       * First two bytes represent the status of the connection.
       *
       * 0 is a request, 2 is an accept
       */ 
      status = aimutil_get16(block1->value+0);
      
      /*
       * Next comes the cookie.  Should match the ICBM cookie.
       */
      if (memcmp(block1->value+2, cookie, 8) != 0) 
	printf("faim: rend: warning cookies don't match!\n");

      /*
       * The next 16bytes are a capability block so we can
       * identify what type of rendezvous this is.
       *
       * Thanks to Eric Warmenhoven <warmenhoven@linux.com> (of GAIM)
       * for pointing some of this out to me.  In fact, a lot of 
       * the client-to-client info comes from the work of the GAIM 
       * developers. Thanks!
       *
       * Read off one capability string and we should have it ID'd.
       * 
       */
      reqclass = aim_getcap(block1->value+2+8, 0x10);
      if (reqclass == 0x0000) {
	printf("faim: rend: no ID block\n");
	aim_freetlvchain(&tlvlist);
	return 1;
      }

      /* 
       * What follows may be TLVs or nothing, depending on the
       * purpose of the message.
       *
       * Ack packets for instance have nothing more to them.
       */
      list2 = aim_readtlvchain(block1->value+2+8+16, block1->length-2-8-16);
      
      if (!list2 || ((reqclass != AIM_CAPS_IMIMAGE) && !(aim_gettlv(list2, 0x2711, 1)))) {
	struct aim_msgcookie_t *cook;

	if ((cook = aim_uncachecookie(sess, cookie)) == NULL) {
	  printf("faim: non-data rendezvous thats not in cache!\n");
	  aim_freetlvchain(&list2);
	  aim_freetlvchain(&tlvlist);
	  return 1;
	}

	if (cook->type == AIM_CAPS_SENDFILE) {
	  struct aim_filetransfer_t *ft;

	  if (cook->data) {
	    struct aim_tlv_t *errortlv;
	    int errorcode = -1;

	    ft = (struct aim_filetransfer_t *)cook->data;

	    if ((errortlv = aim_gettlv(list2, 0x000b, 1))) {
	      errorcode = aimutil_get16(errortlv->value);
	    }
	    if (errorcode) {
	      printf("faim: transfer from %s (%s) for %s cancelled (error code %d)\n", ft->sender, ft->ip, ft->filename, errorcode);
	    } else if (status == 0x0002) { /* connection accepted */
	      printf("faim: transfer from %s (%s) for %s accepted\n", ft->sender, ft->ip, ft->filename);
	    }
	    free(cook->data);
	  } else {
	    printf("faim: not data attached to file transfer\n");
	  }
	} else if (cook->type == AIM_CAPS_VOICE) {
	  printf("faim: voice request cancelled\n");
	} else {
	  printf("faim: unknown cookie cache type %d\n", cook->type);
	}

	free(cook);
	if (list2)
	  aim_freetlvchain(&list2);
	aim_freetlvchain(&tlvlist);
	return 1;
      }

      /*
       * The rest of the handling depends on what type it is.
       */
      if (reqclass & AIM_CAPS_BUDDYICON) {

	/*
	 * Call client.
	 */
	userfunc = aim_callhandler(command->conn, 0x0004, 0x0007);
	if (userfunc || (i = 0))
	  i = userfunc(sess, 
		       command, 
		       channel, 
		       reqclass,
		       &userinfo);

      } else if (reqclass & AIM_CAPS_VOICE) {
	struct aim_msgcookie_t cachedcook;

	printf("faim: rend: voice!\n");

	memcpy(cachedcook.cookie, cookie, 8);
	cachedcook.type = AIM_CAPS_VOICE;
	cachedcook.data = NULL;
	if (aim_cachecookie(sess, &cachedcook) != 0)
	  printf("faim: ERROR caching message cookie\n");

	/* XXX: implement all this */

	/*
	 * Call client.
	 */
	userfunc = aim_callhandler(command->conn, 0x0004, 0x0007);
	if (userfunc || (i = 0)) {
	  i = userfunc(sess, 
		       command, 
		       channel, 
		       reqclass,
		       &userinfo);
	}
      } else if (reqclass & AIM_CAPS_IMIMAGE) {
	char ip[30];
	struct aim_msgcookie_t cachedcook;

	memset(ip, 0, sizeof(ip));
	
	if (aim_gettlv(list2, 0x0003, 1) && aim_gettlv(list2, 0x0003, 1)) {
	  struct aim_tlv_t *iptlv, *porttlv;
	  
	  iptlv = aim_gettlv(list2, 0x0003, 1);
	  porttlv = aim_gettlv(list2, 0x0005, 1);

	  snprintf(ip, sizeof(ip)-1, "%d.%d.%d.%d:%d", 
		  aimutil_get8(iptlv->value+0),
		  aimutil_get8(iptlv->value+1),
		  aimutil_get8(iptlv->value+2),
		  aimutil_get8(iptlv->value+3),
		  4443 /*aimutil_get16(porttlv->value)*/);
	}

	printf("faim: rend: directIM request from %s (%s)\n",
	       userinfo.sn,
	       ip);

#if 0
	{
	  struct aim_conn_t *newconn;
	  
	  newconn = aim_newconn(sess, AIM_CONN_TYPE_RENDEZVOUS, ip);
	  if (!newconn || (newconn->fd == -1)) { 
	    printf("could not connect to %s\n", ip);
	    perror("aim_newconn");
	    aim_conn_kill(sess, &newconn);
	  } else {
	    struct aim_directim_priv *priv;
	    priv = (struct aim_directim_priv *)malloc(sizeof(struct aim_directim_priv));
	    memcpy(priv->cookie, cookie, 8);
	    strncpy(priv->sn, userinfo.sn, MAXSNLEN);
	    newconn->priv = priv;
	    printf("faim: connected to peer (fd = %d)\n", newconn->fd);
	  }
	}
#endif
	
#if 0
	memcpy(cachedcook.cookie, cookie, 8);
	
	ft = malloc(sizeof(struct aim_filetransfer_t));
	strncpy(ft->sender, userinfo.sn, sizeof(ft->sender));
	strncpy(ft->ip, ip, sizeof(ft->ip));
	ft->filename = strdup(miscinfo->value+8);
	cachedcook.type = AIM_CAPS_SENDFILE; 
	cachedcook.data = ft;

	if (aim_cachecookie(sess, &cachedcook) != 0)
	  printf("faim: ERROR caching message cookie\n");
#endif	

	/*
	 * Call client.
	 */
	userfunc = aim_callhandler(command->conn, 0x0004, 0x0007);
	if (userfunc || (i = 0))
	  i = userfunc(sess, 
		       command, 
		       channel, 
		       reqclass,
		       &userinfo);

      } else if (reqclass & AIM_CAPS_CHAT) {
	struct aim_tlv_t *miscinfo;
	struct aim_chat_roominfo roominfo;
	char *msg=NULL,*encoding=NULL,*lang=NULL;

	miscinfo = aim_gettlv(list2, 0x2711, 1);
	aim_chat_readroominfo(miscinfo->value, &roominfo);
		  
	if (aim_gettlv(list2, 0x000c, 1))
	  msg = aim_gettlv_str(list2, 0x000c, 1);
	  
	if (aim_gettlv(list2, 0x000d, 1))
	  encoding = aim_gettlv_str(list2, 0x000d, 1);
	  
	if (aim_gettlv(list2, 0x000e, 1))
	  lang = aim_gettlv_str(list2, 0x000e, 1);
      
	/*
	 * Call client.
	 */
	userfunc = aim_callhandler(command->conn, 0x0004, 0x0007);
	if (userfunc || (i = 0))
	  i = userfunc(sess, 
		       command, 
		       channel, 
		       reqclass,
		       &userinfo, 
		       &roominfo, 
		       msg, 
		       encoding?encoding+1:NULL, 
		       lang?lang+1:NULL);
	  free(roominfo.name);
	  free(msg);
	  free(encoding);
	  free(lang);
      } else if (reqclass & AIM_CAPS_GETFILE) {

	/*
	 * Call client.
	 */
	userfunc = aim_callhandler(command->conn, 0x0004, 0x0007);
	if (userfunc || (i = 0))
	  i = userfunc(sess, 
		       command, 
		       channel, 
		       reqclass,
		       &userinfo);

      } else if (reqclass & AIM_CAPS_SENDFILE) {
	/*
	 * Call client.
	 */
	userfunc = aim_callhandler(command->conn, 0x0004, 0x0007);
	if (userfunc || (i = 0))
	  i = userfunc(sess, 
		       command, 
		       channel, 
		       reqclass,
		       &userinfo);
#if 0
	char ip[30];
	char *desc = NULL;
	struct aim_msgcookie_t cachedcook;
	struct aim_filetransfer_t *ft;
	struct aim_tlv_t *miscinfo;

	memset(ip, 0, sizeof(ip));
	
	miscinfo = aim_gettlv(list2, 0x2711, 1);

	if (aim_gettlv(list2, 0x0003, 1) && aim_gettlv(list2, 0x0003, 1)) {
	  struct aim_tlv_t *iptlv, *porttlv;
	  
	  iptlv = aim_gettlv(list2, 0x0003, 1);
	  porttlv = aim_gettlv(list2, 0x0005, 1);

	  snprintf(ip, sizeof(ip)-1, "%d.%d.%d.%d:%d", 
		  aimutil_get8(iptlv->value+0),
		  aimutil_get8(iptlv->value+1),
		  aimutil_get8(iptlv->value+2),
		  aimutil_get8(iptlv->value+3),
		  aimutil_get16(porttlv->value));
	}

	if (aim_gettlv(list2, 0x000c, 1)) {
	  desc = aim_gettlv_str(list2, 0x000c, 1);
	}

	printf("faim: rend: file transfer request from %s for %s: %s (%s)\n",
	       userinfo.sn,
	       miscinfo->value+8,
	       desc, 
	       ip);
	
	memcpy(cachedcook.cookie, cookie, 8);
	
	ft = malloc(sizeof(struct aim_filetransfer_t));
	strncpy(ft->sender, userinfo.sn, sizeof(ft->sender));
	strncpy(ft->ip, ip, sizeof(ft->ip));
	ft->filename = strdup(miscinfo->value+8);
	cachedcook.type = AIM_CAPS_SENDFILE; 
	cachedcook.data = ft;

	if (aim_cachecookie(sess, &cachedcook) != 0)
	  printf("faim: ERROR caching message cookie\n");
	
	
	aim_accepttransfer(sess, command->conn, ft->sender, cookie, AIM_CAPS_SENDFILE);

	free(desc);
#endif	
	i = 1;
      } else {
	printf("faim: rend: unknown rendezvous 0x%04x\n", reqclass);
      }

      aim_freetlvchain(&list2);
    }

  /*
   * Free up the TLV chain.
   */
  aim_freetlvchain(&tlvlist);
  

  return i;
}

u_long aim_accepttransfer(struct aim_session_t *sess,
			  struct aim_conn_t *conn, 
			  char *sender,
			  char *cookie,
			  unsigned short rendid)
{
  struct command_tx_struct *newpacket;
  int curbyte, i;

  if(!(newpacket = aim_tx_new(AIM_FRAMETYPE_OSCAR, 0x0002, conn, 10+8+2+1+strlen(sender)+4+2+8+16)))
    return -1;

  newpacket->lock = 1;

  curbyte = aim_putsnac(newpacket->data, 0x0004, 0x0006, 0x0000, sess->snac_nextid);
  for (i = 0; i < 8; i++)
    curbyte += aimutil_put8(newpacket->data+curbyte, cookie[i]);
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0002);
  curbyte += aimutil_put8(newpacket->data+curbyte, strlen(sender));
  curbyte += aimutil_putstr(newpacket->data+curbyte, sender, strlen(sender));
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0005);
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x001a);
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0002 /* accept */);
  for (i = 0; i < 8; i++)
    curbyte += aimutil_put8(newpacket->data+curbyte, cookie[i]);
  curbyte += aim_putcap(newpacket->data+curbyte, 0x10, rendid);

  newpacket->lock = 0;
  aim_tx_enqueue(sess, newpacket);

  return (sess->snac_nextid++);
}

/*
 * Possible codes:
 *    AIM_TRANSFER_DENY_NOTSUPPORTED -- "client does not support"
 *    AIM_TRANSFER_DENY_DECLINE -- "client has declined transfer"
 *    AIM_TRANSFER_DENY_NOTACCEPTING -- "client is not accepting transfers"
 * 
 */
u_long aim_denytransfer(struct aim_session_t *sess,
			struct aim_conn_t *conn, 
			char *sender,
			char *cookie, 
			unsigned short code)
{
  struct command_tx_struct *newpacket;
  int curbyte, i;

  if(!(newpacket = aim_tx_new(AIM_FRAMETYPE_OSCAR, 0x0002, conn, 10+8+2+1+strlen(sender)+6)))
    return -1;

  newpacket->lock = 1;

  curbyte = aim_putsnac(newpacket->data, 0x0004, 0x000b, 0x0000, sess->snac_nextid);
  for (i = 0; i < 8; i++)
    curbyte += aimutil_put8(newpacket->data+curbyte, cookie[i]);
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0002);
  curbyte += aimutil_put8(newpacket->data+curbyte, strlen(sender));
  curbyte += aimutil_putstr(newpacket->data+curbyte, sender, strlen(sender));
  curbyte += aim_puttlv_16(newpacket->data+curbyte, 0x0003, code);

  newpacket->lock = 0;
  aim_tx_enqueue(sess, newpacket);

  return (sess->snac_nextid++);
}

/*
 * Not real sure what this does, nor does anyone I've talk to.
 *
 * Didn't use to send it.  But now I think it might be a good
 * idea. 
 *
 */
u_long aim_seticbmparam(struct aim_session_t *sess,
			struct aim_conn_t *conn)
{
  struct command_tx_struct *newpacket;
  int curbyte;

  if(!(newpacket = aim_tx_new(AIM_FRAMETYPE_OSCAR, 0x0002, conn, 10+16)))
    return -1;

  newpacket->lock = 1;

  curbyte = aim_putsnac(newpacket->data, 0x0004, 0x0002, 0x0000, sess->snac_nextid);
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0000);
  curbyte += aimutil_put32(newpacket->data+curbyte, 0x00000003);
  curbyte += aimutil_put8(newpacket->data+curbyte,  0x1f);
  curbyte += aimutil_put8(newpacket->data+curbyte,  0x40);
  curbyte += aimutil_put8(newpacket->data+curbyte,  0x03);
  curbyte += aimutil_put8(newpacket->data+curbyte,  0xe7);
  curbyte += aimutil_put8(newpacket->data+curbyte,  0x03);
  curbyte += aimutil_put8(newpacket->data+curbyte,  0xe7);
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0000);
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0000);

  newpacket->lock = 0;
  aim_tx_enqueue(sess, newpacket);

  return (sess->snac_nextid++);
}

int aim_parse_msgerror_middle(struct aim_session_t *sess,
			      struct command_rx_struct *command)
{
  u_long snacid = 0x000000000;
  struct aim_snac_t *snac = NULL;
  int ret = 0;
  rxcallback_t userfunc = NULL;

  /*
   * Get SNAC from packet and look it up 
   * the list of unrepliedto/outstanding
   * SNACs.
   *
   * After its looked up, the SN that the
   * message should've gone to will be 
   * in the ->data element of the snac struct.
   *
   */
  snacid = aimutil_get32(command->data+6);
  snac = aim_remsnac(sess, snacid);

  if (!snac) {
    printf("faim: msgerr: got an ICBM-failed error on an unknown SNAC ID! (%08lx)\n", snacid);
  }

  /*
   * Call client.
   */
  userfunc = aim_callhandler(command->conn, 0x0004, 0x0001);
  if (userfunc)
    ret =  userfunc(sess, command, (snac)?snac->data:"(UNKNOWN)");
  else
    ret = 0;
  
  if (snac) {
    free(snac->data);
    free(snac);
  }

  return ret;
}


