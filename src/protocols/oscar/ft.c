/*
 * Oscar File transfer (OFT) and Oscar Direct Connect (ODC).
 * (ODC is also referred to as DirectIM and IM Image.)
 *
 * There are a few static helper functions at the top, then 
 * ODC stuff, then ft stuff.
 *
 * I feel like this is a good place to explain OFT, so I'm going to 
 * do just that.  Each OFT packet has a header type.  I guess this 
 * is pretty similar to the subtype of a SNAC packet.  The type 
 * basically tells the other client the meaning of the OFT packet.  
 * There are two distinct types of file transfer, which I usually 
 * call "sendfile" and "getfile."  Sendfile is when you send a file 
 * to another AIM user.  Getfile is when you share a group of files, 
 * and other users request that you send them the files.
 *
 * A typical sendfile file transfer goes like this:
 *   1) Sender sends a channel 2 ICBM telling the other user that 
 *      we want to send them a file.  At the same time, we open a 
 *      listener socket (this should be done before sending the 
 *      ICBM) on some port, and wait for them to connect to us.  
 *      The ICBM we sent should contain our IP address and the port 
 *      number that we're listening on.
 *   2) The receiver connects to the sender on the given IP address 
 *      and port.  After the connection is established, the receiver 
 *      sends another ICBM signifying that we are ready and waiting.
 *   3) The sender sends an OFT PROMPT message over the OFT 
 *      connection.
 *   4) The receiver of the file sends back an exact copy of this 
 *      OFT packet, except the cookie is filled in with the cookie 
 *      from the ICBM.  I think this might be an attempt to verify 
 *      that the user that is connected is actually the guy that 
 *      we sent the ICBM to.  Oh, I've been calling this the ACK.
 *   5) The sender starts sending raw data across the connection 
 *      until the entire file has been sent.
 *   6) The receiver knows the file is finished because the sender 
 *      sent the file size in an earlier OFT packet.  So then the 
 *      receiver sends the DONE thingy and closes the connection.
 */

#define FAIM_INTERNAL

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <aim.h>

#ifndef _WIN32
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/utsname.h> /* for aim_odc_initiate */
#include <arpa/inet.h> /* for inet_ntoa */
#define G_DIR_SEPARATOR '/'
#endif

#ifdef _WIN32
#include "win32dep.h"
#endif

struct aim_odc_intdata {
	fu8_t cookie[8];
	char sn[MAXSNLEN+1];
	char ip[22];
};

/**
 * Convert the directory separator from / (0x2f) to ^A (0x01)
 *
 * @param name The filename to convert.
 */
static void aim_oft_dirconvert_tostupid(char *name)
{
	while (name[0]) {
		if (name[0] == 0x01)
			name[0] = G_DIR_SEPARATOR;
		name++;
	}
}

/**
 * Convert the directory separator from ^A (0x01) to / (0x2f)
 *
 * @param name The filename to convert.
 */
static void aim_oft_dirconvert_fromstupid(char *name)
{
	while (name[0]) {
		if (name[0] == G_DIR_SEPARATOR)
			name[0] = 0x01;
		name++;
	}
}

/**
 * Calculate oft checksum of buffer
 *
 * Prevcheck should be 0xFFFF0000 when starting a checksum of a file.  The 
 * checksum is kind of a rolling checksum thing, so each time you get bytes 
 * of a file you just call this puppy and it updates the checksum.  You can 
 * calculate the checksum of an entire file by calling this in a while or a 
 * for loop, or something.
 *
 * Thanks to Graham Booker for providing this improved checksum routine, 
 * which is simpler and should be more accurate than Josh Myer's original 
 * code. -- wtm
 *
 * This algorithim works every time I have tried it.  The other fails 
 * sometimes.  So, AOL who thought this up?  It has got to be the weirdest 
 * checksum I have ever seen.
 *
 * @param buffer Buffer of data to checksum.  Man I'd like to buff her...
 * @param bufsize Size of buffer.
 * @param prevcheck Previous checksum.
 */
