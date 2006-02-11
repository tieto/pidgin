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
 * Main libfaim header.  Must be included in client for prototypes/macros.
 *
 * "come on, i turned a chick lesbian; i think this is the hackish equivalent"
 *                                                -- Josh Myer
 *
 */

#ifndef _OSCAR_H_
#define _OSCAR_H_

#include "snactypes.h"

#include "debug.h"
#include "internal.h"

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <time.h>

#ifndef _WIN32
#include <sys/time.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#else
#include "libc_interface.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef guint32 aim_snacid_t;
typedef guint16 flap_seqnum_t;

#define WIN32_STATIC
#if defined(_WIN32) && !defined(WIN32_STATIC)
/*
 * For a win32 DLL, we define WIN32_INDLL if this file
 * is included while compiling the DLL. If it's not
 * defined (it's included in a client app), the symbols
 * will be imported instead of exported.
 */
#ifdef WIN32_INDLL
#define faim_export __declspec(dllexport)
#else
#define faim_export __declspec(dllimport)
#endif /* WIN32_INDLL */
#define faim_internal
#else
/*
 * Nothing normally needed for unix...
 */
#define faim_export
#define faim_internal
#endif

#ifndef FALSE
#define FALSE (0)
#endif

#ifndef TRUE
#define TRUE (!FALSE)
#endif

#define FAIM_SNAC_HASH_SIZE 16

/*
 * Current Maximum Length for Screen Names (not including NULL)
 *
 * Currently only names up to 16 characters can be registered
 * however it is apparently legal for them to be larger.
 */
#define MAXSNLEN 97

/*
 * Current Maximum Length for Instant Messages
 *
 * This was found basically by experiment, but not wholly
 * accurate experiment.  It should not be regarded
 * as completely correct.  But its a decent approximation.
 *
 * Note that although we can send this much, its impossible
 * for WinAIM clients (up through the latest (4.0.1957)) to
 * send any more than 1kb.  Amaze all your windows friends
 * with utterly oversized instant messages!
 *
 * XXX: the real limit is the total SNAC size at 8192. Fix this.
 *
 */
#define MAXMSGLEN 7987

/*
 * Maximum size of a Buddy Icon.
 */
#define MAXICONLEN 7168
#define AIM_ICONIDENT "AVT1picture.id"

/*
 * Current Maximum Length for Chat Room Messages
 *
 * This is actually defined by the protocol to be
 * dynamic, but I have yet to see due cause to 
 * define it dynamically here.  Maybe later.
 *
 */
#define MAXCHATMSGLEN 512

/**
 * Maximum length for the password of an ICQ account
 */
#define MAXICQPASSLEN 8

#define AIM_MD5_STRING "AOL Instant Messenger (SM)"

/*
 * Client info.  Filled in by the client and passed in to 
 * aim_send_login().  The information ends up getting passed to OSCAR
 * through the initial login command.
 *
 */
struct client_info_s {
	const char *clientstring;
	guint16 clientid;
	guint16 major;
	guint16 minor;
	guint16 point;
	guint16 build;
	guint32 distrib;
	const char *country; /* two-letter abbrev */
	const char *lang; /* two-letter abbrev */
};

/* Needs to be checked */
#define CLIENTINFO_AIM_3_5_1670 { \
	"AOL Instant Messenger (SM), version 3.5.1670/WIN32", \
	0x0004, \
	0x0003, 0x0005, \
	0x0000, 0x0686, \
	0x0000002a, \
	"us", "en", \
}

/* Needs to be checked */
/* Latest winaim without ssi */
#define CLIENTINFO_AIM_4_1_2010 { \
	"AOL Instant Messenger (SM), version 4.1.2010/WIN32", \
	0x0004, \
	0x0004, 0x0001, \
	0x0000, 0x07da, \
	0x0000004b, \
	"us", "en", \
}

/* Needs to be checked */
#define CLIENTINFO_AIM_4_3_2188 { \
	"AOL Instant Messenger (SM), version 4.3.2188/WIN32", \
	0x0109, \
	0x0400, 0x0003, \
	0x0000, 0x088c, \
	0x00000086, \
	"us", "en", \
}

/* Needs to be checked */
#define CLIENTINFO_AIM_4_8_2540 { \
	"AOL Instant Messenger (SM), version 4.8.2540/WIN32", \
	0x0109, \
	0x0004, 0x0008, \
	0x0000, 0x09ec, \
	0x000000af, \
	"us", "en", \
}

/* Needs to be checked */
#define CLIENTINFO_AIM_5_0_2938 { \
	"AOL Instant Messenger, version 5.0.2938/WIN32", \
	0x0109, \
	0x0005, 0x0000, \
	0x0000, 0x0b7a, \
	0x00000000, \
	"us", "en", \
}

#define CLIENTINFO_AIM_5_1_3036 { \
	"AOL Instant Messenger, version 5.1.3036/WIN32", \
	0x0109, \
	0x0005, 0x0001, \
	0x0000, 0x0bdc, \
	0x000000d2, \
	"us", "en", \
}

#define CLIENTINFO_AIM_5_5_3415 { \
	"AOL Instant Messenger, version 5.5.3415/WIN32", \
	0x0109, \
	0x0005, 0x0005, \
	0x0000, 0x0057, \
	0x000000ef, \
	"us", "en", \
}

#define CLIENTINFO_AIM_5_9_3702 { \
	"AOL Instant Messenger, version 5.9.3702/WIN32", \
	0x0109, \
	0x0005, 0x0009, \
	0x0000, 0x0e76, \
	0x00000111, \
	"us", "en", \
}

#define CLIENTINFO_ICHAT_1_0 { \
	"Apple iChat", \
	0x311a, \
	0x0001, 0x0000, \
	0x0000, 0x003c, \
	0x000000c6, \
	"us", "en", \
}

/* Needs to be checked */
#define CLIENTINFO_ICQ_4_65_3281 { \
	"ICQ Inc. - Product of ICQ (TM) 2000b.4.65.1.3281.85", \
	0x010a, \
	0x0004, 0x0041, \
	0x0001, 0x0cd1, \
	0x00000055, \
	"us", "en", \
}

/* Needs to be checked */
#define CLIENTINFO_ICQ_5_34_3728 { \
	"ICQ Inc. - Product of ICQ (TM).2002a.5.34.1.3728.85", \
	0x010a, \
	0x0005, 0x0022, \
	0x0001, 0x0e8f, \
	0x00000055, \
	"us", "en", \
}

#define CLIENTINFO_ICQ_5_45_3777 { \
	"ICQ Inc. - Product of ICQ (TM).2003a.5.45.1.3777.85", \
	0x010a, \
	0x0005, 0x002d, \
	0x0001, 0x0ec1, \
	0x00000055, \
	"us", "en", \
}

#define CLIENTINFO_ICQBASIC_14_3_1068 { \
	"ICQBasic", \
	0x010a, \
	0x0014, 0x0003, \
	0x0000, 0x042c, \
	0x0000043d, \
	"us", "en", \
}

#define CLIENTINFO_NETSCAPE_7_0_1 { \
	"Netscape 2000 an approved user of AOL Instant Messenger (SM)", \
	0x1d0d, \
	0x0007, 0x0000, \
	0x0001, 0x0000, \
	0x00000058, \
	"us", "en", \
}

#define CLIENTINFO_GAIM { \
	"Gaim/" VERSION, \
	0x0109, \
	0x0005, 0x0001, \
	0x0000, 0x0bdc, \
	0x000000d2, \
	"us", "en", \
}

#define CLIENTINFO_AIM_KNOWNGOOD CLIENTINFO_AIM_5_1_3036
#define CLIENTINFO_ICQ_KNOWNGOOD CLIENTINFO_ICQ_5_45_3777

/*
 * These could be arbitrary, but its easier to use the actual AIM values
 */
