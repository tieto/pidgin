#include <faim/aim.h>

#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/utsname.h> /* for aim_directim_initiate */
#include <arpa/inet.h> /* for inet_ntoa */

#include "config.h"

/* aim_msgcookies.c is mostly new. just look at the diff and replace yours, easiest. */

/* 
   function name       where i had it
   aim_send_im_direct aim_im.c
   aim_directim_initiate aim_im.c
   aim_filetransfer_accept aim_im.c
   aim_getlisting aim_misc.c (?!) -- prototype function. can be ignored.
   establish aim_misc.c
   aim_get_command_rendezvous aim_r
   oft_getfh aim_rxqueue.c
*/

faim_export int aim_handlerendconnect(struct aim_session_t *sess, struct aim_conn_t *cur)
{
  int acceptfd = 0;
  rxcallback_t userfunc;
  struct sockaddr cliaddr;
  socklen_t clilen = sizeof(cliaddr);
  int ret = 0;

  /*
   * Listener sockets only have incoming connections. No data.
   */
  if( (acceptfd = accept(cur->fd, &cliaddr, &clilen)) == -1)
    return -1;

  if (cliaddr.sa_family != AF_INET) /* just in case IPv6 really is happening */
    return -1;

  switch(cur->subtype) {
  case AIM_CONN_SUBTYPE_OFT_DIRECTIM: {
    struct aim_directim_priv *priv;
    
    priv = (struct aim_directim_priv *)calloc(1, sizeof(struct aim_directim_priv));

    snprintf(priv->ip, sizeof(priv->ip), "%s:%u", inet_ntoa(((struct sockaddr_in *)&cliaddr)->sin_addr), ntohs(((struct sockaddr_in *)&cliaddr)->sin_port));

    if(!cur->priv)
      cur->priv = priv; /* what happens if there is one?! -- mid */

    cur->type = AIM_CONN_TYPE_RENDEZVOUS;
    close(cur->fd); /* should we really do this? seems like the client should decide. maybe clone the connection and keep the listener open. -- mid */
    cur->fd = acceptfd;

    if ( (userfunc = aim_callhandler(cur, AIM_CB_FAM_OFT, AIM_CB_OFT_DIRECTIMINITIATE)))
      ret = userfunc(sess, NULL, cur);
				   
    break;
  }
  case AIM_CONN_SUBTYPE_OFT_GETFILE: {
    struct aim_filetransfer_priv *priv;

    priv->state = 0;

    priv = (struct aim_filetransfer_priv *)calloc(1, sizeof(struct aim_filetransfer_priv));

    snprintf(priv->ip, sizeof(priv->ip), "%s:%u", inet_ntoa(((struct sockaddr_in *)&cliaddr)->sin_addr), ntohs(((struct sockaddr_in *)&cliaddr)->sin_port));

    if(!cur->priv)
      cur->priv = priv;

    if ( (userfunc = aim_callhandler(cur, AIM_CB_FAM_OFT, AIM_CB_OFT_GETFILEINITIATE)))
      ret = userfunc(sess, NULL, cur);
    break;
  } 
  default: {
    /* XXX */
  }
  }
  return ret;
}


/*
 * aim_send_im_direct:
 * sess - session
 * conn - directim connection
 * msg  - null-terminated string to send
 */

