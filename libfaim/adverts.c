/*
 *
 *
 */

#define FAIM_INTERNAL
#include <aim.h>

faim_export unsigned long aim_ads_clientready(struct aim_session_t *sess,
					      struct aim_conn_t *conn)
{
  struct command_tx_struct *newpacket;
  int i;

  if (!(newpacket = aim_tx_new(sess, conn, AIM_FRAMETYPE_OSCAR, 0x0002, 0x1a)))
    return -1;

  newpacket->lock = 1;

  i = aim_putsnac(newpacket->data, 0x0001, 0x0002, 0x0000, sess->snac_nextid);

  i+= aimutil_put16(newpacket->data+i, 0x0001);
  i+= aimutil_put16(newpacket->data+i, 0x0002);

  i+= aimutil_put16(newpacket->data+i, 0x0001);
  i+= aimutil_put16(newpacket->data+i, 0x0013);

  i+= aimutil_put16(newpacket->data+i, 0x0005);
  i+= aimutil_put16(newpacket->data+i, 0x0001);
  i+= aimutil_put16(newpacket->data+i, 0x0001);
  i+= aimutil_put16(newpacket->data+i, 0x0001);

  newpacket->lock = 0;
  aim_tx_enqueue(sess, newpacket);

  return (sess->snac_nextid++);
}

faim_export unsigned long aim_ads_requestads(struct aim_session_t *sess,
					     struct aim_conn_t *conn)
{
  return aim_genericreq_n(sess, conn, 0x0005, 0x0002);
}
