/*
  aim_info.c

  The functions here are responsible for requesting and parsing information-
  gathering SNACs.  
  
 */


#include "aim.h" /* for most everything */

u_long aim_getinfo(struct aim_conn_t *conn, const char *sn)
{
  struct command_tx_struct newpacket;

  if (conn)
    newpacket.conn = conn;
  else
    newpacket.conn = aim_getconn_type(AIM_CONN_TYPE_BOS);

  newpacket.lock = 1;
  newpacket.type = 0x0002;

  newpacket.commandlen = 12 + 1 + strlen(sn);
  newpacket.data = (char *) malloc(newpacket.commandlen);

  newpacket.data[0] = 0x00;
  newpacket.data[1] = 0x02;
  newpacket.data[2] = 0x00;
  newpacket.data[3] = 0x05;
  newpacket.data[4] = 0x00;
  newpacket.data[5] = 0x00;

  /* SNAC reqid */
  newpacket.data[6] = (aim_snac_nextid >> 24) & 0xFF;
  newpacket.data[7] = (aim_snac_nextid >> 16) & 0xFF;
  newpacket.data[8] = (aim_snac_nextid >>  8) & 0xFF;
  newpacket.data[9] = (aim_snac_nextid) & 0xFF;

  /* TLV: Screen Name */
  /* t(0x0001) */
  newpacket.data[10] = 0x00;
  newpacket.data[11] = 0x01; 
  /* l() */
  newpacket.data[12] = strlen(sn);
  /* v() */
  memcpy(&(newpacket.data[13]), sn, strlen(sn));

  aim_tx_enqueue(&newpacket);

  {
    struct aim_snac_t snac;
    
    snac.id = aim_snac_nextid;
    snac.family = 0x0002;
    snac.type = 0x0005;
    snac.flags = 0x0000;

    snac.data = malloc(strlen(sn)+1);
    memcpy(snac.data, sn, strlen(sn)+1);

    aim_newsnac(&snac);
  }

  return (aim_snac_nextid++);
}

/*
 * This parses the user info stuff out all nice and pretty then calls 
 * the higher-level callback (in the user app).
 *
 */

int aim_parse_userinfo_middle(struct command_rx_struct *command)
{
  char *sn = NULL;
  char *prof_encoding = NULL;
  char *prof = NULL;
  u_short warnlevel = 0x0000;
  u_short idletime = 0x0000;
  u_short class = 0x0000;
  u_long membersince = 0x00000000;
  u_long onlinesince = 0x00000000;
  int tlvcnt = 0;
  int i = 0;

  {
    u_long snacid = 0x000000000;
    struct aim_snac_t *snac = NULL;

    snacid = (command->data[6] << 24) & 0xFF000000;
    snacid+= (command->data[7] << 16) & 0x00FF0000;
    snacid+= (command->data[8] <<  8) & 0x0000FF00;
    snacid+= (command->data[9])       & 0x000000FF;

    snac = aim_remsnac(snacid);

    free(snac->data);
    free(snac);

  }

  sn = (char *) malloc(command->data[10]+1);
  memcpy(sn, &(command->data[11]), command->data[10]);
  sn[(int)command->data[10]] = '\0';
  
  i = 11 + command->data[10];
  warnlevel = ((command->data[i++]) << 8) & 0xFF00;
  warnlevel += (command->data[i++]) & 0x00FF;

  tlvcnt = ((command->data[i++]) << 8) & 0xFF00;
  tlvcnt += (command->data[i++]) & 0x00FF;

  /* a mini TLV parser */
  {
    int curtlv = 0;
    int tlv1 = 0;

    while (curtlv < tlvcnt)
      {
	if ((command->data[i] == 0x00) &&
	    (command->data[i+1] == 0x01) )
	  {
	    if (tlv1)
	      break;
	    /* t(0001) = class */
	    class = ((command->data[i+4]) << 8) & 0xFF00;
	    class += (command->data[i+5]) & 0x00FF;
	    i += (2 + 2 + command->data[i+3]);
	    tlv1++;
	  }
	else if ((command->data[i] == 0x00) &&
		 (command->data[i+1] == 0x02))
	  {
	    /* t(0002) = member since date  */
	    if (command->data[i+3] != 0x04)
	      printf("faim: userinfo: **warning: strange v(%x) for t(2)\n", command->data[i+3]);

	    membersince = ((command->data[i+4]) << 24) &  0xFF000000;
	    membersince += ((command->data[i+5]) << 16) & 0x00FF0000;
	    membersince += ((command->data[i+6]) << 8) &  0x0000FF00;
	    membersince += ((command->data[i+7]) ) &      0x000000FF;
	    i += (2 + 2 + command->data[i+3]);
	  }
	else if ((command->data[i] == 0x00) &&
		 (command->data[i+1] == 0x03))
	  {
	    /* t(0003) = on since date  */
	    if (command->data[i+3] != 0x04)
	      printf("faim: userinfo: **warning: strange v(%x) for t(3)\n", command->data[i+3]);

	    onlinesince = ((command->data[i+4]) << 24) &  0xFF000000;
	    onlinesince += ((command->data[i+5]) << 16) & 0x00FF0000;
	    onlinesince += ((command->data[i+6]) << 8) &  0x0000FF00;
	    onlinesince += ((command->data[i+7]) ) &      0x000000FF;
	    i += (2 + 2 + command->data[i+3]);
	  }
	else if ((command->data[i] == 0x00) &&
		 (command->data[i+1] == 0x04) )
	  {
	    /* t(0004) = idle time */
	    if (command->data[i+3] != 0x02)
	      printf("faim: userinfo: **warning: strange v(%x) for t(4)\n", command->data[i+3]);
	    idletime = ((command->data[i+4]) << 8) & 0xFF00;
	    idletime += (command->data[i+5]) & 0x00FF;
	    i += (2 + 2 + command->data[i+3]);
	  }  
	else
	  {
	    printf("faim: userinfo: **warning: unexpected TLV t(%02x%02x) l(%02x%02x)\n", command->data[i], command->data[i+1], command->data[i+2], command->data[i+3]);
	    i += (2 + 2 + command->data[i+3]);
	  }
	curtlv++;
      }
  }
  if (i < command->commandlen)
    {
      if ( (command->data[i] == 0x00) &&
	   (command->data[i+1] == 0x01) )
	{
	  int len = 0;
	  
	  len = ((command->data[i+2] << 8) & 0xFF00);
	  len += (command->data[i+3]) & 0x00FF;
	  
	  prof_encoding = (char *) malloc(len+1);
	  memcpy(prof_encoding, &(command->data[i+4]), len);
	  prof_encoding[len] = '\0';
	  
	  i += (2+2+len);
	}
      else
	{
	  printf("faim: userinfo: **warning: unexpected TLV after TLVblock t(%02x%02x) l(%02x%02x)\n", command->data[i], command->data[i+1], command->data[i+2], command->data[i+3]);
	  i += 2 + 2 + command->data[i+3];
	}
    }

  if (i < command->commandlen)
    {
      int len = 0;

      len = ((command->data[i+2]) << 8) & 0xFF00;
      len += (command->data[i+3]) & 0x00FF;

      prof = (char *) malloc(len+1);
      memcpy(prof, &(command->data[i+4]), len);
      prof[len] = '\0';
    }
  else
    printf("faim: userinfo: **early parse abort...no profile?\n");

  i = (aim_callbacks[AIM_CB_USERINFO])(command, sn, prof_encoding, prof, warnlevel, idletime, class, membersince, onlinesince);

  free(sn);
  free(prof_encoding);
  free(prof);

  return i;
}
