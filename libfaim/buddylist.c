
#define FAIM_INTERNAL
#include <aim.h>

/*
 * Oncoming Buddy notifications contain a subset of the
 * user information structure.  Its close enough to run
 * through aim_extractuserinfo() however.
 *
 * Although the offgoing notification contains no information,
 * it is still in a format parsable by extractuserinfo.
 *
 */
static int buddychange(struct aim_session_t *sess, aim_module_t *mod, struct command_rx_struct *rx, aim_modsnac_t *snac, unsigned char *data, int datalen)
{
  struct aim_userinfo_s userinfo;
  rxcallback_t userfunc;

  aim_extractuserinfo(sess, data, &userinfo);

  if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
    return userfunc(sess, rx, &userinfo);

  return 0;
}

static int rights(struct aim_session_t *sess, aim_module_t *mod, struct command_rx_struct *rx, aim_modsnac_t *snac, unsigned char *data, int datalen)
{
  rxcallback_t userfunc;
  struct aim_tlvlist_t *tlvlist;
  unsigned short maxbuddies = 0, maxwatchers = 0;
  int ret = 0;

  /* 
   * TLVs follow 
   */
  if (!(tlvlist = aim_readtlvchain(data, datalen)))
    return 0;

  /*
   * TLV type 0x0001: Maximum number of buddies.
   */
  if (aim_gettlv(tlvlist, 0x0001, 1))
    maxbuddies = aim_gettlv16(tlvlist, 0x0001, 1);

  /*
   * TLV type 0x0002: Maximum number of watchers.
   *
   * XXX: what the hell is a watcher? 
   *
   */
  if (aim_gettlv(tlvlist, 0x0002, 1))
    maxwatchers = aim_gettlv16(tlvlist, 0x0002, 1);
  
  if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
    ret = userfunc(sess, rx, maxbuddies, maxwatchers);

  aim_freetlvchain(&tlvlist);

  return ret;  
}

static int snachandler(struct aim_session_t *sess, aim_module_t *mod, struct command_rx_struct *rx, aim_modsnac_t *snac, unsigned char *data, int datalen)
{

  if (snac->subtype == 0x0003)
    return rights(sess, mod, rx, snac, data, datalen);
  else if ((snac->subtype == 0x000b) || (snac->subtype == 0x000c))
    return buddychange(sess, mod, rx, snac, data, datalen);

  return 0;
}

faim_internal int buddylist_modfirst(struct aim_session_t *sess, aim_module_t *mod)
{

  mod->family = 0x0003;
  mod->version = 0x0000;
  mod->flags = 0;
  strncpy(mod->name, "buddylist", sizeof(mod->name));
  mod->snachandler = snachandler;

  return 0;
}

/*
 * aim_add_buddy()
 *
 * Adds a single buddy to your buddy list after login.
 *
 * XXX this should just be an extension of setbuddylist()
 *
 */
faim_export unsigned long aim_add_buddy(struct aim_session_t *sess,
					struct aim_conn_t *conn, 
					char *sn )
{
   struct command_tx_struct *newpacket;
   int i;

   if(!sn)
     return -1;

   if (!(newpacket = aim_tx_new(sess, conn, AIM_FRAMETYPE_OSCAR, 0x0002, 10+1+strlen(sn))))
     return -1;

   newpacket->lock = 1;

   i = aim_putsnac(newpacket->data, 0x0003, 0x0004, 0x0000, sess->snac_nextid);
   i += aimutil_put8(newpacket->data+i, strlen(sn));
   i += aimutil_putstr(newpacket->data+i, sn, strlen(sn));

   aim_tx_enqueue(sess, newpacket );

   aim_cachesnac(sess, 0x0003, 0x0004, 0x0000, sn, strlen(sn)+1);

   return sess->snac_nextid;
}

/*
 * XXX generalise to support removing multiple buddies (basically, its
 * the same as setbuddylist() but with a different snac subtype).
 *
 */
faim_export unsigned long aim_remove_buddy(struct aim_session_t *sess,
					   struct aim_conn_t *conn, 
					   char *sn )
{
   struct command_tx_struct *newpacket;
   int i;

   if(!sn)
     return -1;

   if (!(newpacket = aim_tx_new(sess, conn, AIM_FRAMETYPE_OSCAR, 0x0002, 10+1+strlen(sn))))
     return -1;

   newpacket->lock = 1;

   i = aim_putsnac(newpacket->data, 0x0003, 0x0005, 0x0000, sess->snac_nextid);

   i += aimutil_put8(newpacket->data+i, strlen(sn));
   i += aimutil_putstr(newpacket->data+i, sn, strlen(sn));

   aim_tx_enqueue(sess, newpacket);

   aim_cachesnac(sess, 0x0003, 0x0005, 0x0000, sn, strlen(sn)+1);

   return sess->snac_nextid;
}

