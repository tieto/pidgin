/*
 *
 *
 *
 *
 */

#include <faim/aim.h>


/*
 * conn must be a chatnav connection!
 */
faim_export unsigned long aim_chatnav_reqrights(struct aim_session_t *sess,
						struct aim_conn_t *conn)
{
  struct aim_snac_t snac;

  snac.id = aim_genericreq_n(sess, conn, 0x000d, 0x0002);

  snac.family = 0x000d;
  snac.type = 0x0002;
  snac.flags = 0x0000;
  snac.data = NULL;
  
  aim_newsnac(sess, &snac);

  return (sess->snac_nextid); /* already incremented */
}

faim_export unsigned long aim_chatnav_clientready(struct aim_session_t *sess,
						  struct aim_conn_t *conn)
{
  struct command_tx_struct *newpacket; 
  int i;

  if (!(newpacket = aim_tx_new(AIM_FRAMETYPE_OSCAR, 0x0002, conn, 0x20)))
    return -1;

  newpacket->lock = 1;

  i = aim_putsnac(newpacket->data, 0x0001, 0x0002, 0x0000, sess->snac_nextid);

  i+= aimutil_put16(newpacket->data+i, 0x000d);
  i+= aimutil_put16(newpacket->data+i, 0x0001);

  i+= aimutil_put16(newpacket->data+i, 0x0004);
  i+= aimutil_put16(newpacket->data+i, 0x0001);

  i+= aimutil_put16(newpacket->data+i, 0x0001);
  i+= aimutil_put16(newpacket->data+i, 0x0003);

  i+= aimutil_put16(newpacket->data+i, 0x0004);
  i+= aimutil_put16(newpacket->data+i, 0x0686);

  aim_tx_enqueue(sess, newpacket);

  return (sess->snac_nextid++);
}

/*
 * Since multiple things can trigger this callback,
 * we must lookup the snacid to determine the original
 * snac subtype that was called.
 */
