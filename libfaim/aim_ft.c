#include <faim/aim.h>

#ifndef _WIN32
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/utsname.h> /* for aim_directim_initiate */
#include <arpa/inet.h> /* for inet_ntoa */
#endif

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
  size_t clilen = sizeof(cliaddr);
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

  if (!sess || !conn || !(conn->type) || (conn->type != AIM_CONN_TYPE_RENDEZVOUS) || !conn->priv || !msg) {
    printf("faim: directim: invalid arguments\n");
    return -1;
  };

  if (strlen(msg) >= MAXMSGLEN)
    return -1;

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

  i += aimutil_putstr(newpacket2->hdr.oft.hdr2+i, sess->sn, strlen(sess->sn));
  
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

  i += aimutil_putstr(newpacket->hdr.oft.hdr2+i, sess->sn, strlen(sess->sn));
  
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
  char localhost[129];

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

  if(gethostname(localhost, 128) < 0)
    return NULL;

  if( (hptr = gethostbyname(localhost)) == NULL)
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
    curbyte += aimutil_put8(newpacket->data+curbyte, 0x30 + ((u_char) rand() % 20));
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

  return (newconn);
} 

/*
 * struct aim_conn_t *aim_directim_connect(struct aim_session_t *sess, struct aim_conn_t *conn, struct aim_directim_priv *priv)
 * sess is the session to append the conn to,
 * conn is the BOS connection,
 * priv is the filled-in priv data structure for the connection
 * returns conn if connected, NULL on error
 */


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

/*
 * struct aim_conn_t *aim_directim_getconn(struct aim_session_t *sess, const char *name)
 * sess is your session, 
 * name is the name to get,
 * returns conn for directim with name, NULL if none found.
 */

faim_export struct aim_conn_t *aim_directim_getconn(struct aim_session_t *sess, const char *name)
{
  struct aim_conn_t *cur;
  struct aim_directim_priv *priv;
  
  faim_mutex_lock(&sess->connlistlock);
  for (cur = sess->connlist; cur; cur = cur->next) {
    if (cur->type != AIM_CONN_TYPE_RENDEZVOUS || cur->subtype != AIM_CONN_SUBTYPE_OFT_DIRECTIM)
      continue;
    
    priv = cur->priv;
    if (aim_sncmp(priv->sn, name) == 0)
      break;
  }
  faim_mutex_unlock(&sess->connlistlock);

  return cur;
}

/*
 * aim_accepttransfer:
 * accept a file transfer request.
 * sess is the session,
 * conn is the BOS conn for the CAP reply
 * sn is the screenname to send it to,
 * cookie is the cookie used
 * ip is the ip to connect to
 * file is the listing file to use
 * rendid is CAP type
 *
 * returns connection
 */

faim_export struct aim_conn_t *aim_accepttransfer(struct aim_session_t *sess,
			  struct aim_conn_t *conn, 
			  char *sn,
			  char *cookie,
			  char *ip,
			  FILE *file,
			  unsigned short rendid)
{
  struct command_tx_struct *newpacket, *newoft;

  struct aim_conn_t *newconn;
  struct aim_fileheader_t *listingfh;
  struct aim_filetransfer_priv *priv;
  struct aim_msgcookie_t *cachedcook;

  int curbyte, i;

  if(rendid == AIM_CAPS_GETFILE) {

    newconn = aim_newconn(sess, AIM_CONN_TYPE_RENDEZVOUS, ip);
    
    newconn->subtype = AIM_CONN_SUBTYPE_OFT_GETFILE;
    
    if (!newconn || (newconn->fd == -1)) {
      perror("aim_newconn");
      faimdprintf(2, "could not connect to %s (fd: %i)\n", ip, newconn?newconn->fd:0);
      aim_conn_kill(sess, &newconn);
      return NULL;
    } else {
      priv = (struct aim_filetransfer_priv *)calloc(1, sizeof(struct aim_filetransfer_priv));
      memcpy(priv->cookie, cookie, 8);
      strncpy(priv->sn, sn, MAXSNLEN);
      strncpy(priv->ip, ip, sizeof(priv->ip));
      newconn->priv = (void *)priv;
      faimdprintf(2, "faim: connected to peer (fd = %d)\n", newconn->fd);
    }
 
    /* cache da cookie. COOOOOKIES! */

    if(!(cachedcook = (struct aim_msgcookie_t *)calloc(1, sizeof(struct aim_msgcookie_t)))) {
      faimdprintf(1, "faim: accepttransfer: couldn't calloc cachedcook. yeep!\n");
      /* XXX die here, or go on? search for cachedcook for future references */
    }

    if(cachedcook)
      memcpy(cachedcook->cookie, cookie, 8);
  
    /*  strncpy(ft->fh.name, miscinfo->value+8, sizeof(ft->fh.name)); */

    if(cachedcook) { /* see above calloc of cachedcook */
      cachedcook->type = AIM_COOKIETYPE_OFTGET;
      cachedcook->data = (void *)priv;
    
      if (aim_cachecookie(sess, cachedcook) != 0)
	faimdprintf(1, "faim: ERROR caching message cookie\n");
    }
  
    if(rendid == AIM_CAPS_GETFILE) {
      faimdprintf(2, "faim: getfile request accept\n");
      if(!(newoft = aim_tx_new(AIM_FRAMETYPE_OFT, 0x1108, newconn, 0))) {
	faimdprintf(2, "faim: aim_accepttransfer: tx_new OFT failed\n");
	/* XXX: what else do we need to clean up here? */
	return NULL;
      }
    
      newoft->lock = 1;
    
      memcpy(newoft->hdr.oft.magic, "OFT2", 4);
      newoft->hdr.oft.hdr2len = 0xf8; /* 0x100 - 8 */
    
      if(!(listingfh = aim_getlisting(file))) {
	return NULL;
      }      

      if (!(newoft->hdr.oft.hdr2 = (char *)calloc(1,newoft->hdr.oft.hdr2len))) {
	free(newoft);
	return NULL;
      }  

      memcpy(listingfh->bcookie, cookie, 8);

      aim_oft_buildheader((void *)newoft->hdr.oft.hdr2, listingfh);

      free(listingfh);

      newoft->lock = 0;
      aim_tx_enqueue(sess, newoft);
      printf("faim: getfile: OFT listing enqueued.\n");
    
    }

    /* OSCAR CAP accept packet */

    if(!(newpacket = aim_tx_new(AIM_FRAMETYPE_OSCAR, 0x0002, conn, 10+8+2+1+strlen(sn)+4+2+8+16)))
      return NULL;
  
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

    return newconn;
  }
  return NULL;
}

