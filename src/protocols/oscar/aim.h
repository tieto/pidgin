/* 
 * Main libfaim header.  Must be included in client for prototypes/macros.
 *
 * "come on, i turned a chick lesbian; i think this is the hackish equivalent"
 *                                                -- Josh Meyer
 *
 */

#ifndef __AIM_H__
#define __AIM_H__

#define FAIM_VERSION_MAJOR 0
#define FAIM_VERSION_MINOR 99
#define FAIM_VERSION_MINORMINOR 1

#include <faimconfig.h>
#include <aim_cbtypes.h>

#if !defined(FAIM_USEPTHREADS) && !defined(FAIM_USEFAKELOCKS) && !defined(FAIM_USENOPLOCKS)
#error pthreads, fakelocks, or noplocks are currently required.
#endif

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <sys/time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

/* XXX adjust these based on autoconf-detected platform */
typedef unsigned char fu8_t;
typedef unsigned short fu16_t;
typedef unsigned long fu32_t;
typedef fu32_t aim_snacid_t;
typedef fu16_t flap_seqnum_t;

#ifdef FAIM_USEPTHREADS
#include <pthread.h>
#define faim_mutex_t pthread_mutex_t 
#define faim_mutex_init(x) pthread_mutex_init(x, NULL)
#define faim_mutex_lock(x) pthread_mutex_lock(x)
#define faim_mutex_unlock(x) pthread_mutex_unlock(x)
#define faim_mutex_destroy(x) pthread_mutex_destroy(x)
#elif defined(FAIM_USEFAKELOCKS)
/*
 * For platforms without pthreads, we also assume
 * we're not linking against a threaded app.  Which
 * means we don't have to do real locking.  The 
 * macros below do nothing really.  They're a joke.
 * But they get it to compile.
 * 
 * XXX NOTE that locking hasn't really been tested in a long time,
 * and most code written after dec2000 --is not thread safe--.  You'll
 * want to audit locking use before you use less-than-library level
 * concurrency.
 *
 */
#define faim_mutex_t fu8_t 
#define faim_mutex_init(x) *x = 0
#define faim_mutex_lock(x) while(*x != 0) {/* spin */}; *x = 1;
#define faim_mutex_unlock(x) while(*x != 0) {/* spin spin spin */}; *x = 0;
#define faim_mutex_destroy(x) while(*x != 0) {/* spiiiinnn */}; *x = 0;
#elif defined(FAIM_USENOPLOCKS)
#define faim_mutex_t fu8_t 
#define faim_mutex_init(x)
#define faim_mutex_lock(x)
#define faim_mutex_unlock(x)
#define faim_mutex_destroy(x)
#endif

/* Portability stuff (DMP) */

#ifdef _WIN32
#define sleep(x) Sleep((x)*1000)
#define snprintf _snprintf /* I'm not sure whats wrong with Microsoft here */
#define close(x) closesocket(x) /* no comment */
#endif

#if defined(mach) && defined(__APPLE__)
#define gethostbyname(x) gethostbyname2(x, AF_INET) 
#endif

#if defined(_WIN32) || defined(STRICT_ANSI)
#define faim_shortfunc
#else
#define faim_shortfunc inline
#endif

#if defined(_WIN32) && !defined(WIN32_STATIC)
/*
 * For a win32 DLL, we define WIN32_INDLL if this file
 * is included while compiling the DLL. If its not 
 * defined (its included in a client app), the symbols
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

/* 
 * Current Maximum Length for Screen Names (not including NULL) 
 *
 * Currently only names up to 16 characters can be registered
 * however it is aparently legal for them to be larger.
 */
#define MAXSNLEN 32

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

/*
 * Standard size of an AIM authorization cookie
 */
#define AIM_COOKIELEN            0x100

#define AIM_MD5_STRING "AOL Instant Messenger (SM)"

/*
 * Client info.  Filled in by the client and passed
 * in to aim_login().  The information ends up
 * getting passed to OSCAR through the initial
 * login command.
 *
 * XXX: Should this be per-session? -mid
 *
 */
struct client_info_s {
	char clientstring[100]; /* arbitrary size */
	int major;
	int minor;
	int build;
	char country[3];
	char lang[3];
	int major2;
	int minor2;
	long unknown;
};