faim_export fu32_t aim_oft_checksum(const unsigned char *buffer, int bufferlen, fu32_t prevcheck)
{
	fu32_t check = (prevcheck >> 16) & 0xffff, oldcheck;
	int i;
	unsigned short val;

	for (i=0; i<bufferlen; i++) {
		oldcheck = check;
		if (i&1)
			val = buffer[i];
		else
			val = buffer[i] << 8;
		check -= val;
		/*
		 * The following appears to be necessary.... It happens 
		 * every once in a while and the checksum doesn't fail.
		 */
		if (check > oldcheck)
			check--;
	}
	check = ((check & 0x0000ffff) + (check >> 16));
	check = ((check & 0x0000ffff) + (check >> 16));
	return check << 16;
}

/**
 * Create a listening socket on a given port.
 *
 * XXX - Give the client author the responsibility of setting up a 
 *       listener, then we no longer have a libfaim problem with broken 
 *       solaris *innocent smile* -- jbm
 *
 * @param portnum The port number to bind to.
 * @return The file descriptor of the listening socket.
 */
static int listenestablish(fu16_t portnum)
{
#if HAVE_GETADDRINFO
	int listenfd;
	const int on = 1;
	struct addrinfo hints, *res, *ressave;
	char serv[5];

	snprintf(serv, sizeof(serv), "%d", portnum);
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if (getaddrinfo(NULL /* any IP */, serv, &hints, &res) != 0) {
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

	freeaddrinfo(ressave);
#else
	int listenfd;
	const int on = 1;
	struct sockaddr_in sockin;

	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		return -1;
	}

	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) != 0) {
		perror("setsockopt");
		close(listenfd);
		return -1;
	}

	memset(&sockin, 0, sizeof(struct sockaddr_in));
	sockin.sin_family = AF_INET;
	sockin.sin_port = htons(portnum);

	if (bind(listenfd, (struct sockaddr *)&sockin, sizeof(struct sockaddr_in)) != 0) {
		perror("bind");
		close(listenfd);
		return -1;
	}
#endif

	if (listen(listenfd, 4) != 0) {
		perror("listen");
		close(listenfd);
		return -1;
	}
	fcntl(listenfd, F_SETFL, O_NONBLOCK);

	return listenfd;
}

/**
 * After establishing a listening socket, this is called to accept a connection.  It
 * clones the conn used by the listener, and passes both of these to a signal handler.
 * The signal handler should close the listener conn and keep track of the new conn,
 * since this is what is used for file transfers and what not.
 *
 * @param sess The session.
 * @param cur The conn the incoming connection is on.
 * @return Return 0 if no errors, otherwise return the error number.
 */
faim_export int aim_handlerendconnect(aim_session_t *sess, aim_conn_t *cur)
{
	int acceptfd = 0;
	struct sockaddr addr;
	socklen_t addrlen = sizeof(addr);
	int ret = 0;
	aim_conn_t *newconn;
	char ip[20];
	int port;

	if ((acceptfd = accept(cur->fd, &addr, &addrlen)) == -1)
		return 0; /* not an error */

	if (addr.sa_family != AF_INET) { /* just in case IPv6 really is happening */
		close(acceptfd);
		aim_conn_close(cur);
		return -1;
	}

	strncpy(ip, inet_ntoa(((struct sockaddr_in *)&addr)->sin_addr), sizeof(ip));
	port = ntohs(((struct sockaddr_in *)&addr)->sin_port);

	if (!(newconn = aim_cloneconn(sess, cur))) {
		close(acceptfd);
		aim_conn_close(cur);
		return -ENOMEM;
	}

	newconn->type = AIM_CONN_TYPE_RENDEZVOUS;
	newconn->fd = acceptfd;

	if (newconn->subtype == AIM_CONN_SUBTYPE_OFT_DIRECTIM) {
		aim_rxcallback_t userfunc;
		struct aim_odc_intdata *priv;

		priv = (struct aim_odc_intdata *)(newconn->internal = cur->internal);
		cur->internal = NULL;
		snprintf(priv->ip, sizeof(priv->ip), "%s:%u", ip, port);

		if ((userfunc = aim_callhandler(sess, newconn, AIM_CB_FAM_OFT, AIM_CB_OFT_DIRECTIM_ESTABLISHED)))
			ret = userfunc(sess, NULL, newconn, cur);

	} else if (newconn->subtype == AIM_CONN_SUBTYPE_OFT_GETFILE) {
	} else if (newconn->subtype == AIM_CONN_SUBTYPE_OFT_SENDFILE) {
		aim_rxcallback_t userfunc;

		if ((userfunc = aim_callhandler(sess, newconn, AIM_CB_FAM_OFT, AIM_CB_OFT_ESTABLISHED)))
			ret = userfunc(sess, NULL, newconn, cur);

	} else {
		faimdprintf(sess, 1,"Got a connection on a listener that's not rendezvous.  Closing connection.\n");
		aim_conn_close(newconn);
		ret = -1;
	}

	return ret;
}