faim_export int aim_send_im_direct(struct aim_session_t *sess, 
				   struct aim_conn_t *conn,
				   char *msg)
{
  struct command_tx_struct *newpacket , *newpacket2; 

  /* newpacket contains a real header with data, newpacket2 is just a
     null packet, with a cookie and a lot of 0x00s. newpacket is the
     "i'm sending", newpacket2 is the "i'm typing".*/

  /* uhm. the client should send those as two seperate things -- mid */

  struct aim_directim_priv *priv = NULL;
  int i;

  if (strlen(msg) >= MAXMSGLEN)
    return -1;

  if (!sess || !conn || !(conn->type) || (conn->type != AIM_CONN_TYPE_RENDEZVOUS) || !conn->priv) {
    printf("faim: directim: invalid arguments\n");
    return -1;
  };

  priv = (struct aim_directim_priv *)conn->priv;

  /* NULLish Header */

  if (!(newpacket2 = aim_tx_new(AIM_FRAMETYPE_OFT, 0x0001, conn, 0))) {
    printf("faim: directim: tx_new2 failed\n");
    return -1;
  }                                                                           

  newpacket2->lock = 1; /* lock struct */

  memcpy(newpacket2->hdr.oft.magic, "ODC2", 4);
  newpacket2->hdr.oft.hdr2len = 0x44;

  if (!(newpacket2->hdr.oft.hdr2 = calloc(1,newpacket2->hdr.oft.hdr2len))) {
    free(newpacket2);
    return -1;
  }

  i = 0;
  i += aimutil_put16(newpacket2->hdr.oft.hdr2+i, 0x0006);
  i += aimutil_put16(newpacket2->hdr.oft.hdr2+i, 0x0000);

  i += aimutil_putstr(newpacket2->hdr.oft.hdr2+i, (char *)priv->cookie, 8);

  i += aimutil_put16(newpacket2->hdr.oft.hdr2+i, 0x0000);
  i += aimutil_put16(newpacket2->hdr.oft.hdr2+i, 0x0000);
  i += aimutil_put16(newpacket2->hdr.oft.hdr2+i, 0x0000);
  i += aimutil_put16(newpacket2->hdr.oft.hdr2+i, 0x0000);

  i += aimutil_put32(newpacket2->hdr.oft.hdr2+i, 0x00000000);

  i += aimutil_put16(newpacket2->hdr.oft.hdr2+i, 0x0000);
  i += aimutil_put16(newpacket2->hdr.oft.hdr2+i, 0x0000);
  i += aimutil_put16(newpacket2->hdr.oft.hdr2+i, 0x0000);

  i += aimutil_put16(newpacket2->hdr.oft.hdr2+i, 0x000e);

  i += aimutil_put16(newpacket2->hdr.oft.hdr2+i, 0x0000);
  i += aimutil_put16(newpacket2->hdr.oft.hdr2+i, 0x0000);

  i += aimutil_putstr(newpacket2->hdr.oft.hdr2+i, sess->logininfo.screen_name, strlen(sess->logininfo.screen_name));
  
  i = 52; /* 0x34 */
  i += aimutil_put8(newpacket2->hdr.oft.hdr2+i, 0x00); /* 53 */
  i += aimutil_put16(newpacket2->hdr.oft.hdr2+i, 0x0000); /* 55 */
  i += aimutil_put16(newpacket2->hdr.oft.hdr2+i, 0x0000);
  i += aimutil_put16(newpacket2->hdr.oft.hdr2+i, 0x0000);
  i += aimutil_put16(newpacket2->hdr.oft.hdr2+i, 0x0000);/* 61 */
  i += aimutil_put16(newpacket2->hdr.oft.hdr2+i, 0x0000);
  i += aimutil_put16(newpacket2->hdr.oft.hdr2+i, 0x0000);/* 65 */
  i += aimutil_put16(newpacket2->hdr.oft.hdr2+i, 0x0000);/* end of hdr2 */

  newpacket2->lock = 0;
  newpacket2->data = NULL;
  
  aim_tx_enqueue(sess, newpacket2);

  /* Header packet */

  if (!(newpacket = aim_tx_new(AIM_FRAMETYPE_OFT, 0x0001, conn, strlen(msg)))) {
    printf("faim: directim: tx_new failed\n");
    return -1;
  }

  newpacket->lock = 1; /* lock struct */

  memcpy(newpacket->hdr.oft.magic, "ODC2", 4);
  newpacket->hdr.oft.hdr2len = 0x54;

  if (!(newpacket->hdr.oft.hdr2 = calloc(1,newpacket->hdr.oft.hdr2len))) {
    free(newpacket);
    return -1;
  }

  i = 0;
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0006);
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0000);

  i += aimutil_putstr(newpacket->hdr.oft.hdr2+i, (char *)priv->cookie, 8);

  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0000);
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0000);
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0000);
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0000);

  i += aimutil_put32(newpacket->hdr.oft.hdr2+i, strlen(msg));

  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0000);
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0000);
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0000);
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0000);
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0000);
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0000);

  i += aimutil_putstr(newpacket->hdr.oft.hdr2+i, sess->logininfo.screen_name, strlen(sess->logininfo.screen_name));
  
  i = 52; /* 0x34 */
  i += aimutil_put8(newpacket->hdr.oft.hdr2+i, 0x00); /* 53 */
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0000); /* 55 */
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0000);
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0000);
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0000);/* 61 */
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0000);
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0000);/* 65 */
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0000);/* end of hdr2 */

  /* values grabbed from a dump */
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0008); /* 69 */
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x000c);
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0000);/* 71 */
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x1466);/* 73 */ 
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x0001);/* 73 */ 
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x2e0f);
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0x393e);
  i += aimutil_put16(newpacket->hdr.oft.hdr2+i, 0xcac8);

  memcpy(newpacket->data, msg, strlen(msg));

  newpacket->lock = 0;

  aim_tx_enqueue(sess, newpacket);

  return 0;
}

/*
 * aim_directim_intitiate:
 * For those times when we want to open up the directim channel ourselves.
 * sess is your session,
 * conn is the BOS conn,
 * priv is a dummy priv value (we'll let it get filled in later) (if
 * you pass a NULL, we alloc one) 
 * destsn is the SN to connect to.  
 */