#define AIM_CLIENTINFO_KNOWNGOOD_3_5_1670 { \
	"AOL Instant Messenger (SM), version 3.5.1670/WIN32", \
	0x0003, \
	0x0005, \
	0x0686, \
	"us", \
	"en", \
	0x0004, \
	0x0000, \
	0x0000002a, \
}

#define AIM_CLIENTINFO_KNOWNGOOD_4_1_2010 { \
	  "AOL Instant Messenger (SM), version 4.1.2010/WIN32", \
	  0x0004, \
	  0x0001, \
	  0x07da, \
	  "us", \
	  "en", \
	  0x0004, \
	  0x0000, \
	  0x0000004b, \
}

/*
 * I would make 4.1.2010 the default, but they seem to have found
 * an alternate way of breaking that one. 
 *
 * 3.5.1670 should work fine, however, you will be subjected to the
 * memory test, which may require you to have a WinAIM binary laying 
 * around. (see login.c::memrequest())
 */
#define AIM_CLIENTINFO_KNOWNGOOD AIM_CLIENTINFO_KNOWNGOOD_3_5_1670

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

/* 
 * These could be arbitrary, but its easier to use the actual AIM values 
 */
#define AIM_CONN_TYPE_AUTH          0x0007
#define AIM_CONN_TYPE_ADS           0x0005
#define AIM_CONN_TYPE_BOS           0x0002
#define AIM_CONN_TYPE_CHAT          0x000e
#define AIM_CONN_TYPE_CHATNAV       0x000d

/* they start getting arbitrary in rendezvous stuff =) */
#define AIM_CONN_TYPE_RENDEZVOUS    0x0101 /* these do not speak FLAP! */
#define AIM_CONN_TYPE_RENDEZVOUS_OUT 0x0102 /* socket waiting for accept() */

/*
 * Subtypes, we need these for OFT stuff.
 */
#define AIM_CONN_SUBTYPE_OFT_DIRECTIM  0x0001
#define AIM_CONN_SUBTYPE_OFT_GETFILE   0x0002
#define AIM_CONN_SUBTYPE_OFT_SENDFILE  0x0003
#define AIM_CONN_SUBTYPE_OFT_BUDDYICON 0x0004
#define AIM_CONN_SUBTYPE_OFT_VOICE     0x0005

/*
 * Status values returned from aim_conn_new().  ORed together.
 */
#define AIM_CONN_STATUS_READY       0x0001
#define AIM_CONN_STATUS_INTERNALERR 0x0002
#define AIM_CONN_STATUS_RESOLVERR   0x0040
#define AIM_CONN_STATUS_CONNERR     0x0080
#define AIM_CONN_STATUS_INPROGRESS  0x0100

#define AIM_FRAMETYPE_FLAP 0x0000
#define AIM_FRAMETYPE_OFT  0x0001

typedef struct aim_conn_s {
	int fd;
	fu16_t type;
	fu16_t subtype;
	flap_seqnum_t seqnum;
	fu32_t status;
	void *priv; /* misc data the client may want to store */
	void *internal; /* internal conn-specific libfaim data */
	time_t lastactivity; /* time of last transmit */
	int forcedlatency; 
	void *handlerlist;
	faim_mutex_t active; /* lock around read/writes */
	faim_mutex_t seqnum_lock; /* lock around ->seqnum changes */
	void *sessv; /* pointer to parent session */
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
	fu8_t *data;
	fu16_t len;
	fu16_t offset;
} aim_bstream_t;

typedef struct aim_frame_s {
	fu8_t hdrtype; /* defines which piece of the union to use */
	union {
		struct { 
			fu8_t type;
			flap_seqnum_t seqnum;     
		} flap;
		struct {
			fu16_t type;
			fu8_t magic[4]; /* ODC2 OFT2 */
			fu16_t hdr2len;
			fu8_t *hdr2; /* rest of bloated header */
		} oft;
	} hdr;
	aim_bstream_t data;	/* payload stream */
	fu8_t handled;		/* 0 = new, !0 = been handled */
	fu8_t nofree;		/* 0 = free data on purge, 1 = only unlink */
	aim_conn_t *conn;  /* the connection it came in on... */
	struct aim_frame_s *next;
} aim_frame_t;

