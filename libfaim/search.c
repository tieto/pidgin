
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


faim_internal unsigned long aim_parse_searcherror(struct aim_session_t *sess, struct command_rx_struct *command) 
{
  u_int i, ret;
  int snacid;
  rxcallback_t userfunc;
  struct aim_snac_t *snac;

  i = 6;

  snacid = aimutil_get32(command->data+i);
  i += 4;
  
  if(!(snac = aim_remsnac(sess, snacid))) {
    faimdprintf(sess, 2, "faim: couldn't get a snac for %d, probably should crash.\n", snacid);
    return 0;
  }

  if((userfunc = aim_callhandler(sess, command->conn, 0x000a, 0x0001)))
    ret = userfunc(sess, command, snac->data /* address */);
  else
    ret = 0;

  if(snac) {
    if(snac->data)
      free(snac->data);
    free(snac);
  }

  return ret;
}
	
  
faim_internal unsigned long aim_parse_searchreply(struct aim_session_t *sess, struct command_rx_struct *command)
{
  u_int i, j, m, ret;
  int snacid;
  struct aim_tlvlist_t *tlvlist;
  char *cur = NULL, *buf = NULL;
  rxcallback_t userfunc;
  struct aim_snac_t *snac;

  i = 6;

  snacid = aimutil_get32(command->data+i);
  i += 4;

  if(!(snac = aim_remsnac(sess, snacid))) {
    faimdprintf(sess, 2, "faim: couldn't get a snac for %d, probably should crash.\n", snacid);
    return 0;
  }

  tlvlist = aim_readtlvchain(command->data+i, command->commandlen-i);

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

  if((userfunc = aim_callhandler(sess, command->conn, 0x000a, 0x0003)))
    ret = userfunc(sess, command, snac->data /* address */, j, buf);
  else
    ret = 0;

  if(snac) {
    if(snac->data)
      free(snac->data);
    free(snac);
  }

  if(buf)
    free(buf);

  return ret;
}
