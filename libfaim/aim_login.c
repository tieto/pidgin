/*
 *  aim_login.c
 *
 *  This contains all the functions needed to actually login.
 *
 */

#include <faim/aim.h>


/*
 * FIXME: Reimplement the TIS stuff.
 */
#ifdef TIS_TELNET_PROXY
#include "tis_telnet_proxy.h"
#endif

int aim_sendconnack(struct aim_session_t *sess,
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

#ifdef SNACLOGIN
/*
 * In AIM 3.5 protocol, the first stage of login is to request
 * login from the Authorizer, passing it the screen name
 * for verification.  If the name is invalid, a 0017/0003 
 * is spit back, with the standard error contents.  If valid,
 * a 0017/0007 comes back, which is the signal to send
 * it the main login command (0017/0002).  
 */
int aim_request_login(struct aim_session_t *sess,
		      struct aim_conn_t *conn, 
		      char *sn)
{
  int curbyte=0;
  
  struct command_tx_struct *newpacket;

  if (!(newpacket = aim_tx_new(AIM_FRAMETYPE_OSCAR, 0x0002, conn, 10+2+2+strlen(sn))))
    return -1;

  newpacket->lock = 1;
  
  curbyte += aim_putsnac(newpacket->data+curbyte, 0x0017, 0x0006, 0x0000, 0x00010000);
  curbyte += aim_puttlv_str(newpacket->data+curbyte, 0x0001, strlen(sn), sn);

  newpacket->lock = 0;
  return aim_tx_enqueue(sess, newpacket);
}
#endif /* SNACLOGIN */

/*
 * send_login(int socket, char *sn, char *password)
 *  
 * This is the initial login request packet.
 *
 * The password is encoded before transmition, as per
 * encode_password().  See that function for their
 * stupid method of doing it.
 *
 */
int aim_send_login (struct aim_session_t *sess,
		    struct aim_conn_t *conn, 
		    char *sn, char *password, struct client_info_s *clientinfo)
{
  u_char *password_encoded = NULL;  /* to store encoded password */
  int curbyte=0;
  int icqmode = 0;
  
  struct command_tx_struct *newpacket;

  if (!clientinfo || !sn || !password)
    return -1;

  if (!(newpacket = aim_tx_new(AIM_FRAMETYPE_OSCAR, 0x0002, conn, 1152)))
    return -1;

  /*
   * For ICQ logins, the client version must be at
   * least as high as ICQ2000a.
   */
  if ((sn[0] >= '0') && (sn[0] <= '9')) {
    icqmode = 1; /* needs a different password encoding */
    if (clientinfo && (clientinfo->major < 4)) {
      printf("faim: icq: version must be at least 4.30.3141 for ICQ OSCAR login\n");
      return -1;
    }
    if (strlen(password) > 8) {
      printf("faim: icq: password too long (8 char max)\n");
      return -1;
    }
  }

#ifdef SNACLOGIN 
  newpacket->commandlen = 10;
  newpacket->commandlen += 2 + 2 + strlen(sn);
  newpacket->commandlen += 2 + 2 + strlen(password);
  newpacket->commandlen += 2 + 2 + strlen(clientinfo->clientstring);
  newpacket->commandlen += 56;
  
  newpacket->lock = 1;

  curbyte = aim_putsnac(newpacket->data+curbyte, 0x0017, 0x0002, 0x0000, 0x00010000);
  curbyte+= aim_puttlv_str(newpacket->data+curbyte, 0x0001, strlen(sn), sn);
  password_encoded = (u_char *) malloc(strlen(password));
  aim_encode_password(password, password_encoded);
  curbyte+= aim_puttlv_str(newpacket->data+curbyte, 0x0002, strlen(password), password_encoded);
  curbyte+= aim_puttlv_str(newpacket->data+curbyte, 0x0003, 
			   strlen(clientinfo->clientstring), 
			   clientinfo->clientstring);
  /* XXX: should use clientinfo provided version info */
  curbyte+= aim_puttlv_16(newpacket->data+curbyte, 0x0016, 0x0004);
  curbyte+= aim_puttlv_16(newpacket->data+curbyte, 0x0017, 0x0003);
  curbyte+= aim_puttlv_16(newpacket->data+curbyte, 0x0018, 0x0005);
  curbyte+= aim_puttlv_16(newpacket->data+curbyte, 0x0019, 0x0000);
  curbyte+= aim_puttlv_16(newpacket->data+curbyte, 0x001a, 0x0686);
  curbyte+= aim_puttlv_str(newpacket->data+curbyte, 0x0001, 0x0002, clientinfo->country);
  curbyte+= aim_puttlv_str(newpacket->data+curbyte, 0x0001, 0x0002, clientinfo->lang);
  curbyte+= aim_puttlv_32(newpacket->data+curbyte, 0x0014, 0x0000002a);
  curbyte+= aim_puttlv_16(newpacket->data+curbyte, 0x0009, 0x0015);
#else
  
  newpacket->commandlen = 4 + 4 + strlen(sn) + 4+strlen(password) + 6;
 
  newpacket->commandlen += 8; /* tlv 0x0014 */

  if (clientinfo) {
    if (strlen(clientinfo->clientstring))
      newpacket->commandlen += 4+strlen(clientinfo->clientstring);
    newpacket->commandlen += 6+6+6+6;
    if (strlen(clientinfo->country))
      newpacket->commandlen += 4+strlen(clientinfo->country);
    if (strlen(clientinfo->lang))
      newpacket->commandlen += 4+strlen(clientinfo->lang);
  }

  newpacket->lock = 1;
  newpacket->hdr.oscar.type = 0x01;

  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0000);
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0001);
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0001);
  curbyte += aimutil_put16(newpacket->data+curbyte, strlen(sn));
  curbyte += aimutil_putstr(newpacket->data+curbyte, sn, strlen(sn));

  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0002);
  curbyte += aimutil_put16(newpacket->data+curbyte, strlen(password));
  password_encoded = (char *) malloc(strlen(password));
  aim_encode_password(password, password_encoded);
  curbyte += aimutil_putstr(newpacket->data+curbyte, password_encoded, strlen(password));
  free(password_encoded);
  
  if (strlen(clientinfo->clientstring)) {
    curbyte += aimutil_put16(newpacket->data+curbyte, 0x0003);
    curbyte += aimutil_put16(newpacket->data+curbyte, strlen(clientinfo->clientstring));
    curbyte += aimutil_putstr(newpacket->data+curbyte, clientinfo->clientstring, strlen(clientinfo->clientstring));
  }
  curbyte += aim_puttlv_16(newpacket->data+curbyte, 0x0016, /*0x010a*/ 0x0004);
  curbyte += aim_puttlv_16(newpacket->data+curbyte, 0x0017, clientinfo->major /*0x0001*/);
  curbyte += aim_puttlv_16(newpacket->data+curbyte, 0x0018, clientinfo->minor /*0x0001*/);
  curbyte += aim_puttlv_16(newpacket->data+curbyte, 0x0019, 0x0001);
  curbyte += aim_puttlv_16(newpacket->data+curbyte, 0x001a, clientinfo->build /*0x0013*/);

  curbyte += aim_puttlv_32(newpacket->data+curbyte, 0x0014, 0x00000055);

  if (strlen(clientinfo->country)) {
    curbyte += aimutil_put16(newpacket->data+curbyte, 0x000e);
    curbyte += aimutil_put16(newpacket->data+curbyte, strlen(clientinfo->country));
    curbyte += aimutil_putstr(newpacket->data+curbyte, clientinfo->country, strlen(clientinfo->country));
  }
  if (strlen(clientinfo->lang)) {
    curbyte += aimutil_put16(newpacket->data+curbyte, 0x000f);
    curbyte += aimutil_put16(newpacket->data+curbyte, strlen(clientinfo->lang));
    curbyte += aimutil_putstr(newpacket->data+curbyte, clientinfo->lang, strlen(clientinfo->lang));
  }

