
/*
 * aim_search.c
 *
 * TODO: Add aim_usersearch_name()
 *
 */

#define FAIM_INTERNAL
#include <aim.h>

faim_export unsigned long aim_usersearch_address(struct aim_session_t *sess,
						 struct aim_conn_t *conn, 
						 char *address)
{
  struct command_tx_struct *newpacket;
  
  if (!address)
    return -1;

  if (!(newpacket = aim_tx_new(sess, conn, AIM_FRAMETYPE_OSCAR, 0x0002, 10+strlen(address))))
    return -1;

  newpacket->lock = 1;

  aim_putsnac(newpacket->data, 0x000a, 0x0002, 0x0000, sess->snac_nextid);

  aimutil_putstr(newpacket->data+10, address, strlen(address));

  aim_tx_enqueue(sess, newpacket);

  aim_cachesnac(sess, 0x000a, 0x0002, 0x0000, address, strlen(address)+1);

  return sess->snac_nextid;
}

/* XXX can this be integrated with the rest of the error handling? */
static int error(struct aim_session_t *sess, aim_module_t *mod, struct command_rx_struct *rx, aim_modsnac_t *snac, unsigned char *data, int datalen)
{
  int ret = 0;
  rxcallback_t userfunc;
  struct aim_snac_t *snac2;

  /* XXX the modules interface should have already retrieved this for us */
  if(!(snac2 = aim_remsnac(sess, snac->id))) {
    faimdprintf(sess, 2, "couldn't get a snac for 0x%08lx\n", snac->id);
    return 0;
  }

  if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
    ret = userfunc(sess, rx, snac2->data /* address */);

  /* XXX freesnac()? */
  if (snac2) {
    if(snac2->data)
      free(snac2->data);
    free(snac2);
  }

  return ret;
}

static int reply(struct aim_session_t *sess, aim_module_t *mod, struct command_rx_struct *rx, aim_modsnac_t *snac, unsigned char *data, int datalen)
{
  unsigned int j, m, ret = 0;
  struct aim_tlvlist_t *tlvlist;
  char *cur = NULL, *buf = NULL;
  rxcallback_t userfunc;
  struct aim_snac_t *snac2;

  if (!(snac2 = aim_remsnac(sess, snac->id))) {
    faimdprintf(sess, 2, "faim: couldn't get a snac for 0x%08lx\n", snac->id);
    return 0;
  }

  if (!(tlvlist = aim_readtlvchain(data, datalen)))
    return 0;

  j = 0;

  m = aim_counttlvchain(&tlvlist);

  while((cur = aim_gettlv_str(tlvlist, 0x0001, j+1)) && j < m) {
    if(!(buf = realloc(buf, (j+1) * (MAXSNLEN+1))))
      faimdprintf(sess, 2, "faim: couldn't realloc buf. oh well.\n");

    strncpy(&buf[j * (MAXSNLEN+1)], cur, MAXSNLEN);
    free(cur);

    j++; 
  }

  aim_freetlvchain(&tlvlist);

  if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
    ret = userfunc(sess, rx, snac2->data /* address */, j, buf);

  /* XXX freesnac()? */
  if(snac2) {
    if(snac2->data)
      free(snac2->data);
    free(snac2);
  }

  if(buf)
    free(buf);

  return ret;
}

static int snachandler(struct aim_session_t *sess, aim_module_t *mod, struct command_rx_struct *rx, aim_modsnac_t *snac, unsigned char *data, int datalen)
{

  if (snac->subtype == 0x0001)
    return error(sess, mod, rx, snac, data, datalen);
  else if (snac->subtype == 0x0003)
    return reply(sess, mod, rx, snac, data, datalen);

  return 0;
}

faim_internal int search_modfirst(struct aim_session_t *sess, aim_module_t *mod)
{

  mod->family = 0x000a;
  mod->version = 0x0000;
  mod->flags = 0;
  strncpy(mod->name, "search", sizeof(mod->name));
  mod->snachandler = snachandler;

  return 0;
}