#define AIM_CONN_TYPE_BOS		0x0002
#define AIM_CONN_TYPE_ADS		0x0005
#define AIM_CONN_TYPE_AUTH		0x0007
#define AIM_CONN_TYPE_CHATNAV	0x000d
#define AIM_CONN_TYPE_CHAT		0x000e
#define AIM_CONN_TYPE_SEARCH	0x000f
#define AIM_CONN_TYPE_ICON		0x0010
#define AIM_CONN_TYPE_EMAIL		0x0018

/* they start getting arbitrary for rendezvous stuff =) */
#define AIM_CONN_TYPE_RENDEZVOUS_PROXY	0xfffd /* these speak a strange language */
#define AIM_CONN_TYPE_RENDEZVOUS	0xfffe /* these do not speak FLAP! */
#define AIM_CONN_TYPE_LISTENER		0xffff /* socket waiting for accept() */

/* Command types for doing a rendezvous proxy login
 * Thanks to Keith Lea and the Joust project for documenting these commands well */
#define AIM_RV_PROXY_PACKETVER_DFLT	0x044a
#define AIM_RV_PROXY_ERROR		0x0001
#define AIM_RV_PROXY_INIT_SEND		0x0002 /* First command sent when creating a connection */
#define AIM_RV_PROXY_INIT_RECV		0x0004 /* First command sent when receiving existing connection */
#define AIM_RV_PROXY_ACK		0x0003
#define AIM_RV_PROXY_READY		0x0005

/* Number of bytes expected in each of the above packets, including the 2 bytes specifying length */
#define AIM_RV_PROXY_ERROR_LEN		14
#define AIM_RV_PROXY_INIT_SEND_LEN	55
#define AIM_RV_PROXY_INIT_RECV_LEN	57
#define AIM_RV_PROXY_ACK_LEN		18
#define AIM_RV_PROXY_READY_LEN		12
#define AIM_RV_PROXY_HDR_LEN		12	/* Bytes in just the header alone */

/* Default values for unknown/unused values in rendezvous proxy negotiation packets */
#define AIM_RV_PROXY_SERVER_FLAGS	0x0220		/* Default flags sent by proxy server */
#define AIM_RV_PROXY_CLIENT_FLAGS	0x0000		/* Default flags sent by client */
#define AIM_RV_PROXY_UNKNOWNA_DFLT	0x00000000	/* Default value for an unknown block */
#define AIM_RV_PROXY_SERVER_URL		"ars.oscar.aol.com"
#define AIM_RV_PROXY_CONNECT_PORT	5190		/* The port we should always connect to */

/* What is the purpose of this transfer? (Who will end up with a new file?)
 * These values are used in oft_info->send_or_recv */
#define AIM_XFER_SEND			0x0001
#define AIM_XFER_RECV			0x0002

/* Via what method is the data getting routed?
 * These values are used in oft_info->method */
#define AIM_XFER_DIRECT			0x0001	/* Direct connection; receiver connects to sender */
#define AIM_XFER_REDIR			0x0002	/* Redirected connection; sender connects to receiver */
#define AIM_XFER_PROXY			0x0003	/* Proxied connection */

/* Who requested the proxy?
 * The difference between a stage 2 and stage 3 proxied transfer is that the receiver does the
 * initial login for a stage 2, but the sender must do it for a stage 3.
 * These values are used in oft_info->stage */
#define AIM_XFER_PROXY_NONE		0x0001
#define AIM_XFER_PROXY_STG1		0x0002	/* Sender requested a proxy be used (stage1) */
#define AIM_XFER_PROXY_STG2		0x0003	/* Receiver requested a proxy be used (stage2) */
#define AIM_XFER_PROXY_STG3		0x0004	/* Receiver requested a proxy be used (stage3) */

/*
 * Subtypes, we need these for OFT stuff.
 */
#define AIM_CONN_SUBTYPE_OFT_DIRECTIM	0x0001
#define AIM_CONN_SUBTYPE_OFT_GETFILE	0x0002
#define AIM_CONN_SUBTYPE_OFT_SENDFILE	0x0003
#define AIM_CONN_SUBTYPE_OFT_BUDDYICON	0x0004
#define AIM_CONN_SUBTYPE_OFT_VOICE	0x0005

/*
 * Status values returned from aim_conn_new().  ORed together.
 */
#define AIM_CONN_STATUS_READY		0x0001
#define AIM_CONN_STATUS_INTERNALERR	0x0002
#define AIM_CONN_STATUS_RESOLVERR	0x0040
#define AIM_CONN_STATUS_CONNERR		0x0080
#define AIM_CONN_STATUS_INPROGRESS	0x0100

#define AIM_FRAMETYPE_FLAP		0x0000
#define AIM_FRAMETYPE_OFT		0x0001

typedef struct aim_conn_s {
	int fd;
	guint16 type;
	guint16 subtype;
	flap_seqnum_t seqnum;
	guint32 status;
	void *priv; /* misc data the client may want to store */
	void *internal; /* internal conn-specific libfaim data */
	time_t lastactivity; /* time of last transmit */
	int forcedlatency;
	void *handlerlist;
	void *sessv; /* pointer to parent session */
	void *inside; /* only accessible from inside libfaim */
	struct aim_conn_s *next;
} aim_conn_t;

/*
 * Byte Stream type. Sort of.
 *
 * Use of this type serves a couple purposes:
 *   - Buffer/buflen pairs are passed all around everywhere. This turns
 *     that into one value, as well as abstracting it slightly.
 *   - Through the abstraction, it is possible to enable bounds checking
 *     for robustness at the cost of performance.  But a clean failure on
 *     weird packets is much better than a segfault.
 *   - I like having variables named "bs".
 *
 * Don't touch the insides of this struct.  Or I'll have to kill you.
 *
 */
typedef struct aim_bstream_s {
	guint8 *data;
	guint32 len;
	guint32 offset;
} aim_bstream_t;

typedef struct aim_frame_s {
	guint8 hdrtype; /* defines which piece of the union to use */
	union {
		struct {
			guint8 channel;
			flap_seqnum_t seqnum;
		} flap;
		struct {
			guint8 magic[4];	/* ODC2 or OFT2 */
			guint16 hdrlen;
			guint16 type;
		} rend;
	} hdr;
	aim_bstream_t data;		/* payload stream */
	aim_conn_t *conn;		/* the connection it came in on/is going out on */
	guint8 handled;			/* 0 = new, !0 = been handled */
	struct aim_frame_s *next;
} aim_frame_t;

typedef struct aim_msgcookie_s {
	guchar cookie[8];
	int type;
	void *data;
	time_t addtime;
	struct aim_msgcookie_s *next;
} aim_msgcookie_t;

/*
 * AIM Session: The main client-data interface.
 *
 */
typedef struct aim_session_s {

	/* ---- Client Accessible ------------------------ */

	/* Our screen name. */
	char sn[MAXSNLEN+1];

	/*
	 * Pointer to anything the client wants to 
	 * explicitly associate with this session.
	 *
	 * This is for use in the callbacks mainly. In any
	 * callback, you can access this with sess->aux_data.
	 *
	 */
	void *aux_data;

	/* ---- Internal Use Only ------------------------ */

	/* Connection information */
	aim_conn_t *connlist;

	/*
	 * Transmit/receive queues.
	 *
	 * These are only used when you don't use your own lowlevel
	 * I/O.  I don't suggest that you use libfaim's internal I/O.
	 * Its really bad and the API/event model is quirky at best.
	 *  
	 */
	aim_frame_t *queue_outgoing;
	aim_frame_t *queue_incoming;

	/*
	 * Tx Enqueuing function.
	 *
	 * This is how you override the transmit direction of libfaim's
	 * internal I/O.  This function will be called whenever it needs
	 * to send something.
	 *
	 */
	int (*tx_enqueue)(struct aim_session_s *, aim_frame_t *);

	void *modlistv;

	struct {
		char server[128];
		char username[128];
		char password[128];
	} socksproxy;

	guint8 nonblocking;

	/*
	 * Outstanding snac handling
	 *
	 * XXX: Should these be per-connection? -mid
	 */
	void *snac_hash[FAIM_SNAC_HASH_SIZE];
	aim_snacid_t snacid_next;

	aim_msgcookie_t *msgcookies;
	struct aim_icq_info *icq_info;
	struct aim_oft_info *oft_info;
	struct aim_authresp_info *authinfo;
	struct aim_emailinfo *emailinfo;

	struct {
		struct aim_userinfo_s *userinfo;
		struct userinfo_node *torequest;
		struct userinfo_node *requested;
		int waiting_for_response;
	} locate;

	/* Server-stored information (ssi) */
	struct {
		int received_data;
		guint16 numitems;
		struct aim_ssi_item *official;
		struct aim_ssi_item *local;
		struct aim_ssi_tmp *pending;
		time_t timestamp;
		int waiting_for_ack;
	} ssi;
} aim_session_t;