/**
 * Send client-to-client typing notification over an established direct connection.
 *
 * @param sess The session.
 * @param conn The already-connected ODC connection.
 * @param typing If true, notify user has started typing; if false, notify user has stopped.
 * @return Return 0 if no errors, otherwise return the error number.
 */
faim_export int aim_odc_send_typing(aim_session_t *sess, aim_conn_t *conn, int typing)
{
	struct aim_odc_intdata *intdata = (struct aim_odc_intdata *)conn->internal;
	aim_frame_t *fr;
	aim_bstream_t *hdrbs;
	fu8_t *hdr;
	int hdrlen = 0x44;

	if (!sess || !conn || (conn->type != AIM_CONN_TYPE_RENDEZVOUS))
		return -EINVAL;

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_OFT, 0x01, 0)))
		return -ENOMEM;
	memcpy(fr->hdr.rend.magic, "ODC2", 4);
	fr->hdr.rend.hdrlen = hdrlen;

	if (!(hdr = calloc(1, hdrlen))) {
		aim_frame_destroy(fr);
		return -ENOMEM;
	}

	hdrbs = &(fr->data);
	aim_bstream_init(hdrbs, hdr, hdrlen);

	aimbs_put16(hdrbs, 0x0006);
	aimbs_put16(hdrbs, 0x0000);
	aimbs_putraw(hdrbs, intdata->cookie, 8);
	aimbs_put16(hdrbs, 0x0000);
	aimbs_put16(hdrbs, 0x0000);
	aimbs_put16(hdrbs, 0x0000);
	aimbs_put16(hdrbs, 0x0000);
	aimbs_put32(hdrbs, 0x00000000);
	aimbs_put16(hdrbs, 0x0000);
	aimbs_put16(hdrbs, 0x0000);
	aimbs_put16(hdrbs, 0x0000);

	/* flags -- 0x000e for "started typing", 0x0002 for "stopped typing */
	aimbs_put16(hdrbs, ( typing ? 0x000e : 0x0002));

	aimbs_put16(hdrbs, 0x0000);
	aimbs_put16(hdrbs, 0x0000);
	aimbs_putraw(hdrbs, sess->sn, strlen(sess->sn));

	aim_bstream_setpos(hdrbs, 52); /* bleeehh */

	aimbs_put8(hdrbs, 0x00);
	aimbs_put16(hdrbs, 0x0000);
	aimbs_put16(hdrbs, 0x0000);
	aimbs_put16(hdrbs, 0x0000);
	aimbs_put16(hdrbs, 0x0000);
	aimbs_put16(hdrbs, 0x0000);
	aimbs_put16(hdrbs, 0x0000);
	aimbs_put16(hdrbs, 0x0000);
	aimbs_put8(hdrbs, 0x00);

	/* end of hdr */

	aim_tx_enqueue(sess, fr);

	return 0;
}

/**
 * Send client-to-client IM over an established direct connection.
 * Call this just like you would aim_send_im, to send a directim.
 * 
 * @param sess The session.
 * @param conn The already-connected ODC connection.
 * @param msg Null-terminated string to send.
 * @param len The length of the message to send, including binary data.
 * @param encoding 0 for ascii, 2 for Unicode, 3 for ISO 8859-1.
 * @return Return 0 if no errors, otherwise return the error number.
 */