faim_internal int aim_chatnav_parse_info(struct aim_session_t *sess, struct command_rx_struct *command)
{
  struct aim_snac_t *snac;
  u_long snacid;
  rxcallback_t userfunc;
  int ret=1;
  
  snacid = aimutil_get32(command->data+6);
  snac = aim_remsnac(sess, snacid);

  if (!snac)
    {
      printf("faim: chatnav_parse_info: received response to unknown request! (%08lx)\n", snacid);
      return 1;
    }
  
  if (snac->family != 0x000d)
    {
      printf("faim: chatnav_parse_info: recieved response that maps to corrupt request! (fam=%04x)\n", snac->family);
      return 1;
    }

  /*
   * We now know what the original SNAC subtype was.
   */
  switch(snac->type)
    {
    case 0x0002: /* request chat rights */
      {
	  struct aim_tlvlist_t *tlvlist;
	  struct aim_chat_exchangeinfo *exchanges = NULL;
	  int curexchange = 0;
	  struct aim_tlv_t *exchangetlv;
	  u_char maxrooms = 0;
	  struct aim_tlvlist_t *innerlist;
	 
	  tlvlist = aim_readtlvchain(command->data+10, command->commandlen-10);
	  
	  /* 
	   * Type 0x0002: Maximum concurrent rooms.
	   */ 
	  if (aim_gettlv(tlvlist, 0x0002, 1))
	    {
	      struct aim_tlv_t *maxroomstlv;
	      maxroomstlv = aim_gettlv(tlvlist, 0x0002, 1);
	      maxrooms = aimutil_get8(maxroomstlv->value);
	    }

	  /* 
	   * Type 0x0003: Exchange information
	   *
	   * There can be any number of these, each one
	   * representing another exchange.  
	   * 
	   */
	  curexchange = 0;
	  while ((exchangetlv = aim_gettlv(tlvlist, 0x0003, curexchange+1)))
	    {	
	      curexchange++;
	      exchanges = realloc(exchanges, curexchange * sizeof(struct aim_chat_exchangeinfo));
	      

	      /* exchange number */
	      exchanges[curexchange-1].number = aimutil_get16(exchangetlv->value);
	      innerlist = aim_readtlvchain(exchangetlv->value+2, exchangetlv->length-2);
	      
	      /* 
	       * Type 0x000d: Unknown.
	       */
	      if (aim_gettlv(innerlist, 0x000d, 1))
		;
	      
	      /* 
	       * Type 0x0004: Unknown
	       */
	      if (aim_gettlv(innerlist, 0x0004, 1))
		;

	      /* 
	       * Type 0x0002: Unknown
	       */
	      if (aim_gettlv(innerlist, 0x0002, 1)) {
		struct aim_tlv_t *tmptlv;
		unsigned short classperms = 0;

		tmptlv = aim_gettlv(innerlist, 0x0002, 1);
		classperms = aimutil_get16(tmptlv->value);
		
		printf("faim: class permissions %x\n", classperms);
	      }

	      /*
	       * Type 0x00c9: Unknown
	       */ 
	      if (aim_gettlv(innerlist, 0x00c9, 1))
		;
	      
	      /*
	       * Type 0x00ca: Creation Date 
	       */
	      if (aim_gettlv(innerlist, 0x00ca, 1))
		;
	      
	      /*
	       * Type 0x00d0: Mandatory Channels?
	       */
	      if (aim_gettlv(innerlist, 0x00d0, 1))
		;

	      /*
	       * Type 0x00d1: Maximum Message length
	       */
	      if (aim_gettlv(innerlist, 0x00d1, 1))
		;

	      /*
	       * Type 0x00d2: Maximum Occupancy?
	       */
	      if (aim_gettlv(innerlist, 0x00d2, 1))	
		;	

	      /*
	       * Type 0x00d3: Exchange Name
	       */
	      if (aim_gettlv(innerlist, 0x00d3, 1))	
		exchanges[curexchange-1].name = aim_gettlv_str(innerlist, 0x00d3, 1);
	      else
		exchanges[curexchange-1].name = NULL;

	      /*
	       * Type 0x00d5: Creation Permissions
	       *
	       * 0  Creation not allowed
	       * 1  Room creation allowed
	       * 2  Exchange creation allowed
	       * 
	       */
	      if (aim_gettlv(innerlist, 0x00d5, 1)) {
		struct aim_tlv_t *tmptlv;
		unsigned char createperms = 0;

		tmptlv = aim_gettlv(innerlist, 0x00d5, 1);
		createperms = aimutil_get8(tmptlv->value);
		
	      }

	      /*
	       * Type 0x00d6: Character Set (First Time)
	       */	      
	      if (aim_gettlv(innerlist, 0x00d6, 1))	
		exchanges[curexchange-1].charset1 = aim_gettlv_str(innerlist, 0x00d6, 1);
	      else
		exchanges[curexchange-1].charset1 = NULL;
	      
	      /*
	       * Type 0x00d7: Language (First Time)
	       */	      
	      if (aim_gettlv(innerlist, 0x00d7, 1))	
		exchanges[curexchange-1].lang1 = aim_gettlv_str(innerlist, 0x00d7, 1);
	      else
		exchanges[curexchange-1].lang1 = NULL;

	      /*
	       * Type 0x00d8: Character Set (Second Time)
	       */	      
	      if (aim_gettlv(innerlist, 0x00d8, 1))	
		exchanges[curexchange-1].charset2 = aim_gettlv_str(innerlist, 0x00d8, 1);
	      else
		exchanges[curexchange-1].charset2 = NULL;

	      /*
	       * Type 0x00d9: Language (Second Time)
	       */	      
	      if (aim_gettlv(innerlist, 0x00d9, 1))	
		exchanges[curexchange-1].lang2 = aim_gettlv_str(innerlist, 0x00d9, 1);
	      else
		exchanges[curexchange-1].lang2 = NULL;
	      
	      aim_freetlvchain(&innerlist);
	    }
	  
	  /*
	   * Call client.
	   */
	  userfunc = aim_callhandler(command->conn, 0x000d, 0x0009);
	  if (userfunc)
	    ret = userfunc(sess, 
			   command, 
			   snac->type,
			   maxrooms,
			   curexchange, 
			   exchanges);
	  curexchange--;
	  while(curexchange >= 0)
	    {
	      if (exchanges[curexchange].name)
		free(exchanges[curexchange].name);
	      if (exchanges[curexchange].charset1)
		free(exchanges[curexchange].charset1);
	      if (exchanges[curexchange].lang1)
		free(exchanges[curexchange].lang1);
	      if (exchanges[curexchange].charset2)
		free(exchanges[curexchange].charset2);
	      if (exchanges[curexchange].lang2)
		free(exchanges[curexchange].lang2);
	      curexchange--;
	    }
	  free(exchanges);
	  aim_freetlvchain(&tlvlist);
	  
	  break;
      }
    case 0x0003: /* request exchange info */
      printf("faim: chatnav_parse_info: resposne to exchange info\n");
      break;
    case 0x0004: /* request room info */
      printf("faim: chatnav_parse_info: response to room info\n");
      break;
    case 0x0005: /* request more room info */
      printf("faim: chatnav_parse_info: response to more room info\n");
      break;
    case 0x0006: /* request occupant list */
      printf("faim: chatnav_parse_info: response to occupant info\n");
      break;
    case 0x0007: /* search for a room */
      printf("faim: chatnav_parse_info: search results\n");
      break;
    case 0x0008: { /* create room */
      /*
	000d 0009 0000 0000 0010 
	
	0004 0053 
	     0004 -- exchange
	     0c 7a 6f6f 6f6d 7a6f 6f6f 6d34 32 cookie/name
	     0000 -- instance
	     02 -- detail level
	     0007 -- unknown!
	     006a 000c 7a 6f 6f6f 6d7a 6f6f 6f6d 3432 -- fully qualified name 
	     00c9 0002 0011 -- flags
	     00ca 0004 39c0 0883 -- create time
	     00d1 0002 0200 -- max msg len 
	     00d2 0002 0018 -- max occupants
	     00d3 000c -- name
	          7a6f 6f6f 6d7a 6f6f 6f6d 3432 
	     00d5 0001 02 -- creation permission
       */
      struct aim_tlvlist_t *tlvlist, *innerlist;
      char *ck = NULL, *fqcn = NULL, *name = NULL;
      unsigned short exchange, instance, unknown, flags, maxmsglen, maxoccupancy;
      unsigned long createtime = 0;
      unsigned char createperms;
      int i, cklen;
      struct aim_tlv_t *bigblock, *tmp;

      i = 10;
      if (!(tlvlist = aim_readtlvchain(command->data+i, command->commandlen-i))) {
	printf("faim: unable to read top tlv in create room response\n");
	break;
      }

      if (!(bigblock = aim_gettlv(tlvlist, 0x0004, 1))) {
	printf("faim: no bigblock in top tlv in create room response\n");
	aim_freetlvchain(&tlvlist);
	break;
      }
      i = 0;

      exchange = aimutil_get16(bigblock->value+i);
      i += 2;

      cklen = aimutil_get8(bigblock->value+i);
      i++;

      ck = malloc(cklen+1);
      memcpy(ck, bigblock->value+i, cklen);
      ck[cklen] = '\0';
      i += cklen;

      instance = aimutil_get16(bigblock->value+i);
      i += 2;

      if (aimutil_get8(bigblock->value+i) != 0x02) {
	printf("faim: unknown detaillevel in create room response (0x%02x)\n", aimutil_get8(bigblock->value+i));
	aim_freetlvchain(&tlvlist);
	free(ck);	
	break;
      }
      i += 1;
      
      unknown = aimutil_get16(bigblock->value+i);
      i += 2;

      if (!(innerlist = aim_readtlvchain(bigblock->value+i, bigblock->length-i))) {
	printf("faim: unable to read inner tlv chain in create room response\n");
	aim_freetlvchain(&tlvlist);
	free(ck);
	break;
      }

      if (aim_gettlv(innerlist, 0x006a, 1))
	fqcn = aim_gettlv_str(innerlist, 0x006a, 1);

      if ((tmp = aim_gettlv(innerlist, 0x00c9, 1)))
	flags = aimutil_get16(tmp->value);

      if ((tmp = aim_gettlv(innerlist, 0x00ca, 1)))
	createtime = aimutil_get32(tmp->value);

      if ((tmp = aim_gettlv(innerlist, 0x00d1, 1)))
	maxmsglen = aimutil_get16(tmp->value);

      if ((tmp = aim_gettlv(innerlist, 0x00d2, 1)))
	maxoccupancy = aimutil_get16(tmp->value);

      if (aim_gettlv(innerlist, 0x00d3, 1))
	name = aim_gettlv_str(innerlist, 0x00d3, 1);

      if ((tmp = aim_gettlv(innerlist, 0x00d5, 1)))
	createperms = aimutil_get8(tmp->value);

      if ((userfunc = aim_callhandler(command->conn, 0x000d, 0x0009))) {
	ret = userfunc(sess, command, snac->type, fqcn, instance, flags, createtime, maxmsglen, maxoccupancy, createperms, unknown, name, ck);
      }
     
      if (ck)
	free(ck);
      if (name)
	free(name);
      if (fqcn)
	free(fqcn);
      aim_freetlvchain(&innerlist);
      aim_freetlvchain(&tlvlist);

      break;
    }
    default: /* unknown */
      printf("faim: chatnav_parse_info: unknown request subtype (%04x)\n", snac->type);
    }

  if (snac && snac->data)
    free(snac->data);
  if (snac)
    free(snac);
  
  return ret;
}

