/*
 *  aim_im.c
 *
 *  The routines for sending/receiving Instant Messages.
 *
 */

#define FAIM_INTERNAL
#include <aim.h>

/*
 * Takes a msghdr (and a length) and returns a client type
 * code.  Note that this is *only a guess* and has a low likelihood
 * of actually being accurate.
 *
 * Its based on experimental data, with the help of Eric Warmenhoven
 * who seems to have collected a wide variety of different AIM clients.
 *
 *
 * Heres the current collection:
 *  0501 0003 0101 0101 01       AOL Mobile Communicator, WinAIM 1.0.414
 *  0501 0003 0101 0201 01       WinAIM 2.0.847, 2.1.1187, 3.0.1464, 
 *                                      4.3.2229, 4.4.2286
 *  0501 0004 0101 0102 0101     WinAIM 4.1.2010, libfaim (right here)
 *  0501 0001 0101 01            AOL v6.0, CompuServe 2000 v6.0, any
 *                                      TOC client
 */
faim_export unsigned short aim_fingerprintclient(unsigned char *msghdr, int len)
{
  static const struct {
    unsigned short clientid;
    int len;
    unsigned char data[10];
  } fingerprints[] = {
    /* AOL Mobile Communicator, WinAIM 1.0.414 */
    { AIM_CLIENTTYPE_MC, 
      9, {0x05, 0x01, 0x00, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01}},

    /* WinAIM 2.0.847, 2.1.1187, 3.0.1464, 4.3.2229, 4.4.2286 */
    { AIM_CLIENTTYPE_WINAIM, 
      9, {0x05, 0x01, 0x00, 0x03, 0x01, 0x01, 0x02, 0x01, 0x01}},

    /* WinAIM 4.1.2010, libfaim */
    { AIM_CLIENTTYPE_WINAIM41,
      10, {0x05, 0x01, 0x00, 0x04, 0x01, 0x01, 0x01, 0x02, 0x01, 0x01}},

    /* AOL v6.0, CompuServe 2000 v6.0, any TOC client */
    { AIM_CLIENTTYPE_AOL_TOC,
      7, {0x05, 0x01, 0x00, 0x01, 0x01, 0x01, 0x01}},

    { 0, 0}
  };
  int i;

  if (!msghdr || (len <= 0))
    return 0;

  for (i = 0; fingerprints[i].len; i++) {
    if (fingerprints[i].len != len)
      continue;
    if (memcmp(fingerprints[i].data, msghdr, fingerprints[i].len) == 0)
      return fingerprints[i].clientid;
  }

  return AIM_CLIENTTYPE_UNKNOWN;
}

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
faim_export unsigned long aim_send_im(struct aim_session_t *sess,
				      struct aim_conn_t *conn, 
				      char *destsn, u_int flags, char *msg)
{   

  int curbyte,i;
  struct command_tx_struct *newpacket;
  
  if (strlen(msg) >= MAXMSGLEN)
    return -1;

  if (!(newpacket = aim_tx_new(sess, conn, AIM_FRAMETYPE_OSCAR, 0x0002, strlen(msg)+256)))
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
    curbyte += aimutil_put8(newpacket->data+curbyte, (u_char) rand());

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
   *
   * I don't know what these are...
   *
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

  aim_cachesnac(sess, 0x0004, 0x0006, 0x0000, destsn, strlen(destsn)+1);
  aim_cleansnacs(sess, 60); /* clean out all SNACs over 60sec old */

  return sess->snac_nextid;
}