faim_export struct aim_conn_t *aim_directim_initiate(struct aim_session_t *sess,
						     struct aim_conn_t *conn,
						     struct aim_directim_priv *priv,
						     char *destsn)
{
  struct command_tx_struct *newpacket;
  struct aim_conn_t *newconn;

  struct aim_msgcookie_t *cookie;

  int curbyte, i, listenfd;
  short port = 4443;

  struct hostent *hptr;
  struct utsname myname;

  unsigned char cap[16];
  char d[4]; /* XXX: IPv6. *cough* */

  /*
   * Open our socket
   */

  if( (listenfd = aim_listenestablish(port)) == -1)
    return NULL;

  /*
   * get our local IP
   */

  if(uname(&myname) < 0)
    return NULL;

  if( (hptr = gethostbyname(myname.nodename)) == NULL)
    return NULL;

  memcpy(&d, hptr->h_addr_list[0], 4); /* XXX: this probably isn't quite kosher, but it works */

  aim_putcap(cap, 16, AIM_CAPS_IMIMAGE);

  /*
   * create the OSCAR packet
   */

  if (!(newpacket = aim_tx_new(AIM_FRAMETYPE_OSCAR, 0x0002, conn, 10+8+2+1+strlen(destsn)+4+4+0x32)))
    return NULL;

  newpacket->lock = 1; /* lock struct */

  curbyte  = 0;
  curbyte += aim_putsnac(newpacket->data+curbyte, 
			 0x0004, 0x0006, 0x0000, sess->snac_nextid);

  /* 
   * Generate a random message cookie 
   * This cookie needs to be alphanumeric and NULL-terminated to be TOC-compatible.
   */
  for (i=0;i<7;i++)
    curbyte += aimutil_put8(newpacket->data+curbyte, 0x30 + ((u_char) random() % 20));
  curbyte += aimutil_put8(newpacket->data+curbyte, 0x00);

  /*
   * grab all the data for cookie caching.
   */
  cookie = (struct aim_msgcookie_t *)calloc(1, sizeof(struct aim_msgcookie_t));

  memcpy(cookie->cookie, newpacket->data+curbyte-8, 8);
  cookie->type = AIM_COOKIETYPE_OFTIM;
  
  if(!priv)
    priv = (struct aim_directim_priv *)calloc(1, sizeof(struct aim_directim_priv));

  memcpy(priv->cookie, cookie, 8);
  memcpy(priv->sn, destsn, sizeof(priv->sn));
 
  cookie->data = priv;

  aim_cachecookie(sess, cookie);  /* cache da cookie */

  /*
   * Channel ID
   */
  curbyte += aimutil_put16(newpacket->data+curbyte,0x0002);

  /* 
   * Destination SN (prepended with byte length)
   */
  curbyte += aimutil_put8(newpacket->data+curbyte,strlen(destsn));
  curbyte += aimutil_putstr(newpacket->data+curbyte, destsn, strlen(destsn));

  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0003);
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0000);

  /* 
   * enTLV start
   */
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0005);
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0032);

  /*
   * Flag data / ICBM Parameters?
   */
  curbyte += aimutil_put8(newpacket->data+curbyte, 0x00);
  curbyte += aimutil_put8(newpacket->data+curbyte, 0x00);

  /*
   * Cookie 
   */
  curbyte += aimutil_putstr(newpacket->data+curbyte, (char *)cookie, 8);

  /*
   * Capability String
   */
  curbyte += aimutil_putstr(newpacket->data+curbyte, (char *)cap, 0x10);

  /*
   * 000a/0002 : 0001
   */
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x000a);
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0002);
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0001);

  /*
   * 0003/0004: IP address
   */

  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0003);
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0004);

  for(i = 0; i < 4; i++)
    curbyte += aimutil_put8(newpacket->data+curbyte, d[i]); /* already in network byte order */

  /*
   * 0005/0002: Port
   */

  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0005);
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0002);
  curbyte += aimutil_put16(newpacket->data+curbyte, port);

  /*
   * 000f/0000: umm.. dunno. Zigamorph[1]?
   * [1]: see esr's TNHD.
   */

  curbyte += aimutil_put16(newpacket->data+curbyte, 0x000f);
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0000);

  printf("curbyte: 0x%x\n",curbyte);

  newpacket->commandlen = curbyte;
  newpacket->lock = 0;

  aim_tx_enqueue(sess, newpacket);

  /*
   * allocate and set up our connection
   */

  i = fcntl(listenfd, F_GETFL, 0);
  fcntl(listenfd, F_SETFL, i | O_NONBLOCK);

  newconn = aim_newconn(sess, AIM_CONN_TYPE_RENDEZVOUS_OUT, NULL);
  if (!newconn) { 
    perror("aim_newconn");
    aim_conn_kill(sess, &newconn);
    return NULL;
  } 

  newconn->fd = listenfd;
  newconn->subtype = AIM_CONN_SUBTYPE_OFT_DIRECTIM;
  newconn->priv = priv;
  printf("faim: listening (fd = %d, unconnected)\n", newconn->fd);

  /*
   * XXX We need some way of closing the listener socket after
   * n seconds of no connection. -- mid
   */

#ifdef USE_SNAC_FOR_IMS
 {
    struct aim_snac_t snac;

    snac.id = sess->snac_nextid;
    snac.family = 0x0004;
    snac.type = 0x0006;
    snac.flags = 0x0000;

    snac.data = malloc(strlen(destsn)+1);
    memcpy(snac.data, destsn, strlen(destsn)+1);

    aim_newsnac(sess, &snac);

    aim_cleansnacs(sess, 60); /* clean out all SNACs over 60sec old */
  }
#endif
  
  return (newconn);
} 


faim_export struct aim_conn_t *aim_directim_connect(struct aim_session_t *sess,
						    struct aim_conn_t *conn,
						    struct aim_directim_priv *priv )
{
  struct aim_conn_t *newconn = NULL;;