/* Valid for calling aim_icq_setstatus() and for aim_userinfo_t->icqinfo.status */
#define AIM_ICQ_STATE_NORMAL		0x00000000
#define AIM_ICQ_STATE_AWAY		0x00000001
#define AIM_ICQ_STATE_DND		0x00000002
#define AIM_ICQ_STATE_OUT		0x00000004
#define AIM_ICQ_STATE_BUSY		0x00000010
#define AIM_ICQ_STATE_CHAT		0x00000020
#define AIM_ICQ_STATE_INVISIBLE		0x00000100
#define AIM_ICQ_STATE_WEBAWARE		0x00010000
#define AIM_ICQ_STATE_HIDEIP		0x00020000
#define AIM_ICQ_STATE_BIRTHDAY		0x00080000
#define AIM_ICQ_STATE_DIRECTDISABLED	0x00100000
#define AIM_ICQ_STATE_ICQHOMEPAGE	0x00200000
#define AIM_ICQ_STATE_DIRECTREQUIREAUTH	0x10000000
#define AIM_ICQ_STATE_DIRECTCONTACTLIST	0x20000000

/*
 * Get command from connections
 *
 * aim_get_commmand() is the libfaim lowlevel I/O in the receive direction.
 * XXX Make this easily overridable.
 *
 */
faim_export int aim_get_command(aim_session_t *, aim_conn_t *);

/*
 * Dispatch commands that are in the rx queue.
 */
faim_export void aim_rxdispatch(aim_session_t *);

faim_export int aim_debugconn_sendconnect(aim_session_t *sess, aim_conn_t *conn);

faim_export int aim_logoff(aim_session_t *);

/* the library should never call aim_conn_kill */
faim_export void aim_conn_kill(aim_session_t *sess, aim_conn_t **deadconn);

typedef int (*aim_rxcallback_t)(aim_session_t *, aim_frame_t *, ...);


/* auth.c */
struct aim_clientrelease {
	char *name;
	guint32 build;
	char *url;
	char *info;
};

struct aim_authresp_info {
	char *sn;
	guint16 errorcode;
	char *errorurl;
	guint16 regstatus;
	char *email;
	char *bosip;
	guint16 cookielen;
	guint8 *cookie;
	char *chpassurl;
	struct aim_clientrelease latestrelease;
	struct aim_clientrelease latestbeta;
};

/* Callback data for redirect. */
struct aim_redirect_data {
	guint16 group;
	const char *ip;
	guint16 cookielen;
	const guint8 *cookie;
	struct { /* group == AIM_CONN_TYPE_CHAT */
		guint16 exchange;
		const char *room;
		guint16 instance;
	} chat;
};

faim_export int aim_clientready(aim_session_t *sess, aim_conn_t *conn);
faim_export int aim_sendflapver(aim_session_t *sess, aim_conn_t *conn);
faim_export int aim_request_login(aim_session_t *sess, aim_conn_t *conn, const char *sn);
faim_export int aim_send_login(aim_session_t *, aim_conn_t *, const char *, const char *, struct client_info_s *, const char *key);
/* 0x000b */ faim_export int aim_auth_securid_send(aim_session_t *sess, const char *securid);

faim_export void aim_purge_rxqueue(aim_session_t *);
faim_export void aim_cleansnacs(aim_session_t *, int maxage);

#define AIM_TX_QUEUED    0 /* default */
#define AIM_TX_IMMEDIATE 1
#define AIM_TX_USER      2
faim_export int aim_tx_setenqueue(aim_session_t *sess, int what, int (*func)(aim_session_t *, aim_frame_t *));

faim_export int aim_tx_flushqueue(aim_session_t *);
faim_export void aim_tx_purgequeue(aim_session_t *);

faim_export int aim_conn_setlatency(aim_conn_t *conn, int newval);

faim_export int aim_conn_addhandler(aim_session_t *, aim_conn_t *conn, guint16 family, guint16 type, aim_rxcallback_t newhandler, guint16 flags);
faim_export int aim_clearhandlers(aim_conn_t *conn);

faim_export aim_conn_t *aim_conn_findbygroup(aim_session_t *sess, guint16 group);
faim_export aim_session_t *aim_conn_getsess(aim_conn_t *conn);
faim_export void aim_conn_close(aim_conn_t *deadconn);
faim_export aim_conn_t *aim_newconn(aim_session_t *, int type);
faim_export int aim_conn_in_sess(aim_session_t *sess, aim_conn_t *conn);
faim_export int aim_conn_isready(aim_conn_t *);
faim_export int aim_conn_setstatus(aim_conn_t *, int);
faim_export int aim_conn_completeconnect(aim_session_t *sess, aim_conn_t *conn);
faim_export int aim_conn_isconnecting(aim_conn_t *conn);

faim_export void aim_session_init(aim_session_t *, guint8 nonblocking);
faim_export void aim_session_kill(aim_session_t *);
faim_export aim_conn_t *aim_getconn_type(aim_session_t *, int type);
faim_export aim_conn_t *aim_getconn_type_all(aim_session_t *, int type);
faim_export aim_conn_t *aim_getconn_fd(aim_session_t *, int fd);

/* 0x0001 - service.c */
faim_export int aim_srv_setstatusmsg(aim_session_t *sess, const char *msg);
faim_export int aim_srv_setidle(aim_session_t *sess, guint32 idletime);

/* misc.c */

#define AIM_VISIBILITYCHANGE_PERMITADD    0x05
#define AIM_VISIBILITYCHANGE_PERMITREMOVE 0x06
#define AIM_VISIBILITYCHANGE_DENYADD      0x07
#define AIM_VISIBILITYCHANGE_DENYREMOVE   0x08

#define AIM_PRIVFLAGS_ALLOWIDLE           0x01
#define AIM_PRIVFLAGS_ALLOWMEMBERSINCE    0x02

#define AIM_WARN_ANON                     0x01

faim_export int aim_sendpauseack(aim_session_t *sess, aim_conn_t *conn);
faim_export int aim_nop(aim_session_t *, aim_conn_t *);
faim_export int aim_flap_nop(aim_session_t *sess, aim_conn_t *conn);
faim_export int aim_bos_changevisibility(aim_session_t *, aim_conn_t *, int, const char *);
faim_export int aim_bos_setgroupperm(aim_session_t *, aim_conn_t *, guint32 mask);
faim_export int aim_bos_setprivacyflags(aim_session_t *, aim_conn_t *, guint32);
faim_export int aim_reqpersonalinfo(aim_session_t *, aim_conn_t *);
faim_export int aim_reqservice(aim_session_t *, aim_conn_t *, guint16);
faim_export int aim_bos_reqrights(aim_session_t *, aim_conn_t *);
faim_export int aim_setextstatus(aim_session_t *sess, guint32 status);