typedef struct aim_msgcookie_s {
	unsigned char cookie[8];
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
	faim_mutex_t connlistlock;

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

	/*
	 * This is a dreadful solution to the what-room-are-we-joining
	 * problem.  (There's no connection between the service
	 * request and the resulting redirect.)
	 */ 
	char *pendingjoin;
	fu16_t pendingjoinexchange;

	/*
	 * Outstanding snac handling 
	 *
	 * XXX: Should these be per-connection? -mid
	 */
	void *snac_hash[FAIM_SNAC_HASH_SIZE];
	faim_mutex_t snac_hash_locks[FAIM_SNAC_HASH_SIZE];
	aim_snacid_t snacid_next;

	struct {
		char server[128];
		char username[128];
		char password[128];
	} socksproxy;

	fu32_t flags; /* AIM_SESS_FLAGS_ */

	int debug;
	void (*debugcb)(struct aim_session_s *sess, int level, const char *format, va_list va); /* same as faim_debugging_callback_t */

	aim_msgcookie_t *msgcookies;

	void *modlistv;
} aim_session_t;

/* Values for sess->flags */
#define AIM_SESS_FLAGS_SNACLOGIN       0x00000001
#define AIM_SESS_FLAGS_XORLOGIN        0x00000002
#define AIM_SESS_FLAGS_NONBLOCKCONNECT 0x00000004

/*
 * AIM User Info, Standard Form.
 */
struct aim_userinfo_s {
	char sn[MAXSNLEN+1];
	fu16_t warnlevel;
	fu16_t idletime;
	fu16_t flags;
	fu32_t membersince;
	fu32_t onlinesince;
	fu32_t sessionlen;  
	fu16_t capabilities;
	struct {
		fu16_t status;
		fu32_t ipaddr;
		fu8_t crap[0x25]; /* until we figure it out... */
	} icqinfo;
};

#define AIM_FLAG_UNCONFIRMED 	0x0001 /* "damned transients" */
#define AIM_FLAG_ADMINISTRATOR	0x0002
#define AIM_FLAG_AOL		0x0004
#define AIM_FLAG_OSCAR_PAY	0x0008
#define AIM_FLAG_FREE 		0x0010
#define AIM_FLAG_AWAY		0x0020
#define AIM_FLAG_UNKNOWN40	0x0040
#define AIM_FLAG_UNKNOWN80	0x0080

#define AIM_FLAG_ALLUSERS	0x001f


#if defined(FAIM_INTERNAL) || defined(FAIM_NEED_TLV)
/*
 * TLV handling
 */

/* Generic TLV structure. */
typedef struct aim_tlv_s {
	fu16_t type;
	fu16_t length;
	fu8_t *value;
} aim_tlv_t;

/* List of above. */
typedef struct aim_tlvlist_s {
	aim_tlv_t *tlv;
	struct aim_tlvlist_s *next;
} aim_tlvlist_t;

/* TLV-handling functions */

#if 0
/* Very, very raw TLV handling. */
faim_internal int aim_puttlv_8(fu8_t *buf, const fu16_t t, const fu8_t v);
faim_internal int aim_puttlv_16(fu8_t *buf, const fu16_t t, const fu16_t v);
faim_internal int aim_puttlv_32(fu8_t *buf, const fu16_t t, const fu32_t v);
faim_internal int aim_puttlv_raw(fu8_t *buf, const fu16_t t, const fu16_t l, const fu8_t *v);
#endif

/* TLV list handling. */
faim_internal aim_tlvlist_t *aim_readtlvchain(aim_bstream_t *bs);
faim_internal void aim_freetlvchain(aim_tlvlist_t **list);
faim_internal aim_tlv_t *aim_gettlv(aim_tlvlist_t *, fu16_t t, const int n);
faim_internal char *aim_gettlv_str(aim_tlvlist_t *, const fu16_t t, const int n);
faim_internal fu8_t aim_gettlv8(aim_tlvlist_t *list, const fu16_t type, const int num);
faim_internal fu16_t aim_gettlv16(aim_tlvlist_t *list, const fu16_t t, const int n);
faim_internal fu32_t aim_gettlv32(aim_tlvlist_t *list, const fu16_t t, const int n);
faim_internal int aim_writetlvchain(aim_bstream_t *bs, aim_tlvlist_t **list);
faim_internal int aim_addtlvtochain16(aim_tlvlist_t **list, const fu16_t t, const fu16_t v);
faim_internal int aim_addtlvtochain32(aim_tlvlist_t **list, const fu16_t type, const fu32_t v);
faim_internal int aim_addtlvtochain_raw(aim_tlvlist_t **list, const fu16_t t, const fu16_t l, const fu8_t *v);
faim_internal int aim_addtlvtochain_caps(aim_tlvlist_t **list, const fu16_t t, const fu16_t caps);
faim_internal int aim_addtlvtochain_noval(aim_tlvlist_t **list, const fu16_t type);
faim_internal int aim_addtlvtochain_frozentlvlist(aim_tlvlist_t **list, fu16_t type, aim_tlvlist_t **tl);
faim_internal int aim_counttlvchain(aim_tlvlist_t **list);
faim_export int aim_sizetlvchain(aim_tlvlist_t **list);
#endif /* FAIM_INTERNAL */

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