#endif

  newpacket->lock = 0;
  return aim_tx_enqueue(sess, newpacket);
}

/*
 *  int encode_password(
 *                      const char *password,
 *	                char *encoded
 *	                ); 
 *
 * This takes a const pointer to a (null terminated) string
 * containing the unencoded password.  It also gets passed
 * an already allocated buffer to store the encoded password.
 * This buffer should be the exact length of the password without
 * the null.  The encoded password buffer IS NOT NULL TERMINATED.
 *
 * The encoding_table seems to be a fixed set of values.  We'll
 * hope it doesn't change over time!  
 *
 */
int aim_encode_password(const char *password, u_char *encoded)
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
 * The client should check the value of logininfo->errorcode. If
 * its nonzero, there was an error.
 *
 */
int aim_authparse(struct aim_session_t *sess, 
		  struct command_rx_struct *command)
{
  struct aim_tlvlist_t *tlvlist;
  int ret = 1;
  char *sn;
  rxcallback_t userfunc = NULL;

  memset(&sess->logininfo, 0x00, sizeof(sess->logininfo));

  /*
   * Read block of TLVs.  All further data is derived
   * from what is parsed here.
   */
#ifdef SNACLOGIN
  tlvlist = aim_readtlvchain(command->data+10, command->commandlen-10);
#else
  tlvlist = aim_readtlvchain(command->data, command->commandlen);
#endif
  /*
   * No matter what, we should have a screen name.
   */
  sn = aim_gettlv_str(tlvlist, 0x0001, 1);
  memcpy(sess->logininfo.screen_name, sn, strlen(sn));
  sn[(strlen(sn))] = '\0';
  