#define AIM_CLIENTTYPE_UNKNOWN  0x0000
#define AIM_CLIENTTYPE_MC       0x0001
#define AIM_CLIENTTYPE_WINAIM   0x0002
#define AIM_CLIENTTYPE_WINAIM41 0x0003
#define AIM_CLIENTTYPE_AOL_TOC  0x0004
faim_export guint16 aim_im_fingerprint(const guint8 *msghdr, int len);

#define AIM_RATE_CODE_CHANGE     0x0001
#define AIM_RATE_CODE_WARNING    0x0002
#define AIM_RATE_CODE_LIMIT      0x0003
#define AIM_RATE_CODE_CLEARLIMIT 0x0004
faim_export int aim_ads_requestads(aim_session_t *sess, aim_conn_t *conn);



/* im.c */
#define AIM_OFT_SUBTYPE_SEND_FILE	0x0001
#define AIM_OFT_SUBTYPE_SEND_DIR	0x0002
#define AIM_OFT_SUBTYPE_GET_FILE	0x0011
#define AIM_OPT_SUBTYPE_GET_LIST	0x0012

#define AIM_TRANSFER_DENY_NOTSUPPORTED	0x0000
#define AIM_TRANSFER_DENY_DECLINE	0x0001
#define AIM_TRANSFER_DENY_NOTACCEPTING	0x0002

#define AIM_IMPARAM_FLAG_CHANMSGS_ALLOWED	0x00000001
#define AIM_IMPARAM_FLAG_MISSEDCALLS_ENABLED	0x00000002

/* This is what the server will give you if you don't set them yourself. */
#define AIM_IMPARAM_DEFAULTS { \
	0, \
	AIM_IMPARAM_FLAG_CHANMSGS_ALLOWED | AIM_IMPARAM_FLAG_MISSEDCALLS_ENABLED, \
	512, /* !! Note how small this is. */ \
	(99.9)*10, (99.9)*10, \
	1000 /* !! And how large this is. */ \
}

/* This is what most AIM versions use. */
#define AIM_IMPARAM_REASONABLE { \
	0, \
	AIM_IMPARAM_FLAG_CHANMSGS_ALLOWED | AIM_IMPARAM_FLAG_MISSEDCALLS_ENABLED, \
	8000, \
	(99.9)*10, (99.9)*10, \
	0 \
}

struct aim_icbmparameters {
	guint16 maxchan;
	guint32 flags; /* AIM_IMPARAM_FLAG_ */
	guint16 maxmsglen; /* message size that you will accept */
	guint16 maxsenderwarn; /* this and below are *10 (999=99.9%) */
	guint16 maxrecverwarn;
	guint32 minmsginterval; /* in milliseconds? */
};

struct aim_chat_roominfo {
	guint16 exchange;
	char *name;
	guint16 instance;
};

#define AIM_IMFLAGS_AWAY				0x0001 /* mark as an autoreply */
#define AIM_IMFLAGS_ACK					0x0002 /* request a receipt notice */
#define AIM_IMFLAGS_BUDDYREQ			0x0010 /* buddy icon requested */
#define AIM_IMFLAGS_HASICON				0x0020 /* already has icon */
#define AIM_IMFLAGS_SUBENC_MACINTOSH	0x0040 /* damn that Steve Jobs! */
#define AIM_IMFLAGS_CUSTOMFEATURES		0x0080 /* features field present */
#define AIM_IMFLAGS_EXTDATA				0x0100
#define AIM_IMFLAGS_X					0x0200
#define AIM_IMFLAGS_MULTIPART			0x0400 /* ->mpmsg section valid */
#define AIM_IMFLAGS_OFFLINE				0x0800 /* send to offline user */
#define AIM_IMFLAGS_TYPINGNOT			0x1000 /* typing notification */

#define AIM_CHARSET_ASCII		0x0000
#define AIM_CHARSET_UNICODE	0x0002 /* UCS-2BE */
#define AIM_CHARSET_CUSTOM	0x0003

/*
 * Multipart message structures.
 */
typedef struct aim_mpmsg_section_s {
	guint16 charset;
	guint16 charsubset;
	gchar *data;
	guint16 datalen;
	struct aim_mpmsg_section_s *next;
} aim_mpmsg_section_t;

typedef struct aim_mpmsg_s {
	unsigned int numparts;
	aim_mpmsg_section_t *parts;
} aim_mpmsg_t;

faim_export int aim_mpmsg_init(aim_session_t *sess, aim_mpmsg_t *mpm);
faim_export int aim_mpmsg_addraw(aim_session_t *sess, aim_mpmsg_t *mpm, guint16 charset, guint16 charsubset, const gchar *data, guint16 datalen);
faim_export int aim_mpmsg_addascii(aim_session_t *sess, aim_mpmsg_t *mpm, const char *ascii);
faim_export int aim_mpmsg_addunicode(aim_session_t *sess, aim_mpmsg_t *mpm, const guint16 *unicode, guint16 unicodelen);
faim_export void aim_mpmsg_free(aim_session_t *sess, aim_mpmsg_t *mpm);

/*
 * Arguments to aim_send_im_ext().
 *
 * This is really complicated.  But immensely versatile.
 *
 */
struct aim_sendimext_args {

	/* These are _required_ */
	const char *destsn;
	guint32 flags; /* often 0 */

	/* Only required if not using multipart messages */
	const char *msg;
	int msglen;

	/* Required if ->msg is not provided */
	aim_mpmsg_t *mpmsg;

	/* Only used if AIM_IMFLAGS_HASICON is set */
	guint32 iconlen;
	time_t iconstamp;
	guint32 iconsum;

	/* Only used if AIM_IMFLAGS_CUSTOMFEATURES is set */
	guint16 featureslen;
	guint8 *features;

	/* Only used if AIM_IMFLAGS_CUSTOMCHARSET is set and mpmsg not used */
	guint16 charset;
	guint16 charsubset;
};

/*
 * Arguments to aim_send_rtfmsg().
 */
struct aim_sendrtfmsg_args {
	const char *destsn;
	guint32 fgcolor;
	guint32 bgcolor;
	const char *rtfmsg; /* must be in RTF */
};

/*
 * This information is provided in the Incoming ICBM callback for
 * Channel 1 ICBM's.  
 *
 * Note that although CUSTOMFEATURES and CUSTOMCHARSET say they
 * are optional, both are always set by the current libfaim code.
 * That may or may not change in the future.  It is mainly for
 * consistency with aim_sendimext_args.
 *
 * Multipart messages require some explanation. If you want to use them,
 * I suggest you read all the comments in im.c.
 *
 */
struct aim_incomingim_ch1_args {

	/* Always provided */
	aim_mpmsg_t mpmsg;
	guint32 icbmflags; /* some flags apply only to ->msg, not all mpmsg */
	
	/* Only provided if message has a human-readable section */
	gchar *msg;
	int msglen;

	/* Only provided if AIM_IMFLAGS_HASICON is set */
	time_t iconstamp;
	guint32 iconlen;
	guint16 iconsum;

	/* Only provided if AIM_IMFLAGS_CUSTOMFEATURES is set */
	guint8 *features;
	guint8 featureslen;

	/* Only provided if AIM_IMFLAGS_EXTDATA is set */
	guint8 extdatalen;
	guint8 *extdata;

	/* Only used if AIM_IMFLAGS_CUSTOMCHARSET is set */
	guint16 charset;
	guint16 charsubset;
};

/* Valid values for channel 2 args->status */
#define AIM_RENDEZVOUS_PROPOSE	0x0000
#define AIM_RENDEZVOUS_CANCEL	0x0001
#define AIM_RENDEZVOUS_ACCEPT	0x0002

