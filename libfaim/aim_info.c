/*
 * aim_info.c
 *
 * The functions here are responsible for requesting and parsing information-
 * gathering SNACs.  
 *
 */


#include <faim/aim.h>

struct aim_priv_inforeq {
  char sn[MAXSNLEN+1];
  unsigned short infotype;
};

faim_export unsigned long aim_getinfo(struct aim_session_t *sess,
				      struct aim_conn_t *conn, 
				      const char *sn,
				      unsigned short infotype)
{
  struct command_tx_struct *newpacket;
  struct aim_priv_inforeq privdata;
  int i = 0;

  if (!sess || !conn || !sn)
    return 0;

  if (!(newpacket = aim_tx_new(AIM_FRAMETYPE_OSCAR, 0x0002, conn, 12+1+strlen(sn))))
    return -1;

  newpacket->lock = 1;

  i = aim_putsnac(newpacket->data, 0x0002, 0x0005, 0x0000, sess->snac_nextid);

  i += aimutil_put16(newpacket->data+i, infotype);
  i += aimutil_put8(newpacket->data+i, strlen(sn));
  i += aimutil_putstr(newpacket->data+i, sn, strlen(sn));

  newpacket->lock = 0;
  aim_tx_enqueue(sess, newpacket);

  strncpy(privdata.sn, sn, sizeof(privdata.sn));
  privdata.infotype = infotype;
  aim_cachesnac(sess, 0x0002, 0x0005, 0x0000, &privdata, sizeof(struct aim_priv_inforeq));

  return sess->snac_nextid;
}

faim_internal int aim_parse_locateerr(struct aim_session_t *sess,
				      struct command_rx_struct *command)
{
  u_long snacid = 0x000000000;
  struct aim_snac_t *snac = NULL;
  int ret = 0;
  rxcallback_t userfunc = NULL;
  char *dest;
  unsigned short reason = 0;

  /*
   * Get SNAC from packet and look it up 
   * the list of unrepliedto/outstanding
   * SNACs.
   *
   */
  snacid = aimutil_get32(command->data+6);
  snac = aim_remsnac(sess, snacid);

  if (!snac) {
    printf("faim: locerr: got an locate-failed error on an unknown SNAC ID! (%08lx)\n", snacid);
    dest = NULL;
  } else
    dest = snac->data;

  reason = aimutil_get16(command->data+10);

  /*
   * Call client.
   */
  userfunc = aim_callhandler(command->conn, 0x0002, 0x0001);
  if (userfunc)
    ret =  userfunc(sess, command, dest, reason);
  else
    ret = 0;
  
  if (snac) {
    free(snac->data);
    free(snac);
  }

  return ret;
}

/*
 * Capability blocks.  
 */
u_char aim_caps[8][16] = {
  
  /* Buddy icon */
  {0x09, 0x46, 0x13, 0x46, 0x4c, 0x7f, 0x11, 0xd1, 
   0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00},
  
  /* Voice */
  {0x09, 0x46, 0x13, 0x41, 0x4c, 0x7f, 0x11, 0xd1, 
   0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00},
  
  /* IM image */
  {0x09, 0x46, 0x13, 0x45, 0x4c, 0x7f, 0x11, 0xd1, 
   0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00},
  
  /* Chat */
  {0x74, 0x8f, 0x24, 0x20, 0x62, 0x87, 0x11, 0xd1, 
   0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00},
  
  /* Get file */
  {0x09, 0x46, 0x13, 0x48, 0x4c, 0x7f, 0x11, 0xd1,
   0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00},
  
  /* Send file */
  {0x09, 0x46, 0x13, 0x43, 0x4c, 0x7f, 0x11, 0xd1, 
   0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00},

  /* Saves stock portfolios */
  {0x09, 0x46, 0x13, 0x47, 0x4c, 0x7f, 0x11, 0xd1,
   0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00},

  /* Games */
  {0x09, 0x46, 0x13, 0x4a, 0x4c, 0x7f, 0x11, 0xd1,
   0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00},
};

