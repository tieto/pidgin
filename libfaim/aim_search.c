
/*
 * aim_search.c
 *
 * TODO: Add aim_usersearch_name()
 *
 */

#include <aim.h>

u_long aim_usersearch_address(struct aim_conn_t *conn, char *address)
{
  struct command_tx_struct newpacket;
  
  if (!address)
    return -1;

  newpacket.lock = 1;

  if (conn)
    newpacket.conn = conn;
  else
    newpacket.conn = aim_getconn_type(AIM_CONN_TYPE_BOS);

  newpacket.type = 0x0002;
  
  newpacket.commandlen = 10 + strlen(address);
  newpacket.data = (char *) malloc(newpacket.commandlen);

  newpacket.data[0] = 0x00;
  newpacket.data[1] = 0x0a;
  newpacket.data[2] = 0x00;
  newpacket.data[3] = 0x02;
  newpacket.data[4] = 0x00;
  newpacket.data[5] = 0x00;

  /* SNAC reqid */
  newpacket.data[6] = (aim_snac_nextid >> 24) & 0xFF;
  newpacket.data[7] = (aim_snac_nextid >> 16) & 0xFF;
  newpacket.data[8] = (aim_snac_nextid >>  8) & 0xFF;
  newpacket.data[9] = (aim_snac_nextid) & 0xFF;

  memcpy(&(newpacket.data[10]), address, strlen(address));

  aim_tx_enqueue(&newpacket);

  {
    struct aim_snac_t snac;
    
    snac.id = aim_snac_nextid;
    snac.family = 0x000a;
    snac.type = 0x0002;
    snac.flags = 0x0000;

    snac.data = malloc(strlen(address)+1);
    memcpy(snac.data, address, strlen(address)+1);

    aim_newsnac(&snac);
  }

  return (aim_snac_nextid++);
}