faim_export int aim_odc_send_im(aim_session_t *sess, aim_conn_t *conn, const char *msg, int len, int encoding)
{
	aim_frame_t *fr;
	aim_bstream_t *hdrbs;
	struct aim_odc_intdata *intdata = (struct aim_odc_intdata *)conn->internal;
	int hdrlen = 0x44;
	fu8_t *hdr;

	if (!sess || !conn || (conn->type != AIM_CONN_TYPE_RENDEZVOUS) || !msg)
		return -EINVAL;

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_OFT, 0x01, len)))
		return -ENOMEM;

	memcpy(fr->hdr.rend.magic, "ODC2", 4);
	fr->hdr.rend.hdrlen = hdrlen;

	if (!(hdr = calloc(1, hdrlen + len))) {
		aim_frame_destroy(fr);
		return -ENOMEM;
	}

	hdrbs = &(fr->data);
	aim_bstream_init(hdrbs, hdr, hdrlen + len);

	aimbs_put16(hdrbs, 0x0006);
	aimbs_put16(hdrbs, 0x0000);
	aimbs_putraw(hdrbs, intdata->cookie, 8);
	aimbs_put16(hdrbs, 0x0000);
	aimbs_put16(hdrbs, 0x0000);
	aimbs_put16(hdrbs, 0x0000);
	aimbs_put16(hdrbs, 0x0000);
	aimbs_put32(hdrbs, len);
	aimbs_put16(hdrbs, encoding);
	aimbs_put16(hdrbs, 0x0000);
	aimbs_put16(hdrbs, 0x0000);

	/* flags -- 0x000e for "started typing", 0x0002 for "stopped typing, 0x0000 for message */
	aimbs_put16(hdrbs, 0x0000);

	aimbs_put16(hdrbs, 0x0000);
	aimbs_put16(hdrbs, 0x0000);
	aimbs_putraw(hdrbs, sess->sn, strlen(sess->sn));

	aim_bstream_setpos(hdrbs, 52); /* bleeehh */

	aimbs_put8(hdrbs, 0x00);
	aimbs_put16(hdrbs, 0x0000);
	aimbs_put16(hdrbs, 0x0000);
	aimbs_put16(hdrbs, 0x0000);
	aimbs_put16(hdrbs, 0x0000);
	aimbs_put16(hdrbs, 0x0000);
	aimbs_put16(hdrbs, 0x0000);
	aimbs_put16(hdrbs, 0x0000);
	aimbs_put8(hdrbs, 0x00);

	/* end of hdr2 */

#if 0 /* XXX - this is how you send buddy icon info... */	
	aimbs_put16(hdrbs, 0x0008);
	aimbs_put16(hdrbs, 0x000c);
	aimbs_put16(hdrbs, 0x0000);
	aimbs_put16(hdrbs, 0x1466);
	aimbs_put16(hdrbs, 0x0001);
	aimbs_put16(hdrbs, 0x2e0f);
	aimbs_put16(hdrbs, 0x393e);
	aimbs_put16(hdrbs, 0xcac8);
#endif
	aimbs_putraw(hdrbs, msg, len);

	aim_tx_enqueue(sess, fr);

	return 0;
}

/**
 * Get the screen name of the peer of a direct connection.
 * 
 * @param conn The ODC connection.
 * @return The screen name of the dude, or NULL if there was an anomaly.
 */
faim_export const char *aim_odc_getsn(aim_conn_t *conn)
{
	struct aim_odc_intdata *intdata;

	if (!conn || !conn->internal)
		return NULL;

	if ((conn->type != AIM_CONN_TYPE_RENDEZVOUS) || 
			(conn->subtype != AIM_CONN_SUBTYPE_OFT_DIRECTIM))
		return NULL;

	intdata = (struct aim_odc_intdata *)conn->internal;

	return intdata->sn;
}

/**
 * Find the conn of a direct connection with the given buddy.
 *
 * @param sess The session.
 * @param sn The screen name of the buddy whose direct connection you want to find.
 * @return The conn for the direct connection with the given buddy, or NULL if no 
 *         connection was found.
 */
faim_export aim_conn_t *aim_odc_getconn(aim_session_t *sess, const char *sn)
{
	aim_conn_t *cur;
	struct aim_odc_intdata *intdata;

	if (!sess || !sn || !strlen(sn))
		return NULL;

	for (cur = sess->connlist; cur; cur = cur->next) {
		if ((cur->type == AIM_CONN_TYPE_RENDEZVOUS) && (cur->subtype == AIM_CONN_SUBTYPE_OFT_DIRECTIM)) {
			intdata = cur->internal;
			if (!aim_sncmp(intdata->sn, sn))
				return cur;
		}
	}

	return NULL;
}

/**
 * For those times when we want to open up the direct connection channel ourselves.
 *
 * You'll want to set up some kind of watcher on this socket.  
 * When the state changes, call aim_handlerendconnection with 
 * the connection returned by this.  aim_handlerendconnection 
 * will accept the pending connection and stop listening.
 *
 * @param sess The session
 * @param conn The BOS conn.
 * @param priv A dummy priv value (we'll let it get filled in later)
 *             (if you pass a %NULL, we alloc one).
 * @param sn The screen name to connect to.
 * @return The new connection.
 */