/*
 * aim_getlisting(FILE *file)
 * 
 * file is an opened listing file
 * returns a pointer to the filled-in fileheader_t
 *
 * currently omits checksum. we'll fix this when AOL breaks us, i
 * guess.
 *
 */

faim_internal struct aim_fileheader_t *aim_getlisting(FILE *file) 
{
  struct aim_fileheader_t *fh;
  u_long totsize = 0, size = 0, checksum = 0xffff0000;
  short totfiles = 0;
  char *linebuf, sizebuf[9];
  
  int linelength = 1024;

  /* XXX: if we have a line longer than 1024chars, God help us. */
  if( (linebuf = (char *)calloc(1, linelength)) == NULL ) {
    faimdprintf(2, "linebuf calloc failed\n");
    return NULL;
  }  

  if(fseek(file, 0, SEEK_END) == -1) { /* use this for sanity check */
    perror("getlisting END1 fseek:");
    faimdprintf(2, "getlising fseek END1 error\n");
  }

  if(fgetpos(file, &size) == -1) {
    perror("getlisting END1 getpos:");
    faimdprintf(2, "getlising getpos END1 error\n");
  }

  if(fseek(file, 0, SEEK_SET) != 0) {
    perror("getlesting fseek(SET):");
    faimdprintf(2, "faim: getlisting: couldn't seek to beginning of listing file\n");
  }

  bzero(linebuf, linelength);

  size = 0;

  while(fgets(linebuf,  linelength, file)) {
    totfiles++;
    bzero(sizebuf, 9);

    size += strlen(linebuf);
    
    if(strlen(linebuf) < 23) {
      faimdprintf(2, "line \"%s\" too short. skipping\n", linebuf);
      continue;
    }
    if(linebuf[strlen(linebuf)-1] != '\n') {
      faimdprintf(2, "faim: OFT: getlisting -- hit EOF or line too long!\n");
    }

    memcpy(sizebuf, linebuf+17, 8);

    totsize += strtol(sizebuf, NULL, 10);
    bzero(linebuf, linelength);
  }   

  /*  if(size != 0) {
    faimdprintf(2, "faim: getlisting: size != 0 after while.. %i\n", size);
    }*/

  if(fseek(file, 0, SEEK_SET) == -1) {
    perror("getlisting END2 fseek:");
    faimdprintf(2, "getlising fseek END2 error\n");
  }  

  free(linebuf);

  /* we're going to ignore checksumming the data for now -- that
   * requires walking the whole listing.txt. it should probably be
   * done at register time and cached, but, eh. */  

  if(!(fh = (struct aim_fileheader_t*)calloc(1, sizeof(struct aim_fileheader_t))))
    return NULL;

  printf( "faim: OFT: getlisting: totfiles: %u, totsize: %lu, size: %lu\n", totfiles, totsize, size);

  fh->encrypt     = 0x0000;
  fh->compress    = 0x0000; 
  fh->totfiles    = totfiles;
  fh->filesleft   = totfiles; /* is this right ?*/
  fh->totparts    = 0x0001;
  fh->partsleft   = 0x0001;
  fh->totsize     = totsize;
  fh->size        = size; /* ls -l listing.txt */
  fh->modtime     = (int)time(NULL); /* we'll go with current time for now */
  fh->checksum    = checksum; /* XXX: checksum ! */
  fh->rfcsum      = 0x00000000;
  fh->rfsize      = 0x00000000;
  fh->cretime     = 0x00000000;
  fh->rfcsum      = 0x00000000;
  fh->nrecvd      = 0x00000000;
  fh->recvcsum    = 0x00000000;

  /*  memset(fh->idstring, 0, sizeof(fh->idstring)); */
  memcpy(fh->idstring, "OFT_Windows ICBMFT V1.1 32", sizeof(fh->idstring));
  memset(fh->idstring+strlen(fh->idstring), 0, sizeof(fh->idstring)-strlen(fh->idstring));

  fh->flags       = 0x02;
  fh->lnameoffset = 0x1a;
  fh->lsizeoffset = 0x10;

  /*  memset(fh->dummy, 0, sizeof(fh->dummy)); */
  memset(fh->macfileinfo, 0, sizeof(fh->macfileinfo));

  fh->nencode     = 0x0000; /* we need to figure out these encodings for filenames */
  fh->nlanguage   = 0x0000;

  /*  memset(fh->name, 0, sizeof(fh->name)); */
  memcpy(fh->name, "listing.txt", sizeof(fh->name));
  memset(fh->name+strlen(fh->name), 0, 64-strlen(fh->name));

  faimdprintf(2, "faim: OFT: listing fh name %s / %s\n", fh->name, (fh->name+(strlen(fh->name))));
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
#if defined(__linux__) /* XXX what other OS's support getaddrinfo? */
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
  int listenfd;
  const int on = 1;
  struct sockaddr_in sockin;
  
  if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket(listenfd)");
    return -1;
  } 
  if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on) != 0)) {
    perror("setsockopt(listenfd)");
    close(listenfd);
    return -1;
  }
  memset(&sockin, 0, sizeof(struct sockaddr_in));
  sockin.sin_family = AF_INET;
  sockin.sin_port = htons(portnum);
  if (bind(listenfd, (struct sockaddr *)&sockin, sizeof(struct sockaddr_in)) != 0) {
    perror("bind(listenfd)");
    close(listenfd);
    return -1;
  }
  if (listen(listenfd, 4) != 0) {
    perror("listen(listenfd)");
    close(listenfd);
    return -1;
  }

  return listenfd;
