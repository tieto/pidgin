/*
 * aim_info.c
 *
 * The functions here are responsible for requesting and parsing information-
 * gathering SNACs.  
 *
 */

#define FAIM_INTERNAL
#include <aim.h>

struct aim_priv_inforeq {
  char sn[MAXSNLEN+1];
  unsigned short infotype;
};

faim_export int aim_getinfo(struct aim_session_t *sess,
			    struct aim_conn_t *conn, 
			    const char *sn,
			    unsigned short infotype)
{
  struct command_tx_struct *newpacket;
  struct aim_priv_inforeq privdata;
  int i = 0;

  if (!sess || !conn || !sn)
    return -1;

  if ((infotype != AIM_GETINFO_GENERALINFO) &&
      (infotype != AIM_GETINFO_AWAYMESSAGE))
    return -1;

  if (!(newpacket = aim_tx_new(sess, conn, AIM_FRAMETYPE_OSCAR, 0x0002, 12+1+strlen(sn))))
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

  return 0;
}

/*
 * Capability blocks.  
 */
static const struct {
  unsigned short flag;
  unsigned char data[16];
} aim_caps[] = {
  
  {AIM_CAPS_BUDDYICON,
   {0x09, 0x46, 0x13, 0x46, 0x4c, 0x7f, 0x11, 0xd1, 
    0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},
  
  {AIM_CAPS_VOICE,
   {0x09, 0x46, 0x13, 0x41, 0x4c, 0x7f, 0x11, 0xd1, 
    0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

  {AIM_CAPS_IMIMAGE,
   {0x09, 0x46, 0x13, 0x45, 0x4c, 0x7f, 0x11, 0xd1, 
    0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},
  
  {AIM_CAPS_CHAT,
   {0x74, 0x8f, 0x24, 0x20, 0x62, 0x87, 0x11, 0xd1, 
    0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},
  
  {AIM_CAPS_GETFILE,
   {0x09, 0x46, 0x13, 0x48, 0x4c, 0x7f, 0x11, 0xd1,
    0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

  {AIM_CAPS_SENDFILE,
   {0x09, 0x46, 0x13, 0x43, 0x4c, 0x7f, 0x11, 0xd1, 
    0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

  {AIM_CAPS_SAVESTOCKS,
   {0x09, 0x46, 0x13, 0x47, 0x4c, 0x7f, 0x11, 0xd1,
    0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

  /*
   * Indeed, there are two of these.  The former appears
   * to be correct, but in some versions of winaim, the
   * second one is set.  Either they forgot to fix endianness,
   * or they made a typo. It really doesn't matter which.
   */
  {AIM_CAPS_GAMES,
   {0x09, 0x46, 0x13, 0x4a, 0x4c, 0x7f, 0x11, 0xd1,
    0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},
  {AIM_CAPS_GAMES2,
   {0x09, 0x46, 0x13, 0x4a, 0x4c, 0x7f, 0x11, 0xd1,
    0x22, 0x82, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

  {AIM_CAPS_SENDBUDDYLIST,
   {0x09, 0x46, 0x13, 0x4b, 0x4c, 0x7f, 0x11, 0xd1,
    0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

  {AIM_CAPS_LAST}
};

faim_internal unsigned short aim_getcap(struct aim_session_t *sess, unsigned char *capblock, int buflen)
{
  unsigned short flags;
  int i;
  int offset = 0;
  int identified;

  for (offset = 0, flags = 0; offset < buflen; offset += 0x0010) {

    for (i = 0, identified = 0; !(aim_caps[i].flag & AIM_CAPS_LAST); i++) {

      if (memcmp(&aim_caps[i].data, capblock+offset, 0x10) == 0) {
	flags |= aim_caps[i].flag;
	identified++;
	break; /* should only match once... */
      }

    }

    if (!identified)
      faimdprintf(sess, 0, "unknown capability!\n");

  }

  return flags;
}

faim_internal int aim_putcap(unsigned char *capblock, int buflen, unsigned short caps)
{
  int offset, i;

  if (!capblock)
    return 0;

  for (i = 0, offset = 0; 
       !(aim_caps[i].flag & AIM_CAPS_LAST) && (offset < buflen); i++) {

    if (caps & aim_caps[i].flag) {
      memcpy(capblock+offset, aim_caps[i].data, 16);
      offset += 16;
    }

  }

  return offset;
}

/*
 * AIM is fairly regular about providing user info.  This
 * is a generic routine to extract it in its standard form.
 */
faim_internal int aim_extractuserinfo(struct aim_session_t *sess, unsigned char *buf, struct aim_userinfo_s *outinfo)
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
	
	outinfo->capabilities = aim_getcap(sess, buf+i+4, len);
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
	char tmpstr[160];

        faimdprintf(sess, 0, "userinfo: **warning: unexpected TLV:\n");
	faimdprintf(sess, 0, "userinfo:   sn    =%s\n", outinfo->sn);
	faimdprintf(sess, 0, "userinfo:   curtlv=0x%04x\n", curtlv);
	faimdprintf(sess, 0, "userinfo:   type  =0x%04x\n",aimutil_get16(&buf[i]));
	faimdprintf(sess, 0, "userinfo:   length=0x%04x\n", len = aimutil_get16(&buf[i+2]));
	faimdprintf(sess, 0, "userinfo:   data: \n");
	while (z<len)
	  {
	    x = snprintf(tmpstr, sizeof(tmpstr), "userinfo:      ");
	    for (y = 0; y < 8; y++)
	      {
		if (z<len)
		  {
		    snprintf(tmpstr+x, sizeof(tmpstr)-x, "%02x ", buf[i+4+z]);
		    z++;
		    x += 3;
		  }
		else
		  break;
	      }
	    faimdprintf(sess, 0, "%s\n", tmpstr);
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

  if (!(tx = aim_tx_new(sess, conn, AIM_FRAMETYPE_OSCAR, 0x0002, 1152)))
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

  if (!(tx = aim_tx_new(sess, conn, AIM_FRAMETYPE_OSCAR, 0x0002, 10+1+strlen(sn))))
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

faim_export int aim_0002_000b(struct aim_session_t *sess, struct aim_conn_t *conn, const char *sn)
{
  struct command_tx_struct *tx;
  int i = 0;

  if (!sess || !conn || !sn)
    return 0;

  if (!(tx = aim_tx_new(sess, conn, AIM_FRAMETYPE_OSCAR, 0x0002, 10+1+strlen(sn))))
    return -1;

  tx->lock = 1;

  i = aim_putsnac(tx->data, 0x0002, 0x000b, 0x0000, sess->snac_nextid);
  i += aimutil_put8(tx->data+i, strlen(sn));
  i += aimutil_putstr(tx->data+i, sn, strlen(sn));
  
  tx->commandlen = i;
  tx->lock = 0;
  aim_tx_enqueue(sess, tx);

  return 0;
}

static int rights(struct aim_session_t *sess, aim_module_t *mod, struct command_rx_struct *rx, aim_modsnac_t *snac, unsigned char *data, int datalen)
{
  struct aim_tlvlist_t *tlvlist;
  aim_rxcallback_t userfunc;
  int ret = 0;

  tlvlist = aim_readtlvchain(data, datalen);

  if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
    ret = userfunc(sess, rx);

  aim_freetlvchain(&tlvlist);

  return ret;
}

static int userinfo(struct aim_session_t *sess, aim_module_t *mod, struct command_rx_struct *rx, aim_modsnac_t *snac, unsigned char *data, int datalen)
{
  struct aim_userinfo_s userinfo;
  char *text_encoding = NULL;
  char *text = NULL;
  int i = 0;
  aim_rxcallback_t userfunc;
  struct aim_tlvlist_t *tlvlist;
  struct aim_snac_t *origsnac = NULL;
  struct aim_priv_inforeq *inforeq;
  int ret = 0;

  origsnac = aim_remsnac(sess, snac->id);

  if (!origsnac || !origsnac->data) {
    faimdprintf(sess, 0, "parse_userinfo_middle: major problem: no snac stored!\n");
    return 0;
  }

  inforeq = (struct aim_priv_inforeq *)origsnac->data;

  if ((inforeq->infotype != AIM_GETINFO_GENERALINFO) &&
      (inforeq->infotype != AIM_GETINFO_AWAYMESSAGE)) {
    faimdprintf(sess, 0, "parse_userinfo_middle: unknown infotype in request! (0x%04x)\n", inforeq->infotype);
    return 0;
  }

  i = aim_extractuserinfo(sess, data, &userinfo);
  
  tlvlist = aim_readtlvchain(data+i, datalen-i);

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

  if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
    ret = userfunc(sess, rx, &userinfo, text_encoding, text, inforeq->infotype);

  free(text_encoding);
  free(text);

  aim_freetlvchain(&tlvlist);

  if (origsnac) {
    if (origsnac->data)
      free(origsnac->data);
    free(origsnac);
  }

  return ret;
}

static int snachandler(struct aim_session_t *sess, aim_module_t *mod, struct command_rx_struct *rx, aim_modsnac_t *snac, unsigned char *data, int datalen)
{

  if (snac->subtype == 0x0003)
    return rights(sess, mod, rx, snac, data, datalen);
  else if (snac->subtype == 0x0006)
    return userinfo(sess, mod, rx, snac, data, datalen);

  return 0;
}

faim_internal int locate_modfirst(struct aim_session_t *sess, aim_module_t *mod)
{

  mod->family = 0x0002;
  mod->version = 0x0000;
  mod->flags = 0;
  strncpy(mod->name, "locate", sizeof(mod->name));
  mod->snachandler = snachandler;

  return 0;
}