  newconn = aim_newconn(sess, AIM_CONN_TYPE_RENDEZVOUS, priv->ip);
  if (!newconn || (newconn->fd == -1)) { 
    printf("could not connect to %s\n", priv->ip);
    perror("aim_newconn");
    aim_conn_kill(sess, &newconn);
    return NULL;
  } else {    
    newconn->subtype = AIM_CONN_SUBTYPE_OFT_DIRECTIM;
    newconn->priv = priv;
    printf("faim: connected to peer (fd = %d)\n", newconn->fd);
    return newconn;
  }
  return newconn;
}

faim_export unsigned long aim_accepttransfer(struct aim_session_t *sess,
					     struct aim_conn_t *conn, 
					     struct aim_conn_t *oftconn,
					     char *sn,
					     char *cookie,
					     unsigned short rendid)
{
  struct command_tx_struct *newpacket, *newoft;
  struct aim_fileheader_t *listingfh;
  int curbyte, i;
  /* now for the oft bits */

  if(rendid == AIM_CAPS_GETFILE) {
    printf("jbm: getfile request accept\n");
    if(!(newoft = aim_tx_new(AIM_FRAMETYPE_OFT, 0x1108, oftconn, 0))) {
      printf("faim: accept_transfer: tx_new OFT failed\n");
      return -1;
    }
    
    newoft->lock = 1;
    
    memcpy(newoft->hdr.oft.magic, "OFT2", 4);
    newoft->hdr.oft.hdr2len = 0xf8; /* 0x100 - 8 */
    
    if (!(newoft->hdr.oft.hdr2 = calloc(1,newoft->hdr.oft.hdr2len))) {
      free(newoft);
      return -1;
    }  

    listingfh = aim_getlisting(sess);

    memcpy(listingfh->bcookie, cookie, 8);

    curbyte = 0;
    
    for(i = 0; i < 8; i++)
      curbyte += aimutil_put8(newoft->hdr.oft.hdr2+curbyte, cookie[i]);
    curbyte += aimutil_put16(newoft->hdr.oft.hdr2+curbyte, listingfh->encrypt);
    curbyte += aimutil_put16(newoft->hdr.oft.hdr2+curbyte, listingfh->compress);
    curbyte += aimutil_put16(newoft->hdr.oft.hdr2+curbyte, listingfh->totfiles);
    curbyte += aimutil_put16(newoft->hdr.oft.hdr2+curbyte, listingfh->filesleft);
    curbyte += aimutil_put16(newoft->hdr.oft.hdr2+curbyte, listingfh->totparts);
    curbyte += aimutil_put16(newoft->hdr.oft.hdr2+curbyte, listingfh->partsleft);
    curbyte += aimutil_put32(newoft->hdr.oft.hdr2+curbyte, listingfh->totsize);
    curbyte += aimutil_put32(newoft->hdr.oft.hdr2+curbyte, listingfh->size);
    curbyte += aimutil_put32(newoft->hdr.oft.hdr2+curbyte, listingfh->modtime);
    curbyte += aimutil_put32(newoft->hdr.oft.hdr2+curbyte, listingfh->checksum);
    curbyte += aimutil_put32(newoft->hdr.oft.hdr2+curbyte, listingfh->rfrcsum);
    curbyte += aimutil_put32(newoft->hdr.oft.hdr2+curbyte, listingfh->rfsize);
    curbyte += aimutil_put32(newoft->hdr.oft.hdr2+curbyte, listingfh->cretime);
    curbyte += aimutil_put32(newoft->hdr.oft.hdr2+curbyte, listingfh->rfcsum);
    curbyte += aimutil_put32(newoft->hdr.oft.hdr2+curbyte, listingfh->nrecvd);
    curbyte += aimutil_put32(newoft->hdr.oft.hdr2+curbyte, listingfh->recvcsum);

    memcpy(newoft->hdr.oft.hdr2+curbyte, listingfh->idstring, 32);
    curbyte += 32;

    curbyte += aimutil_put8(newoft->hdr.oft.hdr2+curbyte, listingfh->flags);
    curbyte += aimutil_put8(newoft->hdr.oft.hdr2+curbyte, listingfh->lnameoffset);
    curbyte += aimutil_put8(newoft->hdr.oft.hdr2+curbyte, listingfh->lsizeoffset);

    memcpy(newoft->hdr.oft.hdr2+curbyte, listingfh->dummy, 69);
    curbyte += 69;
    
    memcpy(newoft->hdr.oft.hdr2+curbyte, listingfh->macfileinfo, 16);
    curbyte += 16;

    curbyte += aimutil_put16(newoft->hdr.oft.hdr2+curbyte, listingfh->nencode);
    curbyte += aimutil_put16(newoft->hdr.oft.hdr2+curbyte, listingfh->nlanguage);

    memcpy(newoft->hdr.oft.hdr2+curbyte, listingfh->name, 64);
    curbyte += 64;

    free(listingfh);

    newoft->lock = 0;
    aim_tx_enqueue(sess, newoft);
    printf("faim: getfile: OFT listing enqueued.\n");
    
  }


  if(!(newpacket = aim_tx_new(AIM_FRAMETYPE_OSCAR, 0x0002, conn, 10+8+2+1+strlen(sn)+4+2+8+16)))
    return -1;
  
  newpacket->lock = 1;
  
  curbyte = aim_putsnac(newpacket->data, 0x0004, 0x0006, 0x0000, sess->snac_nextid);
  for (i = 0; i < 8; i++)
    curbyte += aimutil_put8(newpacket->data+curbyte, cookie[i]);
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0002);
  curbyte += aimutil_put8(newpacket->data+curbyte, strlen(sn));
  curbyte += aimutil_putstr(newpacket->data+curbyte, sn, strlen(sn));
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0005);
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x001a);
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0002 /* accept */);
  for (i = 0; i < 8; i++)
    curbyte += aimutil_put8(newpacket->data+curbyte, cookie[i]);
  curbyte += aim_putcap(newpacket->data+curbyte, 0x10, rendid);

  newpacket->lock = 0;
  aim_tx_enqueue(sess, newpacket);



  return (sess->snac_nextid++);
}

