/*
 *  aim_login.c
 *
 *  This contains all the functions needed to actually login.
 *
 */

#define FAIM_INTERNAL
#include <aim.h>

#include "md5.h"

static int aim_encode_password(const char *password, unsigned char *encoded);

faim_export int aim_sendconnack(struct aim_session_t *sess,
				struct aim_conn_t *conn)
{
  int curbyte=0;
  
  struct command_tx_struct *newpacket;

  if (!(newpacket = aim_tx_new(sess, conn, AIM_FRAMETYPE_OSCAR, 0x0001, 4)))
    return -1;

  newpacket->lock = 1;
  
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0000);
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0001);

  newpacket->lock = 0;
  return aim_tx_enqueue(sess, newpacket);
}

/*
 * In AIM 3.5 protocol, the first stage of login is to request
 * login from the Authorizer, passing it the screen name
 * for verification.  If the name is invalid, a 0017/0003 
 * is spit back, with the standard error contents.  If valid,
 * a 0017/0007 comes back, which is the signal to send
 * it the main login command (0017/0002).  
 */
faim_export int aim_request_login(struct aim_session_t *sess,
				  struct aim_conn_t *conn, 
				  char *sn)
{
  int curbyte;
  struct command_tx_struct *newpacket;

  if (!sess || !conn || !sn)
    return -1;

  /*
   * For ICQ, we enable the ancient horrible login and stuff
   * a key packet into the queue to make it look like we got
   * a reply back. This is so the client doesn't know we're
   * really not doing MD5 login.
   *
   * This may sound stupid, but I'm not in the best of moods and 
   * I don't plan to keep support for this crap around much longer.
   * Its all AOL's fault anyway, really. I hate AOL.  Really.  They
   * always seem to be able to piss me off by doing the dumbest little
   * things.  Like disabling MD5 logins for ICQ UINs, or adding purposefully
   * wrong TLV lengths, or adding superfluous information to host strings,
   * or... I'll stop.
   *
   */
  if ((sn[0] >= '0') && (sn[0] <= '9')) {
    struct command_rx_struct *newrx;
    int i;

    if (!(newrx = (struct command_rx_struct *)malloc(sizeof(struct command_rx_struct))))
      return -1;
    memset(newrx, 0x00, sizeof(struct command_rx_struct));
    newrx->lock = 1; 
    newrx->hdrtype = AIM_FRAMETYPE_OSCAR;
    newrx->hdr.oscar.type = 0x02;
    newrx->hdr.oscar.seqnum = 0;
    newrx->commandlen = 10+2+1;
    newrx->nofree = 0; 
    if (!(newrx->data = malloc(newrx->commandlen))) {
      free(newrx);
      return -1;
    }

    i = aim_putsnac(newrx->data, 0x0017, 0x0007, 0x0000, 0x0000);
    i += aimutil_put16(newrx->data+i, 0x01);
    i += aimutil_putstr(newrx->data+i, "0", 1);

    newrx->conn = conn;

    newrx->next = sess->queue_incoming;
    sess->queue_incoming = newrx;

    newrx->lock = 0;

    sess->flags &= ~AIM_SESS_FLAGS_SNACLOGIN;

    return 0;
  } 

  sess->flags |= AIM_SESS_FLAGS_SNACLOGIN;

  aim_sendconnack(sess, conn);

  if (!(newpacket = aim_tx_new(sess, conn, AIM_FRAMETYPE_OSCAR, 0x0002, 10+2+2+strlen(sn))))
    return -1;

  newpacket->lock = 1;
  
  curbyte  = aim_putsnac(newpacket->data, 0x0017, 0x0006, 0x0000, 0x00010000);
  curbyte += aim_puttlv_str(newpacket->data+curbyte, 0x0001, strlen(sn), sn);

  newpacket->commandlen = curbyte;
  newpacket->lock = 0;

  return aim_tx_enqueue(sess, newpacket);
}

/*
 * send_login(int socket, char *sn, char *password)
 *  
 * This is the initial login request packet.
 *
 * The password is encoded before transmition, as per
 * encode_password().  See that function for their
 * stupid method of doing it.
 *
 * Latest WinAIM:
 *   clientstring = "AOL Instant Messenger (SM), version 4.3.2188/WIN32"
 *   major2 = 0x0109
 *   major = 0x0400
 *   minor = 0x0003
 *   minor2 = 0x0000
 *   build = 0x088c
 *   unknown = 0x00000086
 *   lang = "en"
 *   country = "us"
 *   unknown4a = 0x01
 *
 * Latest WinAIM that libfaim can emulate without server-side buddylists:
 *   clientstring = "AOL Instant Messenger (SM), version 3.5.1670/WIN32"
 *   major2 = 0x0004
 *   major =  0x0003
 *   minor =  0x0005
 *   minor2 = 0x0000
 *   build =  0x0686
 *   unknown =0x0000002a
 *
 */
