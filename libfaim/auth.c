/*
  aim_auth.c

  Deals with the authorizer.

 */

#define FAIM_INTERNAL
#include <aim.h> 

/* this just pushes the passed cookie onto the passed connection -- NO SNAC! */
faim_export int aim_auth_sendcookie(struct aim_session_t *sess, 
				    struct aim_conn_t *conn, 
				    unsigned char *chipsahoy)
{
  struct command_tx_struct *newpacket;
  int curbyte=0;
  
  if (!(newpacket = aim_tx_new(sess, conn, AIM_FRAMETYPE_OSCAR, 0x0001, 4+2+2+AIM_COOKIELEN)))
    return -1;

  newpacket->lock = 1;

  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0000);
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0001);
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0006);
  curbyte += aimutil_put16(newpacket->data+curbyte, AIM_COOKIELEN);
  memcpy(newpacket->data+curbyte, chipsahoy, AIM_COOKIELEN);

  return aim_tx_enqueue(sess, newpacket);
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
static int parse(struct aim_session_t *sess, aim_module_t *mod, struct command_rx_struct *rx, aim_modsnac_t *snac, unsigned char *data, int datalen)
{
  struct aim_tlvlist_t *tlvlist;
  int ret = 0;
  rxcallback_t userfunc;
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
   */
  tlvlist = aim_readtlvchain(data, datalen);

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
   *
   *   1 = No disclosure
   *   2 = Limited disclosure
   *   3 = Full disclosure
   *
   * This has to do with whether your email address is available
   * to other users or not.  AFAIK, this feature is no longer used.
   *
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


  if ((userfunc = aim_callhandler(sess, rx->conn, snac?snac->family:0x0017, snac?snac->subtype:0x0003)))
    ret = userfunc(sess, rx, sn, errorcode, errurl, regstatus, email, bosip, cookie, latestrelease, latestbuild, latestreleaseurl, latestreleaseinfo, latestbeta, latestbetabuild, latestbetaurl, latestbetainfo);


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
static int keyparse(struct aim_session_t *sess, aim_module_t *mod, struct command_rx_struct *rx, aim_modsnac_t *snac, unsigned char *data, int datalen)
{
  unsigned char *key;
  int keylen;
  int ret = 1;
  rxcallback_t userfunc;

  keylen = aimutil_get16(data);
  if (!(key = malloc(keylen+1)))
    return ret;
  memcpy(key, data+2, keylen);
  key[keylen] = '\0';
  
  if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
    ret = userfunc(sess, rx, (char *)key);

  free(key);  

  return ret;
}

static int snachandler(struct aim_session_t *sess, aim_module_t *mod, struct command_rx_struct *rx, aim_modsnac_t *snac, unsigned char *data, int datalen)
{

  if (snac->subtype == 0x0003)
    return parse(sess, mod, rx, snac, data, datalen);
  else if (snac->subtype == 0x0007)
    return keyparse(sess, mod, rx, snac, data, datalen);

  return 0;
}

faim_internal int auth_modfirst(struct aim_session_t *sess, aim_module_t *mod)
{

  mod->family = 0x0017;
  mod->version = 0x0000;
  mod->flags = 0;
  strncpy(mod->name, "auth", sizeof(mod->name));
  mod->snachandler = snachandler;

  return 0;
}