#endif
}

faim_internal int aim_get_command_rendezvous(struct aim_session_t *sess, struct aim_conn_t *conn)
{
  unsigned char hdrbuf1[6];
  unsigned char *hdr = NULL;
  int hdrlen, hdrtype;
  int flags = 0;
  rxcallback_t userfunc = NULL;


  memset(hdrbuf1, 0, sizeof(hdrbuf1));

  faim_mutex_lock(&conn->active); /* gets locked down for the entirety */

  if ( (hdrlen = aim_recv(conn->fd, hdrbuf1, 6)) < 6) {    
 
    faimdprintf(2, "faim: rend: read error (fd: %i) %02x%02x%02x%02x%02x%02x (%i)\n", conn->fd, hdrbuf1[0],hdrbuf1[1],hdrbuf1[0],hdrbuf1[0],hdrbuf1[0],hdrbuf1[0],hdrlen);
    faim_mutex_unlock(&conn->active);
    
    if(hdrlen < 0)
      perror("read");
    else { /* disconnected */
      switch(conn->subtype) {
      case AIM_CONN_SUBTYPE_OFT_DIRECTIM: { /* XXX: clean up cookies here ? */
	struct aim_directim_priv *priv = NULL;
	if(!(priv = (struct aim_directim_priv *)conn->priv) ) 
	  return -1; /* not much we can do */
	aim_uncachecookie(sess, priv->cookie, AIM_COOKIETYPE_OFTIM);

	
	if ( (userfunc = aim_callhandler(conn, AIM_CB_FAM_OFT, AIM_CB_OFT_DIRECTIMDISCONNECT)) ) {
	  aim_conn_close(conn);
	  return  userfunc(sess, NULL, conn, priv->sn);
	}

	break;
      }

      case AIM_CONN_SUBTYPE_OFT_GETFILE: {
	struct aim_filetransfer_priv *priv;
	if(!(priv = (struct aim_filetransfer_priv *)conn->priv))
	  return -1;

	aim_uncachecookie(sess, priv->cookie, AIM_COOKIETYPE_OFTGET);

	if ( (userfunc = aim_callhandler(conn, AIM_CB_FAM_OFT, AIM_CB_OFT_GETFILEDISCONNECT)) ) {
	  aim_conn_close(conn);
	  return userfunc(sess, NULL, conn, priv->sn);
	}

	break;
      }

      case AIM_CONN_SUBTYPE_OFT_SENDFILE: {
	struct aim_filetransfer_priv *priv;
	if(!(priv = (struct aim_filetransfer_priv *)conn->priv))
	  return -1;

	aim_uncachecookie(sess, priv->cookie, AIM_COOKIETYPE_OFTSEND);

	if ( (userfunc = aim_callhandler(conn, AIM_CB_FAM_OFT, AIM_CB_OFT_SENDFILEDISCONNECT)) ) {
	  aim_conn_close(conn);
	  return userfunc(sess, NULL, conn, priv->sn);
	}

	break;
      }
      }

      aim_conn_close(conn);
      aim_conn_kill(sess, &conn);
      
      return -1;
    }
  }

  hdrlen = aimutil_get16(hdrbuf1+4);

  hdrlen -= 6;
  if (!(hdr = malloc(hdrlen))) {
    faim_mutex_unlock(&conn->active);
    return -1;
  }

  if (aim_recv(conn->fd, hdr, hdrlen) < hdrlen) {
    perror("read");
    faimdprintf(2,"faim: rend: read2 error\n");
    free(hdr);
    faim_mutex_unlock(&conn->active);
    aim_conn_close(conn);
    return -1;
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

    faimdprintf(2, "faim: OFT frame: %04x / %04x / %04x / %s\n", hdrtype, payloadlength, flags, snptr); 

    if (flags == 0x000e) {
      faim_mutex_unlock(&conn->active);
      if ( (userfunc = aim_callhandler(conn, AIM_CB_FAM_OFT, AIM_CB_OFT_DIRECTIMTYPING)) )
	return userfunc(sess, NULL, snptr);
    } else if ((flags == 0x0000) && payloadlength) {
      unsigned char *msg;

      if(! (msg = calloc(1, payloadlength+1)) ) {
	faim_mutex_unlock(&conn->active);
	return -1;
      }
      
      if (aim_recv(conn->fd, msg, payloadlength) < payloadlength) {
	perror("read");
	printf("faim: rend: read3 error\n");
	free(msg);
	faim_mutex_unlock(&conn->active);
	aim_conn_close(conn);
	return -1;
      }
      faim_mutex_unlock(&conn->active);
      msg[payloadlength] = '\0';
      faimdprintf(2, "faim: directim: %s/%04x/%04x/%s\n", snptr, payloadlength, flags, msg);


      if ( (userfunc = aim_callhandler(conn, AIM_CB_FAM_OFT, AIM_CB_OFT_DIRECTIMINCOMING)) )
	i = userfunc(sess, NULL, conn, snptr, msg);
      
      free(msg);
      return i;
    }
    break;
  }
#if 0
  /* currently experimental to a non-compiling degree */
  case 0x1108: { /* getfile listing.txt incoming tx->rx */
    struct aim_filetransfer_priv *ft;
    struct aim_fileheader_t *fh;
    struct aim_msgcookie_t *cook;
    struct aim_conn_type *newoft;

    int commandlen;
    char *data;

    faimdprintf(2,"faim: rend: fileget 0x1108\n");
    
    if(!(ft = (struct aim_filetransfer_priv *)calloc(1, sizeof(struct aim_filetransfer_priv)))) {
      faimdprintf(2, "faim: couldn't malloc ft. um. bad. bad bad. file transfer will likely fail, sorry.\n");
      faim_mutex_unlock(&conn->active);
      return -1;
    }

    ft->state = 1; /* we're waaaaiiiting.. */

    fh = aim_oft_getfh(hdr);

    memcpy(&(ft->fh), fh, sizeof(struct aim_fileheader_t));
    
    if(!(cook = aim_checkcookie(sess, ft->fh.bcookie, AIM_COOKIETYPE_OFTGET))) {
      faim_mutex_unlock(&conn->active);
      return -1;
    }

    if(cook->data)
      free(cook->data);

    cook->data = ft;

    aim_cachecookie(sess, cook);

    if(!(newoft = aim_tx_new(AIM_FRAMETYPE_OFT, 0x1209, conn, 0))) {
      /* XXX: what else do we need to clean up here? */
      faim_mutex_unlock(&conn->active);
      return -1;
    }


    aim_oft_buildheader((void *)newoft->hdr.oft.hdr2, ft->fh); /* no change */
    newoft->lock = 0;
    aim_tx_enqueue(sess, newoft);
    break;
  }
#endif 
  case 0x1209: { /* get file listing ack rx->tx */
    struct aim_filetransfer_priv *ft;
    struct aim_fileheader_t *fh;
    struct aim_msgcookie_t *cook;

    int commandlen;
    char *data;

    faimdprintf(2,"faim: rend: fileget 0x1209\n");

    if(!(ft = (struct aim_filetransfer_priv *)calloc(1, sizeof(struct aim_filetransfer_priv)))) {
      faimdprintf(2, "faim: couldn't malloc ft. um. bad. bad bad. file transfer will likely fail, sorry.\n");
      faim_mutex_unlock(&conn->active);
      return -1;
    }

    fh = aim_oft_getfh(hdr);

    memcpy(&(ft->fh), fh, sizeof(struct aim_fileheader_t));
    
    cook = aim_checkcookie(sess, ft->fh.bcookie, AIM_COOKIETYPE_OFTGET);

    /* we currently trust the other client to be giving us Valid
     *  Enough input, else this gets to be a messy function (and we
     *  won't break like winaim does when it gets bad input =) */

    if(cook->data)
      free(cook->data); /* XXX */
  
    cook->data = ft;
    
    aim_cachecookie(sess, cook);

    /* XXX: have this send chunks of the file instead of the whole
     * file. requires rethinking some code. */

    if(fseek(sess->oft.listing, 0, SEEK_SET) != 0) {
      perror("get_command_rendezvous 1209 fseek(SET):");
      faimdprintf(2, "faim: getlisting: couldn't seek to beginning of listing file\n");
    }
    commandlen = ft->fh.size;

    if((data = (char *)calloc(1, commandlen)) == NULL) {
      faimdprintf(2, "faim: get_command_rendezvous 1209: couldn't malloc data.\n");
      faim_mutex_unlock(&conn->active);
      return -1;

    }

    for(commandlen = 0; commandlen < ft->fh.size; commandlen++)
      if( (data[commandlen] = (unsigned char)fgetc(sess->oft.listing)) == EOF)
	faimdprintf(2, "faim: get_command_rendezvous 1209: got early EOF (error?)\n");

    commandlen = ft->fh.size;

    if (write(conn->fd, data, commandlen) != commandlen) {
      perror("listing write error");
    }
    faim_mutex_unlock(&conn->active);

    faimdprintf(2, "faim: get_command_rendezvous: hit end of 1209\n");

    free(data);

    break;
  }
  case 0x120b: { /* get file second */
    struct aim_filetransfer_priv *ft;
    struct aim_msgcookie_t *cook;

    struct aim_fileheader_t *fh;

    faimdprintf(2, "faim: rend: fileget 120b\n");

    if(!(ft = (struct aim_filetransfer_priv *)calloc(1, sizeof(struct aim_filetransfer_priv)))) {
      faimdprintf(2, "faim: couldn't malloc ft. um. bad. bad bad. file transfer will likely fail, sorry.\n");
      faim_mutex_unlock(&conn->active);
      return -1;
    }

    fh = aim_oft_getfh(hdr);

    memcpy(&(ft->fh), fh, sizeof(struct aim_fileheader_t));
 
    if(!(cook = aim_checkcookie(sess, ft->fh.bcookie, AIM_COOKIETYPE_OFTGET))) {
      faim_mutex_unlock(&conn->active);
      return -1;
    }
  
    if(cook->data)
      free(cook->data); /* XXX: integrate cookie caching */

    cook->data = ft;
    
    aim_cachecookie(sess, cook);

    faim_mutex_unlock(&conn->active);

    /* call listing.txt rx confirm */    

    break;
  }
  case 0x120c: { /* yet more get file */
    struct aim_filetransfer_priv *ft;
    struct aim_msgcookie_t *cook;
    struct aim_fileheader_t *listingfh;
    struct command_tx_struct *newoft;

    int i;

    rxcallback_t userfunc;
    
    printf("faim: rend: fileget 120c\n");

    if(!(ft = (struct aim_filetransfer_priv *)calloc(1, sizeof(struct aim_filetransfer_priv)))) {
      printf("faim: couldn't malloc ft. um. bad. bad bad. file transfer will likely fail, sorry.\n");
      faim_mutex_unlock(&conn->active);
      return -1;
    }

    if(hdrlen != 0x100)
      printf("faim: fileget_command(120c): um. hdrlen != 0x100..\n");

    listingfh = aim_oft_getfh((char *)hdr);

    memcpy(&(ft->fh), listingfh, sizeof(struct aim_fileheader_t));
    
    if(!(cook = aim_checkcookie(sess, ft->fh.bcookie, AIM_COOKIETYPE_OFTGET))) {
      faim_mutex_unlock(&conn->active);
      return -1;
    }
  
    if(cook->data)
      free(cook->data); /* XXX */

    cook->data = ft;
    
    aim_cachecookie(sess, cook);

    faim_mutex_unlock(&conn->active);

    faimdprintf(2, "faim: fileget: %s seems to want %s\n", ft->sn, ft->fh.name);

    if( (userfunc = aim_callhandler(conn, AIM_CB_FAM_OFT, AIM_CB_OFT_GETFILEFILEREQ)) )
      i = userfunc(sess, NULL, conn, &ft->fh, cook->cookie);
    
    if(i < 0)
      return -1;
      
    if(!(newoft = aim_tx_new(AIM_FRAMETYPE_OFT, 0x0101, conn, 0))) {
      faimdprintf(2, "faim: send_final_transfer: tx_new OFT failed\n");
      return -1;
    }
    
    newoft->lock = 1;
    
    memcpy(newoft->hdr.oft.magic, "OFT2", 4);
    newoft->hdr.oft.hdr2len = 0xf8; /* 0x100 - 8 */
    
    if (!(newoft->hdr.oft.hdr2 = calloc(1,newoft->hdr.oft.hdr2len))) {
      newoft->lock = 0;
      aim_tx_destroy(newoft);
      return -1;
    }  
    
    /*  memcpy(listingfh->bcookie, ft->fh.bcookie, 8); */
    
    listingfh->nrecvd = 0; /* these need reset for protocol-correctness */
    listingfh->recvcsum = 0;

    aim_oft_buildheader((void *)newoft->hdr.oft.hdr2, listingfh);
    
    newoft->lock = 0;
    aim_tx_enqueue(sess, newoft);
    faimdprintf(2, "faim: OFT: OFT file enqueued.\n");
    
    if( (userfunc = aim_callhandler(conn, AIM_CB_FAM_OFT, AIM_CB_OFT_GETFILEFILEREQ)) == NULL)
      return 1;
    
    i = userfunc(sess, NULL, conn, listingfh, listingfh->bcookie);

    free(listingfh);

    return i;

    break;
  }
  case 0x0202: { /* get file: ready to recieve data */
    struct aim_fileheader_t *fh;    
    fh = aim_oft_getfh((char *)hdr);

    faimdprintf(2, "faim: get_rend: looks like we're ready to send data.(oft 0x0202)\n");

    faim_mutex_unlock(&conn->active);
    
    if ( (userfunc = aim_callhandler(conn, AIM_CB_FAM_OFT, AIM_CB_OFT_GETFILEFILESEND)) == NULL)
      return 1;

    return userfunc(sess, NULL, conn, fh);       

    free(fh);

    break;
  }
  case 0x0204: { /* get file: finished. close it up */
    int i;

    struct aim_fileheader_t *fh;    
    fh = aim_oft_getfh((char *)hdr);

    faimdprintf(2, "faim: get_rend: looks like we're done with a transfer (oft 0x0204)\n");

    faim_mutex_unlock(&conn->active);
    
    if ( (userfunc = aim_callhandler(conn, AIM_CB_FAM_OFT, AIM_CB_OFT_GETFILECOMPLETE)) )
      i = userfunc(sess, NULL, conn, fh);
    else
      i = 1;

    /*
	  free(fh); */
    /* not sure where to do this yet, as we need to keep it to allow multiple file sends... bleh */

    return i;
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
 * aim_oft_registerlisting()
 * sess: aim session
 * file: opened FILE *
 * listingdir: the path to listing.txt
 * returns -1 on error, 0 on success.
 *
 * it's not my problem if the listing fd is already set.
 */

faim_export int aim_oft_registerlisting(struct aim_session_t *sess, FILE *file, char* listingdir) 
{
  if(!sess)
    return -1;

  /* XXX: checksum each file in the listing */

#if 0
   if(sess->oft.listing) {
     faimdprintf(1, "We already have a file pointer. Closing and overwriting.\n");
    fclose(sess->oft.listing);
  }
#endif
  sess->oft.listing = file;
#if 0
  if(sess->oft.listingdir) {
    faimdprintf(1, "We already have a directory string. Freeing and overwriting\n");
    free(sess->oft.listingdir);
  }
#endif
  
  if( (sess->oft.listingdir = (char *)calloc(1, strlen(listingdir)+1)) )
    memcpy(sess->oft.listingdir, listingdir, strlen(listingdir));
  else
    return -1;
  return 0;
}

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

faim_export int aim_oft_checksum(char *buffer, int bufsize, int *checksum)
{    
  short check0, check1;
  int i;
  
  check0 = ((*checksum & 0xFF000000) >> 16);
  check1 = ((*checksum & 0x00ff0000) >> 16);

  for(i = 0; i < bufsize; i++) {

    if(i % 2)  { /* use check1 -- second byte */
      if ( (short)buffer[i] > check1 ) { /* wrapping */
	
	check1 += 0x100; /* this is a cheap way to wrap */

	/* if we're wrapping, decrement the other one */
	if(check0 == 0) /* XXX: check this corner case */
	  check0 = 0x00ff;
	else
	  check0--;			
      }

      check1 -= buffer[i];
    } else { /* use check0 -- first byte */
      if ( (short)buffer[i] > check0 ) { /* wrapping */
	
	check0 += 0x100; /* this is a cheap way to wrap */

	/* if we're wrapping, decrement the other one */
	if(check1 == 0) /* XXX: check this corner case */
	  check1 = 0x00ff;
	else
	  check1--;			
      }

      check0 -= buffer[i];
    } 
  }

  if(check0 > 0xff || check1 > 0xff) { /* they shouldn't be able to do this. error! */
    faimdprintf(2, "check0 or check1 is too high: 0x%04x, 0x%04x\n", check0, check1);
    return -1;
  }
  
  check0 &= 0xff; /* grab just the lowest byte */
  check1 &= 0xff; /* this should be clean, but just in case */

  *checksum = ((check0 * 0x1000000) + (check1 * 0x10000));

  return *checksum;
}


/*
 * aim_oft_buildheader:
 * fills a buffer with network-order fh data.
 * returns length written; -1 on error.
 * dest: buffer to fill -- pre-alloced
 * listingfh: fh to get data from
 *
 * DOES NOT DO BOUNDS CHECKING!
 */


faim_internal int aim_oft_buildheader(char *dest,struct aim_fileheader_t *listingfh) 
{
  int i, curbyte;

  if(!dest || !listingfh)
    return -1;
  
  curbyte = 0;    
  for(i = 0; i < 8; i++)
    curbyte += aimutil_put8(dest+curbyte, listingfh->bcookie[i]);
  curbyte += aimutil_put16(dest+curbyte, listingfh->encrypt);
  curbyte += aimutil_put16(dest+curbyte, listingfh->compress);
  curbyte += aimutil_put16(dest+curbyte, listingfh->totfiles);
  curbyte += aimutil_put16(dest+curbyte, listingfh->filesleft);
  curbyte += aimutil_put16(dest+curbyte, listingfh->totparts);
  curbyte += aimutil_put16(dest+curbyte, listingfh->partsleft);
  curbyte += aimutil_put32(dest+curbyte, listingfh->totsize);
  curbyte += aimutil_put32(dest+curbyte, listingfh->size);
  curbyte += aimutil_put32(dest+curbyte, listingfh->modtime);
  curbyte += aimutil_put32(dest+curbyte, listingfh->checksum);
  curbyte += aimutil_put32(dest+curbyte, listingfh->rfrcsum);
  curbyte += aimutil_put32(dest+curbyte, listingfh->rfsize);
  curbyte += aimutil_put32(dest+curbyte, listingfh->cretime);
  curbyte += aimutil_put32(dest+curbyte, listingfh->rfcsum);
  curbyte += aimutil_put32(dest+curbyte, listingfh->nrecvd);
  curbyte += aimutil_put32(dest+curbyte, listingfh->recvcsum);

  memcpy(dest+curbyte, listingfh->idstring, 32);
  curbyte += 32;

  curbyte += aimutil_put8(dest+curbyte, listingfh->flags);
  curbyte += aimutil_put8(dest+curbyte, listingfh->lnameoffset);
  curbyte += aimutil_put8(dest+curbyte, listingfh->lsizeoffset);

  memcpy(dest+curbyte, listingfh->dummy, 69);
  curbyte += 69;
    
  memcpy(dest+curbyte, listingfh->macfileinfo, 16);
  curbyte += 16;

  curbyte += aimutil_put16(dest+curbyte, listingfh->nencode);
  curbyte += aimutil_put16(dest+curbyte, listingfh->nlanguage);

  memcpy(dest+curbyte, listingfh->name, 64); /* XXX: Filenames longer than 64B */
  curbyte += 64;

  return curbyte;
}

/*
 * int aim_getfile_send(struct aim_conn_t *conn, FILE *tosend, struct aim_fileheader_t *fh)
 * conn is the OFT connection to shove the data down,
 * tosend is the FILE * for the file to send
 * fh is the filled-in fh value
 * returns -1 on error, 1 on success.
 */

faim_export int aim_getfile_send(struct aim_conn_t *conn, FILE *tosend, struct aim_fileheader_t *fh)
{
  int  pos, bufpos, i; 
  const int bufsize = 4096;
  char *buf;

  /* sanity checks */
  if(conn->type != AIM_CONN_TYPE_RENDEZVOUS || conn->subtype != AIM_CONN_SUBTYPE_OFT_GETFILE) {
    faimdprintf(1, "getfile_send: conn->type(0x%04x) != RENDEZVOUS or conn->subtype(0x%04x) != GETFILE\n", conn->type, conn->subtype);
    return -1;
  }
  
  if(!tosend) {
    faimdprintf(1, "getfile_send: file pointer isn't valid\n");
    return -1;
  }

  if(!fh) {
    faimdprintf(1, "getfile_send: fh isn't valid\n");
    return -1;
  }

  /* real code */

  if(!(buf = (char *)calloc(1, bufsize))) {
    perror("faim: getfile_send: calloc:");
    faimdprintf(2, "getfile_send calloc error\n");
    return -1;
  }

  pos = 0;

  if( fseek(tosend, 0, SEEK_SET) == -1) {
    perror("faim: getfile_send:  fseek:");
    faimdprintf(2, "getfile_send fseek error\n");
  }  

  faimdprintf(2, "faim: getfile_send: using %i byte blocks\n", bufsize);

  for(pos = 0; pos < fh->size; pos++) {
    bufpos = pos % bufsize;

    if(bufpos == 0 && pos > 0) { /* filled our buffer. spit it across the wire */
      if ( (i = write(conn->fd, buf, bufsize)) != bufsize ) {
	perror("faim: getfile_send: write1: ");
	faimdprintf(1, "faim: getfile_send: whoopsy, didn't write it all...\n");
	free(buf);   
	return -1;
      }
    }
    if( (buf[bufpos] = fgetc(tosend)) == EOF) {
      if(pos != fh->size) {
	printf("faim: getfile_send: hrm... apparent early EOF at pos 0x%x of 0x%lx\n", pos, fh->size);
	faimdprintf(1, "faim: getfile_send: hrm... apparent early EOF at pos 0x%lx of 0x%lx\n", pos, fh->size);
	free(buf);   
	return -1;
      }
    }
      
      
  }

  if( (i = write(conn->fd, buf, bufpos+1)) != (bufpos+1)) {
    perror("faim: getfile_send: write2: ");
    faimdprintf(1, "faim: getfile_send cleanup: whoopsy, didn't write it all...\n");
    free(buf);   
    return -1;
  }

  free(buf);   
  
  fclose(tosend);
  return 1; 
}

/*
 * int aim_getfile_send_chunk(struct aim_conn_t *conn, FILE *tosend, struct aim_fileheader_t *fh, int pos, int bufsize)
 * conn is the OFT connection to shove the data down,
 * tosend is the FILE * for the file to send
 * fh is the filled-in fh value
 * pos is the position to start at (at beginning should be 0, after 5
 *  bytes are sent should be 5); -1 for "don't seek"
 * bufsize is the size of the chunk to send
 *
 * returns -1 on error, new pos on success.
 *
 * 
 * Notes on usage: 
 * You don't really have to keep track of pos if you don't want
 *  to. just always call with -1 for pos, and it'll use the one
 *  contained within the FILE *.
 *
 * if (pos + chunksize > fh->size), we only send as much data as we
 *  can get (ie: up to fh->size.  
 */
faim_export int aim_getfile_send_chunk(struct aim_conn_t *conn, FILE *tosend, struct aim_fileheader_t *fh, int pos, int bufsize)
{
  int bufpos; 
  char *buf;

  /* sanity checks */
  if(conn->type != AIM_CONN_TYPE_RENDEZVOUS || conn->type != AIM_CONN_SUBTYPE_OFT_GETFILE) {
    faimdprintf(1, "faim: getfile_send_chunk: conn->type(0x%04x) != RENDEZVOUS or conn->subtype(0x%04x) != GETFILE\n", conn->type, conn->subtype);
    return -1;
  }
  
  if(!tosend) {
    faimdprintf(1, "faim: getfile_send_chunk: file pointer isn't valid\n");
    return -1;
  }

  if(!fh) {
    faimdprintf(1, "faim: getfile_send_chunk: fh isn't valid\n");
    return -1;
  }

  /* real code */

  if(!(buf = (char *)calloc(1, bufsize))) {
    perror("faim: getfile_send_chunk: calloc:");
    faimdprintf(2, "faim: getfile_send_chunk calloc error\n");
    return -1;
  }
  
  if(pos != -1) {
    if( fseek(tosend, pos, SEEK_SET) == -1) {
      perror("faim: getfile_send_chunk:  fseek:");
      faimdprintf(2, "faim: getfile_send_chunk fseek error\n");
    }  
  }

  faimdprintf(2, "faim: getfile_send_chunk: using %i byte blocks\n", bufsize);

  for(bufpos = 0; pos < fh->size; bufpos++, pos++) {
    if( (buf[bufpos] = fgetc(tosend)) == EOF) {
      if(pos != fh->size) {
	faimdprintf(1, "faim: getfile_send_chunk: hrm... apparent early EOF at pos 0x%x of 0x%x\n", pos, fh->size);
	free(buf);   
	return -1;
      }
    }      
  }

  if( write(conn->fd, buf, bufpos+1) != (bufpos+1)) {
    faimdprintf(1, "faim: getfile_send_chunk cleanup: whoopsy, didn't write it all...\n");
    free(buf);   
    return -1;
  }

  free(buf);   
  
  return (pos + bufpos);
}

/*
 * aim_tx_destroy:
 * free's tx_command_t's. if command is locked, doesn't free.
 * returns -1 on error (locked struct); 0 on success.
 * command is the command to free 
 */

faim_internal int aim_tx_destroy(struct command_tx_struct *command)
{
  if(command->lock)
    return -1;
  if(command->data)
    free(command->data);
  free(command);

  return 0;
}

#if 0
/*
 * aim_getfile_intitiate:
 * For those times when we want to open up the directim channel ourselves.
 * sess is your session,
 * conn is the BOS conn,
 * priv is a dummy priv value (we'll let it get filled in later) (if
 * you pass a NULL, we alloc one) 
 * destsn is the SN to connect to.  
 */


faim_export struct aim_conn_t *aim_getfile_initiate(struct aim_session_t *sess,
			  struct aim_conn_t *conn,
			  struct aim_getfile_priv *priv,
			  char *destsn)
{
  struct command_tx_struct *newpacket;
  struct aim_conn_t *newconn;

  struct aim_msgcookie_t *cookie;

  int curbyte, i, listenfd;
  short port = 4443;

  struct hostent *hptr;
  struct utsname myname;

  char cap[16];
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

  return newconn;
}

#endif
