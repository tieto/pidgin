/*
  aim_auth.c

  Deals with the authorizer.

 */

#include <faim/aim.h> 

/* this just pushes the passed cookie onto the passed connection -- NO SNAC! */
int aim_auth_sendcookie(struct aim_session_t *sess, 
			struct aim_conn_t *conn, 
			u_char *chipsahoy)
{
  struct command_tx_struct *newpacket;
  int curbyte=0;
  
  if (!(newpacket = aim_tx_new(AIM_FRAMETYPE_OSCAR, 0x0001, conn, 4+2+2+AIM_COOKIELEN)))
    return -1;

  newpacket->lock = 1;

  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0000);
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0001);
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0006);
  curbyte += aimutil_put16(newpacket->data+curbyte, AIM_COOKIELEN);
  memcpy(newpacket->data+curbyte, chipsahoy, AIM_COOKIELEN);

  return aim_tx_enqueue(sess, newpacket);
}

u_long aim_auth_clientready(struct aim_session_t *sess,
			    struct aim_conn_t *conn)
{
  struct command_tx_struct *newpacket;
  int curbyte = 0;

  if (!(newpacket = aim_tx_new(AIM_FRAMETYPE_OSCAR, 0x0002, conn, 26)))
    return -1;

  newpacket->lock = 1;

  curbyte += aim_putsnac(newpacket->data+curbyte, 0x0001, 0x0002, 0x0000, sess->snac_nextid);
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0001);
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0002);
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0001);
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0013);
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0007);
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0001);
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0001);
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0001);

  aim_tx_enqueue(sess, newpacket);

  {
    struct aim_snac_t snac;
    
    snac.id = sess->snac_nextid;
    snac.family = 0x0001;
    snac.type = 0x0004;
    snac.flags = 0x0000;

    snac.data = NULL;

    aim_newsnac(sess, &snac);
  }

  return (sess->snac_nextid++);
}

u_long aim_auth_changepasswd(struct aim_session_t *sess,
			     struct aim_conn_t *conn, 
			     char *new, char *current)
{
  struct command_tx_struct *newpacket;
  int i;

  if (!(newpacket = aim_tx_new(AIM_FRAMETYPE_OSCAR, 0x0002, conn, 10+4+strlen(current)+4+strlen(new))))
    return -1;

  newpacket->lock = 1;

  i = aim_putsnac(newpacket->data, 0x0007, 0x0004, 0x0000, sess->snac_nextid);

  /* current password TLV t(0002) */
  i += aim_puttlv_str(newpacket->data+i, 0x0002, strlen(current), current);

  /* new password TLV t(0012) */
  i += aim_puttlv_str(newpacket->data+i, 0x0012, strlen(new), new);

  aim_tx_enqueue(sess, newpacket);

  {
    struct aim_snac_t snac;
    
    snac.id = sess->snac_nextid;
    snac.family = 0x0001;
    snac.type = 0x0004;
    snac.flags = 0x0000;

    snac.data = NULL;

    aim_newsnac(sess, &snac);
  }

  return (sess->snac_nextid++);
}