#if !defined(FAIM_INTERNAL) || defined(FAIM_INTERNAL_INSANE)
/* the library should never call aim_conn_kill */
faim_export void aim_conn_kill(aim_session_t *sess, aim_conn_t **deadconn);
#endif

typedef int (*aim_rxcallback_t)(aim_session_t *, aim_frame_t *, ...);

/* aim_login.c */
faim_export int aim_sendflapver(aim_session_t *sess, aim_conn_t *conn);
faim_export int aim_request_login(aim_session_t *sess, aim_conn_t *conn, const char *sn);
faim_export int aim_send_login(aim_session_t *, aim_conn_t *, const char *, const char *, struct client_info_s *, const char *key);
faim_export int aim_encode_password_md5(const char *password, const char *key, unsigned char *digest);
faim_export int aim_sendauthresp(aim_session_t *sess, aim_conn_t *conn, const char *sn, int errorcode, const char *errorurl, const char *bosip, const char *cookie, const char *email, int regstatus);
faim_export int aim_gencookie(unsigned char *buf);
faim_export int aim_sendserverready(aim_session_t *sess, aim_conn_t *conn);
faim_export int aim_sendredirect(aim_session_t *sess, aim_conn_t *conn, fu16_t servid, const char *ip, const char *cookie);
faim_export void aim_purge_rxqueue(aim_session_t *);

#define AIM_TX_QUEUED    0 /* default */
#define AIM_TX_IMMEDIATE 1
#define AIM_TX_USER      2
faim_export int aim_tx_setenqueue(aim_session_t *sess, int what, int (*func)(aim_session_t *, aim_frame_t *));

faim_export int aim_tx_flushqueue(aim_session_t *);
faim_export void aim_tx_purgequeue(aim_session_t *);

faim_export int aim_conn_setlatency(aim_conn_t *conn, int newval);

faim_export int aim_conn_addhandler(aim_session_t *, aim_conn_t *conn, u_short family, u_short type, aim_rxcallback_t newhandler, u_short flags);
faim_export int aim_clearhandlers(aim_conn_t *conn);

faim_export aim_session_t *aim_conn_getsess(aim_conn_t *conn);
faim_export void aim_conn_close(aim_conn_t *deadconn);
faim_export aim_conn_t *aim_newconn(aim_session_t *, int type, const char *dest);
faim_export int aim_conngetmaxfd(aim_session_t *);
faim_export aim_conn_t *aim_select(aim_session_t *, struct timeval *, int *);
faim_export int aim_conn_isready(aim_conn_t *);
faim_export int aim_conn_setstatus(aim_conn_t *, int);
faim_export int aim_conn_completeconnect(aim_session_t *sess, aim_conn_t *conn);
faim_export int aim_conn_isconnecting(aim_conn_t *conn);

typedef void (*faim_debugging_callback_t)(aim_session_t *sess, int level, const char *format, va_list va);
faim_export int aim_setdebuggingcb(aim_session_t *sess, faim_debugging_callback_t);
faim_export void aim_session_init(aim_session_t *, unsigned long flags, int debuglevel);
faim_export void aim_session_kill(aim_session_t *);
faim_export void aim_setupproxy(aim_session_t *sess, const char *server, const char *username, const char *password);
faim_export aim_conn_t *aim_getconn_type(aim_session_t *, int type);
faim_export aim_conn_t *aim_getconn_type_all(aim_session_t *, int type);
faim_export aim_conn_t *aim_getconn_fd(aim_session_t *, int fd);

/* aim_misc.c */

#define AIM_VISIBILITYCHANGE_PERMITADD    0x05
#define AIM_VISIBILITYCHANGE_PERMITREMOVE 0x06
#define AIM_VISIBILITYCHANGE_DENYADD      0x07
#define AIM_VISIBILITYCHANGE_DENYREMOVE   0x08