faim_export aim_conn_t *aim_odc_initiate(aim_session_t *sess, const char *sn)
{
	aim_conn_t *newconn;
	aim_msgcookie_t *cookie;
	struct aim_odc_intdata *priv;
	int listenfd;
	fu16_t port = 4443;
	fu8_t localip[4];
	fu8_t ck[8];

	if (aim_util_getlocalip(localip) == -1)
		return NULL;

	if ((listenfd = listenestablish(port)) == -1)
		return NULL;

	aim_im_sendch2_odcrequest(sess, ck, sn, localip, port);

	cookie = (aim_msgcookie_t *)calloc(1, sizeof(aim_msgcookie_t));
	memcpy(cookie->cookie, ck, 8);
	cookie->type = AIM_COOKIETYPE_OFTIM;

	/* this one is for the cookie */
	priv = (struct aim_odc_intdata *)calloc(1, sizeof(struct aim_odc_intdata));

	memcpy(priv->cookie, ck, 8);
	strncpy(priv->sn, sn, sizeof(priv->sn));
	cookie->data = priv;
	aim_cachecookie(sess, cookie);

	/* XXX - switch to aim_cloneconn()? */
	if (!(newconn = aim_newconn(sess, AIM_CONN_TYPE_LISTENER, NULL))) {
		close(listenfd);
		return NULL;
	}

	/* this one is for the conn */
	priv = (struct aim_odc_intdata *)calloc(1, sizeof(struct aim_odc_intdata));

	memcpy(priv->cookie, ck, 8);
	strncpy(priv->sn, sn, sizeof(priv->sn));

	newconn->fd = listenfd;
	newconn->subtype = AIM_CONN_SUBTYPE_OFT_DIRECTIM;
	newconn->internal = priv;
	newconn->lastactivity = time(NULL);

	return newconn;
}

/**
 * Connect directly to the given buddy for directim.
 *
 * This is a wrapper for aim_newconn.
 *
 * If addr is NULL, the socket is not created, but the connection is 
 * allocated and setup to connect.
 *
 * @param sess The Godly session.
 * @param sn The screen name we're connecting to.  I hope it's a girl...
 * @param addr Address to connect to.
 * @return The new connection.
 */
faim_export aim_conn_t *aim_odc_connect(aim_session_t *sess, const char *sn, const char *addr, const fu8_t *cookie)
{
	aim_conn_t *newconn;
	struct aim_odc_intdata *intdata;

	if (!sess || !sn)
		return NULL;

	if (!(intdata = malloc(sizeof(struct aim_odc_intdata))))
		return NULL;
	memset(intdata, 0, sizeof(struct aim_odc_intdata));
	memcpy(intdata->cookie, cookie, 8);
	strncpy(intdata->sn, sn, sizeof(intdata->sn));
	if (addr)
		strncpy(intdata->ip, addr, sizeof(intdata->ip));

	/* XXX - verify that non-blocking connects actually work */
	if (!(newconn = aim_newconn(sess, AIM_CONN_TYPE_RENDEZVOUS, addr))) {
		free(intdata);
		return NULL;
	}

	newconn->internal = intdata;
	newconn->subtype = AIM_CONN_SUBTYPE_OFT_DIRECTIM;

	return newconn;
}

/**
 * Creates a listener socket so the other dude can connect to us.
 *
 * You'll want to set up some kind of watcher on this socket.  
 * When the state changes, call aim_handlerendconnection with 
 * the connection returned by this.  aim_handlerendconnection 
 * will accept the pending connection and stop listening.
 *
 * @param sess The session.
 * @param cookie This better be Mrs. Fields or I'm going to be pissed.
 * @param ip Should be 4 bytes,  each byte is 1 quartet of the IP address.
 * @param port Ye olde port number to listen on.
 * @return Return the new conn if everything went as planned.  Otherwise, 
 *         return NULL.
 */
faim_export aim_conn_t *aim_sendfile_listen(aim_session_t *sess, const fu8_t *cookie, const fu8_t *ip, fu16_t port)
{
	aim_conn_t *newconn;
	int listenfd;

	if ((listenfd = listenestablish(port)) == -1)
		return NULL;

	if (!(newconn = aim_newconn(sess, AIM_CONN_TYPE_LISTENER, NULL))) {
		close(listenfd);
		return NULL;
	}

	newconn->fd = listenfd;
	newconn->subtype = AIM_CONN_SUBTYPE_OFT_SENDFILE;
	newconn->lastactivity = time(NULL);

	return newconn;
}

