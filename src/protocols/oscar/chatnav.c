/*
 * Handle ChatNav.
 *
 * [The ChatNav(igation) service does various things to keep chat
 *  alive.  It provides room information, room searching and creating, 
 *  as well as giving users the right ("permission") to use chat.]
 *
 */

#define FAIM_INTERNAL
#include <aim.h>

/*
 * conn must be a chatnav connection!
 */
faim_export unsigned long aim_chatnav_reqrights(struct aim_session_t *sess,
						struct aim_conn_t *conn)
{

  return aim_genericreq_n_snacid(sess, conn, 0x000d, 0x0002);
}

faim_export unsigned long aim_chatnav_clientready(struct aim_session_t *sess,
						  struct aim_conn_t *conn)
{
  struct command_tx_struct *newpacket; 
  int i;

  if (!(newpacket = aim_tx_new(sess, conn, AIM_FRAMETYPE_OSCAR, 0x0002, 0x20)))
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

faim_export unsigned long aim_chatnav_createroom(struct aim_session_t *sess,
						 struct aim_conn_t *conn,
						 char *name, 
						 u_short exchange)
{
  struct command_tx_struct *newpacket; 
  int i;

  if (!(newpacket = aim_tx_new(sess, conn, AIM_FRAMETYPE_OSCAR, 0x0002, 10+12+strlen("invite")+strlen(name))))
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

  aim_cachesnac(sess, 0x000d, 0x0008, 0x0000, NULL, 0);

  aim_tx_enqueue(sess, newpacket);

  return sess->snac_nextid;
}

static int parseinfo_perms(struct aim_session_t *sess, aim_module_t *mod, struct command_rx_struct *rx, aim_modsnac_t *snac, unsigned char *data, int datalen, struct aim_snac_t *snac2)
{
  aim_rxcallback_t userfunc;
  int ret = 0;
  struct aim_tlvlist_t *tlvlist;
  struct aim_chat_exchangeinfo *exchanges = NULL;
  int curexchange = 0;
  struct aim_tlv_t *exchangetlv;
  unsigned char maxrooms = 0;
  struct aim_tlvlist_t *innerlist;

  tlvlist = aim_readtlvchain(data, datalen);

  /* 
   * Type 0x0002: Maximum concurrent rooms.
   */ 
  if (aim_gettlv(tlvlist, 0x0002, 1))
    maxrooms = aim_gettlv8(tlvlist, 0x0002, 1);

  /* 
   * Type 0x0003: Exchange information
   *
   * There can be any number of these, each one
   * representing another exchange.  
   * 
   */
  curexchange = 0;
  while ((exchangetlv = aim_gettlv(tlvlist, 0x0003, curexchange+1))) {
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
      unsigned short classperms;

      classperms = aim_gettlv16(innerlist, 0x0002, 1);
		
      faimdprintf(sess, 1, "faim: class permissions %x\n", classperms);
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
      unsigned char createperms;

      createperms = aim_gettlv8(innerlist, 0x00d5, 1);
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
  if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
    ret = userfunc(sess, rx, snac2->type, maxrooms, curexchange, exchanges);
  curexchange--;
  while(curexchange >= 0) {
    free(exchanges[curexchange].name);
    free(exchanges[curexchange].charset1);
    free(exchanges[curexchange].lang1);
    free(exchanges[curexchange].charset2);
    free(exchanges[curexchange].lang2);
    curexchange--;
  }
  free(exchanges);
  aim_freetlvchain(&tlvlist);

  return ret;
}

static int parseinfo_create(struct aim_session_t *sess, aim_module_t *mod, struct command_rx_struct *rx, aim_modsnac_t *snac, unsigned char *data, int datalen, struct aim_snac_t *snac2)
{
  aim_rxcallback_t userfunc;
  struct aim_tlvlist_t *tlvlist, *innerlist;
  char *ck = NULL, *fqcn = NULL, *name = NULL;
  unsigned short exchange = 0, instance = 0, unknown = 0, flags = 0, maxmsglen = 0, maxoccupancy = 0;
  unsigned long createtime = 0;
  unsigned char createperms = 0;
  int i = 0, cklen;
  struct aim_tlv_t *bigblock;
  int ret = 0;

  if (!(tlvlist = aim_readtlvchain(data, datalen))) {
    faimdprintf(sess, 0, "unable to read top tlv in create room response\n");
    return 0;
  }

  if (!(bigblock = aim_gettlv(tlvlist, 0x0004, 1))) {
    faimdprintf(sess, 0, "no bigblock in top tlv in create room response\n");
    aim_freetlvchain(&tlvlist);
    return 0;
  }

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
    faimdprintf(sess, 0, "unknown detaillevel in create room response (0x%02x)\n", aimutil_get8(bigblock->value+i));
    aim_freetlvchain(&tlvlist);
    free(ck);
    return 0;
  }
  i += 1;
      
  unknown = aimutil_get16(bigblock->value+i);
  i += 2;

  if (!(innerlist = aim_readtlvchain(bigblock->value+i, bigblock->length-i))) {
    faimdprintf(sess, 0, "unable to read inner tlv chain in create room response\n");
    aim_freetlvchain(&tlvlist);
    free(ck);
    return 0;
  }

  if (aim_gettlv(innerlist, 0x006a, 1))
    fqcn = aim_gettlv_str(innerlist, 0x006a, 1);

  if (aim_gettlv(innerlist, 0x00c9, 1))
    flags = aim_gettlv16(innerlist, 0x00c9, 1);

  if (aim_gettlv(innerlist, 0x00ca, 1))
    createtime = aim_gettlv32(innerlist, 0x00ca, 1);

  if (aim_gettlv(innerlist, 0x00d1, 1))
    maxmsglen = aim_gettlv16(innerlist, 0x00d1, 1);

  if (aim_gettlv(innerlist, 0x00d2, 1))
    maxoccupancy = aim_gettlv16(innerlist, 0x00d2, 1);

  if (aim_gettlv(innerlist, 0x00d3, 1))
    name = aim_gettlv_str(innerlist, 0x00d3, 1);

  if (aim_gettlv(innerlist, 0x00d5, 1))
    createperms = aim_gettlv8(innerlist, 0x00d5, 1);

  if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
    ret = userfunc(sess, rx, snac2->type, fqcn, instance, exchange, flags, createtime, maxmsglen, maxoccupancy, createperms, unknown, name, ck);

  free(ck);
  free(name);
  free(fqcn);
  aim_freetlvchain(&innerlist);
  aim_freetlvchain(&tlvlist);

  return ret;
}

/*
 * Since multiple things can trigger this callback,
 * we must lookup the snacid to determine the original
 * snac subtype that was called.
 */
static int parseinfo(struct aim_session_t *sess, aim_module_t *mod, struct command_rx_struct *rx, aim_modsnac_t *snac, unsigned char *data, int datalen)
{
  struct aim_snac_t *snac2;
  int ret = 0;

  if (!(snac2 = aim_remsnac(sess, snac->id))) {
    faimdprintf(sess, 0, "faim: chatnav_parse_info: received response to unknown request! (%08lx)\n", snac->id);
    return 0;
  }
  
  if (snac2->family != 0x000d) {
    faimdprintf(sess, 0, "faim: chatnav_parse_info: recieved response that maps to corrupt request! (fam=%04x)\n", snac2->family);
    return 0;
  }

  /*
   * We now know what the original SNAC subtype was.
   */
  if (snac2->type == 0x0002) /* request chat rights */
    ret = parseinfo_perms(sess, mod, rx, snac, data, datalen, snac2);
  else if (snac2->type == 0x0003) /* request exchange info */
    faimdprintf(sess, 0, "chatnav_parse_info: resposne to exchange info\n");
  else if (snac2->type == 0x0004) /* request room info */
    faimdprintf(sess, 0, "chatnav_parse_info: response to room info\n");
  else if (snac2->type == 0x0005) /* request more room info */
    faimdprintf(sess, 0, "chatnav_parse_info: response to more room info\n");
  else if (snac2->type == 0x0006) /* request occupant list */
    faimdprintf(sess, 0, "chatnav_parse_info: response to occupant info\n");
  else if (snac2->type == 0x0007) /* search for a room */
    faimdprintf(sess, 0, "chatnav_parse_info: search results\n");
  else if (snac2->type == 0x0008) /* create room */
    ret = parseinfo_create(sess, mod, rx, snac, data, datalen, snac2);
  else
    faimdprintf(sess, 0, "chatnav_parse_info: unknown request subtype (%04x)\n", snac2->type);

  if (snac2)
    free(snac2->data);
  free(snac2);
  
  return ret;
}

static int snachandler(struct aim_session_t *sess, aim_module_t *mod, struct command_rx_struct *rx, aim_modsnac_t *snac, unsigned char *data, int datalen)
{

  if (snac->subtype == 0x0009)
    return parseinfo(sess, mod, rx, snac, data, datalen);

  return 0;
}

faim_internal int chatnav_modfirst(struct aim_session_t *sess, aim_module_t *mod)
{

  mod->family = 0x000d;
  mod->version = 0x0000;
  mod->flags = 0;
  strncpy(mod->name, "chatnav", sizeof(mod->name));
  mod->snachandler = snachandler;

  return 0;
}