#define AIM_PRIVFLAGS_ALLOWIDLE           0x01
#define AIM_PRIVFLAGS_ALLOWMEMBERSINCE    0x02

#define AIM_WARN_ANON                     0x01

faim_export int aim_send_warning(aim_session_t *sess, aim_conn_t *conn, const char *destsn, fu32_t flags);
faim_export int aim_bos_nop(aim_session_t *, aim_conn_t *);
faim_export int aim_flap_nop(aim_session_t *sess, aim_conn_t *conn);
faim_export int aim_bos_setidle(aim_session_t *, aim_conn_t *, fu32_t);
faim_export int aim_bos_changevisibility(aim_session_t *, aim_conn_t *, int, const char *);
faim_export int aim_bos_setbuddylist(aim_session_t *, aim_conn_t *, const char *);
faim_export int aim_bos_setprofile(aim_session_t *sess, aim_conn_t *conn, const char *profile, const char *awaymsg, fu16_t caps);
faim_export int aim_bos_setgroupperm(aim_session_t *, aim_conn_t *, fu32_t mask);
faim_export int aim_bos_clientready(aim_session_t *, aim_conn_t *);
faim_export int aim_bos_reqrate(aim_session_t *, aim_conn_t *);
faim_export int aim_bos_ackrateresp(aim_session_t *, aim_conn_t *);
faim_export int aim_bos_setprivacyflags(aim_session_t *, aim_conn_t *, fu32_t);
faim_export int aim_bos_reqpersonalinfo(aim_session_t *, aim_conn_t *);
faim_export int aim_bos_reqservice(aim_session_t *, aim_conn_t *, fu16_t);
faim_export int aim_bos_reqrights(aim_session_t *, aim_conn_t *);
faim_export int aim_bos_reqbuddyrights(aim_session_t *, aim_conn_t *);
faim_export int aim_bos_reqlocaterights(aim_session_t *, aim_conn_t *);
faim_export int aim_setversions(aim_session_t *sess, aim_conn_t *conn);
faim_export int aim_setdirectoryinfo(aim_session_t *sess, aim_conn_t *conn, const char *first, const char *middle, const char *last, const char *maiden, const char *nickname, const char *street, const char *city, const char *state, const char *zip, int country, fu16_t privacy);
faim_export int aim_setuserinterests(aim_session_t *sess, aim_conn_t *conn, const char *interest1, const char *interest2, const char *interest3, const char *interest4, const char *interest5, fu16_t privacy);
faim_export int aim_icq_setstatus(aim_session_t *sess, aim_conn_t *conn, fu32_t status);

faim_export struct aim_fileheader_t *aim_getlisting(aim_session_t *sess, FILE *);

#define AIM_CLIENTTYPE_UNKNOWN  0x0000
#define AIM_CLIENTTYPE_MC       0x0001
#define AIM_CLIENTTYPE_WINAIM   0x0002
#define AIM_CLIENTTYPE_WINAIM41 0x0003
#define AIM_CLIENTTYPE_AOL_TOC  0x0004
faim_export unsigned short aim_fingerprintclient(unsigned char *msghdr, int len);

#define AIM_RATE_CODE_CHANGE     0x0001
#define AIM_RATE_CODE_WARNING    0x0002
#define AIM_RATE_CODE_LIMIT      0x0003
#define AIM_RATE_CODE_CLEARLIMIT 0x0004
faim_export int aim_ads_clientready(aim_session_t *sess, aim_conn_t *conn);
faim_export int aim_ads_requestads(aim_session_t *sess, aim_conn_t *conn);

/* aim_im.c */

struct aim_fileheader_t {
#if 0
	char  magic[4];		/* 0 */
	short hdrlen; 		/* 4 */
	short hdrtype;		/* 6 */
#endif
	char  bcookie[8];       /* 8 */
	short encrypt;          /* 16 */
	short compress;         /* 18 */
	short totfiles;         /* 20 */
	short filesleft;        /* 22 */
	short totparts;         /* 24 */
	short partsleft;        /* 26 */
	long  totsize;          /* 28 */
	long  size;             /* 32 */
	long  modtime;          /* 36 */
	long  checksum;         /* 40 */
	long  rfrcsum;          /* 44 */
	long  rfsize;           /* 48 */
	long  cretime;          /* 52 */
	long  rfcsum;           /* 56 */
	long  nrecvd;           /* 60 */
	long  recvcsum;         /* 64 */
	char  idstring[32];     /* 68 */
	char  flags;            /* 100 */
	char  lnameoffset;      /* 101 */
	char  lsizeoffset;      /* 102 */
	char  dummy[69];        /* 103 */
	char  macfileinfo[16];  /* 172 */
	short nencode;          /* 188 */
	short nlanguage;        /* 190 */
	char  name[64];         /* 192 */
				/* 256 */
};