/**
 * Extract an &aim_fileheader_t from the given buffer.
 *
 * @param bs The should be from an incoming rendezvous packet.
 * @return A pointer to new struct on success, or NULL on error.
 */
static struct aim_fileheader_t *aim_oft_getheader(aim_bstream_t *bs)
{
	struct aim_fileheader_t *fh;

	if (!(fh = calloc(1, sizeof(struct aim_fileheader_t))))
		return NULL;

	/* The bstream should be positioned after the hdrtype. */
	aimbs_getrawbuf(bs, fh->bcookie, 8);
	fh->encrypt = aimbs_get16(bs);
	fh->compress = aimbs_get16(bs);
	fh->totfiles = aimbs_get16(bs);
	fh->filesleft = aimbs_get16(bs);
	fh->totparts = aimbs_get16(bs);
	fh->partsleft = aimbs_get16(bs);
	fh->totsize = aimbs_get32(bs);
	fh->size = aimbs_get32(bs);
	fh->modtime = aimbs_get32(bs);
	fh->checksum = aimbs_get32(bs);
	fh->rfrcsum = aimbs_get32(bs);
	fh->rfsize = aimbs_get32(bs);
	fh->cretime = aimbs_get32(bs);
	fh->rfcsum = aimbs_get32(bs);
	fh->nrecvd = aimbs_get32(bs);
	fh->recvcsum = aimbs_get32(bs);
	aimbs_getrawbuf(bs, fh->idstring, 32);
	fh->flags = aimbs_get8(bs);
	fh->lnameoffset = aimbs_get8(bs);
	fh->lsizeoffset = aimbs_get8(bs);
	aimbs_getrawbuf(bs, fh->dummy, 69);
	aimbs_getrawbuf(bs, fh->macfileinfo, 16);
	fh->nencode = aimbs_get16(bs);
	fh->nlanguage = aimbs_get16(bs);
	aimbs_getrawbuf(bs, fh->name, 64); /* XXX - filenames longer than 64B */

	return fh;
} 

/**
 * Fills a buffer with network-order fh data
 *
 * @param bs A bstream to fill -- automatically initialized
 * @param fh A struct aim_fileheader_t to get data from.
 * @return Return non-zero on error.
 */
static int aim_oft_buildheader(aim_bstream_t *bs, struct aim_fileheader_t *fh)
{ 
	fu8_t *hdr;

	if (!bs || !fh)
		return -EINVAL;

	if (!(hdr = (unsigned char *)calloc(1, 0x100 - 8)))
		return -ENOMEM;

	aim_bstream_init(bs, hdr, 0x100 - 8);
	aimbs_putraw(bs, fh->bcookie, 8);
	aimbs_put16(bs, fh->encrypt);
	aimbs_put16(bs, fh->compress);
	aimbs_put16(bs, fh->totfiles);
	aimbs_put16(bs, fh->filesleft);
	aimbs_put16(bs, fh->totparts);
	aimbs_put16(bs, fh->partsleft);
	aimbs_put32(bs, fh->totsize);
	aimbs_put32(bs, fh->size);
	aimbs_put32(bs, fh->modtime);
	aimbs_put32(bs, fh->checksum);
	aimbs_put32(bs, fh->rfrcsum);
	aimbs_put32(bs, fh->rfsize);
	aimbs_put32(bs, fh->cretime);
	aimbs_put32(bs, fh->rfcsum);
	aimbs_put32(bs, fh->nrecvd);
	aimbs_put32(bs, fh->recvcsum);
	aimbs_putraw(bs, fh->idstring, 32);
	aimbs_put8(bs, fh->flags);
	aimbs_put8(bs, fh->lnameoffset);
	aimbs_put8(bs, fh->lsizeoffset);
	aimbs_putraw(bs, fh->dummy, 69);
	aimbs_putraw(bs, fh->macfileinfo, 16);
	aimbs_put16(bs, fh->nencode);
	aimbs_put16(bs, fh->nlanguage);
	aimbs_putraw(bs, fh->name, 64);	/* XXX - filenames longer than 64B */

	return 0;
}