faim_export int aim_send_login (struct aim_session_t *sess,
				struct aim_conn_t *conn, 
				char *sn, char *password, 
				struct client_info_s *clientinfo,
				char *key)
{
  int curbyte=0;
  struct command_tx_struct *newpacket;

  if (!clientinfo || !sn || !password)
    return -1;

  if (!(newpacket = aim_tx_new(sess, conn, AIM_FRAMETYPE_OSCAR, 0x0002, 1152)))
    return -1;

  newpacket->lock = 1;

  newpacket->hdr.oscar.type = (sess->flags & AIM_SESS_FLAGS_SNACLOGIN)?0x02:0x01;
  
  if (sess->flags & AIM_SESS_FLAGS_SNACLOGIN)
    curbyte = aim_putsnac(newpacket->data, 0x0017, 0x0002, 0x0000, 0x00010000);
  else {
    curbyte  = aimutil_put16(newpacket->data, 0x0000);
    curbyte += aimutil_put16(newpacket->data+curbyte, 0x0001);
  }

  curbyte += aim_puttlv_str(newpacket->data+curbyte, 0x0001, strlen(sn), sn);
  
  if (sess->flags & AIM_SESS_FLAGS_SNACLOGIN) {
    unsigned char digest[16];

    aim_encode_password_md5(password, key, digest);
    curbyte+= aim_puttlv_str(newpacket->data+curbyte, 0x0025, 16, (char *)digest);
  } else { 
    char *password_encoded;

    password_encoded = (char *) malloc(strlen(password));
    aim_encode_password(password, password_encoded);
    curbyte += aim_puttlv_str(newpacket->data+curbyte, 0x0002, strlen(password), password_encoded);
    free(password_encoded);
  }

  curbyte += aim_puttlv_str(newpacket->data+curbyte, 0x0003, strlen(clientinfo->clientstring), clientinfo->clientstring);

  if (sess->flags & AIM_SESS_FLAGS_SNACLOGIN) {

    curbyte += aim_puttlv_16(newpacket->data+curbyte, 0x0016, (unsigned short)clientinfo->major2);
    curbyte += aim_puttlv_16(newpacket->data+curbyte, 0x0017, (unsigned short)clientinfo->major);
    curbyte += aim_puttlv_16(newpacket->data+curbyte, 0x0018, (unsigned short)clientinfo->minor);
    curbyte += aim_puttlv_16(newpacket->data+curbyte, 0x0019, (unsigned short)clientinfo->minor2);
    curbyte += aim_puttlv_16(newpacket->data+curbyte, 0x001a, (unsigned short)clientinfo->build);
  
  } else {
    /* Use very specific version numbers, to further indicate the hack. */
    curbyte += aim_puttlv_16(newpacket->data+curbyte, 0x0016, 0x010a);
    curbyte += aim_puttlv_16(newpacket->data+curbyte, 0x0017, 0x0004);
    curbyte += aim_puttlv_16(newpacket->data+curbyte, 0x0018, 0x003c);
    curbyte += aim_puttlv_16(newpacket->data+curbyte, 0x0019, 0x0001);
    curbyte += aim_puttlv_16(newpacket->data+curbyte, 0x001a, 0x0cce);
    curbyte += aim_puttlv_32(newpacket->data+curbyte, 0x0014, 0x00000055);
  }

  curbyte += aim_puttlv_str(newpacket->data+curbyte, 0x000e, strlen(clientinfo->country), clientinfo->country);
  curbyte += aim_puttlv_str(newpacket->data+curbyte, 0x000f, strlen(clientinfo->lang), clientinfo->lang);
  
  if (sess->flags & AIM_SESS_FLAGS_SNACLOGIN) {
    curbyte += aim_puttlv_32(newpacket->data+curbyte, 0x0014, clientinfo->unknown);
    curbyte += aim_puttlv_16(newpacket->data+curbyte, 0x0009, 0x0015);
  }

  newpacket->commandlen = curbyte;

  newpacket->lock = 0;
  return aim_tx_enqueue(sess, newpacket);
}