faim_internal unsigned short aim_getcap(unsigned char *capblock, int buflen)
{
  u_short ret = 0;
  int y;
  int offset = 0;
  int identified;

  while (offset < buflen) {
    identified = 0;
    for(y=0; y < (sizeof(aim_caps)/0x10); y++) {
      if (memcmp(&aim_caps[y], capblock+offset, 0x10) == 0) {
	switch(y) {
	case 0: ret |= AIM_CAPS_BUDDYICON; identified++; break;
	case 1: ret |= AIM_CAPS_VOICE; identified++; break;
	case 2: ret |= AIM_CAPS_IMIMAGE; identified++; break;
	case 3: ret |= AIM_CAPS_CHAT; identified++; break;
	case 4: ret |= AIM_CAPS_GETFILE; identified++; break;
	case 5: ret |= AIM_CAPS_SENDFILE; identified++; break;
	case 6: ret |= AIM_CAPS_GAMES; identified++; break;
	case 7: ret |= AIM_CAPS_SAVESTOCKS; identified++; break;
	}
      }
    }
    if (!identified) {
      faimdprintf(1, "faim: unknown capability ");
      for (y = 0; y < 0x10; y++)
        faimdprintf(2, "%02x ", capblock[offset+y]);
      faimdprintf(1, "\n");
      ret |= 0xff00;
    }

    offset += 0x10;
  } 
  return ret;
}

faim_internal int aim_putcap(unsigned char *capblock, int buflen, u_short caps)
{
  int offset = 0;

  if (!capblock)
    return -1;

  if ((caps & AIM_CAPS_BUDDYICON) && (offset < buflen)) {
    memcpy(capblock+offset, aim_caps[0], sizeof(aim_caps[0]));
    offset += sizeof(aim_caps[1]);
  }
  if ((caps & AIM_CAPS_VOICE) && (offset < buflen)) {
    memcpy(capblock+offset, aim_caps[1], sizeof(aim_caps[1]));
    offset += sizeof(aim_caps[1]);
  }
  if ((caps & AIM_CAPS_IMIMAGE) && (offset < buflen)) {
    memcpy(capblock+offset, aim_caps[2], sizeof(aim_caps[2]));
    offset += sizeof(aim_caps[2]);
  }
  if ((caps & AIM_CAPS_CHAT) && (offset < buflen)) {
    memcpy(capblock+offset, aim_caps[3], sizeof(aim_caps[3]));
    offset += sizeof(aim_caps[3]);
  }
  if ((caps & AIM_CAPS_GETFILE) && (offset < buflen)) {
    memcpy(capblock+offset, aim_caps[4], sizeof(aim_caps[4]));
    offset += sizeof(aim_caps[4]);
  }
  if ((caps & AIM_CAPS_SENDFILE) && (offset < buflen)) {
    memcpy(capblock+offset, aim_caps[5], sizeof(aim_caps[5]));
    offset += sizeof(aim_caps[5]);
  }
  if ((caps & AIM_CAPS_GAMES) && (offset < buflen)) {
    memcpy(capblock+offset, aim_caps[6], sizeof(aim_caps[6]));
    offset += sizeof(aim_caps[6]);
  }
  if ((caps & AIM_CAPS_SAVESTOCKS) && (offset < buflen)) {
    memcpy(capblock+offset, aim_caps[7], sizeof(aim_caps[7]));
    offset += sizeof(aim_caps[7]);
  }

  return offset;
}

/*
 * AIM is fairly regular about providing user info.  This
 * is a generic routine to extract it in its standard form.
 */