struct aim_filetransfer_priv {
	char sn[MAXSNLEN];
	char cookie[8];
	char ip[30];
	int state;
	struct aim_fileheader_t fh;
};

struct aim_chat_roominfo {
	unsigned short exchange;
	char *name;
	unsigned short instance;
};

#define AIM_IMFLAGS_AWAY		0x0001 /* mark as an autoreply */
#define AIM_IMFLAGS_ACK			0x0002 /* request a receipt notice */
#define AIM_IMFLAGS_UNICODE		0x0004
#define AIM_IMFLAGS_ISO_8859_1		0x0008
#define AIM_IMFLAGS_BUDDYREQ		0x0010 /* buddy icon requested */
#define AIM_IMFLAGS_HASICON		0x0020 /* already has icon */
#define AIM_IMFLAGS_SUBENC_MACINTOSH	0x0040 /* damn that Steve Jobs! */
#define AIM_IMFLAGS_CUSTOMFEATURES 	0x0080 /* features field present */
#define AIM_IMFLAGS_EXTDATA		0x0100

struct aim_sendimext_args {

	/* These are _required_ */
	const char *destsn;
	fu32_t flags;
	const char *msg;
	int msglen;

	/* Only used if AIM_IMFLAGS_HASICON is set */
	fu32_t iconlen;
	time_t iconstamp;
	fu32_t iconsum;

	/* Only used if AIM_IMFLAGS_CUSTOMFEATURES is set */
	fu8_t *features;
	fu8_t featureslen;
};

struct aim_incomingim_ch1_args {

	/* Always provided */
	char *msg;
	int msglen;
	fu32_t icbmflags;
	fu16_t flag1;
	fu16_t flag2;

	/* Only provided if AIM_IMFLAGS_HASICON is set */
	time_t iconstamp;
	fu32_t iconlen;
	fu32_t iconsum;

	/* Only provided if AIM_IMFLAGS_CUSTOMFEATURES is set */
	fu8_t *features;
	fu8_t featureslen;

	/* Only provided if AIM_IMFLAGS_EXTDATA is set */
	fu8_t extdatalen;
	fu8_t *extdata;
};

struct aim_incomingim_ch2_args {
	fu8_t cookie[8];
	fu16_t reqclass;
	fu16_t status;
	union {
		struct {
			fu32_t checksum;
			fu32_t length;
			time_t timestamp;
			fu8_t *icon;
		} icon;
		struct {
		} voice;
		struct {
			fu8_t ip[22]; /* xxx.xxx.xxx.xxx:xxxxx\0 */
		} imimage;
		struct {
			char *msg;
			char *encoding;
			char *lang;
		struct aim_chat_roominfo roominfo;
		} chat;
		struct {
			char *ip;
			unsigned char *cookie;
		} getfile;
		struct {
		} sendfile;
	} info;
};

faim_export int aim_send_im_ext(aim_session_t *sess, aim_conn_t *conn, struct aim_sendimext_args *args);
faim_export int aim_send_im(aim_session_t *, aim_conn_t *, const char *destsn, unsigned short flags, const char *msg);
faim_export int aim_send_icon(aim_session_t *sess, aim_conn_t *conn, const char *sn, const fu8_t *icon, int iconlen, time_t stamp, fu32_t iconsum);
faim_export fu32_t aim_iconsum(const fu8_t *buf, int buflen);
faim_export int aim_send_im_direct(aim_session_t *, aim_conn_t *, const char *msg);
faim_export const char *aim_directim_getsn(aim_conn_t *conn);
faim_export aim_conn_t *aim_directim_initiate(aim_session_t *, aim_conn_t *, const char *destsn);
faim_export aim_conn_t *aim_directim_connect(aim_session_t *, const char *sn, const char *addr, const fu8_t *cookie);

