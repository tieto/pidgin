/*
 * aim_chat.c
 *
 * Routines for the Chat service.  Nothing works (yet).
 *
 */

#include "aim.h"

/*
 * FIXME: Doesn't work.
 *
 */
u_long aim_chat_join(struct aim_conn_t *conn, const char *roomname)
{
  struct command_tx_struct newpacket;
  
  newpacket.lock = 1;
  if (conn)
    newpacket.conn = conn;
  else
    newpacket.conn = aim_getconn_type(AIM_CONN_TYPE_BOS);
  newpacket.type = 0x0002;
  
  newpacket.commandlen = 12+7+strlen(roomname)+6;
  newpacket.data = (char *) malloc(newpacket.commandlen);
  memset(newpacket.data, 0x00, newpacket.commandlen);

  newpacket.data[0] = 0x00;
  newpacket.data[1] = 0x01;

  newpacket.data[2] = 0x00;
  newpacket.data[3] = 0x04;

  newpacket.data[4] = 0x00;
  newpacket.data[5] = 0x00;

  /* SNAC reqid */
  newpacket.data[6] = (aim_snac_nextid >> 24) & 0xFF;
  newpacket.data[7] = (aim_snac_nextid >> 16) & 0xFF;
  newpacket.data[8] = (aim_snac_nextid >>  8) & 0xFF;
  newpacket.data[9] = (aim_snac_nextid) & 0xFF;

  newpacket.data[10] = 0x00;
  newpacket.data[11] = 0x0e;
  newpacket.data[12] = 0x00;
  newpacket.data[13] = 0x01;
  newpacket.data[14] = 0x00;
  newpacket.data[15] = 0x0c;
  newpacket.data[16] = 0x00;
  newpacket.data[17] = 0x04;
  newpacket.data[18] = strlen(roomname) & 0x00ff;
  memcpy(&(newpacket.data[19]), roomname, strlen(roomname));
  
  {
    int i = 0;
    printf("\n\n\n");
    for (i = 0;i < newpacket.commandlen; i++)
      printf("0x%02x ", newpacket.data[i]);
    printf("\n\n\n");
  }

  aim_tx_enqueue(&newpacket);

  {
    struct aim_snac_t snac;
    
    snac.id = aim_snac_nextid;
    snac.family = 0x0001;
    snac.type = 0x0004;
    snac.flags = 0x0000;

    snac.data = malloc(strlen(roomname));
    memcpy(snac.data, roomname, strlen(roomname));

    aim_newsnac(&snac);
  }

  return (aim_snac_nextid++);
}
