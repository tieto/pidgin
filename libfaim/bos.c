
#define FAIM_INTERNAL
#include <aim.h>

/* 
 * aim_bos_setgroupperm(mask)
 * 
 * Set group permisson mask.  Normally 0x1f (all classes).
 *
 * The group permission mask allows you to keep users of a certain
 * class or classes from talking to you.  The mask should be
 * a bitwise OR of all the user classes you want to see you.
 *
 */
faim_export unsigned long aim_bos_setgroupperm(struct aim_session_t *sess,
					       struct aim_conn_t *conn, 
					       u_long mask)
{
  return aim_genericreq_l(sess, conn, 0x0009, 0x0004, &mask);
}

static int rights(struct aim_session_t *sess, aim_module_t *mod, struct command_rx_struct *rx, aim_modsnac_t *snac, unsigned char *data, int datalen)
{
  aim_rxcallback_t userfunc;
  int ret = 0;
  struct aim_tlvlist_t *tlvlist;
  unsigned short maxpermits = 0, maxdenies = 0;

  /* 
   * TLVs follow 
   */
  if (!(tlvlist = aim_readtlvchain(data, datalen)))
    return 0;

  /*
   * TLV type 0x0001: Maximum number of buddies on permit list.
   */
  if (aim_gettlv(tlvlist, 0x0001, 1))
    maxpermits = aim_gettlv16(tlvlist, 0x0001, 1);

  /*
   * TLV type 0x0002: Maximum number of buddies on deny list.
   *
   */
  if (aim_gettlv(tlvlist, 0x0002, 1)) 
    maxdenies = aim_gettlv16(tlvlist, 0x0002, 1);
  
  if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
    ret = userfunc(sess, rx, maxpermits, maxdenies);

  aim_freetlvchain(&tlvlist);

  return ret;  
}

static int snachandler(struct aim_session_t *sess, aim_module_t *mod, struct command_rx_struct *rx, aim_modsnac_t *snac, unsigned char *data, int datalen)
{

  if (snac->subtype == 0x0003)
    return rights(sess, mod, rx, snac, data, datalen);

  return 0;
}

faim_internal int bos_modfirst(struct aim_session_t *sess, aim_module_t *mod)
{

  mod->family = 0x0009;
  mod->version = 0x0000;
  mod->flags = 0;
  strncpy(mod->name, "bos", sizeof(mod->name));
  mod->snachandler = snachandler;

  return 0;
}
