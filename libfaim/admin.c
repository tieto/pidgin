
#define FAIM_INTERNAL
#include <aim.h>

/* called for both reply and change-reply */
static int infochange(struct aim_session_t *sess, aim_module_t *mod, struct command_rx_struct *rx, aim_modsnac_t *snac, unsigned char *data, int datalen)
{
  int i;

  /*
   * struct {
   *    unsigned short perms;
   *    unsigned short tlvcount;
   *    aim_tlv_t tlvs[tlvcount];
   *  } admin_info[n];
   */
  for (i = 0; i < datalen; ) {
    int perms, tlvcount;

    perms = aimutil_get16(data+i);
    i += 2;

    tlvcount = aimutil_get16(data+i);
    i += 2;

    while (tlvcount) {
      rxcallback_t userfunc;
      struct aim_tlv_t *tlv;
      int str = 0;

      if ((aimutil_get16(data+i) == 0x0011) ||
	  (aimutil_get16(data+i) == 0x0004))
	str = 1;

      if (str)
	tlv = aim_grabtlvstr(data+i);
      else
	tlv = aim_grabtlv(data+i);

      /* XXX fix so its only called once for the entire packet */
      if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
	userfunc(sess, rx, perms, tlv->type, tlv->length, tlv->value, str);

      if (tlv)
	i += 2+2+tlv->length;

      if (tlv && tlv->value)
	free(tlv->value);
      if (tlv)
	free(tlv);

      tlvcount--;
    }
  }

  return 1;
}

static int accountconfirm(struct aim_session_t *sess, aim_module_t *mod, struct command_rx_struct *rx, aim_modsnac_t *snac, unsigned char *data, int datalen)
{
  rxcallback_t userfunc;
  int status;

  status = aimutil_get16(data);

  if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
    return userfunc(sess, rx, status);

  return 0;
}

static int snachandler(struct aim_session_t *sess, aim_module_t *mod, struct command_rx_struct *rx, aim_modsnac_t *snac, unsigned char *data, int datalen)
{

  if ((snac->subtype == 0x0003) || (snac->subtype == 0x0005))
    return infochange(sess, mod, rx, snac, data, datalen);
  else if (snac->subtype == 0x0007)
    return accountconfirm(sess, mod, rx, snac, data, datalen);

  return 0;
}

faim_internal int admin_modfirst(struct aim_session_t *sess, aim_module_t *mod)
{

  mod->family = 0x0007;
  mod->version = 0x0000;
  mod->flags = 0;
  strncpy(mod->name, "admin", sizeof(mod->name));
  mod->snachandler = snachandler;

  return 0;
}

faim_export unsigned long aim_auth_clientready(struct aim_session_t *sess,
					       struct aim_conn_t *conn)
{
  struct aim_tool_version tools[] = {
    {0x0001, 0x0003,    AIM_TOOL_NEWWIN, 0x0361},
    {0x0007, 0x0001,    AIM_TOOL_NEWWIN, 0x0361},
  };
  int i,j;
  struct command_tx_struct *newpacket;
  int toolcount = sizeof(tools)/sizeof(struct aim_tool_version);

  if (!(newpacket = aim_tx_new(sess, conn, AIM_FRAMETYPE_OSCAR, 0x0002, 1152)))
    return -1;

  newpacket->lock = 1;

  i = aim_putsnac(newpacket->data, 0x0001, 0x0002, 0x0000, sess->snac_nextid);
  aim_cachesnac(sess, 0x0001, 0x0002, 0x0000, NULL, 0);

  for (j = 0; j < toolcount; j++) {
    i += aimutil_put16(newpacket->data+i, tools[j].group);
    i += aimutil_put16(newpacket->data+i, tools[j].version);
    i += aimutil_put16(newpacket->data+i, tools[j].tool);
    i += aimutil_put16(newpacket->data+i, tools[j].toolversion);
  }

  newpacket->commandlen = i;
  newpacket->lock = 0;

  aim_tx_enqueue(sess, newpacket);

  return sess->snac_nextid;
}