faim_internal int aim_extractuserinfo(u_char *buf, struct aim_userinfo_s *outinfo)
{
  int i = 0;
  int tlvcnt = 0;
  int curtlv = 0;
  int tlv1 = 0;
  u_short curtype;
  int lastvalid;


  if (!buf || !outinfo)
    return -1;

  /* Clear out old data first */
  memset(outinfo, 0x00, sizeof(struct aim_userinfo_s));

  /*
   * Screen name.    Stored as an unterminated string prepended
   *                 with an unsigned byte containing its length.
   */
  if (buf[i] < MAXSNLEN) {
    memcpy(outinfo->sn, &(buf[i+1]), buf[i]);
    outinfo->sn[(int)buf[i]] = '\0';
  } else {
    memcpy(outinfo->sn, &(buf[i+1]), MAXSNLEN-1);
    outinfo->sn[MAXSNLEN] = '\0';
  }
  i = 1 + (int)buf[i];

  /*
   * Warning Level.  Stored as an unsigned short.
   */
  outinfo->warnlevel = aimutil_get16(&buf[i]);
  i += 2;

  /*
   * TLV Count.      Unsigned short representing the number of 
   *                 Type-Length-Value triples that follow.
   */
  tlvcnt = aimutil_get16(&buf[i]);
  i += 2;

  /* 
   * Parse out the Type-Length-Value triples as they're found.
   */
  while (curtlv < tlvcnt) {
    lastvalid = 1;
    curtype = aimutil_get16(&buf[i]);
    switch (curtype) {
      /*
       * Type = 0x0000: Invalid
       *
       * AOL has been trying to throw these in just to break us.
       * They're real nice guys over there at AOL.  
       *
       * Just skip the two zero bytes and continue on. (This doesn't
       * count towards tlvcnt!)
       */
    case 0x0000:
      lastvalid = 0;
      i += 2;
      break;

      /*
       * Type = 0x0001: User flags
       * 
       * Specified as any of the following bitwise ORed together:
       *      0x0001  Trial (user less than 60days)
       *      0x0002  Unknown bit 2
       *      0x0004  AOL Main Service user
       *      0x0008  Unknown bit 4
       *      0x0010  Free (AIM) user 
       *      0x0020  Away
       *
       * In some odd cases, we can end up with more
       * than one of these.  We only want the first,
       * as the others may not be something we want.
       *
       */
    case 0x0001:
      if (tlv1) /* use only the first */
	break;
      outinfo->flags = aimutil_get16(&buf[i+4]);
      tlv1++;
      break;
      
      /*
       * Type = 0x0002: Member-Since date. 
       *
       * The time/date that the user originally
       * registered for the service, stored in 
       * time_t format
       */
    case 0x0002: 
      outinfo->membersince = aimutil_get32(&buf[i+4]);
      break;
      
      /*
       * Type = 0x0003: On-Since date.
       *
       * The time/date that the user started 
       * their current session, stored in time_t
       * format.
       */
    case 0x0003:
      outinfo->onlinesince = aimutil_get32(&buf[i+4]);
      break;
      
      /*
       * Type = 0x0004: Idle time.
       *
       * Number of seconds since the user
       * actively used the service.
       */
    case 0x0004:
      outinfo->idletime = aimutil_get16(&buf[i+4]);
      break;
      
      /*
       * Type = 0x0006: ICQ Online Status
       *
       * ICQ's Away/DND/etc "enriched" status
       * Some decoding of values done by Scott <darkagl@pcnet.com>
       */
    case 0x0006:
      outinfo->icqinfo.status = aimutil_get16(buf+i+2+2+2);
      break;


      /*
       * Type = 0x000a
       *
       * ICQ User IP Address.
       * Ahh, the joy of ICQ security.
       */
    case 0x000a:
      outinfo->icqinfo.ipaddr = aimutil_get32(&buf[i+4]);
      break;

      /* Type = 0x000c
       *
       * random crap containing the IP address,
       * apparently a port number, and some Other Stuff.
       *
       */
    case 0x000c:
      memcpy(outinfo->icqinfo.crap, &buf[i+4], 0x25);
      break;

      /*
       * Type = 0x000d
       *
       * Capability information.  Not real sure of
       * actual decoding.  See comment on aim_bos_setprofile()
       * in aim_misc.c about the capability block, its the same.
       *
       */
    case 0x000d:
      {
	int len;
	len = aimutil_get16(buf+i+2);
	if (!len)
	  break;
	
	outinfo->capabilities = aim_getcap(buf+i+4, len);
	if (outinfo->capabilities & 0xff00)
          faimdprintf(2, "%s\n", outinfo->sn);
      }
      break;
      
      /*
       * Type = 0x000e
       *
       * Unknown.  Always of zero length, and always only
       * on AOL users.
       *
       * Ignore.
       *
       */
    case 0x000e:
      break;
      
      /*
       * Type = 0x000f: Session Length. (AIM)
       * Type = 0x0010: Session Length. (AOL)
       *
       * The duration, in seconds, of the user's
       * current session.
       *
       * Which TLV type this comes in depends
       * on the service the user is using (AIM or AOL).
       *
       */
    case 0x000f:
    case 0x0010:
      outinfo->sessionlen = aimutil_get32(&buf[i+4]);
      break;
      
      /*
       * Reaching here indicates that either AOL has
       * added yet another TLV for us to deal with, 
       * or the parsing has gone Terribly Wrong.
       *
       * Either way, inform the owner and attempt
       * recovery.
       *
       */
    default:
      {
	int len,z = 0, y = 0, x = 0;
	char tmpstr[80];
	printf("faim: userinfo: **warning: unexpected TLV:\n");
	printf("faim: userinfo:   sn    =%s\n", outinfo->sn);
	printf("faim: userinfo:   curtlv=0x%04x\n", curtlv);
	printf("faim: userinfo:   type  =0x%04x\n",aimutil_get16(&buf[i]));
	printf("faim: userinfo:   length=0x%04x\n", len = aimutil_get16(&buf[i+2]));
	printf("faim: userinfo:   data: \n");
	while (z<len)
	  {
	    x = sprintf(tmpstr, "faim: userinfo:      ");
	    for (y = 0; y < 8; y++)
	      {
		if (z<len)
		  {
		    sprintf(tmpstr+x, "%02x ", buf[i+4+z]);
		    z++;
		    x += 3;
		  }
		else
		  break;
	      }
	    printf("%s\n", tmpstr);
	  }
      }
      break;
    }  
    /*
     * No matter what, TLV triplets should always look like this:
     *
     *   u_short type;
     *   u_short length;
     *   u_char  data[length];
     *
     */
    if (lastvalid) {
      i += (2 + 2 + aimutil_get16(&buf[i+2])); 	   
      curtlv++;
    }
  }
  
  return i;
}