/*
 * aim_getlisting()
 * 
 * Get file listing.txt info. where else to put it? i
 * dunno. client-side issue for sure tho. for now we just side-step
 * the issue with a nice default. =)
 *  
 */

faim_internal struct aim_fileheader_t *aim_getlisting(struct aim_session_t *sess) 
{
  struct aim_fileheader_t *fh;

  if(!(fh = (struct aim_fileheader_t*)calloc(1, sizeof(struct aim_fileheader_t))))
    return NULL;

  fh->encrypt     = 0x0000;
  fh->compress    = 0x0000;
  fh->totfiles    = 0x0001;
  fh->filesleft   = 0x0001;
  fh->totparts    = 0x0001;
  fh->partsleft   = 0x0001;
  fh->totsize     = 0x00000064;
  fh->size        = 0x00000024; /* ls -l listing.txt */
  fh->modtime     = (int)time(NULL); /*0x39441fb4; */
  fh->checksum    = 0xb8350000;
  fh->rfcsum      = 0x00000000;
  fh->rfsize      = 0x00000000;
  fh->cretime     = 0x00000000;
  fh->rfcsum      = 0x00000000;
  fh->nrecvd      = 0x00000000;
  fh->recvcsum    = 0x00000000;

  memset(fh->idstring, 0, 32/*sizeof(fh->idstring)*/);
  memcpy(fh->idstring, "OFT_Windows ICBMFT V1.1 32", 32/*sizeof(fh->idstring)*/);
  memset(fh->idstring+strlen(fh->idstring), 0, 32-strlen(fh->idstring)); /* jbm hack */ 

  fh->flags       = 0x02;
  fh->lnameoffset = 0x1a;
  fh->lsizeoffset = 0x10;

  memset(fh->dummy, 0, 69/*sizeof(fh->dummy)*/);
  /*  fh->dummy = ;*/

  memset(fh->macfileinfo, 0, 16/*sizeof(fh->macfileinfo)*/);
  /*  fh->macfileinfo = ; */

  fh->nencode     = 0x0000;
  fh->nlanguage   = 0x0000;

  memset(fh->name, 0, 64/*sizeof(fh->name)*/);
  memcpy(fh->name, "listing.txt", 64 /*sizeof(fh->name)*/);
  memset(fh->name+strlen(fh->name), 0, 64-strlen(fh->name)); /* jbm hack */

  printf("jbm: fh name %s / %s\n", fh->name, (fh->name+(strlen(fh->name))));
  return fh;
}

/*
 * establish: create a listening socket on a port. you need to call
 * accept() when it's connected.
 * portnum is the port number to bind to.
 * returns your fd
 */

faim_internal int aim_listenestablish(u_short portnum)
{
#if HAVE_GETADDRINFO
  int listenfd;
  const int on = 1;
  struct addrinfo hints, *res, *ressave;
  char serv[5];
  sprintf(serv, "%d", portnum);
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_flags = AI_PASSIVE;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  if (getaddrinfo(NULL/*any IP*/, serv, &hints, &res) != 0) {
    perror("getaddrinfo");
    return -1;
  }
  ressave = res;
  do {
    listenfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (listenfd < 0)
      continue;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if (bind(listenfd, res->ai_addr, res->ai_addrlen) == 0)
      break; /* success */
    close(listenfd);
  } while ( (res = res->ai_next) );
  if (!res)
    return -1;
  if (listen(listenfd, 1024)!=0) {
    perror("listen");
    return -1;
  }
  freeaddrinfo(ressave);
  return listenfd;
#else
  return -1;
#endif
}