faim_export int aim_encode_password_md5(const char *password, const char *key, unsigned char *digest)
{
  md5_state_t state;

  md5_init(&state);	
  md5_append(&state, (const md5_byte_t *)key, strlen(key));
  md5_append(&state, (const md5_byte_t *)password, strlen(password));
  md5_append(&state, (const md5_byte_t *)AIM_MD5_STRING, strlen(AIM_MD5_STRING));
  md5_finish(&state, (md5_byte_t *)digest);

  return 0;
}

/**
 * aim_encode_password - Encode a password using old XOR method
 * @password: incoming password
 * @encoded: buffer to put encoded password
 *
 * This takes a const pointer to a (null terminated) string
 * containing the unencoded password.  It also gets passed
 * an already allocated buffer to store the encoded password.
 * This buffer should be the exact length of the password without
 * the null.  The encoded password buffer /is not %NULL terminated/.
 *
 * The encoding_table seems to be a fixed set of values.  We'll
 * hope it doesn't change over time!  
 *
 * This is only used for the XOR method, not the better MD5 method.
 *
 */
static int aim_encode_password(const char *password, unsigned char *encoded)
{
  u_char encoding_table[] = {
#if 0 /* old v1 table */
    0xf3, 0xb3, 0x6c, 0x99,
    0x95, 0x3f, 0xac, 0xb6,
    0xc5, 0xfa, 0x6b, 0x63,
    0x69, 0x6c, 0xc3, 0x9f
#else /* v2.1 table, also works for ICQ */
    0xf3, 0x26, 0x81, 0xc4,
    0x39, 0x86, 0xdb, 0x92,
    0x71, 0xa3, 0xb9, 0xe6,
    0x53, 0x7a, 0x95, 0x7c
#endif
  };

  int i;
  
  for (i = 0; i < strlen(password); i++)
      encoded[i] = (password[i] ^ encoding_table[i]);

  return 0;
}

/*
 * Generate an authorization response.  
 *
 * You probably don't want this unless you're writing an AIM server.
 *
 */
faim_export unsigned long aim_sendauthresp(struct aim_session_t *sess, 
					   struct aim_conn_t *conn, 
					   char *sn, int errorcode,
					   char *errorurl, char *bosip, 
					   char *cookie, char *email, 
					   int regstatus)
{	
  struct command_tx_struct *tx;
  struct aim_tlvlist_t *tlvlist = NULL;

  if (!(tx = aim_tx_new(sess, conn, AIM_FRAMETYPE_OSCAR, 0x0004, 1152)))
    return -1;
  
  tx->lock = 1;

  if (sn)
    aim_addtlvtochain_str(&tlvlist, 0x0001, sn, strlen(sn));
  else
    aim_addtlvtochain_str(&tlvlist, 0x0001, sess->sn, strlen(sess->sn));

  if (errorcode) {
    aim_addtlvtochain16(&tlvlist, 0x0008, errorcode);
    aim_addtlvtochain_str(&tlvlist, 0x0004, errorurl, strlen(errorurl));
  } else {
    aim_addtlvtochain_str(&tlvlist, 0x0005, bosip, strlen(bosip));
    aim_addtlvtochain_str(&tlvlist, 0x0006, cookie, AIM_COOKIELEN);
    aim_addtlvtochain_str(&tlvlist, 0x0011, email, strlen(email));
    aim_addtlvtochain16(&tlvlist, 0x0013, (unsigned short)regstatus);
  }

  tx->commandlen = aim_writetlvchain(tx->data, tx->commandlen, &tlvlist);
  tx->lock = 0;

  return aim_tx_enqueue(sess, tx);
}

/*
 * Generate a random cookie.  (Non-client use only)
 */
faim_export int aim_gencookie(unsigned char *buf)
{
  int i;

  srand(time(NULL));

  for (i=0; i < AIM_COOKIELEN; i++)
    buf[i] = 1+(int) (256.0*rand()/(RAND_MAX+0.0));

  return i;
}

/*
 * Send Server Ready.  (Non-client)
 */