struct aim_incomingim_ch2_args {
	guint16 status;
	guchar cookie[8];
	int reqclass;
	const char *proxyip;
	const char *clientip;
	const char *verifiedip;
	guint16 port;
	guint16 errorcode;
	const char *msg; /* invite message or file description */
	guint16 msglen;
	const char *encoding;
	const char *language;
	union {
		struct {
			guint32 checksum;
			guint32 length;
			time_t timestamp;
			guint8 *icon;
		} icon;
		struct {
			struct aim_chat_roominfo roominfo;
		} chat;
		struct {
			guint16 msgtype;
			guint32 fgcolor;
			guint32 bgcolor;
			const char *rtfmsg;
		} rtfmsg;
		struct {
			guint16 subtype;
			guint16 totfiles;
			guint32 totsize;
			char *filename;
			/* reqnum: 0x0001 usually; 0x0002 = reply request for stage 2 proxy transfer */
			guint16 reqnum;
			guint8 use_proxy; /* Did the user request that we use a rv proxy? */
		} sendfile;
	} info;
	void *destructor; /* used internally only */
};

/* Valid values for channel 4 args->type */
#define AIM_ICQMSG_AUTHREQUEST	0x0006
#define AIM_ICQMSG_AUTHDENIED	0x0007
#define AIM_ICQMSG_AUTHGRANTED	0x0008

struct aim_incomingim_ch4_args {
	guint32 uin; /* Of the sender of the ICBM */
	guint8 type;
	guint8 flags;
	gchar *msg; /* Reason for auth request, deny, or accept */
	int msglen;
};

/* SNAC sending functions */
/* 0x0002 */ faim_export int aim_im_setparams(aim_session_t *sess, struct aim_icbmparameters *params);
/* 0x0004 */ faim_export int aim_im_reqparams(aim_session_t *sess);
/* 0x0006 */ faim_export int aim_im_sendch1_ext(aim_session_t *sess, struct aim_sendimext_args *args);
/* 0x0006 */ faim_export int aim_im_sendch1(aim_session_t *, const char *destsn, guint16 flags, const char *msg);
/* 0x0006 */ faim_export int aim_im_sendch2_chatinvite(aim_session_t *sess, const char *sn, const char *msg, guint16 exchange, const char *roomname, guint16 instance);
/* 0x0006 */ faim_export int aim_im_sendch2_icon(aim_session_t *sess, const char *sn, const guint8 *icon, int iconlen, time_t stamp, guint16 iconsum);
/* 0x0006 */ faim_export int aim_im_sendch2_rtfmsg(aim_session_t *sess, struct aim_sendrtfmsg_args *args);
/* 0x0006 */ faim_export int aim_im_sendch2_odcrequest(aim_session_t *sess, guchar *cookie, gboolean usecookie, const char *sn, const guint8 *ip, guint16 port);
/* 0x0006 */ faim_export int aim_im_sendch2_sendfile_ask(aim_session_t *sess, struct aim_oft_info *oft_info);
/* 0x0006 */ faim_export int aim_im_sendch2_sendfile_accept(aim_session_t *sess, struct aim_oft_info *info);
/* 0x0006 */ faim_export int aim_im_sendch2_sendfile_cancel(aim_session_t *sess, struct aim_oft_info *oft_info);
/* 0x0006 */ faim_export int aim_im_sendch2_geticqaway(aim_session_t *sess, const char *sn, int type);
/* 0x0006 */ faim_export int aim_im_sendch4(aim_session_t *sess, const char *sn, guint16 type, const char *message);
/* 0x0008 */ faim_export int aim_im_warn(aim_session_t *sess, aim_conn_t *conn, const char *destsn, guint32 flags);
/* 0x000b */ faim_export int aim_im_denytransfer(aim_session_t *sess, const char *sender, const guchar *cookie, guint16 code);
/* 0x0014 */ faim_export int aim_im_sendmtn(aim_session_t *sess, guint16 type1, const char *sn, guint16 type2);
faim_export void aim_icbm_makecookie(guchar* cookie);


/* 0x0002 - locate.c */
/*
 * AIM User Info, Standard Form.
 */
#define AIM_FLAG_UNCONFIRMED	0x0001 /* "damned transients" */
#define AIM_FLAG_ADMINISTRATOR	0x0002
#define AIM_FLAG_AOL			0x0004
#define AIM_FLAG_OSCAR_PAY		0x0008
#define AIM_FLAG_FREE 			0x0010
#define AIM_FLAG_AWAY			0x0020
#define AIM_FLAG_ICQ			0x0040
#define AIM_FLAG_WIRELESS		0x0080
#define AIM_FLAG_UNKNOWN100		0x0100
#define AIM_FLAG_UNKNOWN200		0x0200
#define AIM_FLAG_ACTIVEBUDDY	0x0400
#define AIM_FLAG_UNKNOWN800		0x0800
#define AIM_FLAG_ABINTERNAL		0x1000
#define AIM_FLAG_ALLUSERS		0x001f

#define AIM_USERINFO_PRESENT_FLAGS        0x00000001
#define AIM_USERINFO_PRESENT_MEMBERSINCE  0x00000002
#define AIM_USERINFO_PRESENT_ONLINESINCE  0x00000004
#define AIM_USERINFO_PRESENT_IDLE         0x00000008
#define AIM_USERINFO_PRESENT_ICQEXTSTATUS 0x00000010
#define AIM_USERINFO_PRESENT_ICQIPADDR    0x00000020
#define AIM_USERINFO_PRESENT_ICQDATA      0x00000040
#define AIM_USERINFO_PRESENT_CAPABILITIES 0x00000080
#define AIM_USERINFO_PRESENT_SESSIONLEN   0x00000100
#define AIM_USERINFO_PRESENT_CREATETIME   0x00000200

struct userinfo_node {
	char *sn;
	struct userinfo_node *next;
};

typedef struct aim_userinfo_s {
	char *sn;
	guint16 warnlevel; /* evil percent * 10 (999 = 99.9%) */
	guint16 idletime; /* in seconds */
	guint16 flags;
	guint32 createtime; /* time_t */
	guint32 membersince; /* time_t */
	guint32 onlinesince; /* time_t */
	guint32 sessionlen;  /* in seconds */
	guint32 capabilities;
	struct {
		guint32 status;
		guint32 ipaddr;
		guint8 crap[0x25]; /* until we figure it out... */
	} icqinfo;
	guint32 present;

	guint8 iconcsumtype;
	guint16 iconcsumlen;
	guint8 *iconcsum;

	char *info;
	char *info_encoding;
	guint16 info_len;

	char *status;
	char *status_encoding;
	guint16 status_len;

	char *away;
	char *away_encoding;
	guint16 away_len;

	struct aim_userinfo_s *next;
} aim_userinfo_t;

#define AIM_CAPS_BUDDYICON		0x00000001
#define AIM_CAPS_TALK			0x00000002
#define AIM_CAPS_DIRECTIM		0x00000004
#define AIM_CAPS_CHAT			0x00000008
#define AIM_CAPS_GETFILE		0x00000010
#define AIM_CAPS_SENDFILE		0x00000020
#define AIM_CAPS_GAMES			0x00000040
#define AIM_CAPS_ADDINS			0x00000080
#define AIM_CAPS_SENDBUDDYLIST	0x00000100
#define AIM_CAPS_GAMES2			0x00000200
#define AIM_CAPS_ICQ_DIRECT		0x00000400
#define AIM_CAPS_APINFO			0x00000800
#define AIM_CAPS_ICQRTF			0x00001000
#define AIM_CAPS_EMPTY			0x00002000
#define AIM_CAPS_ICQSERVERRELAY	0x00004000
#define AIM_CAPS_ICQUTF8OLD		0x00008000
#define AIM_CAPS_TRILLIANCRYPT	0x00010000
#define AIM_CAPS_ICQUTF8		0x00020000
#define AIM_CAPS_INTEROPERATE	0x00040000
#define AIM_CAPS_ICHAT			0x00080000
#define AIM_CAPS_HIPTOP			0x00100000
#define AIM_CAPS_SECUREIM		0x00200000
#define AIM_CAPS_SMS			0x00400000
#define AIM_CAPS_GENERICUNKNOWN	0x00800000
#define AIM_CAPS_VIDEO			0x01000000
#define AIM_CAPS_ICHATAV		0x02000000
#define AIM_CAPS_LIVEVIDEO		0x04000000
#define AIM_CAPS_CAMERA			0x08000000
#define AIM_CAPS_LAST			0x10000000