/**
 * Create an OFT packet based on the given information, and send it on its merry way.
 *
 * @param sess The session.
 * @param conn The already-connected OFT connection.
 * @param cookie The cookie associated with this file transfer.
 * @param filename The filename.
 * @param filesdone Number of files already transferred.
 * @param numfiles Total number of files.
 * @param size Size in bytes of this file.
 * @param totsize Size in bytes of all files combined.
 * @param checksum Funky checksum of this file.
 * @param flags Any flags you want, baby.  Send 0x21 when sending the 
 *        "AIM_CB_OFT_DONE" message, and "0x02" for everything else.
 * @return Return 0 if no errors, otherwise return the error number.
 */
faim_export int aim_oft_sendheader(aim_session_t *sess, aim_conn_t *conn, fu16_t type, const fu8_t *cookie, const char *filename, fu16_t filesdone, fu16_t numfiles, fu32_t size, fu32_t totsize, fu32_t modtime, fu32_t checksum, fu8_t flags, fu32_t bytesreceived, fu32_t recvcsum)
{
	aim_frame_t *newoft;
	struct aim_fileheader_t *fh;

	if (!sess || !conn || (conn->type != AIM_CONN_TYPE_RENDEZVOUS) || !filename)
		return -EINVAL;

	if (!(fh = (struct aim_fileheader_t *)calloc(1, sizeof(struct aim_fileheader_t))))
		return -ENOMEM;

	/*
	 * If you are receiving a file, the cookie should be null, if you are sending a 
	 * file, the cookie should be the same as the one used in the ICBM negotiation 
	 * SNACs.
	 */
	if (cookie)
		memcpy(fh->bcookie, cookie, 8);
	fh->totfiles = numfiles;
	fh->filesleft = numfiles - filesdone;
	fh->totparts = 0x0001; /* set to 0x0002 sending Mac resource forks */
	fh->partsleft = 0x0001;
	fh->totsize = totsize;
	fh->size = size;
	fh->modtime = modtime;
	fh->checksum = checksum;
	fh->nrecvd = bytesreceived;
	fh->recvcsum = recvcsum;

	strncpy(fh->idstring, "OFT_Windows ICBMFT V1.1 32", sizeof(fh->idstring));
	fh->flags = flags;
	fh->lnameoffset = 0x1a;
	fh->lsizeoffset = 0x10;
	memset(fh->dummy, 0, sizeof(fh->dummy));
	memset(fh->macfileinfo, 0, sizeof(fh->macfileinfo));

	/* apparently 0 is ASCII, 2 is UCS-2 */
	/* it is likely that 3 is ISO 8859-1 */
	fh->nencode = 0x0000;
	fh->nlanguage = 0x0000;

	strncpy(fh->name, filename, sizeof(fh->name));
	aim_oft_dirconvert_tostupid(fh->name);

	if (!(newoft = aim_tx_new(sess, conn, AIM_FRAMETYPE_OFT, type, 0))) {
		free(fh);
		return -ENOMEM;
	}

	if (aim_oft_buildheader(&newoft->data, fh) == -1) {
		aim_frame_destroy(newoft);
		free(fh);
		return -ENOMEM;
	}

	memcpy(newoft->hdr.rend.magic, "OFT2", 4);
	newoft->hdr.rend.hdrlen = aim_bstream_curpos(&newoft->data);

	aim_tx_enqueue(sess, newoft);

	free(fh);

	return 0;
}

/**
 * Sometimes you just don't know with these kinds of people.
 *
 * @param sess The session.
 * @param conn The ODC connection of the incoming data.
 * @param frr The frame allocated for the incoming data.
 * @param bs It stands for "bologna sandwich."
 * @return Return 0 if no errors, otherwise return the error number.
 */