faim_export int aim_sendserverready(struct aim_session_t *sess, struct aim_conn_t *conn)
{
  struct command_tx_struct *tx;
  int i = 0;

  if (!(tx = aim_tx_new(sess, conn, AIM_FRAMETYPE_OSCAR, 0x0002, 10+0x22)))
    return -1;

  tx->lock = 1;

  i += aim_putsnac(tx->data, 0x0001, 0x0003, 0x0000, sess->snac_nextid++);
  
  i += aimutil_put16(tx->data+i, 0x0001);  
  i += aimutil_put16(tx->data+i, 0x0002);
  i += aimutil_put16(tx->data+i, 0x0003);
  i += aimutil_put16(tx->data+i, 0x0004);
  i += aimutil_put16(tx->data+i, 0x0006);
  i += aimutil_put16(tx->data+i, 0x0008);
  i += aimutil_put16(tx->data+i, 0x0009);
  i += aimutil_put16(tx->data+i, 0x000a);
  i += aimutil_put16(tx->data+i, 0x000b);
  i += aimutil_put16(tx->data+i, 0x000c);
  i += aimutil_put16(tx->data+i, 0x0013);
  i += aimutil_put16(tx->data+i, 0x0015);

  tx->commandlen = i;
  tx->lock = 0;
  return aim_tx_enqueue(sess, tx);
}


/* 
 * Send service redirect.  (Non-Client)
 */
faim_export unsigned long aim_sendredirect(struct aim_session_t *sess, 
					   struct aim_conn_t *conn, 
					   unsigned short servid, 
					   char *ip,
					   char *cookie)
{	
  struct command_tx_struct *tx;
  struct aim_tlvlist_t *tlvlist = NULL;
  int i = 0;

  if (!(tx = aim_tx_new(sess, conn, AIM_FRAMETYPE_OSCAR, 0x0002, 1152)))
    return -1;

  tx->lock = 1;

  i += aim_putsnac(tx->data+i, 0x0001, 0x0005, 0x0000, 0x00000000);
  
  aim_addtlvtochain16(&tlvlist, 0x000d, servid);
  aim_addtlvtochain_str(&tlvlist, 0x0005, ip, strlen(ip));
  aim_addtlvtochain_str(&tlvlist, 0x0006, cookie, AIM_COOKIELEN);

  tx->commandlen = aim_writetlvchain(tx->data+i, tx->commandlen-i, &tlvlist)+i;
  aim_freetlvchain(&tlvlist);

  tx->lock = 0;
  return aim_tx_enqueue(sess, tx);
}


static int hostonline(struct aim_session_t *sess, aim_module_t *mod, struct command_rx_struct *rx, aim_modsnac_t *snac, unsigned char *data, int datalen)
{
  rxcallback_t userfunc;
  int ret = 0;
  unsigned short *families;
  int famcount, i;

  famcount = datalen/2;

  if (!(families = malloc(datalen)))
    return 0;

  for (i = 0; i < famcount; i++)
    families[i] = aimutil_get16(data+(i*2));

  if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
    ret = userfunc(sess, rx, famcount, families);

  free(families);

  return ret;  
}