#define AIM_SENDMEMBLOCK_FLAG_ISREQUEST  0
#define AIM_SENDMEMBLOCK_FLAG_ISHASH     1

faim_export int aim_sendmemblock(aim_session_t *sess, aim_conn_t *conn, guint32 offset, guint32 len, const guint8 *buf, guint8 flag);

struct aim_invite_priv {
	char *sn;
	char *roomname;
	guint16 exchange;
	guint16 instance;
};

#define AIM_COOKIETYPE_UNKNOWN  0x00
#define AIM_COOKIETYPE_ICBM     0x01
#define AIM_COOKIETYPE_ADS      0x02
#define AIM_COOKIETYPE_BOS      0x03
#define AIM_COOKIETYPE_IM       0x04
#define AIM_COOKIETYPE_CHAT     0x05
#define AIM_COOKIETYPE_CHATNAV  0x06
#define AIM_COOKIETYPE_INVITE   0x07
/* we'll move OFT up a bit to give breathing room.  not like it really
 * matters. */
#define AIM_COOKIETYPE_OFTIM    0x10
#define AIM_COOKIETYPE_OFTGET   0x11
#define AIM_COOKIETYPE_OFTSEND  0x12
#define AIM_COOKIETYPE_OFTVOICE 0x13
#define AIM_COOKIETYPE_OFTIMAGE 0x14
#define AIM_COOKIETYPE_OFTICON  0x15

faim_export aim_userinfo_t *aim_locate_finduserinfo(aim_session_t *sess, const char *sn);
faim_export void aim_locate_dorequest(aim_session_t *sess);

/* 0x0002 */ faim_export int aim_locate_reqrights(aim_session_t *sess);
/* 0x0004 */ faim_export int aim_locate_setcaps(aim_session_t *sess, guint32 caps);
/* 0x0004 */ faim_export int aim_locate_setprofile(aim_session_t *sess, const char *profile_encoding, const gchar *profile, const int profile_len, const char *awaymsg_encoding, const gchar *awaymsg, const int awaymsg_len);
/* 0x0005 */ faim_export int aim_locate_getinfo(aim_session_t *sess, const char *, guint16);
/* 0x0009 */ faim_export int aim_locate_setdirinfo(aim_session_t *sess, const char *first, const char *middle, const char *last, const char *maiden, const char *nickname, const char *street, const char *city, const char *state, const char *zip, int country, guint16 privacy);
/* 0x000b */ faim_export int aim_locate_000b(aim_session_t *sess, const char *sn);
/* 0x000f */ faim_export int aim_locate_setinterests(aim_session_t *sess, const char *interest1, const char *interest2, const char *interest3, const char *interest4, const char *interest5, guint16 privacy);
/* 0x0015 */ faim_export int aim_locate_getinfoshort(aim_session_t *sess, const char *sn, guint32 flags);



/* 0x0003 - buddylist.c */
/* 0x0002 */ faim_export int aim_buddylist_reqrights(aim_session_t *, aim_conn_t *);
/* 0x0004 */ faim_export int aim_buddylist_set(aim_session_t *, aim_conn_t *, const char *);
/* 0x0004 */ faim_export int aim_buddylist_addbuddy(aim_session_t *, aim_conn_t *, const char *);
/* 0x0005 */ faim_export int aim_buddylist_removebuddy(aim_session_t *, aim_conn_t *, const char *);
/* 0x000b */ faim_export int aim_buddylist_oncoming(aim_session_t *sess, aim_conn_t *conn, aim_userinfo_t *info);
/* 0x000c */ faim_export int aim_buddylist_offgoing(aim_session_t *sess, aim_conn_t *conn, const char *sn);



/* 0x000a - search.c */
faim_export int aim_search_address(aim_session_t *, aim_conn_t *, const char *);



/* 0x000d - chatnav.c */
/* 0x000e - chat.c */
/* These apply to exchanges as well. */
#define AIM_CHATROOM_FLAG_EVILABLE 0x0001
#define AIM_CHATROOM_FLAG_NAV_ONLY 0x0002
#define AIM_CHATROOM_FLAG_INSTANCING_ALLOWED 0x0004
#define AIM_CHATROOM_FLAG_OCCUPANT_PEEK_ALLOWED 0x0008

struct aim_chat_exchangeinfo {
	guint16 number;
	guint16 flags;
	char *name;
	char *charset1;
	char *lang1;
	char *charset2;
	char *lang2;
};

#define AIM_CHATFLAGS_NOREFLECT 0x0001
#define AIM_CHATFLAGS_AWAY      0x0002
faim_export int aim_chat_send_im(aim_session_t *sess, aim_conn_t *conn, guint16 flags, const gchar *msg, int msglen, const char *encoding, const char *language);
faim_export int aim_chat_join(aim_session_t *sess, aim_conn_t *conn, guint16 exchange, const char *roomname, guint16 instance);
faim_export int aim_chat_attachname(aim_conn_t *conn, guint16 exchange, const char *roomname, guint16 instance);
faim_export char *aim_chat_getname(aim_conn_t *conn);
faim_export aim_conn_t *aim_chat_getconn(aim_session_t *, const char *name);

faim_export int aim_chatnav_reqrights(aim_session_t *sess, aim_conn_t *conn);

faim_export int aim_chatnav_createroom(aim_session_t *sess, aim_conn_t *conn, const char *name, guint16 exchange);
faim_export int aim_chat_leaveroom(aim_session_t *sess, const char *name);



/* 0x000f - odir.c */
struct aim_odir {
	char *first;
	char *last;
	char *middle;
	char *maiden;
	char *email;
	char *country;
	char *state;
	char *city;
	char *sn;
	char *interest;
	char *nick;
	char *zip;
	char *region;
	char *address;
	struct aim_odir *next;
};

faim_export int aim_odir_email(aim_session_t *, const char *, const char *);
faim_export int aim_odir_name(aim_session_t *, const char *, const char *, const char *, const char *, const char *, const char *, const char *, const char *, const char *, const char *, const char *);
faim_export int aim_odir_interest(aim_session_t *, const char *, const char *);



/* 0x0010 - icon.c */
faim_export int aim_bart_upload(aim_session_t *sess, const guint8 *icon, guint16 iconlen);
faim_export int aim_bart_request(aim_session_t *sess, const char *sn, guint8 iconcsumtype, const guint8 *iconstr, guint16 iconstrlen);



/* 0x0013 - ssi.c */
#define AIM_SSI_TYPE_BUDDY		0x0000
#define AIM_SSI_TYPE_GROUP		0x0001
#define AIM_SSI_TYPE_PERMIT		0x0002
#define AIM_SSI_TYPE_DENY		0x0003
#define AIM_SSI_TYPE_PDINFO		0x0004
#define AIM_SSI_TYPE_PRESENCEPREFS	0x0005
#define AIM_SSI_TYPE_ICONINFO		0x0014

#define AIM_SSI_ACK_SUCCESS		0x0000
#define AIM_SSI_ACK_ITEMNOTFOUND	0x0002
#define AIM_SSI_ACK_IDNUMINUSE		0x000a
#define AIM_SSI_ACK_ATMAX		0x000c
#define AIM_SSI_ACK_INVALIDNAME		0x000d
#define AIM_SSI_ACK_AUTHREQUIRED	0x000e