/*
 * Oncoming Buddy notifications contain a subset of the
 * user information structure.  Its close enough to run
 * through aim_extractuserinfo() however.
 *
 */
faim_internal int aim_parse_oncoming_middle(struct aim_session_t *sess,
					    struct command_rx_struct *command)
{
  struct aim_userinfo_s userinfo;
  u_int i = 0;
  rxcallback_t userfunc=NULL;

  i = 10;
  i += aim_extractuserinfo(command->data+i, &userinfo);

  userfunc = aim_callhandler(command->conn, AIM_CB_FAM_BUD, AIM_CB_BUD_ONCOMING);
  if (userfunc)
    i = userfunc(sess, command, &userinfo);

  return 1;
}

/*
 * Offgoing Buddy notifications contain no useful
 * information other than the name it applies to.
 *
 */
faim_internal int aim_parse_offgoing_middle(struct aim_session_t *sess,
					    struct command_rx_struct *command)
{
  char sn[MAXSNLEN+1];
  u_int i = 0;
  rxcallback_t userfunc=NULL;

  strncpy(sn, (char *)command->data+11, (int)command->data[10]);
  sn[(int)command->data[10]] = '\0';

  userfunc = aim_callhandler(command->conn, AIM_CB_FAM_BUD, AIM_CB_BUD_OFFGOING);
  if (userfunc)
    i = userfunc(sess, command, sn);

  return 1;
}

/*
 * This parses the user info stuff out all nice and pretty then calls 
 * the higher-level callback (in the user app).
 *
 */
faim_internal int aim_parse_userinfo_middle(struct aim_session_t *sess,
					    struct command_rx_struct *command)
{
  struct aim_userinfo_s userinfo;
  char *text_encoding = NULL;
  char *text = NULL;
  u_int i = 0;
  rxcallback_t userfunc=NULL;
  struct aim_tlvlist_t *tlvlist;
  struct aim_snac_t *origsnac = NULL;
  u_long snacid;
  struct aim_priv_inforeq *inforeq;
  
  snacid = aimutil_get32(&command->data[6]);
  origsnac = aim_remsnac(sess, snacid);

  if (!origsnac || !origsnac->data) {
    printf("faim: parse_userinfo_middle: major problem: no snac stored!\n");
    return 1;
  }

  inforeq = (struct aim_priv_inforeq *)origsnac->data;

  switch (inforeq->infotype) {
  case AIM_GETINFO_GENERALINFO:
  case AIM_GETINFO_AWAYMESSAGE:
    i = 10;

    /*
     * extractuserinfo will give us the basic metaTLV information
     */
    i += aim_extractuserinfo(command->data+i, &userinfo);
  
    /*
     * However, in this command, there's usually more TLVs following...
     */ 
    tlvlist = aim_readtlvchain(command->data+i, command->commandlen-i);

    /* 
     * Depending on what informational text was requested, different
     * TLVs will appear here.
     *
     * Profile will be 1 and 2, away message will be 3 and 4.
     */
    if (aim_gettlv(tlvlist, 0x0001, 1)) {
      text_encoding = aim_gettlv_str(tlvlist, 0x0001, 1);
      text = aim_gettlv_str(tlvlist, 0x0002, 1);
    } else if (aim_gettlv(tlvlist, 0x0003, 1)) {
      text_encoding = aim_gettlv_str(tlvlist, 0x0003, 1);
      text = aim_gettlv_str(tlvlist, 0x0004, 1);
    }

    userfunc = aim_callhandler(command->conn, AIM_CB_FAM_LOC, AIM_CB_LOC_USERINFO);
    if (userfunc) {
      i = userfunc(sess,
		   command, 
		   &userinfo, 
		   text_encoding, 
		   text,
		   inforeq->infotype); 
    }
  
    free(text_encoding);
    free(text);
    aim_freetlvchain(&tlvlist);
    break;
  default:
    printf("faim: parse_userinfo_middle: unknown infotype in request! (0x%04x)\n", inforeq->infotype);
    break;
  }