faim_export aim_conn_t *aim_getfile_initiate(aim_session_t *sess, aim_conn_t *conn, const char *destsn);
faim_export int aim_oft_getfile_request(aim_session_t *sess, aim_conn_t *conn, const char *name, int size);
faim_export int aim_oft_getfile_ack(aim_session_t *sess, aim_conn_t *conn);
faim_export int aim_oft_getfile_end(aim_session_t *sess, aim_conn_t *conn);

/* aim_info.c */
#define AIM_CAPS_BUDDYICON      0x0001
#define AIM_CAPS_VOICE          0x0002
#define AIM_CAPS_IMIMAGE        0x0004
#define AIM_CAPS_CHAT           0x0008
#define AIM_CAPS_GETFILE        0x0010
#define AIM_CAPS_SENDFILE       0x0020
#define AIM_CAPS_GAMES          0x0040
#define AIM_CAPS_SAVESTOCKS     0x0080
#define AIM_CAPS_SENDBUDDYLIST  0x0100
#define AIM_CAPS_GAMES2         0x0200
#define AIM_CAPS_LAST           0x8000

faim_export int aim_0002_000b(aim_session_t *sess, aim_conn_t *conn, const char *sn);

#define AIM_SENDMEMBLOCK_FLAG_ISREQUEST  0
#define AIM_SENDMEMBLOCK_FLAG_ISHASH     1

faim_export int aim_sendmemblock(aim_session_t *sess, aim_conn_t *conn, unsigned long offset, unsigned long len, const unsigned char *buf, unsigned char flag);

#define AIM_GETINFO_GENERALINFO 0x00001
#define AIM_GETINFO_AWAYMESSAGE 0x00003

struct aim_invite_priv {
	char *sn;
	char *roomname;
	fu16_t exchange;
	fu16_t instance;
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

faim_export int aim_handlerendconnect(aim_session_t *sess, aim_conn_t *cur);

#define AIM_TRANSFER_DENY_NOTSUPPORTED 0x0000
#define AIM_TRANSFER_DENY_DECLINE 0x0001
#define AIM_TRANSFER_DENY_NOTACCEPTING 0x0002
faim_export int aim_denytransfer(aim_session_t *sess, aim_conn_t *conn, const char *sender, const char *cookie, unsigned short code);
faim_export aim_conn_t *aim_accepttransfer(aim_session_t *sess, aim_conn_t *conn, const char *sn, const fu8_t *cookie, const fu8_t *ip, fu16_t listingfiles, fu16_t listingtotsize, fu16_t listingsize, fu32_t listingchecksum, fu16_t rendid);

faim_export int aim_getinfo(aim_session_t *, aim_conn_t *, const char *, unsigned short);
faim_export int aim_sendbuddyoncoming(aim_session_t *sess, aim_conn_t *conn, struct aim_userinfo_s *info);
faim_export int aim_sendbuddyoffgoing(aim_session_t *sess, aim_conn_t *conn, const char *sn);

#define AIM_IMPARAM_FLAG_CHANMSGS_ALLOWED	0x00000001
#define AIM_IMPARAM_FLAG_MISSEDCALLS_ENABLED	0x00000002

/* This is what the server will give you if you don't set them yourself. */
#define AIM_IMPARAM_DEFAULTS { \
	0, \
	AIM_IMPARAM_FLAG_CHANMSGS_ALLOWED | AIM_IMPARAM_FLAG_MISSEDCALLS_ENABLED, \
	512, /* !! Note how small this is. */ \
	(99.9)*10, (99.9)*10, \
	1000 \
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
	unsigned short maxchan;
	unsigned long flags; /* AIM_IMPARAM_FLAG_ */
	unsigned short maxmsglen; /* message size that you will accept */
	unsigned short maxsenderwarn; /* this and below are *10 (999=99.9%) */
	unsigned short maxrecverwarn;
	unsigned long minmsginterval; /* in milliseconds? */
};

faim_export int aim_reqicbmparams(aim_session_t *sess, aim_conn_t *conn);
faim_export int aim_seticbmparam(aim_session_t *sess, aim_conn_t *conn, struct aim_icbmparameters *params);


/* auth.c */
faim_export int aim_auth_sendcookie(aim_session_t *, aim_conn_t *, const fu8_t *);

