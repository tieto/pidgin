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
