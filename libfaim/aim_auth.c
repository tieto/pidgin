/*
  aim_auth.c

  Deals with the authorizer.

 */

#include "aim.h"

/* this just pushes the passed cookie onto the passed connection -- NO SNAC! */
int aim_auth_sendcookie(struct aim_conn_t *conn, char *chipsahoy)
{
  struct command_tx_struct newpacket;
  int curbyte=0;
  
  newpacket.lock = 1;

  if (conn==NULL)
    newpacket.conn = aim_getconn_type(AIM_CONN_TYPE_AUTH);
  else
    newpacket.conn = conn;

  newpacket.type = 0x0001;  /* channel 1 (no SNACs, you know) */
  
  newpacket.commandlen = 4 + 2 + 2 + 0x100;
  newpacket.data = (char *) calloc(1, newpacket.commandlen);
  
  curbyte += aimutil_put16(newpacket.data+curbyte, 0x0000);
  curbyte += aimutil_put16(newpacket.data+curbyte, 0x0001);
  curbyte += aimutil_put16(newpacket.data+curbyte, 0x0006);
  curbyte += aimutil_put16(newpacket.data+curbyte, 0x0100);
  memcpy(&(newpacket.data[curbyte]), chipsahoy, 0x100);

  aim_tx_enqueue(&newpacket);
  
  return 0;
}

u_long aim_auth_clientready(struct aim_conn_t *conn)
{
  struct command_tx_struct newpacket;
  int curbyte = 0;

  newpacket.lock = 1;

  if (conn==NULL)
    newpacket.conn = aim_getconn_type(AIM_CONN_TYPE_AUTH);
  else
    newpacket.conn = conn;

  newpacket.type = 0x0002;
  
  newpacket.commandlen = 26;
  newpacket.data = (char *) malloc(newpacket.commandlen);
  
  curbyte += aim_putsnac(newpacket.data+curbyte, 0x0001, 0x0002, 0x0000, aim_snac_nextid);
  curbyte += aimutil_put16(newpacket.data+curbyte, 0x0001);
  curbyte += aimutil_put16(newpacket.data+curbyte, 0x0002);
  curbyte += aimutil_put16(newpacket.data+curbyte, 0x0001);
  curbyte += aimutil_put16(newpacket.data+curbyte, 0x0013);
  curbyte += aimutil_put16(newpacket.data+curbyte, 0x0007);
  curbyte += aimutil_put16(newpacket.data+curbyte, 0x0001);
  curbyte += aimutil_put16(newpacket.data+curbyte, 0x0001);
  curbyte += aimutil_put16(newpacket.data+curbyte, 0x0001);

  aim_tx_enqueue(&newpacket);

  {
    struct aim_snac_t snac;
    
    snac.id = aim_snac_nextid;
    snac.family = 0x0001;
    snac.type = 0x0004;
    snac.flags = 0x0000;

    snac.data = NULL;

    aim_newsnac(&snac);
  }

  return (aim_snac_nextid++);
}

u_long aim_auth_changepasswd(struct aim_conn_t *conn, char *new, char *current)
{
  struct command_tx_struct newpacket;
  int i;

  newpacket.lock = 1;

  if (conn==NULL)
    newpacket.conn = aim_getconn_type(AIM_CONN_TYPE_AUTH);
  else
    newpacket.conn = conn;

  newpacket.type = 0x0002;
  
  newpacket.commandlen = 10 + 4 + strlen(current) + 4 + strlen(new);
  newpacket.data = (char *) malloc(newpacket.commandlen);

  newpacket.data[0] = 0x00;
  newpacket.data[1] = 0x07;

  newpacket.data[2] = 0x00;
  newpacket.data[3] = 0x04;

  newpacket.data[4] = 0x00;
  newpacket.data[5] = 0x00;

  /* SNAC reqid */
  newpacket.data[6] = (aim_snac_nextid >> 24) & 0xFF;
  newpacket.data[7] = (aim_snac_nextid >> 16) & 0xFF;
  newpacket.data[8] = (aim_snac_nextid >>  8) & 0xFF;
  newpacket.data[9] = (aim_snac_nextid) & 0xFF;

  /* current password TLV t(0002) */
  i = 10;
  newpacket.data[i++] = 0x00;
  newpacket.data[i++] = 0x02;
  newpacket.data[i++] = 0x00;
  newpacket.data[i++] = strlen(current) & 0xff;
  memcpy(&(newpacket.data[i]), current, strlen(current));
  i += strlen(current);

  /* new password TLV t(0012) */
  newpacket.data[i++] = 0x00;
  newpacket.data[i++] = 0x12;
  newpacket.data[i++] = 0x00;
  newpacket.data[i++] = strlen(new) & 0xff;
  memcpy(&(newpacket.data[i]), new, strlen(new));
  i+=strlen(new);

  aim_tx_enqueue(&newpacket);

  {
    struct aim_snac_t snac;
    
    snac.id = aim_snac_nextid;
    snac.family = 0x0001;
    snac.type = 0x0004;
    snac.flags = 0x0000;

    snac.data = NULL;

    aim_newsnac(&snac);
  }

  return (aim_snac_nextid++);
}