/* These flags are set in the 0x00c9 TLV of SSI teyp 0x0005 */
#define AIM_SSI_PRESENCE_FLAG_SHOWIDLE        0x00000400
#define AIM_SSI_PRESENCE_FLAG_NORECENTBUDDIES 0x00020000

struct aim_ssi_item {
	char *name;
	guint16 gid;
	guint16 bid;
	guint16 type;
	struct aim_tlvlist_s *data;
	struct aim_ssi_item *next;
};

struct aim_ssi_tmp {
	guint16 action;
	guint16 ack;
	char *name;
	struct aim_ssi_item *item;
	struct aim_ssi_tmp *next;
};

/* These build the actual SNACs and queue them to be sent */
/* 0x0002 */ faim_export int aim_ssi_reqrights(aim_session_t *sess);
/* 0x0004 */ faim_export int aim_ssi_reqdata(aim_session_t *sess);
/* 0x0005 */ faim_export int aim_ssi_reqifchanged(aim_session_t *sess, time_t localstamp, guint16 localrev);
/* 0x0007 */ faim_export int aim_ssi_enable(aim_session_t *sess);
/* 0x0008 */ faim_export int aim_ssi_addmoddel(aim_session_t *sess);
/* 0x0011 */ faim_export int aim_ssi_modbegin(aim_session_t *sess);
/* 0x0012 */ faim_export int aim_ssi_modend(aim_session_t *sess);
/* 0x0014 */ faim_export int aim_ssi_sendauth(aim_session_t *sess, char *sn, char *msg);
/* 0x0018 */ faim_export int aim_ssi_sendauthrequest(aim_session_t *sess, char *sn, const char *msg);
/* 0x001a */ faim_export int aim_ssi_sendauthreply(aim_session_t *sess, char *sn, guint8 reply, const char *msg);

/* Client functions for retrieving SSI data */
faim_export struct aim_ssi_item *aim_ssi_itemlist_find(struct aim_ssi_item *list, guint16 gid, guint16 bid);
faim_export struct aim_ssi_item *aim_ssi_itemlist_finditem(struct aim_ssi_item *list, const char *gn, const char *sn, guint16 type);
faim_export struct aim_ssi_item *aim_ssi_itemlist_exists(struct aim_ssi_item *list, const char *sn);
faim_export char *aim_ssi_itemlist_findparentname(struct aim_ssi_item *list, const char *sn);
faim_export int aim_ssi_getpermdeny(struct aim_ssi_item *list);
faim_export guint32 aim_ssi_getpresence(struct aim_ssi_item *list);
faim_export char *aim_ssi_getalias(struct aim_ssi_item *list, const char *gn, const char *sn);
faim_export char *aim_ssi_getcomment(struct aim_ssi_item *list, const char *gn, const char *sn);
faim_export int aim_ssi_waitingforauth(struct aim_ssi_item *list, const char *gn, const char *sn);

/* Client functions for changing SSI data */
faim_export int aim_ssi_addbuddy(aim_session_t *sess, const char *name, const char *group, const char *alias, const char *comment, const char *smsnum, int needauth);
faim_export int aim_ssi_addpermit(aim_session_t *sess, const char *name);
faim_export int aim_ssi_adddeny(aim_session_t *sess, const char *name);
faim_export int aim_ssi_delbuddy(aim_session_t *sess, const char *name, const char *group);
faim_export int aim_ssi_delpermit(aim_session_t *sess, const char *name);
faim_export int aim_ssi_deldeny(aim_session_t *sess, const char *name);
faim_export int aim_ssi_movebuddy(aim_session_t *sess, const char *oldgn, const char *newgn, const char *sn);
faim_export int aim_ssi_aliasbuddy(aim_session_t *sess, const char *gn, const char *sn, const char *alias);
faim_export int aim_ssi_editcomment(aim_session_t *sess, const char *gn, const char *sn, const char *alias);
faim_export int aim_ssi_rename_group(aim_session_t *sess, const char *oldgn, const char *newgn);
faim_export int aim_ssi_cleanlist(aim_session_t *sess);
faim_export int aim_ssi_deletelist(aim_session_t *sess);
faim_export int aim_ssi_setpermdeny(aim_session_t *sess, guint8 permdeny, guint32 vismask);
faim_export int aim_ssi_setpresence(aim_session_t *sess, guint32 presence);
faim_export int aim_ssi_seticon(aim_session_t *sess, guint8 *iconsum, guint16 iconsumlen);
faim_export int aim_ssi_delicon(aim_session_t *sess);



/* 0x0015 - icq.c */
#define AIM_ICQ_INFO_SIMPLE	0x001
#define AIM_ICQ_INFO_SUMMARY	0x002
#define AIM_ICQ_INFO_EMAIL	0x004
#define AIM_ICQ_INFO_PERSONAL	0x008
#define AIM_ICQ_INFO_ADDITIONAL	0x010
#define AIM_ICQ_INFO_WORK	0x020
#define AIM_ICQ_INFO_INTERESTS	0x040
#define AIM_ICQ_INFO_ORGS	0x080
#define AIM_ICQ_INFO_UNKNOWN	0x100
#define AIM_ICQ_INFO_HAVEALL	0x1ff

struct aim_icq_offlinemsg {
	guint32 sender;
	guint16 year;
	guint8 month, day, hour, minute;
	guint8 type;
	guint8 flags;
	char *msg;
	int msglen;
};

struct aim_icq_info {
	guint16 reqid;

	/* simple */
	guint32 uin;

	/* general and "home" information (0x00c8) */
	char *nick;
	char *first;
	char *last;
	char *email;
	char *homecity;
	char *homestate;
	char *homephone;
	char *homefax;
	char *homeaddr;
	char *mobile;
	char *homezip;
	guint16 homecountry;
/*	guint8 timezone;
	guint8 hideemail; */

	/* personal (0x00dc) */
	guint8 age;
	guint8 unknown;
	guint8 gender;
	char *personalwebpage;
	guint16 birthyear;
	guint8 birthmonth;
	guint8 birthday;
	guint8 language1;
	guint8 language2;
	guint8 language3;

	/* work (0x00d2) */
	char *workcity;
	char *workstate;
	char *workphone;
	char *workfax;
	char *workaddr;
	char *workzip;
	guint16 workcountry;
	char *workcompany;
	char *workdivision;
	char *workposition;
	char *workwebpage;

	/* additional personal information (0x00e6) */
	char *info;

	/* email (0x00eb) */
	guint16 numaddresses;
	char **email2;

	/* we keep track of these in a linked list because we're 1337 */
	struct aim_icq_info *next;
};

faim_export int aim_icq_reqofflinemsgs(aim_session_t *sess);
faim_export int aim_icq_ackofflinemsgs(aim_session_t *sess);
faim_export int aim_icq_setsecurity(aim_session_t *sess, gboolean auth_required, gboolean webaware);
faim_export int aim_icq_changepasswd(aim_session_t *sess, const char *passwd);
faim_export int aim_icq_getsimpleinfo(aim_session_t *sess, const char *uin);
faim_export int aim_icq_getalias(aim_session_t *sess, const char *uin);
faim_export int aim_icq_getallinfo(aim_session_t *sess, const char *uin);



/* 0x0017 - auth.c */
faim_export int aim_sendcookie(aim_session_t *, aim_conn_t *, const guint16 length, const guint8 *);
faim_export int aim_admin_changepasswd(aim_session_t *, aim_conn_t *, const char *newpw, const char *curpw);
faim_export int aim_admin_reqconfirm(aim_session_t *sess, aim_conn_t *conn);
faim_export int aim_admin_getinfo(aim_session_t *sess, aim_conn_t *conn, guint16 info);
faim_export int aim_admin_setemail(aim_session_t *sess, aim_conn_t *conn, const char *newemail);
faim_export int aim_admin_setnick(aim_session_t *sess, aim_conn_t *conn, const char *newnick);