faim_export int aim_auth_clientready(aim_session_t *, aim_conn_t *);
faim_export int aim_auth_changepasswd(aim_session_t *, aim_conn_t *, const char *newpw, const char *curpw);
faim_export int aim_auth_setversions(aim_session_t *sess, aim_conn_t *conn);
faim_export int aim_auth_reqconfirm(aim_session_t *sess, aim_conn_t *conn);
faim_export int aim_auth_getinfo(aim_session_t *sess, aim_conn_t *conn, fu16_t info);
faim_export int aim_auth_setemail(aim_session_t *sess, aim_conn_t *conn, const char *newemail);

/* aim_buddylist.c */
faim_export int aim_add_buddy(aim_session_t *, aim_conn_t *, const char *);
faim_export int aim_remove_buddy(aim_session_t *, aim_conn_t *, const char *);

/* aim_search.c */
faim_export int aim_usersearch_address(aim_session_t *, aim_conn_t *, const char *);

struct aim_chat_exchangeinfo {
	fu16_t number;
	char *name;
	char *charset1;
	char *lang1;
	char *charset2;
	char *lang2;
};

#define AIM_CHATFLAGS_NOREFLECT 0x0001
#define AIM_CHATFLAGS_AWAY      0x0002
faim_export int aim_chat_send_im(aim_session_t *sess, aim_conn_t *conn, fu16_t flags, const char *msg, int msglen);
faim_export int aim_chat_join(aim_session_t *sess, aim_conn_t *conn, fu16_t exchange, const char *roomname, fu16_t instance);
faim_export int aim_chat_clientready(aim_session_t *sess, aim_conn_t *conn);
faim_export int aim_chat_attachname(aim_conn_t *conn, const char *roomname);
faim_export char *aim_chat_getname(aim_conn_t *conn);
faim_export aim_conn_t *aim_chat_getconn(aim_session_t *, const char *name);

faim_export int aim_chatnav_reqrights(aim_session_t *sess, aim_conn_t *conn);
faim_export int aim_chatnav_clientready(aim_session_t *sess, aim_conn_t *conn);

faim_export int aim_chat_invite(aim_session_t *sess, aim_conn_t *conn, const char *sn, const char *msg, fu16_t exchange, const char *roomname, fu16_t instance);

faim_export int aim_chatnav_createroom(aim_session_t *sess, aim_conn_t *conn, const char *name, fu16_t exchange);
faim_export int aim_chat_leaveroom(aim_session_t *sess, const char *name);

/* aim_util.c */
/*
 * These are really ugly.  You'd think this was LISP.  I wish it was.
 *
 * XXX With the advent of bstream's, these should be removed to enforce
 * their use.
 *
 */
#define aimutil_put8(buf, data) ((*(buf) = (u_char)(data)&0xff),1)
#define aimutil_get8(buf) ((*(buf))&0xff)
#define aimutil_put16(buf, data) ( \
		(*(buf) = (u_char)((data)>>8)&0xff), \
		(*((buf)+1) = (u_char)(data)&0xff),  \
		2)
#define aimutil_get16(buf) ((((*(buf))<<8)&0xff00) + ((*((buf)+1)) & 0xff))
#define aimutil_put32(buf, data) ( \
		(*((buf)) = (u_char)((data)>>24)&0xff), \
		(*((buf)+1) = (u_char)((data)>>16)&0xff), \
		(*((buf)+2) = (u_char)((data)>>8)&0xff), \
		(*((buf)+3) = (u_char)(data)&0xff), \
		4)
#define aimutil_get32(buf) ((((*(buf))<<24)&0xff000000) + \
		(((*((buf)+1))<<16)&0x00ff0000) + \
		(((*((buf)+2))<< 8)&0x0000ff00) + \
		(((*((buf)+3)    )&0x000000ff)))

faim_export int aimutil_putstr(u_char *, const char *, int);
faim_export int aimutil_tokslen(char *toSearch, int index, char dl);
faim_export int aimutil_itemcnt(char *toSearch, char dl);
faim_export char *aimutil_itemidx(char *toSearch, int index, char dl);

faim_export int aim_snlen(const char *sn);
faim_export int aim_sncmp(const char *sn1, const char *sn2);

/* for libc's that dont have it */
faim_export char *aim_strsep(char **pp, const char *delim);

/* aim_meta.c */
faim_export char *aim_getbuilddate(void);
faim_export char *aim_getbuildtime(void);
faim_export int aim_getbuildstring(char *buf, int buflen);

#include <aim_internal.h>

#endif /* __AIM_H__ */

