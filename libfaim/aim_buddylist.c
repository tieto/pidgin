
#include <faim/aim.h>

/*
 * aim_add_buddy()
 *
 * Adds a single buddy to your buddy list after login.
 *
 */
u_long aim_add_buddy(struct aim_session_t *sess,
		     struct aim_conn_t *conn, 
		     char *sn )
{
   struct command_tx_struct *newpacket;
   int i;

   if(!sn)
     return -1;

   if (!(newpacket = aim_tx_new(AIM_FRAMETYPE_OSCAR, 0x0002, conn, 10+1+strlen(sn))))
     return -1;

   newpacket->lock = 1;

   i = aim_putsnac(newpacket->data, 0x0003, 0x0004, 0x0000, sess->snac_nextid);
   i += aimutil_put8(newpacket->data+i, strlen(sn));
   i += aimutil_putstr(newpacket->data+i, sn, strlen(sn));

   aim_tx_enqueue(sess, newpacket );

#if 0 /* do we really need this code? */
   {
      struct aim_snac_t snac;
    
      snac.id = sess->snac_nextid;
      snac.family = 0x0003;
      snac.type = 0x0004;
      snac.flags = 0x0000;

      snac.data = malloc( strlen( sn ) + 1 );
      memcpy( snac.data, sn, strlen( sn ) + 1 );

      aim_newsnac(sess, &snac);
   }
#endif

   return( sess->snac_nextid++ );
}

u_long aim_remove_buddy(struct aim_session_t *sess,
			struct aim_conn_t *conn, 
			char *sn )
{
   struct command_tx_struct *newpacket;
   int i;

   if(!sn)
     return -1;

   if (!(newpacket = aim_tx_new(AIM_FRAMETYPE_OSCAR, 0x0002, conn, 10+1+strlen(sn))))
     return -1;

   newpacket->lock = 1;

   i = aim_putsnac(newpacket->data, 0x0003, 0x0005, 0x0000, sess->snac_nextid);

   i += aimutil_put8(newpacket->data+i, strlen(sn));
   i += aimutil_putstr(newpacket->data+i, sn, strlen(sn));

   aim_tx_enqueue(sess, newpacket);

   {
      struct aim_snac_t snac;
    
      snac.id = sess->snac_nextid;
      snac.family = 0x0003;
      snac.type = 0x0005;
      snac.flags = 0x0000;

      snac.data = malloc( strlen( sn ) + 1 );
      memcpy( snac.data, sn, strlen( sn ) + 1 );

      aim_newsnac(sess, &snac );
   }

   return( sess->snac_nextid++ );
}