faim_internal int aim_get_command_rendezvous(struct aim_session_t *sess, struct aim_conn_t *conn)
{

  /* XXX: NOT THREAD SAFE RIGHT NOW. the locks are acting up. deal. -- jbm */

  unsigned char hdrbuf1[6];
  unsigned char *hdr = NULL;
  int hdrlen, hdrtype;
  int flags = 0;
  rxcallback_t userfunc = NULL;


  memset(hdrbuf1, 0, sizeof(hdrbuf1));

  faim_mutex_lock(&conn->active); /* gets locked down for the entirety */

  if ( (hdrlen = read(conn->fd, hdrbuf1, 6)) < 6) {    
    if(hdrlen < 0)
      perror("read");
    printf("faim: rend: read error (fd: %i) %02x%02x%02x%02x%02x%02x (%i)\n", conn->fd, hdrbuf1[0],hdrbuf1[1],hdrbuf1[0],hdrbuf1[0],hdrbuf1[0],hdrbuf1[0],hdrlen);
    faim_mutex_unlock(&conn->active);
    aim_conn_close(conn);
    return -1;
  }

  hdrlen = aimutil_get16(hdrbuf1+4);

  hdrlen -= 6;
  if (!(hdr = malloc(hdrlen)))
    return -1;

  if (read(conn->fd, hdr, hdrlen) < hdrlen) {
    perror("read");
    printf("faim: rend: read2 error\n");
    free(hdr);
    faim_mutex_unlock(&conn->active);
    aim_conn_close(conn);
    return 0; /* see comment on previous read check */
  }

  hdrtype = aimutil_get16(hdr);  

  switch (hdrtype) {
  case 0x0001: { /* directim */
    int payloadlength = 0;
    char *snptr = NULL;
    struct aim_directim_priv *priv;
    int i;

    priv = (struct aim_directim_priv *)calloc(1, sizeof(struct aim_directim_priv));

    payloadlength = aimutil_get32(hdr+22);
    flags = aimutil_get16(hdr+32);
    snptr = (char *)hdr+38;

    strncpy(priv->sn, snptr, MAXSNLEN);

#if 0
    printf("faim: OFT frame: %04x / %04x / %04x / %s\n", hdrtype, payloadlength, flags, snptr); 
#endif

    if (flags == 0x000e) {
      faim_mutex_unlock(&conn->active);
      if ( (userfunc = aim_callhandler(conn, AIM_CB_FAM_OFT, AIM_CB_OFT_DIRECTIMTYPING)) )
	return userfunc(sess, NULL, snptr);
    } else if ((flags == 0x0000) && payloadlength) {
      unsigned char *msg;

      if(! (msg = calloc(1, payloadlength+1)) ) {
	faim_mutex_unlock(&conn->active);
	return 0;
      }
      
      if (recv(conn->fd, msg, payloadlength, MSG_WAITALL) < payloadlength) {
	perror("read");
	printf("faim: rend: read3 error\n");
	free(msg);
	faim_mutex_unlock(&conn->active);
	aim_conn_close(conn);
	return -1;
      }
      faim_mutex_unlock(&conn->active);
      msg[payloadlength] = '\0';
#if 0     
      printf("faim: directim: %s/%04x/%04x/%s\n", snptr, payloadlength, flags, msg);
#endif

      if ( (userfunc = aim_callhandler(conn, AIM_CB_FAM_OFT, AIM_CB_OFT_DIRECTIMINCOMING)) )
	i = userfunc(sess, NULL, conn, snptr, msg);
      
      free(msg);
      return i;
    }
    break;
  } 
  case 0x1209: { /* get file first */
    struct aim_filetransfer_priv *ft;
    struct aim_fileheader_t *fh;
    struct aim_msgcookie_t *cook;

    int commandlen;
    char *data;

    printf("faim: rend: fileget 0x1209\n");

    if(hdrlen != 0x100)
      printf("faim: fileget_command(1209): um. hdrlen != 0x100.. 0x%x\n", hdrlen);
    
    if(!(ft = (struct aim_filetransfer_priv *)calloc(1, sizeof(struct aim_filetransfer_priv)))) {
      printf("faim: couldn't malloc ft. um. bad. bad bad. file transfer will likely fail, sorry.\n");
      faim_mutex_unlock(&conn->active);
      return 0;
    }

    fh = aim_oft_getfh(hdr);

    memcpy(&(ft->fh), fh, sizeof(struct aim_fileheader_t));
    
    cook = aim_checkcookie(sess, (unsigned char *)ft->fh.bcookie, AIM_COOKIETYPE_OFTGET);

    if(cook->data)
      free(cook->data); /* XXX */
  
    cook->data = ft;
    
    aim_cachecookie(sess, cook);

    commandlen = 36;

    data = calloc(1, commandlen);
    memcpy(data, "01/01/1999 00:00      100 file.txt\r\n", commandlen);

    if (write(conn->fd, data, commandlen) != commandlen) {
      perror("listing write error");
    }
    faim_mutex_unlock(&conn->active);

    printf("jbm: hit end of 1209\n");

    break;
  }
  case 0x120b: { /* get file second */
    struct aim_filetransfer_priv *ft;
    struct aim_msgcookie_t *cook;

    struct aim_fileheader_t *fh;

    printf("faim: rend: fileget 120b\n");

    if(!(ft = (struct aim_filetransfer_priv *)calloc(1, sizeof(struct aim_filetransfer_priv)))) {
      printf("faim: couldn't malloc ft. um. bad. bad bad. file transfer will likely fail, sorry.\n");
      faim_mutex_unlock(&conn->active);
      return 0;
    }

    if(hdrlen != 0x100)
      printf("faim: fileget_command(120b): um. hdrlen != 0x100..\n");
    
    fh = aim_oft_getfh(hdr);

    memcpy(&(ft->fh), fh, sizeof(struct aim_fileheader_t));
    
    cook = aim_checkcookie(sess, (unsigned char *)ft->fh.bcookie, AIM_COOKIETYPE_OFTGET);
  
    if(cook->data)
      free(cook->data); /* XXX: integrate cookie caching */

    cook->data = ft;
    
    aim_cachecookie(sess, cook);

    faim_mutex_unlock(&conn->active);
    
    break;
  }
  case 0x120c: { /* yet more get file */
    struct aim_filetransfer_priv *ft;
    struct aim_msgcookie_t *cook;
    struct aim_fileheader_t *listingfh;
    struct command_tx_struct *newoft;
    int curbyte, i;
    
    printf("faim: rend: fileget 120c\n");

    if(!(ft = (struct aim_filetransfer_priv *)calloc(1, sizeof(struct aim_filetransfer_priv)))) {
      printf("faim: couldn't malloc ft. um. bad. bad bad. file transfer will likely fail, sorry.\n");
      faim_mutex_unlock(&conn->active);
      return 0;
    }

    if(hdrlen != 0x100)
      printf("faim: fileget_command(120c): um. hdrlen != 0x100..\n");

    listingfh = aim_oft_getfh(hdr);

    memcpy(&(ft->fh), listingfh, sizeof(struct aim_fileheader_t));
    
    cook = aim_checkcookie(sess, (unsigned char *)ft->fh.bcookie, AIM_COOKIETYPE_OFTGET);
  
    if(cook->data)
      free(cook->data); /* XXX */

    cook->data = ft;
    
    aim_cachecookie(sess, cook);

    faim_mutex_unlock(&conn->active);

    printf("faim: fileget: %s seems to want %s\n", ft->sn, ft->fh.name);
 
    if(!(newoft = aim_tx_new(AIM_FRAMETYPE_OFT, 0x0101, conn, 0/*listingfh->size*/))) {
      printf("faim: send_final_transfer: tx_new OFT failed\n");
      return 0;
    }
    
    /* XXX: actually implement Real Handling of all this */

    printf("jbm: listingfh->size: 0x%lx\n", listingfh->size);

    newoft->lock = 1;

    /*    if(!(newoft->data = calloc(1, listingfh->size))) {
      printf("newoft data malloc failed. bombing.\n");
      return 0;
      }*/

    if(newoft->commandlen > 0) {
      int i;
      bzero(newoft->data, newoft->commandlen);
      for(i = 0; i < newoft->commandlen; i++)
	newoft->data[i] = 0x30 + (i%10);

      //      memcpy(newoft->data, "This has been a Test\r\n-josh\r\n", newoft->commandlen);
    }

    memcpy(newoft->hdr.oft.magic, "OFT2", 4);
    newoft->hdr.oft.hdr2len = 0xf8; /* 0x100 - 8 */
    
    if (!(newoft->hdr.oft.hdr2 = calloc(1,newoft->hdr.oft.hdr2len))) {
      if(newoft->data)
	free(newoft->data); /* XXX: make this into a destructor function */
      free(newoft);
      return 0;
    }  

    memcpy(listingfh->bcookie, ft->fh.bcookie, 8);

    curbyte = 0;
    
    for(i = 0; i < 8; i++)
      curbyte += aimutil_put8(newoft->hdr.oft.hdr2+curbyte, listingfh->bcookie[i]);
    curbyte += aimutil_put16(newoft->hdr.oft.hdr2+curbyte, listingfh->encrypt);
    curbyte += aimutil_put16(newoft->hdr.oft.hdr2+curbyte, listingfh->compress);
    curbyte += aimutil_put16(newoft->hdr.oft.hdr2+curbyte, listingfh->totfiles);
    curbyte += aimutil_put16(newoft->hdr.oft.hdr2+curbyte, listingfh->filesleft);
    curbyte += aimutil_put16(newoft->hdr.oft.hdr2+curbyte, listingfh->totparts);
    curbyte += aimutil_put16(newoft->hdr.oft.hdr2+curbyte, listingfh->partsleft);
    curbyte += aimutil_put32(newoft->hdr.oft.hdr2+curbyte, listingfh->totsize);
    curbyte += aimutil_put32(newoft->hdr.oft.hdr2+curbyte, listingfh->size);
    curbyte += aimutil_put32(newoft->hdr.oft.hdr2+curbyte, listingfh->modtime);
    curbyte += aimutil_put32(newoft->hdr.oft.hdr2+curbyte, listingfh->checksum);
    curbyte += aimutil_put32(newoft->hdr.oft.hdr2+curbyte, listingfh->rfrcsum);
    curbyte += aimutil_put32(newoft->hdr.oft.hdr2+curbyte, listingfh->rfsize);
    curbyte += aimutil_put32(newoft->hdr.oft.hdr2+curbyte, listingfh->cretime);
    curbyte += aimutil_put32(newoft->hdr.oft.hdr2+curbyte, listingfh->rfcsum);
    curbyte += aimutil_put32(newoft->hdr.oft.hdr2+curbyte, 0 /*listingfh->nrecvd*/);
    curbyte += aimutil_put32(newoft->hdr.oft.hdr2+curbyte, 0/*listingfh->recvcsum*/);

    strncpy((char *)newoft->hdr.oft.hdr2+curbyte, listingfh->idstring, 32);
    curbyte += 32;

    curbyte += aimutil_put8(newoft->hdr.oft.hdr2+curbyte, 0x20 /*listingfh->flags */);
    curbyte += aimutil_put8(newoft->hdr.oft.hdr2+curbyte, listingfh->lnameoffset);
    curbyte += aimutil_put8(newoft->hdr.oft.hdr2+curbyte, listingfh->lsizeoffset);

    memcpy(newoft->hdr.oft.hdr2+curbyte, listingfh->dummy, 69);
    curbyte += 69;
    
    memcpy(newoft->hdr.oft.hdr2+curbyte, listingfh->macfileinfo, 16);
    curbyte += 16;

    curbyte += aimutil_put16(newoft->hdr.oft.hdr2+curbyte, listingfh->nencode);
    curbyte += aimutil_put16(newoft->hdr.oft.hdr2+curbyte, listingfh->nlanguage);

    strncpy((char *)newoft->hdr.oft.hdr2+curbyte, listingfh->name, 64);
    curbyte += 64;

    free(listingfh);

    newoft->lock = 0;
    aim_tx_enqueue(sess, newoft);
    printf("jbm: OFT listing enqueued.\n");

    break;
  }
  case 0x0202: { /* get file: ready to recieve data */
    char *c;
    int i;

    struct aim_fileheader_t *fh;    
    fh = aim_oft_getfh(hdr);

    c = (char *)calloc(1, fh->size);

    printf("looks like we're ready to send data.(oft 0x0202)\n");


    
    for(i = 0; i < fh->size; i++)
      c[i] = 0x30 + (i%10);

    if ( (i = write(conn->fd, c, fh->size)) != fh->size ) {
      printf("whoopsy, didn't write it all...\n");
    }

    faim_mutex_unlock(&conn->active);

    break;
  }
  case 0x0204: { /* get file: finished. close it up */
    printf("looks like we're done with a transfer (oft 0x0204)\n");
    faim_mutex_unlock(&conn->active);
    aim_conn_close(conn);
    break;
  }
  default: {
    printf("OFT frame: type %04x\n", hdrtype);  
    /* data connection may be unreliable here */
    faim_mutex_unlock(&conn->active);
    break;
  }
  } /* switch */

  free(hdr);
  
  return 0;
}