static int handlehdr_odc(aim_session_t *sess, aim_conn_t *conn, aim_frame_t *frr, aim_bstream_t *bs)
{
	aim_frame_t fr;
	aim_rxcallback_t userfunc;
	fu32_t payloadlength;
	fu16_t flags, encoding;
	char *snptr = NULL;

	fr.conn = conn;

	/* AAA - ugly */
	aim_bstream_setpos(bs, 20);
	payloadlength = aimbs_get32(bs);

	aim_bstream_setpos(bs, 24);
	encoding = aimbs_get16(bs);

	aim_bstream_setpos(bs, 30);
	flags = aimbs_get16(bs);

	aim_bstream_setpos(bs, 36);
	/* XXX - create an aimbs_getnullstr function? */
	snptr = aimbs_getstr(bs, MAXSNLEN);

	faimdprintf(sess, 2, "faim: OFT frame: handlehdr_odc: %04x / %04x / %s\n", payloadlength, flags, snptr);

	if (flags & 0x0002) {
		int ret = 0;

		if (flags & 0x000c) {
			if ((userfunc = aim_callhandler(sess, conn, AIM_CB_FAM_OFT, AIM_CB_OFT_DIRECTIMTYPING)))
				ret = userfunc(sess, &fr, snptr, 1);
			return ret;
		}

		if ((userfunc = aim_callhandler(sess, conn, AIM_CB_FAM_OFT, AIM_CB_OFT_DIRECTIMTYPING)))
			ret = userfunc(sess, &fr, snptr, 0);

		return ret;

	} else if (((flags & 0x000f) == 0x0000) && payloadlength) {
		char *msg, *msg2;
		int ret = 0;
		int recvd = 0;
		int i;

		if (!(msg = calloc(1, payloadlength+1)))
			return -1;
		msg2 = msg;
		
		while (payloadlength - recvd) {
			if (payloadlength - recvd >= 1024)
				i = aim_recv(conn->fd, msg2, 1024);
			else 
				i = aim_recv(conn->fd, msg2, payloadlength - recvd);
			if (i <= 0) {
				free(msg);
				return -1;
			}
			recvd = recvd + i;
			msg2 = msg2 + i;
			if ((userfunc = aim_callhandler(sess, conn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_IMAGETRANSFER)))
				userfunc(sess, &fr, snptr, (double)recvd / payloadlength);
		}
		
		if ( (userfunc = aim_callhandler(sess, conn, AIM_CB_FAM_OFT, AIM_CB_OFT_DIRECTIMINCOMING)) )
			ret = userfunc(sess, &fr, snptr, msg, payloadlength, encoding);

		free(msg);

		return ret;
	}

	return 0;
}

/**
 * Handle incoming data on a rendezvous connection.  This is analogous to the 
 * consumesnac function in rxhandlers.c, and I really think this should probably 
 * be in rxhandlers.c as well, but I haven't finished cleaning everything up yet.
 *
 * @param sess The session.
 * @param fr The frame allocated for the incoming data.
 * @return Return 0 if the packet was handled correctly, otherwise return the 
 *         error number.
 */
faim_internal int aim_rxdispatch_rendezvous(aim_session_t *sess, aim_frame_t *fr)
{
	aim_conn_t *conn = fr->conn;
	int ret = 1;

	if (conn->subtype == AIM_CONN_SUBTYPE_OFT_DIRECTIM) {
		if (fr->hdr.rend.type == 0x0001)
			ret = handlehdr_odc(sess, conn, fr, &fr->data);
		else
			faimdprintf(sess, 0, "faim: ODC directim frame unknown, type is %04x\n", fr->hdr.rend.type);

	} else if (conn->subtype == AIM_CONN_SUBTYPE_OFT_GETFILE) {
		switch (fr->hdr.rend.type) {
			case 0x1108: /* getfile listing.txt incoming tx->rx */
				break;
			case 0x1209: /* get file listing ack rx->tx */
				break;
			case 0x120b: /* get file listing rx confirm */
				break;
			case 0x120c: /* getfile request */
				break;
			case 0x0101: /* getfile sending data */
				break;
			case 0x0202: /* getfile recv data */
				break;
			case 0x0204: /* getfile finished */
				break;
			default:
				faimdprintf(sess, 2, "faim: OFT getfile frame uknown, type is %04x\n", fr->hdr.rend.type);
				break;
		}

	} else if (conn->subtype == AIM_CONN_SUBTYPE_OFT_SENDFILE) {
		aim_rxcallback_t userfunc;
		struct aim_fileheader_t *header = aim_oft_getheader(&fr->data);
		aim_oft_dirconvert_fromstupid(header->name); /* XXX - This should be client-side */

		if ((userfunc = aim_callhandler(sess, conn, AIM_CB_FAM_OFT, fr->hdr.rend.type)))
			ret = userfunc(sess, fr, conn, header->bcookie, header);

		free(header);
	}

	if (ret == -1)
		aim_conn_close(conn);

	return ret;
}