static int outgoingim(struct aim_session_t *sess, aim_module_t *mod, struct command_rx_struct *rx, aim_modsnac_t *snac, unsigned char *data, int datalen)
{
  unsigned int i, ret = 0;
  rxcallback_t userfunc;
  unsigned char cookie[8];
  int channel;
  struct aim_tlvlist_t *tlvlist;
  char sn[MAXSNLEN];
  unsigned short icbmflags = 0;
  unsigned char flag1 = 0, flag2 = 0;
  unsigned char *msgblock = NULL, *msg = NULL;

  /* ICBM Cookie. */
  for (i = 0; i < 8; i++)
    cookie[i] = aimutil_get8(data+i);

  /* Channel ID */
  channel = aimutil_get16(data+i);
  i += 2;

  if (channel != 0x01) {
    faimdprintf(sess, 0, "icbm: ICBM recieved on unsupported channel.  Ignoring. (chan = %04x)\n", channel);
    return 1;
  }

  strncpy(sn, (char *) data+i+1, (int) *(data+i));
  i += 1 + (int) *(data+i);

  tlvlist = aim_readtlvchain(data+i, datalen-i);

  if (aim_gettlv(tlvlist, 0x0003, 1))
    icbmflags |= AIM_IMFLAGS_ACK;
  if (aim_gettlv(tlvlist, 0x0004, 1))
    icbmflags |= AIM_IMFLAGS_AWAY;

  if (aim_gettlv(tlvlist, 0x0002, 1)) {
    int j = 0;

    msgblock = (unsigned char *)aim_gettlv_str(tlvlist, 0x0002, 1);

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

  if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
    ret = userfunc(sess, rx, channel, sn, msg, icbmflags, flag1, flag2);
  
  if (msgblock)
    free(msgblock);
  aim_freetlvchain(&tlvlist);

  return ret;
}

static int incomingim_ch1(struct aim_session_t *sess, aim_module_t *mod,  struct command_rx_struct *rx, aim_modsnac_t *snac, unsigned short channel, struct aim_userinfo_s *userinfo, struct aim_tlvlist_t *tlvlist, unsigned char *cookie)
{
  rxcallback_t userfunc;
  int i, j = 0, y = 0, z = 0, ret = 0;
  char *msg = NULL;
  unsigned long icbmflags = 0;
  struct aim_tlv_t *msgblocktlv;
  unsigned char *msgblock;
  unsigned short flag1, flag2;
  int finlen = 0;
  unsigned char fingerprint[10];
  unsigned short wastebits;
           
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
  if (!msgblocktlv || !(msgblock = msgblocktlv->value)) {
    faimdprintf(sess, 0, "icbm: major error! no message block TLV found!\n");
    return 0;
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

  wastebits = aimutil_get8(msgblock+j++);
  wastebits = aimutil_get8(msgblock+j++);
      
  y = aimutil_get16(msgblock+j);
  j += 2;
  for (z = 0; z < y; z++)
    wastebits = aimutil_get8(msgblock+j++);
  wastebits = aimutil_get8(msgblock+j++);
  wastebits = aimutil_get8(msgblock+j++);

  finlen = j;
  if (finlen > sizeof(fingerprint))
    finlen = sizeof(fingerprint);
  memcpy(fingerprint, msgblocktlv->value, finlen);

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
    faimdprintf(sess, 0, "icbm: **warning: encoding flags are being used! {%04x, %04x}\n", flag1, flag2);

  /* 
   * Message string. 
   */
  i -= 4;
  msg = (char *)malloc(i+1);
  memcpy(msg, msgblock+j, i);
  msg[i] = '\0';

  if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
    ret = userfunc(sess, rx, channel, userinfo, msg, icbmflags, flag1, flag2, finlen, fingerprint);

  free(msg);

  return ret;
}

static int incomingim_ch2(struct aim_session_t *sess, aim_module_t *mod,  struct command_rx_struct *rx, aim_modsnac_t *snac, unsigned short channel, struct aim_userinfo_s *userinfo, struct aim_tlvlist_t *tlvlist, unsigned char *cookie)
{
  rxcallback_t userfunc;
  struct aim_tlv_t *block1;
  struct aim_tlvlist_t *list2;
  unsigned short reqclass = 0;
  unsigned short status = 0;
  int ret = 0;
      
  /*
   * There's another block of TLVs embedded in the type 5 here. 
   */
  block1 = aim_gettlv(tlvlist, 0x0005, 1);
  if (!block1 || !block1->value) {
    faimdprintf(sess, 0, "no tlv 0x0005 in rendezvous transaction!\n");
    return 0;
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
    faimdprintf(sess, 0, "rend: warning cookies don't match!\n");

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
  reqclass = aim_getcap(sess, block1->value+2+8, 0x10);
  if (reqclass == 0x0000) {
    faimdprintf(sess, 0, "rend: no ID block\n");
    return 0;
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
    int type;
	
    type = aim_msgcookie_gettype(reqclass); /* XXX: fix this shitty code */

    if ((cook = aim_checkcookie(sess, cookie, type)) == NULL) {
      faimdprintf(sess, 0, "non-data rendezvous thats not in cache %d/%s!\n", type, cookie);
      aim_freetlvchain(&list2);
      return 0;
    }

    if (cook->type == AIM_COOKIETYPE_OFTGET) {
      struct aim_filetransfer_priv *ft;

      if (cook->data) {
	int errorcode = -1; /* XXX shouldnt this be 0? */

	ft = (struct aim_filetransfer_priv *)cook->data;

	if(status != 0x0002) {
	  if (aim_gettlv(list2, 0x000b, 1))
	    errorcode = aim_gettlv16(list2, 0x000b, 1);

	  /* XXX this should make it up to the client, you know.. */
	  if (errorcode)
	    faimdprintf(sess, 0, "transfer from %s (%s) for %s cancelled (error code %d)\n", ft->sn, ft->ip, ft->fh.name, errorcode);
	}
      } else {
	faimdprintf(sess, 0, "no data attached to file transfer\n");
      }
    } else if (cook->type == AIM_CAPS_VOICE) {
      faimdprintf(sess, 0, "voice request cancelled\n");
    } else {
      faimdprintf(sess, 0, "unknown cookie cache type %d\n", cook->type);
    }
	
    aim_freetlvchain(&list2);

    return 1;
  }

  /*
   * The rest of the handling depends on what type it is.
   */
  if (reqclass & AIM_CAPS_BUDDYICON) {

    /* XXX implement this (its in ActiveBuddy...) */
    if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
      ret = userfunc(sess, rx, channel, reqclass, userinfo);

  } else if (reqclass & AIM_CAPS_VOICE) {
    struct aim_msgcookie_t *cachedcook;

    faimdprintf(sess, 0, "rend: voice!\n");

    if(!(cachedcook = (struct aim_msgcookie_t*)calloc(1, sizeof(struct aim_msgcookie_t)))) {
      aim_freetlvchain(&list2);
      return 0;
    }

    memcpy(cachedcook->cookie, cookie, 8);
    cachedcook->type = AIM_COOKIETYPE_OFTVOICE;
    cachedcook->data = NULL;

    if (aim_cachecookie(sess, cachedcook) == -1)
      faimdprintf(sess, 0, "ERROR caching message cookie\n");

    /* XXX: implement all this */

    if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype))) 
      ret = userfunc(sess, rx, channel, reqclass, &userinfo);

  } else if ((reqclass & AIM_CAPS_IMIMAGE) || 
	     (reqclass & AIM_CAPS_BUDDYICON)) {
    char ip[30];
    struct aim_directim_priv *priv;

    memset(ip, 0, sizeof(ip));
	
    if (aim_gettlv(list2, 0x0003, 1) && aim_gettlv(list2, 0x0005, 1)) {
      struct aim_tlv_t *iptlv, *porttlv;
	  
      iptlv = aim_gettlv(list2, 0x0003, 1);
      porttlv = aim_gettlv(list2, 0x0005, 1);

      snprintf(ip, 30, "%d.%d.%d.%d:%d", 
	       aimutil_get8(iptlv->value+0),
	       aimutil_get8(iptlv->value+1),
	       aimutil_get8(iptlv->value+2),
	       aimutil_get8(iptlv->value+3),
	       4443 /*aimutil_get16(porttlv->value)*/);
    }

    faimdprintf(sess, 0, "rend: directIM request from %s (%s)\n",
		userinfo->sn, ip);

    /* 
     * XXX: there are a couple of different request packets for
     *          different things 
     */

    priv = (struct aim_directim_priv *)calloc(1, sizeof(struct aim_directim_priv));
    memcpy(priv->ip, ip, sizeof(priv->ip));
    memcpy(priv->sn, userinfo->sn, sizeof(priv->sn));
    memcpy(priv->cookie, cookie, sizeof(priv->cookie));

    if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
      ret = userfunc(sess, rx, channel, reqclass, userinfo, priv);

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
      
    if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
      ret = userfunc(sess, rx, channel, reqclass, userinfo, &roominfo, msg, encoding?encoding+1:NULL, lang?lang+1:NULL);

    free(roominfo.name);
    free(msg);
    free(encoding);
    free(lang);

  } else if (reqclass & AIM_CAPS_GETFILE) {
    char ip[30];
    struct aim_msgcookie_t *cachedcook;
    struct aim_tlv_t *miscinfo;
    struct aim_tlv_t *iptlv, *porttlv;

    memset(ip, 0, 30);

    if (!(cachedcook = calloc(1, sizeof(struct aim_msgcookie_t)))) {
      aim_freetlvchain(&list2);
      return 0;
    }

    if (!(miscinfo = aim_gettlv(list2, 0x2711, 1)) || 
	!(iptlv = aim_gettlv(list2, 0x0003, 1)) || 
	!(porttlv = aim_gettlv(list2, 0x0005, 1))) {
      faimdprintf(sess, 0, "rend: badly damaged file get request from %s...\n", userinfo->sn);
      aim_cookie_free(sess, cachedcook);
      aim_freetlvchain(&list2);
      return 0;
    }

    snprintf(ip, 30, "%d.%d.%d.%d:%d",
	     aimutil_get8(iptlv->value+0),
	     aimutil_get8(iptlv->value+1),
	     aimutil_get8(iptlv->value+2),
	     aimutil_get8(iptlv->value+3),
	     aimutil_get16(porttlv->value));

    faimdprintf(sess, 0, "rend: file get request from %s (%s)\n", userinfo->sn, ip);
    if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
      ret = userfunc(sess, rx, channel, reqclass, userinfo, ip, cookie);

  } else if (reqclass & AIM_CAPS_SENDFILE) {
#if 0
    char ip[30];
    struct aim_msgcookie_t *cachedcook;
    struct aim_tlv_t *miscinfo;
    struct aim_tlv_t *iptlv, *porttlv;

    memset(ip, 0, 30);

    if (!(cachedcook = calloc(1, sizeof(struct aim_msgcookie_t)))) {
      aim_freetlvchain(&list2);
      return 0;
    }

    if (!(miscinfo = aim_gettlv(list2, 0x2711, 1)) || 
	!(iptlv = aim_gettlv(list2, 0x0003, 1)) || 
	!(porttlv = aim_gettlv(list2, 0x0005, 1))) {
      faimdprintf(sess, 0, "rend: badly damaged file get request from %s...\n", userinfo->sn);
      aim_cookie_free(sess, cachedcook);
      aim_freetlvchain(&list2);
      return 0;
    }

    snprintf(ip, 30, "%d.%d.%d.%d:%d",
	     aimutil_get8(iptlv->value+0),
	     aimutil_get8(iptlv->value+1),
	     aimutil_get8(iptlv->value+2),
	     aimutil_get8(iptlv->value+3),
	     aimutil_get16(porttlv->value));

    if (aim_gettlv(list2, 0x000c, 1))
      desc = aim_gettlv_str(list2, 0x000c, 1);

    faimdprintf(sess, 0, "rend: file transfer request from %s for %s: %s (%s)\n",
		userinfo->sn, miscinfo->value+8,
		desc, ip);
	
    memcpy(cachedcook->cookie, cookie, 8);
	
    ft = malloc(sizeof(struct aim_filetransfer_priv));
    strncpy(ft->sn, userinfo.sn, sizeof(ft->sn));
    strncpy(ft->ip, ip, sizeof(ft->ip));
    strncpy(ft->fh.name, miscinfo->value+8, sizeof(ft->fh.name));
    cachedcook->type = AIM_COOKIETYPE_OFTSEND;
    cachedcook->data = ft;

    if (aim_cachecookie(sess, cachedcook) == -1)
      faimdprintf(sess, 0, "ERROR caching message cookie\n");

    aim_accepttransfer(sess, rx->conn, ft->sn, cookie, AIM_CAPS_SENDFILE);
	
    if (desc)
      free(desc);

    if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
      ret = userfunc(sess, rx, channel, reqclass, userinfo);

#endif	
  } else
    faimdprintf(sess, 0, "rend: unknown rendezvous 0x%04x\n", reqclass);

  aim_freetlvchain(&list2);

  return ret;
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
 */
static int incomingim(struct aim_session_t *sess, aim_module_t *mod, struct command_rx_struct *rx, aim_modsnac_t *snac, unsigned char *data, int datalen)
{
  int i, ret = 0;
  unsigned char cookie[8];
  int channel;
  struct aim_tlvlist_t *tlvlist;
  struct aim_userinfo_s userinfo;

  memset(&userinfo, 0x00, sizeof(struct aim_userinfo_s));
 
  /*
   * Read ICBM Cookie.  And throw away.
   */
  for (i = 0; i < 8; i++)
    cookie[i] = aimutil_get8(data+i);
  
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
  channel = aimutil_get16(data+i);
  i += 2;
  
  /*
   *
   */
  if ((channel != 0x01) && (channel != 0x02)) {
    faimdprintf(sess, 0, "icbm: ICBM received on an unsupported channel.  Ignoring.\n (chan = %04x)", channel);
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
  i += aim_extractuserinfo(sess, data+i, &userinfo);
  
  /*
   * Read block of TLVs (not including the userinfo data).  All 
   * further data is derived from what is parsed here.
   */
  tlvlist = aim_readtlvchain(data+i, datalen-i);

  /*
   * From here on, its depends on what channel we're on.
   */
  if (channel == 1)
    ret = incomingim_ch1(sess, mod, rx, snac, channel, &userinfo, tlvlist, cookie);
  else if (channel == 0x0002)
    ret = incomingim_ch2(sess, mod, rx, snac, channel, &userinfo, tlvlist, cookie);

  /*
   * Free up the TLV chain.
   */
  aim_freetlvchain(&tlvlist);

  return ret;
}

/*
 * Possible codes:
 *    AIM_TRANSFER_DENY_NOTSUPPORTED -- "client does not support"
 *    AIM_TRANSFER_DENY_DECLINE -- "client has declined transfer"
 *    AIM_TRANSFER_DENY_NOTACCEPTING -- "client is not accepting transfers"
 * 
 */
faim_export unsigned long aim_denytransfer(struct aim_session_t *sess,
					   struct aim_conn_t *conn, 
					   char *sender,
					   char *cookie, 
					   unsigned short code)
{
  struct command_tx_struct *newpacket;
  int curbyte, i;

  if(!(newpacket = aim_tx_new(sess, conn, AIM_FRAMETYPE_OSCAR, 0x0002, 10+8+2+1+strlen(sender)+6)))
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
faim_export unsigned long aim_seticbmparam(struct aim_session_t *sess,
					   struct aim_conn_t *conn)
{
  struct command_tx_struct *newpacket;
  int curbyte;

  if(!(newpacket = aim_tx_new(sess, conn, AIM_FRAMETYPE_OSCAR, 0x0002, 10+16)))
    return -1;

  newpacket->lock = 1;

  curbyte = aim_putsnac(newpacket->data, 0x0004, 0x0002, 0x0000, sess->snac_nextid);
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0000);
  curbyte += aimutil_put32(newpacket->data+curbyte, 0x00000003);
  curbyte += aimutil_put16(newpacket->data+curbyte,  0x1f40);
  curbyte += aimutil_put16(newpacket->data+curbyte,  0x03e7);
  curbyte += aimutil_put16(newpacket->data+curbyte,  0x03e7);
  curbyte += aimutil_put32(newpacket->data+curbyte, 0x00000000);

  newpacket->lock = 0;
  aim_tx_enqueue(sess, newpacket);

  return (sess->snac_nextid++);
}

static int paraminfo(struct aim_session_t *sess, aim_module_t *mod, struct command_rx_struct *rx, aim_modsnac_t *snac, unsigned char *data, int datalen)
{
  unsigned long defflags, minmsginterval;
  unsigned short maxicbmlen, maxsenderwarn, maxrecverwarn, maxchannel;
  rxcallback_t userfunc;
  int i = 0;

  maxchannel = aimutil_get16(data+i);
  i += 2;

  defflags = aimutil_get32(data+i);
  i += 4;

  maxicbmlen = aimutil_get16(data+i);
  i += 2;

  maxsenderwarn = aimutil_get16(data+i);
  i += 2;

  maxrecverwarn = aimutil_get16(data+i);
  i += 2;

  minmsginterval = aimutil_get32(data+i);
  i += 4;

  if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
    return userfunc(sess, rx, maxchannel, defflags, maxicbmlen, maxsenderwarn, maxrecverwarn, minmsginterval);

  return 0;
}

static int missedcall(struct aim_session_t *sess, aim_module_t *mod, struct command_rx_struct *rx, aim_modsnac_t *snac, unsigned char *data, int datalen)
{
  int i = 0;
  rxcallback_t userfunc;
  unsigned short channel, nummissed, reason;
  struct aim_userinfo_s userinfo;
 
  /*
   * XXX: supposedly, this entire packet can repeat as many times
   * as necessary. Should implement that.
   */

  /*
   * Channel ID.
   */
  channel = aimutil_get16(data+i);
  i += 2;
  
  /*
   * Extract the standard user info block.
   */
  i += aim_extractuserinfo(sess, data+i, &userinfo);
  
  nummissed = aimutil_get16(data+i);
  i += 2;
  
  reason = aimutil_get16(data+i);
  i += 2;

  if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
    return userfunc(sess, rx, channel, &userinfo, nummissed, reason);
  
  return 0;
}

static int msgack(struct aim_session_t *sess, aim_module_t *mod, struct command_rx_struct *rx, aim_modsnac_t *snac, unsigned char *data, int datalen)
{
  rxcallback_t userfunc;
  char sn[MAXSNLEN];
  unsigned char ck[8];
  unsigned short type;
  int i = 0;
  unsigned char snlen;

  memcpy(ck, data, 8);
  i += 8;

  type = aimutil_get16(data+i);
  i += 2;

  snlen = aimutil_get8(data+i);
  i++;

  memset(sn, 0, sizeof(sn));
  strncpy(sn, (char *)data+i, snlen);

  if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
    return userfunc(sess, rx, type, sn);

  return 0;
}

static int snachandler(struct aim_session_t *sess, aim_module_t *mod, struct command_rx_struct *rx, aim_modsnac_t *snac, unsigned char *data, int datalen)
{

  if (snac->subtype == 0x0005)
    return paraminfo(sess, mod, rx, snac, data, datalen);
  else if (snac->subtype == 0x0006)
    return outgoingim(sess, mod, rx, snac, data, datalen);
  else if (snac->subtype == 0x0007)
    return incomingim(sess, mod, rx, snac, data, datalen);
  else if (snac->subtype == 0x000a)
    return missedcall(sess, mod, rx, snac, data, datalen);
  else if (snac->subtype == 0x000c)
    return msgack(sess, mod, rx, snac, data, datalen);

  return 0;
}

faim_internal int msg_modfirst(struct aim_session_t *sess, aim_module_t *mod)
{

  mod->family = 0x0004;
  mod->version = 0x0000;
  mod->flags = 0;
  strncpy(mod->name, "messaging", sizeof(mod->name));
  mod->snachandler = snachandler;

  return 0;
}
