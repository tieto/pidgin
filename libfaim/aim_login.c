/*
 *  aim_login.c
 *
 *  This contains all the functions needed to actually login.
 *
 */

#include <faim/aim.h>

#include "md5.h"

static int aim_encode_password_md5(const char *password, const char *key, md5_byte_t *digest);
static int aim_encode_password(const char *password, unsigned char *encoded);

/*
 * FIXME: Reimplement the TIS stuff.
 */
#ifdef TIS_TELNET_PROXY
#include "tis_telnet_proxy.h"
#endif

faim_export int aim_sendconnack(struct aim_session_t *sess,
				struct aim_conn_t *conn)
{
  int curbyte=0;
  
  struct command_tx_struct *newpacket;

  if (!(newpacket = aim_tx_new(AIM_FRAMETYPE_OSCAR, 0x0001, conn, 4)))
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

  if (!(newpacket = aim_tx_new(AIM_FRAMETYPE_OSCAR, 0x0002, conn, 10+2+2+strlen(sn))))
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

  if (!(newpacket = aim_tx_new(AIM_FRAMETYPE_OSCAR, 0x0002, conn, 1152)))
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
    md5_byte_t digest[16];

    aim_encode_password_md5(password, key, digest);
    curbyte+= aim_puttlv_str(newpacket->data+curbyte, 0x0025, 16, (char *)digest);
  } else { 
    char *password_encoded;

    password_encoded = (char *) malloc(strlen(password));
    aim_encode_password(password, password_encoded);
    curbyte += aim_puttlv_str(newpacket->data+curbyte, 0x0002, strlen(password), password_encoded);
    free(password_encoded);
  }

  /* XXX is clientstring required by oscar? */
  if (strlen(clientinfo->clientstring))
    curbyte += aim_puttlv_str(newpacket->data+curbyte, 0x0003, strlen(clientinfo->clientstring), clientinfo->clientstring);

  if (sess->flags & AIM_SESS_FLAGS_SNACLOGIN) {
    curbyte += aim_puttlv_16(newpacket->data+curbyte, 0x0016, (unsigned short)clientinfo->major2);
    curbyte += aim_puttlv_16(newpacket->data+curbyte, 0x0017, (unsigned short)clientinfo->major);
    curbyte += aim_puttlv_16(newpacket->data+curbyte, 0x0018, (unsigned short)clientinfo->minor);
    curbyte += aim_puttlv_16(newpacket->data+curbyte, 0x0019, (unsigned short)clientinfo->minor2);
    curbyte += aim_puttlv_16(newpacket->data+curbyte, 0x001a, (unsigned short)clientinfo->build);
  
    curbyte += aim_puttlv_32(newpacket->data+curbyte, 0x0014, clientinfo->unknown);
    curbyte += aim_puttlv_16(newpacket->data+curbyte, 0x0009, 0x0015);
    //curbyte += aim_puttlv_8(newpacket->data+curbyte, 0x004a, 0x01);
  } else {
    /* Use very specific version numbers, to further indicate the hack. */
    curbyte += aim_puttlv_16(newpacket->data+curbyte, 0x0016, 0x010a);
    curbyte += aim_puttlv_16(newpacket->data+curbyte, 0x0017, 0x0004);
    curbyte += aim_puttlv_16(newpacket->data+curbyte, 0x0018, 0x003c);
    curbyte += aim_puttlv_16(newpacket->data+curbyte, 0x0019, 0x0001);
    curbyte += aim_puttlv_16(newpacket->data+curbyte, 0x001a, 0x0cce);
    curbyte += aim_puttlv_32(newpacket->data+curbyte, 0x0014, 0x00000055);
  }

  if (strlen(clientinfo->country))
    curbyte += aim_puttlv_str(newpacket->data+curbyte, 0x000e, strlen(clientinfo->country), clientinfo->country);
  else
    curbyte += aim_puttlv_str(newpacket->data+curbyte, 0x000e, 2, "us");

  if (strlen(clientinfo->lang))
    curbyte += aim_puttlv_str(newpacket->data+curbyte, 0x000f, strlen(clientinfo->lang), clientinfo->lang);
  else
    curbyte += aim_puttlv_str(newpacket->data+curbyte, 0x000f, 2, "en");
  
  newpacket->commandlen = curbyte;

  newpacket->lock = 0;
  return aim_tx_enqueue(sess, newpacket);
}

static int aim_encode_password_md5(const char *password, const char *key, md5_byte_t *digest)
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
 * This is sent back as a general response to the login command.
 * It can be either an error or a success, depending on the
 * precense of certain TLVs.  
 *
 * The client should check the value passed as errorcode. If
 * its nonzero, there was an error.
 *
 */
faim_internal int aim_authparse(struct aim_session_t *sess, 
				struct command_rx_struct *command)
{
  struct aim_tlvlist_t *tlvlist;
  int ret = 1;
  rxcallback_t userfunc = NULL;
  char *sn = NULL, *bosip = NULL, *errurl = NULL, *email = NULL;
  unsigned char *cookie = NULL;
  int errorcode = 0, regstatus = 0;
  int latestbuild = 0, latestbetabuild = 0;
  char *latestrelease = NULL, *latestbeta = NULL;
  char *latestreleaseurl = NULL, *latestbetaurl = NULL;
  char *latestreleaseinfo = NULL, *latestbetainfo = NULL;