faim_export unsigned long aim_auth_changepasswd(struct aim_session_t *sess,
						struct aim_conn_t *conn, 
						char *new, char *current)
{
  struct command_tx_struct *newpacket;
  int i;

  if (!(newpacket = aim_tx_new(sess, conn, AIM_FRAMETYPE_OSCAR, 0x0002, 10+4+strlen(current)+4+strlen(new))))
    return -1;

  newpacket->lock = 1;

  i = aim_putsnac(newpacket->data, 0x0007, 0x0004, 0x0000, sess->snac_nextid);
  aim_cachesnac(sess, 0x0007, 0x0004, 0x0000, NULL, 0);

  /* new password TLV t(0002) */
  i += aim_puttlv_str(newpacket->data+i, 0x0002, strlen(new), new);

  /* current password TLV t(0012) */
  i += aim_puttlv_str(newpacket->data+i, 0x0012, strlen(current), current);

  aim_tx_enqueue(sess, newpacket);

  return sess->snac_nextid;
}

faim_export unsigned long aim_auth_setversions(struct aim_session_t *sess,
					       struct aim_conn_t *conn)
{
  struct command_tx_struct *newpacket;
  int i;

  if (!(newpacket = aim_tx_new(sess, conn, AIM_FRAMETYPE_OSCAR, 0x0002, 10 + (4*2))))
    return -1;

  newpacket->lock = 1;

  i = aim_putsnac(newpacket->data, 0x0001, 0x0017, 0x0000, sess->snac_nextid);
  aim_cachesnac(sess, 0x0001, 0x0017, 0x0000, NULL, 0);

  i += aimutil_put16(newpacket->data+i, 0x0001);
  i += aimutil_put16(newpacket->data+i, 0x0003);

  i += aimutil_put16(newpacket->data+i, 0x0007);
  i += aimutil_put16(newpacket->data+i, 0x0001);

  newpacket->commandlen = i;
  newpacket->lock = 0;
  aim_tx_enqueue(sess, newpacket);

  return sess->snac_nextid;
}

/*
 * Request account confirmation. 
 *
 * This will cause an email to be sent to the address associated with
 * the account.  By following the instructions in the mail, you can
 * get the TRIAL flag removed from your account.
 *
 */
faim_export unsigned long aim_auth_reqconfirm(struct aim_session_t *sess,
					      struct aim_conn_t *conn)
{
  return aim_genericreq_n(sess, conn, 0x0007, 0x0006);
}

/*
 * Request a bit of account info.
 *
 * The only known valid tag is 0x0011 (email address).
 *
 */ 
faim_export unsigned long aim_auth_getinfo(struct aim_session_t *sess,
					   struct aim_conn_t *conn,
					   unsigned short info)
{
  struct command_tx_struct *newpacket;
  int i;

  if (!(newpacket = aim_tx_new(sess, conn, AIM_FRAMETYPE_OSCAR, 0x0002, 10 + 4)))
    return -1;

  newpacket->lock = 1;

  i = aim_putsnac(newpacket->data, 0x0007, 0x0002, 0x0000, sess->snac_nextid);
  aim_cachesnac(sess, 0x0002, 0x0002, 0x0000, NULL, 0);

  i += aimutil_put16(newpacket->data+i, info);
  i += aimutil_put16(newpacket->data+i, 0x0000);

  newpacket->commandlen = i;
  newpacket->lock = 0;
  aim_tx_enqueue(sess, newpacket);

  return sess->snac_nextid;
}

faim_export unsigned long aim_auth_setemail(struct aim_session_t *sess,
					    struct aim_conn_t *conn, 
					    char *newemail)
{
  struct command_tx_struct *newpacket;
  int i;

  if (!(newpacket = aim_tx_new(sess, conn, AIM_FRAMETYPE_OSCAR, 0x0002, 10+2+2+strlen(newemail))))
    return -1;

  newpacket->lock = 1;

  i = aim_putsnac(newpacket->data, 0x0007, 0x0004, 0x0000, sess->snac_nextid);
  aim_cachesnac(sess, 0x0007, 0x0004, 0x0000, NULL, 0);

  i += aim_puttlv_str(newpacket->data+i, 0x0011, strlen(newemail), newemail);

  aim_tx_enqueue(sess, newpacket);

  return sess->snac_nextid;
}