  /*
   * Check for an error code.  If so, we should also
   * have an error url.
   */
  if (aim_gettlv(tlvlist, 0x0008, 1)) {
    struct aim_tlv_t *errtlv;
    errtlv = aim_gettlv(tlvlist, 0x0008, 1);
    sess->logininfo.errorcode = aimutil_get16(errtlv->value);
    sess->logininfo.errorurl = aim_gettlv_str(tlvlist, 0x0004, 1);
  }
  /* 
   * If we have both an IP number (0x0005) and a cookie (0x0006),
   * then the login was successful.
   */
  else if (aim_gettlv(tlvlist, 0x0005, 1) && aim_gettlv(tlvlist, 0x0006, 1) 
	   /*aim_gettlv(tlvlist, 0x0006, 1)->length*/) {
    struct aim_tlv_t *tmptlv;

    /*
     * IP address of BOS server.
     */
    sess->logininfo.BOSIP = aim_gettlv_str(tlvlist, 0x0005, 1);
    
    /*
     * Authorization Cookie
     */
    tmptlv = aim_gettlv(tlvlist, 0x0006, 1);
    memcpy(sess->logininfo.cookie, tmptlv->value, AIM_COOKIELEN);
    
    /*
     * The email address attached to this account
     *   Not available for ICQ logins.
     */
    if (aim_gettlv(tlvlist, 0x0011, 1))
      sess->logininfo.email = aim_gettlv_str(tlvlist, 0x0011, 1);
    
    /*
     * The registration status.  (Not real sure what it means.)
     *   Not available for ICQ logins.
     */
    if ((tmptlv = aim_gettlv(tlvlist, 0x0013, 1)))
      sess->logininfo.regstatus = aimutil_get16(tmptlv->value);
      
  }

#ifdef SNACLOGIN
  userfunc = aim_callhandler(command->conn, 0x0017, 0x0003);
#else
  userfunc = aim_callhandler(command->conn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_AUTHSUCCESS);
#endif
  if (userfunc)
    ret = userfunc(sess, command);

  aim_freetlvchain(&tlvlist);

  /* These have been clobbered by the freetlvchain */
  sess->logininfo.BOSIP = NULL;
  sess->logininfo.email = NULL;
  sess->logininfo.errorurl = NULL;

  return ret;
}

/*
 * Generate an authorization response.  
 *
 * You probably don't want this unless you're writing an AIM server.
 *
 */
unsigned long aim_sendauthresp(struct aim_session_t *sess, 
			       struct aim_conn_t *conn, 
			       char *sn, char *bosip, 
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
    aim_addtlvtochain_str(&tlvlist, 0x0001, sess->logininfo.screen_name, strlen(sess->logininfo.screen_name));

  if (sess->logininfo.errorcode) {
    aim_addtlvtochain16(&tlvlist, 0x0008, sess->logininfo.errorcode);
    aim_addtlvtochain_str(&tlvlist, 0x0004, sess->logininfo.errorurl, strlen(sess->logininfo.errorurl));
  } else {
    aim_addtlvtochain_str(&tlvlist, 0x0005, bosip, strlen(bosip));
    aim_addtlvtochain_str(&tlvlist, 0x0006, cookie, AIM_COOKIELEN);
    aim_addtlvtochain_str(&tlvlist, 0x0011, email, strlen(email));
    aim_addtlvtochain16(&tlvlist, 0x0013, regstatus);
  }

  tx->commandlen = aim_writetlvchain(tx->data, tx->commandlen, &tlvlist);
  tx->lock = 0;
  return aim_tx_enqueue(sess, tx);
}

/*
 * Generate a random cookie.  (Non-client use only)
 */
int aim_gencookie(unsigned char *buf)
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
int aim_sendserverready(struct aim_session_t *sess, struct aim_conn_t *conn)
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
unsigned long aim_sendredirect(struct aim_session_t *sess, 
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

  i += aimutil_put16(tx->data+i, 0x0001);
  i += aimutil_put16(tx->data+i, 0x0005);
  i += aimutil_put16(tx->data+i, 0x0000);
  i += aimutil_put16(tx->data+i, 0x0000);
  i += aimutil_put16(tx->data+i, 0x0000);
  
  aim_addtlvtochain16(&tlvlist, 0x000d, servid);
  aim_addtlvtochain_str(&tlvlist, 0x0005, ip, strlen(ip));
  aim_addtlvtochain_str(&tlvlist, 0x0006, cookie, AIM_COOKIELEN);

  tx->commandlen = aim_writetlvchain(tx->data+i, tx->commandlen-i, &tlvlist)+i;
  aim_freetlvchain(&tlvlist);

  tx->lock = 0;
  return aim_tx_enqueue(sess, tx);
}