  if (origsnac) {
    if (origsnac->data)
      free(origsnac->data);
    free(origsnac);
  }

  return 1;
}

/*
 * Inverse of aim_extractuserinfo()
 */
faim_internal int aim_putuserinfo(u_char *buf, int buflen, struct aim_userinfo_s *info)
{
  int i = 0, numtlv = 0;
  struct aim_tlvlist_t *tlvlist = NULL;

  if (!buf || !info)
    return 0;

  i += aimutil_put8(buf+i, strlen(info->sn));
  i += aimutil_putstr(buf+i, info->sn, strlen(info->sn));

  i += aimutil_put16(buf+i, info->warnlevel);


  aim_addtlvtochain16(&tlvlist, 0x0001, info->flags);
  numtlv++;

  aim_addtlvtochain32(&tlvlist, 0x0002, info->membersince);
  numtlv++;

  aim_addtlvtochain32(&tlvlist, 0x0003, info->onlinesince);
  numtlv++;

  aim_addtlvtochain16(&tlvlist, 0x0004, info->idletime);
  numtlv++;

#if ICQ_OSCAR_SUPPORT
  if(atoi(info->sn) != 0) {
    aim_addtlvtochain16(&tlvlist, 0x0006, info->icqinfo.status);
    aim_addtlvtochain32(&tlvlist, 0x000a, info->icqinfo.ipaddr);
  }
#endif

  aim_addtlvtochain_caps(&tlvlist, 0x000d, info->capabilities);
  numtlv++;

  aim_addtlvtochain32(&tlvlist, (unsigned short)((info->flags)&AIM_FLAG_AOL?0x0010:0x000f), info->sessionlen);
  numtlv++;

  i += aimutil_put16(buf+i, numtlv); /* tlvcount */
  i += aim_writetlvchain(buf+i, buflen-i, &tlvlist); /* tlvs */
  aim_freetlvchain(&tlvlist);

  return i;
}

faim_export int aim_sendbuddyoncoming(struct aim_session_t *sess, struct aim_conn_t *conn, struct aim_userinfo_s *info)
{
  struct command_tx_struct *tx;
  int i = 0;

  if (!sess || !conn || !info)
    return 0;

  if (!(tx = aim_tx_new(AIM_FRAMETYPE_OSCAR, 0x0002, conn, 1152)))
    return -1;

  tx->lock = 1;

  i += aimutil_put16(tx->data+i, 0x0003);
  i += aimutil_put16(tx->data+i, 0x000b);
  i += aimutil_put16(tx->data+i, 0x0000);
  i += aimutil_put16(tx->data+i, 0x0000);
  i += aimutil_put16(tx->data+i, 0x0000);

  i += aim_putuserinfo(tx->data+i, tx->commandlen-i, info);

  tx->commandlen = i;
  tx->lock = 0;
  aim_tx_enqueue(sess, tx);

  return 0;
}

faim_export int aim_sendbuddyoffgoing(struct aim_session_t *sess, struct aim_conn_t *conn, char *sn)
{
  struct command_tx_struct *tx;
  int i = 0;

  if (!sess || !conn || !sn)
    return 0;

  if (!(tx = aim_tx_new(AIM_FRAMETYPE_OSCAR, 0x0002, conn, 10+1+strlen(sn))))
    return -1;

  tx->lock = 1;

  i += aimutil_put16(tx->data+i, 0x0003);
  i += aimutil_put16(tx->data+i, 0x000c);
  i += aimutil_put16(tx->data+i, 0x0000);
  i += aimutil_put16(tx->data+i, 0x0000);
  i += aimutil_put16(tx->data+i, 0x0000);

  i += aimutil_put8(tx->data+i, strlen(sn));
  i += aimutil_putstr(tx->data+i, sn, strlen(sn));
  
  tx->lock = 0;
  aim_tx_enqueue(sess, tx);

  return 0;
}