  /*
   * Read block of TLVs.  All further data is derived
   * from what is parsed here.
   *
   * For SNAC login, there's a 17/3 SNAC header in front.
   *
   */
  if (sess->flags & AIM_SESS_FLAGS_SNACLOGIN)
    tlvlist = aim_readtlvchain(command->data+10, command->commandlen-10);
  else
    tlvlist = aim_readtlvchain(command->data, command->commandlen);

  /*
   * No matter what, we should have a screen name.
   */
  memset(sess->sn, 0, sizeof(sess->sn));
  if (aim_gettlv(tlvlist, 0x0001, 1)) {
    sn = aim_gettlv_str(tlvlist, 0x0001, 1);
    strncpy(sess->sn, sn, sizeof(sess->sn));
  }

  /*
   * Check for an error code.  If so, we should also
   * have an error url.
   */
  if (aim_gettlv(tlvlist, 0x0008, 1)) 
    errorcode = aim_gettlv16(tlvlist, 0x0008, 1);
  if (aim_gettlv(tlvlist, 0x0004, 1))
    errurl = aim_gettlv_str(tlvlist, 0x0004, 1);

  /*
   * BOS server address.
   */
  if (aim_gettlv(tlvlist, 0x0005, 1))
    bosip = aim_gettlv_str(tlvlist, 0x0005, 1);

  /*
   * Authorization cookie.
   */
  if (aim_gettlv(tlvlist, 0x0006, 1)) {
    struct aim_tlv_t *tmptlv;

    tmptlv = aim_gettlv(tlvlist, 0x0006, 1);

    if ((cookie = malloc(tmptlv->length)))
      memcpy(cookie, tmptlv->value, tmptlv->length);
  }

  /*
   * The email address attached to this account
   *   Not available for ICQ logins.
   */
  if (aim_gettlv(tlvlist, 0x0011, 1))
    email = aim_gettlv_str(tlvlist, 0x0011, 1);

  /*
   * The registration status.  (Not real sure what it means.)
   *   Not available for ICQ logins.
   */
  if (aim_gettlv(tlvlist, 0x0013, 1))
    regstatus = aim_gettlv16(tlvlist, 0x0013, 1);

  if (aim_gettlv(tlvlist, 0x0040, 1))
    latestbetabuild = aim_gettlv32(tlvlist, 0x0040, 1);
  if (aim_gettlv(tlvlist, 0x0041, 1))
    latestbetaurl = aim_gettlv_str(tlvlist, 0x0041, 1);
  if (aim_gettlv(tlvlist, 0x0042, 1))
    latestbetainfo = aim_gettlv_str(tlvlist, 0x0042, 1);
  if (aim_gettlv(tlvlist, 0x0043, 1))
    latestbeta = aim_gettlv_str(tlvlist, 0x0043, 1);
  if (aim_gettlv(tlvlist, 0x0048, 1))
    ; /* no idea what this is */

  if (aim_gettlv(tlvlist, 0x0044, 1))
    latestbuild = aim_gettlv32(tlvlist, 0x0044, 1);
  if (aim_gettlv(tlvlist, 0x0045, 1))
    latestreleaseurl = aim_gettlv_str(tlvlist, 0x0045, 1);
  if (aim_gettlv(tlvlist, 0x0046, 1))
    latestreleaseinfo = aim_gettlv_str(tlvlist, 0x0046, 1);
  if (aim_gettlv(tlvlist, 0x0047, 1))
    latestrelease = aim_gettlv_str(tlvlist, 0x0047, 1);
  if (aim_gettlv(tlvlist, 0x0049, 1))
    ; /* no idea what this is */


  if ((userfunc = aim_callhandler(command->conn, 0x0017, 0x0003)))
    ret = userfunc(sess, command, sn, errorcode, errurl, regstatus, email, bosip, cookie, latestrelease, latestbuild, latestreleaseurl, latestreleaseinfo, latestbeta, latestbetabuild, latestbetaurl, latestbetainfo);


  if (sn)
    free(sn);
  if (bosip)
    free(bosip);
  if (errurl)
    free(errurl);
  if (email)
    free(email);
  if (cookie)
    free(cookie);
  if (latestrelease)
    free(latestrelease);
  if (latestreleaseurl)
    free(latestreleaseurl);
  if (latestbeta)
    free(latestbeta);
  if (latestbetaurl)
    free(latestbetaurl);
  if (latestreleaseinfo)
    free(latestreleaseinfo);
  if (latestbetainfo)
    free(latestbetainfo);

  aim_freetlvchain(&tlvlist);

  return ret;
}

/*
 * Middle handler for 0017/0007 SNACs.  Contains the auth key prefixed
 * by only its length in a two byte word.
 *
 * Calls the client, which should then use the value to call aim_send_login.
 *
 */
faim_internal int aim_authkeyparse(struct aim_session_t *sess, struct command_rx_struct *command)
{
  unsigned char *key;
  int keylen;
  int ret = 1;
  rxcallback_t userfunc;

  keylen = aimutil_get16(command->data+10);
  if (!(key = malloc(keylen+1)))
    return ret;
  memcpy(key, command->data+12, keylen);
  key[keylen] = '\0';
  
  if ((userfunc = aim_callhandler(command->conn, 0x0017, 0x0007)))
    ret = userfunc(sess, command, (char *)key);

  free(key);  

  return ret;
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

  if (!(tx = aim_tx_new(AIM_FRAMETYPE_OSCAR, 0x0004, conn, 1152)))
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

  if (!(tx = aim_tx_new(AIM_FRAMETYPE_OSCAR, 0x0002, conn, 10+0x22)))
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

  if (!(tx = aim_tx_new(AIM_FRAMETYPE_OSCAR, 0x0002, conn, 1152)))
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
