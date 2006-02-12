/*
 * Gaim's oscar protocol plugin
 * This file is the legal property of its developers.
 * Please see the AUTHORS file distributed alongside this file.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*
 * OFT Services
 *
 * For all of the above #defines, the number is the subtype
 * of the SNAC.  For OFT #defines, the number is the
 * "hdrtype" which comes after the magic string and OFT
 * packet length.
 *
 * I'm pretty sure the ODC ones are arbitrary right now,
 * that should be changed.
 */

#ifndef _PEER_H_
#define _PEER_H_

typedef struct _PeerConnection     PeerConnection;
typedef struct _PeerFrame          PeerFrame;
typedef struct _PeerProxyInfo      PeerProxyInfo;

#define AIM_CB_FAM_OFT 0xfffe /* OFT/Rvous */

#define PEER_TYPE_DIRECTIMCONNECTREQ 0x0001	/* connect request -- actually an OSCAR CAP */
#define PEER_TYPE_DIRECTIMINCOMING 0x0002
#define PEER_TYPE_DIRECTIMDISCONNECT 0x0003
#define PEER_TYPE_DIRECTIMTYPING 0x0004
#define PEER_TYPE_DIRECTIM_ESTABLISHED 0x0005

#define PEER_TYPE_PROMPT 0x0101		/* "I am going to send you this file, is that ok?" */
#define PEER_TYPE_RESUMESOMETHING 0x0106	/* I really don't know */
#define PEER_TYPE_ACK 0x0202			/* "Yes, it is ok for you to send me that file" */
#define PEER_TYPE_DONE 0x0204			/* "I received that file with no problems, thanks a bunch" */
#define PEER_TYPE_RESUME 0x0205		/* Resume transferring, sent by whoever paused? */
#define PEER_TYPE_RESUMEACK 0x0207		/* Not really sure */

#define PEER_TYPE_GETFILE_REQUESTLISTING 0x1108 /* "I have a listing.txt file, do you want it?" */
#define PEER_TYPE_GETFILE_RECEIVELISTING 0x1209 /* "Yes, please send me your listing.txt file" */
#define PEER_TYPE_GETFILE_RECEIVEDLISTING 0x120a /* received corrupt listing.txt file? I'm just guessing about this one... */
#define PEER_TYPE_GETFILE_ACKLISTING 0x120b	/* "I received the listing.txt file successfully" */
#define PEER_TYPE_GETFILE_REQUESTFILE 0x120c	/* "Please send me this file" */

#define PEER_TYPE_ESTABLISHED 0xFFFF		/* connection to buddy initiated */

struct _PeerFrame
{
#if 0
	char magic[4];           /* 0 */
	guint16 length;          /* 4 */
	guint16 type;            /* 6 */
#endif
	guchar bcookie[8];       /* 8 */
	guint16 encrypt;         /* 16 */
	guint16 compress;        /* 18 */
	guint16 totfiles;        /* 20 */
	guint16 filesleft;       /* 22 */
	guint16 totparts;        /* 24 */
	guint16 partsleft;       /* 26 */
	guint32 totsize;         /* 28 */
	guint32 size;            /* 32 */
	guint32 modtime;         /* 36 */
	guint32 checksum;        /* 40 */
	guint32 rfrcsum;         /* 44 */
	guint32 rfsize;          /* 48 */
	guint32 cretime;         /* 52 */
	guint32 rfcsum;          /* 56 */
	guint32 nrecvd;          /* 60 */
	guint32 recvcsum;        /* 64 */
	char idstring[32];       /* 68 */
	guint8 flags;            /* 100 */
	guint8 lnameoffset;      /* 101 */
	guint8 lsizeoffset;      /* 102 */
	char dummy[69];          /* 103 */
	char macfileinfo[16];    /* 172 */
	guint16 nencode;         /* 188 */
	guint16 nlanguage;       /* 190 */
	char name[64];           /* 192 */
	/* Payload? */           /* 256 */
};

struct _PeerProxyInfo {
	guint16 packet_ver;
	guint16 cmd_type;
	guint16 flags;
	char* ip; /* IP address sent along with this packet */
	guint16 port; /* This is NOT the port we should use to connect. Always connect to 5190 */
	guchar cookie[8];
	guint32 unknownA;
	guint16 err_code; /* Valid only for cmd_type of AIM_RV_PROXY_ERROR */
	OscarConnection *conn;
	OscarSession *sess;
};

struct _PeerConnection {
	guchar cookie[8];
	char *sn;
	char *proxyip;
	char *clientip;
	char *verifiedip;
	guint16 port;

	int send_or_recv; /* Send or receive */
	int method; /* What method is being used to transfer this file? DIRECT, REDIR, or PROXY */
	int stage; /* At what stage was a proxy requested? NONE, STG1, STG2*/
	int xfer_reffed; /* There are many timers, but we should only ref the xfer once */
	int redir_attempted; /* Have we previously attempted to redirect the connection? */
	guint32 res_bytes; /* The bytes already received for resuming a transfer */

	OscarConnection *conn;
	OscarSession *sess;
	int success; /* Was the connection successful? Used for timing out the transfer. */
	PeerFrame fh;
	PeerProxyInfo *proxy_info;
};

int aim_handlerendconnect(OscarSession *sess, OscarConnection *cur);
int aim_rxdispatch_rendezvous(OscarSession *sess, FlapFrame *fr);

/*
 * OFT
 */
int aim_sendfile_listen(OscarSession *sess, PeerConnection *peer_connection, int listenfd);
int aim_oft_sendheader(OscarSession *sess, guint16 type, PeerConnection *peer_connection);
guint32 aim_oft_checksum_chunk(const guint8 *buffer, int bufferlen, guint32 prevcheck);
guint32 aim_oft_checksum_file(char *filename);
int aim_oft_sendheader(OscarSession *sess, guint16 type, PeerConnection *peer_connection);
PeerConnection *aim_oft_createinfo(OscarSession *sess, const guchar *cookie, const char *sn,
	const char *ip, guint16 port, guint32 size, guint32 modtime, char *filename, int send_or_recv,
	int method, int stage);
int aim_oft_destroyinfo(PeerConnection *peer_connection);

/*
 * ODC
 */
int aim_odc_send_typing(OscarSession *sess, OscarConnection *conn, int typing);
int aim_odc_send_im(OscarSession *sess, OscarConnection *conn, const char *msg, int len, int encoding, int isawaymsg);
const char *aim_odc_getsn(OscarConnection *conn);
const guchar *aim_odc_getcookie(OscarConnection *conn);
OscarConnection *aim_odc_getconn(OscarSession *sess, const char *sn);
OscarConnection *aim_odc_initiate(OscarSession *sess, const char *sn, int listenfd,
                                         const guint8 *localip, guint16 port, const guchar *mycookie);
OscarConnection *aim_odc_connect(OscarSession *sess, const char *sn, const char *addr, const guchar *cookie);

/*
 * Rendezvous proxy
 */
PeerProxyInfo *aim_rv_proxy_createinfo(OscarSession *sess, const guchar *cookie, guint16 port);
int aim_rv_proxy_init_recv(PeerProxyInfo *proxy_info);
int aim_rv_proxy_init_send(PeerProxyInfo *proxy_info);

PeerProxyInfo *aim_rv_proxy_read(OscarSession *sess, OscarConnection *conn);

#endif /* _PEER_H_ */