static int redirect(struct aim_session_t *sess, aim_module_t *mod, struct command_rx_struct *rx, aim_modsnac_t *snac, unsigned char *data, int datalen)
{
  int serviceid;
  unsigned char *cookie;
  char *ip;
  rxcallback_t userfunc;
  struct aim_tlvlist_t *tlvlist;
  char *chathack = NULL;
  int chathackex = 0;
  int ret = 0;
  
  tlvlist = aim_readtlvchain(data, datalen);

  if (!aim_gettlv(tlvlist, 0x000d, 1) ||
      !aim_gettlv(tlvlist, 0x0005, 1) ||
      !aim_gettlv(tlvlist, 0x0006, 1)) {
    aim_freetlvchain(&tlvlist);
    return 0;
  }

  serviceid = aim_gettlv16(tlvlist, 0x000d, 1);
  ip = aim_gettlv_str(tlvlist, 0x0005, 1);
  cookie = aim_gettlv_str(tlvlist, 0x0006, 1);

  /*
   * Chat hack.
   *
   */
  if ((serviceid == AIM_CONN_TYPE_CHAT) && sess->pendingjoin) {
    chathack = sess->pendingjoin;
    chathackex = sess->pendingjoinexchange;
    sess->pendingjoin = NULL;
    sess->pendingjoinexchange = 0;
  }

  if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
    ret = userfunc(sess, rx, serviceid, ip, cookie, chathack, chathackex);

  free(ip);
  free(cookie);
  free(chathack);

  aim_freetlvchain(&tlvlist);

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

/* XXX parse this */
static int rateresp(struct aim_session_t *sess, aim_module_t *mod, struct command_rx_struct *rx, aim_modsnac_t *snac, unsigned char *data, int datalen)
{
  rxcallback_t userfunc;

  if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
    return userfunc(sess, rx);

  return 0;
}

static int ratechange(struct aim_session_t *sess, aim_module_t *mod, struct command_rx_struct *rx, aim_modsnac_t *snac, unsigned char *data, int datalen)
{
  rxcallback_t userfunc;
  int i = 0, code;
  unsigned long currentavg, maxavg;
  unsigned long rateclass, windowsize, clear, alert, limit, disconnect;

  code = aimutil_get16(data+i);
  i += 2;

  rateclass = aimutil_get16(data+i);
  i += 2;

  windowsize = aimutil_get32(data+i);
  i += 4;
  clear = aimutil_get32(data+i);
  i += 4;
  alert = aimutil_get32(data+i);
  i += 4;
  limit = aimutil_get32(data+i);
  i += 4;
  disconnect = aimutil_get32(data+i);
  i += 4;
  currentavg = aimutil_get32(data+i);
  i += 4;
  maxavg = aimutil_get32(data+i);
  i += 4;

  if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
    return userfunc(sess, rx, code, rateclass, windowsize, clear, alert, limit, disconnect, currentavg, maxavg);

  return 0;
}

/* XXX parse this */
static int selfinfo(struct aim_session_t *sess, aim_module_t *mod, struct command_rx_struct *rx, aim_modsnac_t *snac, unsigned char *data, int datalen)
{
  rxcallback_t userfunc;

  if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
    return userfunc(sess, rx);

  return 0;
}

static int evilnotify(struct aim_session_t *sess, aim_module_t *mod, struct command_rx_struct *rx, aim_modsnac_t *snac, unsigned char *data, int datalen)
{
  rxcallback_t userfunc = NULL;
  int i = 0;
  unsigned short newevil;
  struct aim_userinfo_s userinfo;

  newevil = aimutil_get16(data);
  i += 2;

  memset(&userinfo, 0, sizeof(struct aim_userinfo_s));

  if (datalen-i)
    i += aim_extractuserinfo(sess, data+i, &userinfo);

  if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
    return userfunc(sess, rx, newevil, &userinfo);
  
  return 0;
}

static int motd(struct aim_session_t *sess, aim_module_t *mod, struct command_rx_struct *rx, aim_modsnac_t *snac, unsigned char *data, int datalen)
{
  rxcallback_t userfunc;
  char *msg = NULL;
  int ret = 0;
  struct aim_tlvlist_t *tlvlist;
  unsigned short id;

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
  id = aimutil_get16(data);

  /* 
   * TLVs follow 
   */
  if ((tlvlist = aim_readtlvchain(data+2, datalen-2)))
    msg = aim_gettlv_str(tlvlist, 0x000b, 1);
  
  if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
    ret =  userfunc(sess, rx, id, msg);

  free(msg);

  aim_freetlvchain(&tlvlist);

  return ret;
}

static int hostversions(struct aim_session_t *sess, aim_module_t *mod, struct command_rx_struct *rx, aim_modsnac_t *snac, unsigned char *data, int datalen)
{
  rxcallback_t userfunc;
  int vercount;

  vercount = datalen/4;
  
  if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
    return userfunc(sess, rx, vercount, data);

  return 0;
}

static int snachandler(struct aim_session_t *sess, aim_module_t *mod, struct command_rx_struct *rx, aim_modsnac_t *snac, unsigned char *data, int datalen)
{

  if (snac->subtype == 0x0003)
    return hostonline(sess, mod, rx, snac, data, datalen);
  else if (snac->subtype == 0x0005)
    return redirect(sess, mod, rx, snac, data, datalen);
  else if (snac->subtype == 0x0007)
    return rateresp(sess, mod, rx, snac, data, datalen);
  else if (snac->subtype == 0x000a)
    return ratechange(sess, mod, rx, snac, data, datalen);
  else if (snac->subtype == 0x000f)
    return selfinfo(sess, mod, rx, snac, data, datalen);
  else if (snac->subtype == 0x0010)
    return evilnotify(sess, mod, rx, snac, data, datalen);
  else if (snac->subtype == 0x0013)
    return motd(sess, mod, rx, snac, data, datalen);
  else if (snac->subtype == 0x0018)
    return hostversions(sess, mod, rx, snac, data, datalen);

  return 0;
}

faim_internal int general_modfirst(struct aim_session_t *sess, aim_module_t *mod)
{

  mod->family = 0x0001;
  mod->version = 0x0000;
  mod->flags = 0;
  strncpy(mod->name, "general", sizeof(mod->name));
  mod->snachandler = snachandler;

  return 0;
}