/*
 * this currently feeds totally bogus data
 */

faim_internal struct aim_fileheader_t *aim_oft_getfh(unsigned char *hdr) 
{
  struct aim_fileheader_t *fh;
  int i, j;

  if(!(fh = calloc(1, sizeof(struct aim_fileheader_t))))
    return NULL;

  /* [0] and [1] are the type. we can ignore those here. */

  i = 2;

  for(j = 0; j < 8; j++, i++)
    fh->bcookie[j] = hdr[i];
  fh->encrypt = aimutil_get16(hdr+i);
  i += 2;
  fh->compress = aimutil_get16(hdr+i);
  i += 2;
  fh->totfiles = aimutil_get16(hdr+i);
  i += 2;
  fh->filesleft = aimutil_get16(hdr+i);
  i += 2;
  fh->totparts = aimutil_get16(hdr+i);
  i += 2;
  fh->partsleft = aimutil_get16(hdr+i);
  i += 2;
  fh->totsize = aimutil_get32(hdr+i);
  i += 4;
  fh->size = aimutil_get32(hdr+i);
  i += 4;
  fh->modtime = aimutil_get32(hdr+i);
  i += 4;
  fh->checksum = aimutil_get32(hdr+i);
  i += 4;
  fh->rfrcsum = aimutil_get32(hdr+i);
  i += 4;
  fh->rfsize = aimutil_get32(hdr+i);
  i += 4;
  fh->cretime = aimutil_get32(hdr+i);
  i += 4;
  fh->rfcsum = aimutil_get32(hdr+i);
  i += 4;
  fh->nrecvd = aimutil_get32(hdr+i);
  i += 4;
  fh->recvcsum = aimutil_get32(hdr+i);
  i += 4;

  memcpy(fh->idstring, hdr+i, 32);
  i += 32;

  fh->flags = aimutil_get8(hdr+i);
  i += 1;
  fh->lnameoffset = aimutil_get8(hdr+i);
  i += 1;
  fh->lsizeoffset = aimutil_get8(hdr+i);
  i += 1;

  memcpy(fh->dummy, hdr+i, 69);
  i += 69;

  memcpy(fh->macfileinfo, hdr+i, 16);
  i += 16;

  fh->nencode = aimutil_get16(hdr+i);
  i += 2;
  fh->nlanguage = aimutil_get16(hdr+i);
  i += 2;

  memcpy(fh->name, hdr+i, 64);
  i += 64;

  return fh;
}
