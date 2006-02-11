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

#define AIM_CB_FAM_OFT 0xfffe /* OFT/Rvous */

#define OFT_TYPE_DIRECTIMCONNECTREQ 0x0001	/* connect request -- actually an OSCAR CAP */
#define OFT_TYPE_DIRECTIMINCOMING 0x0002
#define OFT_TYPE_DIRECTIMDISCONNECT 0x0003
#define OFT_TYPE_DIRECTIMTYPING 0x0004
#define OFT_TYPE_DIRECTIM_ESTABLISHED 0x0005

#define OFT_TYPE_PROMPT 0x0101		/* "I am going to send you this file, is that ok?" */
#define OFT_TYPE_RESUMESOMETHING 0x0106	/* I really don't know */
#define OFT_TYPE_ACK 0x0202			/* "Yes, it is ok for you to send me that file" */
#define OFT_TYPE_DONE 0x0204			/* "I received that file with no problems, thanks a bunch" */
#define OFT_TYPE_RESUME 0x0205		/* Resume transferring, sent by whoever paused? */
#define OFT_TYPE_RESUMEACK 0x0207		/* Not really sure */

#define OFT_TYPE_GETFILE_REQUESTLISTING 0x1108 /* "I have a listing.txt file, do you want it?" */
#define OFT_TYPE_GETFILE_RECEIVELISTING 0x1209 /* "Yes, please send me your listing.txt file" */
#define OFT_TYPE_GETFILE_RECEIVEDLISTING 0x120a /* received corrupt listing.txt file? I'm just guessing about this one... */
#define OFT_TYPE_GETFILE_ACKLISTING 0x120b	/* "I received the listing.txt file successfully" */
#define OFT_TYPE_GETFILE_REQUESTFILE 0x120c	/* "Please send me this file" */

#define OFT_TYPE_ESTABLISHED 0xFFFF		/* connection to buddy initiated */


struct aim_fileheader_t {
#if 0
	char magic[4];		/* 0 */
	guint16 hdrlen;		/* 4 */
	guint16 hdrtype;		/* 6 */
#endif
	guchar bcookie[8];	/* 8 */
	guint16 encrypt;		/* 16 */
	guint16 compress;	/* 18 */
	guint16 totfiles;	/* 20 */
	guint16 filesleft;	/* 22 */
	guint16 totparts;	/* 24 */
	guint16 partsleft;	/* 26 */
	guint32 totsize;		/* 28 */
	guint32 size;		/* 32 */
	guint32 modtime;		/* 36 */
	guint32 checksum;	/* 40 */
	guint32 rfrcsum;		/* 44 */
	guint32 rfsize;		/* 48 */
	guint32 cretime;		/* 52 */
	guint32 rfcsum;		/* 56 */
	guint32 nrecvd;		/* 60 */
	guint32 recvcsum;	/* 64 */
	char idstring[32];	/* 68 */
	guint8 flags;		/* 100 */
	guint8 lnameoffset;	/* 101 */
	guint8 lsizeoffset;	/* 102 */
	char dummy[69];		/* 103 */
	char macfileinfo[16];	/* 172 */
	guint16 nencode;		/* 188 */
	guint16 nlanguage;	/* 190 */
	char name[64];		/* 192 */
				/* 256 */
};

struct aim_rv_proxy_info {
	guint16 packet_ver;
	guint16 cmd_type;
	guint16 flags;
	char* ip; /* IP address sent along with this packet */
	guint16 port; /* This is NOT the port we should use to connect. Always connect to 5190 */
	guchar cookie[8];
	guint32 unknownA;
	guint16 err_code; /* Valid only for cmd_type of AIM_RV_PROXY_ERROR */
	aim_conn_t *conn;
	aim_session_t *sess;
};

struct aim_oft_info {
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

	aim_conn_t *conn;
	aim_session_t *sess;
	int success; /* Was the connection successful? Used for timing out the transfer. */
	struct aim_fileheader_t fh;
	struct aim_oft_info *next;
	struct aim_rv_proxy_info *proxy_info;
};

faim_export guint32 aim_oft_checksum_chunk(const guint8 *buffer, int bufferlen, guint32 prevcheck);
faim_export guint32 aim_oft_checksum_file(char *filename);
faim_export int aim_handlerendconnect(aim_session_t *sess, aim_conn_t *cur);
faim_export int aim_odc_send_typing(aim_session_t *sess, aim_conn_t *conn, int typing);
faim_export int aim_odc_send_im(aim_session_t *sess, aim_conn_t *conn, const char *msg, int len, int encoding, int isawaymsg);
faim_export const char *aim_odc_getsn(aim_conn_t *conn);
faim_export const guchar *aim_odc_getcookie(aim_conn_t *conn);
faim_export aim_conn_t *aim_odc_getconn(aim_session_t *sess, const char *sn);
faim_export aim_conn_t *aim_odc_initiate(aim_session_t *sess, const char *sn, int listenfd,
                                         const guint8 *localip, guint16 port, const guchar *mycookie);
faim_export aim_conn_t *aim_odc_connect(aim_session_t *sess, const char *sn, const char *addr, const guchar *cookie);

faim_export struct aim_oft_info *aim_oft_createinfo(aim_session_t *sess, const guchar *cookie, const char *sn,
	const char *ip, guint16 port, guint32 size, guint32 modtime, char *filename, int send_or_recv,
	int method, int stage);
faim_export int aim_oft_destroyinfo(struct aim_oft_info *oft_info);
faim_export struct aim_rv_proxy_info *aim_rv_proxy_createinfo(aim_session_t *sess, const guchar *cookie, guint16 port);
faim_export int aim_sendfile_listen(aim_session_t *sess, struct aim_oft_info *oft_info, int listenfd);
faim_export int aim_oft_sendheader(aim_session_t *sess, guint16 type, struct aim_oft_info *oft_info);

faim_export int aim_rv_proxy_init_recv(struct aim_rv_proxy_info *proxy_info);
faim_export int aim_rv_proxy_init_send(struct aim_rv_proxy_info *proxy_info);

faim_export int aim_sendfile_listen(aim_session_t *sess, struct aim_oft_info *oft_info, int listenfd);
faim_export int aim_oft_sendheader(aim_session_t *sess, guint16 type, struct aim_oft_info *oft_info);
faim_internal struct aim_rv_proxy_info *aim_rv_proxy_read(aim_session_t *sess, aim_conn_t *conn);


faim_internal int aim_rxdispatch_rendezvous(aim_session_t *sess, aim_frame_t *fr);

#endif /* _PEER_H_ */