faim_export unsigned long aim_chatnav_createroom(struct aim_session_t *sess,
						 struct aim_conn_t *conn,
						 char *name, 
						 u_short exchange)
{
  struct command_tx_struct *newpacket; 
  int i;
  struct aim_snac_t snac;

  if (!(newpacket = aim_tx_new(AIM_FRAMETYPE_OSCAR, 0x0002, conn, 10+12+strlen("invite")+strlen(name))))
    return -1;

  newpacket->lock = 1;

  i = aim_putsnac(newpacket->data, 0x000d, 0x0008, 0x0000, sess->snac_nextid);

  /* exchange */
  i+= aimutil_put16(newpacket->data+i, exchange);

  /* room cookie */
  i+= aimutil_put8(newpacket->data+i, strlen("invite"));
  i+= aimutil_putstr(newpacket->data+i, "invite", strlen("invite"));

  /* instance */
  i+= aimutil_put16(newpacket->data+i, 0xffff);
  
  /* detail level */
  i+= aimutil_put8(newpacket->data+i, 0x01);
  
  /* tlvcount */
  i+= aimutil_put16(newpacket->data+i, 0x0001);

  /* room name */
  i+= aim_puttlv_str(newpacket->data+i, 0x00d3, strlen(name), name);

  snac.id = sess->snac_nextid;
  snac.family = 0x000d;
  snac.type = 0x0008;
  snac.flags = 0x0000;
  snac.data = NULL;
  
  aim_newsnac(sess, &snac);

  aim_tx_enqueue(sess, newpacket);

  return (sess->snac_nextid++);
}
