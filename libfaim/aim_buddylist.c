
#include <aim.h>

/*
 * aim_add_buddy()
 *
 * Adds a single buddy to your buddy list after login.
 *
 */
u_long aim_add_buddy(struct aim_conn_t *conn, char *sn )
{
   struct command_tx_struct newpacket;

   if( !sn )
      return -1;

   if (conn)
     newpacket.conn = conn;
   else
     newpacket.conn = aim_getconn_type(AIM_CONN_TYPE_BOS);

   newpacket.lock = 1;
   newpacket.type = 0x0002;
   newpacket.commandlen = 11 + strlen( sn );
   newpacket.data = (char *)malloc( newpacket.commandlen );

   newpacket.data[0] = 0x00;
   newpacket.data[1] = 0x03;
   newpacket.data[2] = 0x00;
   newpacket.data[3] = 0x04;
   newpacket.data[4] = 0x00;
   newpacket.data[5] = 0x00;

   /* SNAC reqid */
   newpacket.data[6] = (aim_snac_nextid >> 24) & 0xFF;
   newpacket.data[7] = (aim_snac_nextid >> 16) & 0xFF;
   newpacket.data[8] = (aim_snac_nextid >>  8) & 0xFF;
   newpacket.data[9] = (aim_snac_nextid) & 0xFF;

   /* length of screenname */ 
   newpacket.data[10] = strlen( sn );

   memcpy( &(newpacket.data[11]), sn, strlen( sn ) );

   aim_tx_enqueue( &newpacket );

   {
      struct aim_snac_t snac;
    
      snac.id = aim_snac_nextid;
      snac.family = 0x0003;
      snac.type = 0x0004;
      snac.flags = 0x0000;

      snac.data = malloc( strlen( sn ) + 1 );
      memcpy( snac.data, sn, strlen( sn ) + 1 );

      aim_newsnac( &snac );
   }

   return( aim_snac_nextid++ );
}

u_long aim_remove_buddy(struct aim_conn_t *conn, char *sn )
{
   struct command_tx_struct newpacket;

   if( !sn )
      return -1;

   if (conn)
     newpacket.conn = conn;
   else
     newpacket.conn = aim_getconn_type(AIM_CONN_TYPE_BOS);

   newpacket.lock = 1;
   newpacket.type = 0x0002;
   newpacket.commandlen = 11 + strlen(sn);
   newpacket.data = (char *)malloc( newpacket.commandlen );

   newpacket.data[0] = 0x00;
   newpacket.data[1] = 0x03;
   newpacket.data[2] = 0x00;
   newpacket.data[3] = 0x05;
   newpacket.data[4] = 0x00;
   newpacket.data[5] = 0x00;

   /* SNAC reqid */
   newpacket.data[6] = (aim_snac_nextid >> 24) & 0xFF;
   newpacket.data[7] = (aim_snac_nextid >> 16) & 0xFF;
   newpacket.data[8] = (aim_snac_nextid >>  8) & 0xFF;
   newpacket.data[9] = (aim_snac_nextid) & 0xFF;

   /* length of screenname */ 
   newpacket.data[10] = strlen( sn );

   memcpy( &(newpacket.data[11]), sn, strlen( sn ) );

   aim_tx_enqueue( &newpacket );

   {
      struct aim_snac_t snac;
    
      snac.id = aim_snac_nextid;
      snac.family = 0x0003;
      snac.type = 0x0005;
      snac.flags = 0x0000;

      snac.data = malloc( strlen( sn ) + 1 );
      memcpy( snac.data, sn, strlen( sn ) + 1 );

      aim_newsnac( &snac );
   }

   return( aim_snac_nextid++ );
}