/* 0x0018 - email.c */
struct aim_emailinfo {
	guint8 *cookie16;
	guint8 *cookie8;
	char *url;
	guint16 nummsgs;
	guint8 unread;
	char *domain;
	guint16 flag;
	struct aim_emailinfo *next;
};

faim_export int aim_email_sendcookies(aim_session_t *sess);
faim_export int aim_email_activate(aim_session_t *sess);



/* tlv.c - TLV handling */

/* TLV structure */
typedef struct aim_tlv_s {
	guint16 type;
	guint16 length;
	guint8 *value;
} aim_tlv_t;

/* TLV List structure */
typedef struct aim_tlvlist_s {
	aim_tlv_t *tlv;
	struct aim_tlvlist_s *next;
} aim_tlvlist_t;

/* TLV handling functions */
faim_internal aim_tlv_t *aim_tlv_gettlv(aim_tlvlist_t *list, guint16 type, const int nth);
faim_internal int aim_tlv_getlength(aim_tlvlist_t *list, guint16 type, const int nth);
faim_internal char *aim_tlv_getstr(aim_tlvlist_t *list, const guint16 type, const int nth);
faim_internal guint8 aim_tlv_get8(aim_tlvlist_t *list, const guint16 type, const int nth);
faim_internal guint16 aim_tlv_get16(aim_tlvlist_t *list, const guint16 type, const int nth);
faim_internal guint32 aim_tlv_get32(aim_tlvlist_t *list, const guint16 type, const int nth);

/* TLV list handling functions */
faim_internal aim_tlvlist_t *aim_tlvlist_read(aim_bstream_t *bs);
faim_internal aim_tlvlist_t *aim_tlvlist_readnum(aim_bstream_t *bs, guint16 num);
faim_internal aim_tlvlist_t *aim_tlvlist_readlen(aim_bstream_t *bs, guint16 len);
faim_internal aim_tlvlist_t *aim_tlvlist_copy(aim_tlvlist_t *orig);

faim_internal int aim_tlvlist_count(aim_tlvlist_t **list);
faim_internal int aim_tlvlist_size(aim_tlvlist_t **list);
faim_internal int aim_tlvlist_cmp(aim_tlvlist_t *one, aim_tlvlist_t *two);
faim_internal int aim_tlvlist_write(aim_bstream_t *bs, aim_tlvlist_t **list);
faim_internal void aim_tlvlist_free(aim_tlvlist_t **list);

faim_internal int aim_tlvlist_add_raw(aim_tlvlist_t **list, const guint16 type, const guint16 length, const guint8 *value);
faim_internal int aim_tlvlist_add_noval(aim_tlvlist_t **list, const guint16 type);
faim_internal int aim_tlvlist_add_8(aim_tlvlist_t **list, const guint16 type, const guint8 value);
faim_internal int aim_tlvlist_add_16(aim_tlvlist_t **list, const guint16 type, const guint16 value);
faim_internal int aim_tlvlist_add_32(aim_tlvlist_t **list, const guint16 type, const guint32 value);
faim_internal int aim_tlvlist_add_str(aim_tlvlist_t **list, const guint16 type, const char *value);
faim_internal int aim_tlvlist_add_caps(aim_tlvlist_t **list, const guint16 type, const guint32 caps);
faim_internal int aim_tlvlist_add_userinfo(aim_tlvlist_t **list, guint16 type, aim_userinfo_t *userinfo);
faim_internal int aim_tlvlist_add_chatroom(aim_tlvlist_t **list, guint16 type, guint16 exchange, const char *roomname, guint16 instance);
faim_internal int aim_tlvlist_add_frozentlvlist(aim_tlvlist_t **list, guint16 type, aim_tlvlist_t **tl);

faim_internal int aim_tlvlist_replace_raw(aim_tlvlist_t **list, const guint16 type, const guint16 lenth, const guint8 *value);
faim_internal int aim_tlvlist_replace_str(aim_tlvlist_t **list, const guint16 type, const char *str);
faim_internal int aim_tlvlist_replace_noval(aim_tlvlist_t **list, const guint16 type);
faim_internal int aim_tlvlist_replace_8(aim_tlvlist_t **list, const guint16 type, const guint8 value);
faim_internal int aim_tlvlist_replace_16(aim_tlvlist_t **list, const guint16 type, const guint16 value);
faim_internal int aim_tlvlist_replace_32(aim_tlvlist_t **list, const guint16 type, const guint32 value);

faim_internal void aim_tlvlist_remove(aim_tlvlist_t **list, const guint16 type);



/* util.c */
/*
 * These are really ugly.  You'd think this was LISP.  I wish it was.
 *
 * XXX With the advent of bstream's, these should be removed to enforce
 * their use.
 *
 */
#define aimutil_put8(buf, data) ((*(buf) = (guint8)(data)&0xff),1)
#define aimutil_get8(buf) ((*(buf))&0xff)
#define aimutil_put16(buf, data) ( \
		(*(buf) = (guint8)((data)>>8)&0xff), \
		(*((buf)+1) = (guint8)(data)&0xff),  \
		2)
#define aimutil_get16(buf) ((((*(buf))<<8)&0xff00) + ((*((buf)+1)) & 0xff))
#define aimutil_put32(buf, data) ( \
		(*((buf)) = (guint8)((data)>>24)&0xff), \
		(*((buf)+1) = (guint8)((data)>>16)&0xff), \
		(*((buf)+2) = (guint8)((data)>>8)&0xff), \
		(*((buf)+3) = (guint8)(data)&0xff), \
		4)
#define aimutil_get32(buf) ((((*(buf))<<24)&0xff000000) + \
		(((*((buf)+1))<<16)&0x00ff0000) + \
		(((*((buf)+2))<< 8)&0x0000ff00) + \
		(((*((buf)+3)    )&0x000000ff)))

/* Little-endian versions (damn ICQ) */
#define aimutil_putle8(buf, data) ( \
		(*(buf) = (guint8)(data) & 0xff), \
		1)
#define aimutil_getle8(buf) ( \
		(*(buf)) & 0xff \
		)
#define aimutil_putle16(buf, data) ( \
		(*((buf)+0) = (guint8)((data) >> 0) & 0xff),  \
		(*((buf)+1) = (guint8)((data) >> 8) & 0xff), \
		2)
#define aimutil_getle16(buf) ( \
		(((*((buf)+0)) << 0) & 0x00ff) + \
		(((*((buf)+1)) << 8) & 0xff00) \
		)
#define aimutil_putle32(buf, data) ( \
		(*((buf)+0) = (guint8)((data) >>  0) & 0xff), \
		(*((buf)+1) = (guint8)((data) >>  8) & 0xff), \
		(*((buf)+2) = (guint8)((data) >> 16) & 0xff), \
		(*((buf)+3) = (guint8)((data) >> 24) & 0xff), \
		4)
#define aimutil_getle32(buf) ( \
		(((*((buf)+0)) <<  0) & 0x000000ff) + \
		(((*((buf)+1)) <<  8) & 0x0000ff00) + \
		(((*((buf)+2)) << 16) & 0x00ff0000) + \
		(((*((buf)+3)) << 24) & 0xff000000))


faim_export int aimutil_putstr(char *, const char *, int);
faim_export guint16 aimutil_iconsum(const guint8 *buf, int buflen);
faim_export int aimutil_tokslen(char *toSearch, int theindex, char dl);
faim_export int aimutil_itemcnt(char *toSearch, char dl);
faim_export char *aimutil_itemindex(char *toSearch, int theindex, char dl);

faim_export int aim_snvalid(const char *sn);
faim_export int aim_sn_is_icq(const char *sn);
faim_export int aim_sn_is_sms(const char *sn);
faim_export int aim_snlen(const char *sn);
faim_export int aim_sncmp(const char *sn1, const char *sn2);

#include "oscar_internal.h"

#ifdef __cplusplus
}
#endif

#endif /* _OSCAR_H_ */
