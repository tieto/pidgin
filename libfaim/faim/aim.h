/* 
 * Main libfaim header.  Must be included in client for prototypes/macros.
 *
 */

#ifndef __AIM_H__
#define __AIM_H__

#define FAIM_VERSION_MAJOR 0
#define FAIM_VERSION_MINOR 99
#define FAIM_VERSION_MINORMINOR 0

#include <faim/faimconfig.h>
#include <faim/aim_cbtypes.h>

#if !defined(FAIM_USEPTHREADS) && !defined(FAIM_USEFAKELOCKS)
#error pthreads or fakelocks are currently required.
#endif

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>

#ifdef FAIM_USEPTHREADS
#include <pthread.h>
#define faim_mutex_t pthread_mutex_t 
#define faim_mutex_init pthread_mutex_init
#define faim_mutex_lock pthread_mutex_lock
#define faim_mutex_unlock pthread_mutex_unlock
#elif defined(FAIM_USEFAKELOCKS)
/*
 * For platforms without pthreads, we also assume
 * we're not linking against a threaded app.  Which
 * means we don't have to do real locking.  The 
 * macros below do nothing really.  They're a joke.
 * But they get it to compile.
 */
#define faim_mutex_t char
#define faim_mutex_init(x, y) *x = 0
#define faim_mutex_lock(x) *x = 1;
#define faim_mutex_unlock(x) *x = 0;
#endif

#ifdef _WIN32
#include <windows.h>
#include <time.h>
#include <io.h>
#else
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <unistd.h>
#endif

/* Portability stuff (DMP) */

#ifdef _WIN32
#define sleep Sleep
#define strlen(x) (int)strlen(x)  /* win32 has a unsigned size_t */
#endif

#if defined(mach) && defined(__APPLE__)
#define gethostbyname(x) gethostbyname2(x, AF_INET) 
#endif

#if !defined(MSG_WAITALL)
#warning FIX YOUR LIBC! MSG_WAITALL is required!
#define MSG_WAITALL 0x100
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
 */
#define MAXMSGLEN 7987

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

#if debug > 0
#define faimdprintf(l, x...) {if (l >= debug) printf(x); }
#else
#define faimdprintf(l, x...)
#endif

/*
 * Login info.  Passes information from the Authorization
 * stage of login to the service (BOS, etc) connection
 * phase.
 *
 */
struct aim_login_struct {
  char screen_name[MAXSNLEN+1];
  char *BOSIP;
  char cookie[AIM_COOKIELEN];
  char *email;
  u_short regstatus;
  char *errorurl;
  u_short errorcode;
};

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
#define AIM_CONN_TYPE_RENDEZVOUS    0x0101 /* these do not speak OSCAR! */

/*
 * Status values returned from aim_conn_new().  ORed together.
 */
#define AIM_CONN_STATUS_READY       0x0001
#define AIM_CONN_STATUS_INTERNALERR 0x0002
#define AIM_CONN_STATUS_RESOLVERR   0x0080
#define AIM_CONN_STATUS_CONNERR     0x0040

#define AIM_FRAMETYPE_OSCAR 0x0000
#define AIM_FRAMETYPE_OFT 0x0001

struct aim_conn_t {
  int fd;
  int type;
  int seqnum;
  int status;
  void *priv; /* misc data the client may want to store */
  time_t lastactivity; /* time of last transmit */
  int forcedlatency; 
  struct aim_rxcblist_t *handlerlist;
  faim_mutex_t active; /* lock around read/writes */
  faim_mutex_t seqnum_lock; /* lock around ->seqnum changes */
  struct aim_conn_t *next;
};

/* struct for incoming commands */
struct command_rx_struct {
  unsigned char hdrtype; /* defines which piece of the union to use */
  union {
    struct { 
      char type;        
      unsigned short seqnum;     
    } oscar;
    struct {
      unsigned short type;
      unsigned short hdr2len;
      unsigned char *hdr2; /* rest of bloated header */
    } oft;
  } hdr;
  unsigned short commandlen;         /* total payload length */
  unsigned char *data;             /* packet data (from 7 byte on) */
  unsigned char lock;               /* 0 = open, !0 = locked */
  unsigned char handled;            /* 0 = new, !0 = been handled */
  unsigned char nofree;		    /* 0 = free data on purge, 1 = only unlink */
  struct aim_conn_t *conn;  /* the connection it came in on... */
  struct command_rx_struct *next; /* ptr to next struct in list */
};

/* struct for outgoing commands */
struct command_tx_struct {
  unsigned char hdrtype; /* defines which piece of the union to use */
  union {
    struct {
      unsigned char type;
      unsigned short seqnum;
    } oscar;
    struct {
      unsigned short type;
      unsigned short hdr2len;
      unsigned char *hdr2;
    } oft;
  } hdr;
  u_int commandlen;         
  u_char *data;      
  u_int lock;               /* 0 = open, !0 = locked */
  u_int sent;               /* 0 = pending, !0 = has been sent */
  struct aim_conn_t *conn; 
  struct command_tx_struct *next; /* ptr to next struct in list */
};


/*
 * AIM Session: The main client-data interface.  
 *
 */
struct aim_session_t {

  /* ---- Client Accessible ------------------------ */
  /* 
   * Login information.  See definition above.
   *
   */
  struct aim_login_struct logininfo;
  
  /*
   * Pointer to anything the client wants to 
   * explicitly associate with this session.
   */
  void *aux_data;


  /* ---- Internal Use Only ------------------------ */
  /* 
   * Connection information
   */
  struct aim_conn_t *connlist;
  faim_mutex_t connlistlock;
  
  /* 
   * TX/RX queues 
   */
  struct command_tx_struct *queue_outgoing;   
  struct command_rx_struct *queue_incoming; 
  
  /*
   * Tx Enqueuing function
   */
  int (*tx_enqueue)(struct aim_session_t *, struct command_tx_struct *);

  /*
   * This is a dreadful solution to the what-room-are-we-joining
   * problem.  (There's no connection between the service
   * request and the resulting redirect.)
   */ 
  char *pendingjoin;

  /*
   * Outstanding snac handling 
   *
   * XXX: Should these be per-connection? -mid
   **/
  struct aim_snac_t *outstanding_snacs;
  u_long snac_nextid;

  struct aim_msgcookie_t *msgcookies;
};


/*
 * AIM User Info, Standard Form.
 */
struct aim_userinfo_s {
  char sn[MAXSNLEN+1];
  u_short warnlevel;
  u_short idletime;
  u_short class;
  u_long membersince;
  u_long onlinesince;
  u_long sessionlen;  
  u_short capabilities;
};

#define AIM_CLASS_TRIAL 	0x0001
#define AIM_CLASS_UNKNOWN2	0x0002
#define AIM_CLASS_AOL		0x0004
#define AIM_CLASS_UNKNOWN4	0x0008
#define AIM_CLASS_FREE 		0x0010
#define AIM_CLASS_AWAY		0x0020
#define AIM_CLASS_UNKNOWN40	0x0040
#define AIM_CLASS_UNKNOWN80	0x0080

/*
 * TLV handling
 */

/* Generic TLV structure. */
struct aim_tlv_t {
  u_short type;
  u_short length;
  u_char *value;
};

/* List of above. */
struct aim_tlvlist_t {
  struct aim_tlv_t *tlv;
  struct aim_tlvlist_t *next;
};

/* TLV-handling functions */
struct aim_tlvlist_t *aim_readtlvchain(u_char *buf, int maxlen);
void aim_freetlvchain(struct aim_tlvlist_t **list);
struct aim_tlv_t *aim_grabtlv(u_char *src);
struct aim_tlv_t *aim_grabtlvstr(u_char *src);
struct aim_tlv_t *aim_gettlv(struct aim_tlvlist_t *, u_short, int);
char *aim_gettlv_str(struct aim_tlvlist_t *, u_short, int);
int aim_puttlv (u_char *dest, struct aim_tlv_t *newtlv);
struct aim_tlv_t *aim_createtlv(void);
int aim_freetlv(struct aim_tlv_t **oldtlv);
int aim_puttlv_16(u_char *, u_short, u_short);
int aim_puttlv_32(u_char *, u_short, u_long);
int aim_puttlv_str(u_char *buf, u_short t, u_short l, u_char *v);
int aim_writetlvchain(u_char *buf, int buflen, struct aim_tlvlist_t **list);
int aim_addtlvtochain16(struct aim_tlvlist_t **list, unsigned short type, unsigned short val);
int aim_addtlvtochain32(struct aim_tlvlist_t **list, unsigned short type, unsigned long val);
int aim_addtlvtochain_str(struct aim_tlvlist_t **list, unsigned short type, char *str, int len);
int aim_counttlvchain(struct aim_tlvlist_t **list);

/*
 * Get command from connections / Dispatch commands
 * already in queue.
 */
int aim_get_command(struct aim_session_t *, struct aim_conn_t *);
int aim_rxdispatch(struct aim_session_t *);
u_long aim_debugconn_sendconnect(struct aim_session_t *sess,
				 struct aim_conn_t *conn);
int aim_logoff(struct aim_session_t *);

void aim_conn_kill(struct aim_session_t *sess, struct aim_conn_t **deadconn);

typedef int (*rxcallback_t)(struct aim_session_t *, struct command_rx_struct *, ...);
int aim_register_callbacks(rxcallback_t *);

u_long aim_genericreq_n(struct aim_session_t *, struct aim_conn_t *conn, u_short family, u_short subtype);
u_long aim_genericreq_l(struct aim_session_t *, struct aim_conn_t *conn, u_short family, u_short subtype, u_long *);
u_long aim_genericreq_s(struct aim_session_t *, struct aim_conn_t *conn, u_short family, u_short subtype, u_short *);

/* aim_login.c */
int aim_sendconnack(struct aim_session_t *sess, struct aim_conn_t *conn);
int aim_request_login (struct aim_session_t *sess, struct aim_conn_t *conn, char *sn);
int aim_send_login (struct aim_session_t *, struct aim_conn_t *, char *, char *, struct client_info_s *);
int aim_encode_password(const char *, u_char *);
int aimicq_encode_password(const char *password, u_char *encoded);
unsigned long aim_sendauthresp(struct aim_session_t *sess, 
			       struct aim_conn_t *conn, 
			       char *sn, char *bosip, 
			       char *cookie, char *email, 
			       int regstatus);
int aim_gencookie(unsigned char *buf);
int aim_sendserverready(struct aim_session_t *sess, struct aim_conn_t *conn);
unsigned long aim_sendredirect(struct aim_session_t *sess, 
			       struct aim_conn_t *conn, 
			       unsigned short servid, 
			       char *ip,
			       char *cookie);
void aim_purge_rxqueue(struct aim_session_t *);
void aim_rxqueue_cleanbyconn(struct aim_session_t *sess, struct aim_conn_t *conn);

int aim_parse_unknown(struct aim_session_t *, struct command_rx_struct *command, ...);
int aim_parse_missed_im(struct aim_session_t *, struct command_rx_struct *, ...);
int aim_parse_last_bad(struct aim_session_t *, struct command_rx_struct *, ...);


struct command_tx_struct *aim_tx_new(unsigned short framing, int chan, struct aim_conn_t *conn, int datalen);
int aim_tx_enqueue__queuebased(struct aim_session_t *, struct command_tx_struct *);
int aim_tx_enqueue__immediate(struct aim_session_t *, struct command_tx_struct *);
#define aim_tx_enqueue(x, y) ((*(x->tx_enqueue))(x, y))
int aim_tx_sendframe(struct aim_session_t *sess, struct command_tx_struct *cur);
u_int aim_get_next_txseqnum(struct aim_conn_t *);
int aim_tx_flushqueue(struct aim_session_t *);
int aim_tx_printqueue(struct aim_session_t *);
void aim_tx_purgequeue(struct aim_session_t *);

struct aim_rxcblist_t {
  u_short family;
  u_short type;
  rxcallback_t handler;
  u_short flags;
  struct aim_rxcblist_t *next;
};

int aim_conn_setlatency(struct aim_conn_t *conn, int newval);

int aim_conn_addhandler(struct aim_session_t *, struct aim_conn_t *conn, u_short family, u_short type, rxcallback_t newhandler, u_short flags);
rxcallback_t aim_callhandler(struct aim_conn_t *conn, u_short family, u_short type);
int aim_clearhandlers(struct aim_conn_t *conn);

/*
 * Generic SNAC structure.  Rarely if ever used.
 */
struct aim_snac_t {
  u_long id;
  u_short family;
  u_short type;
  u_short flags;
  void *data;
  time_t issuetime;
  struct aim_snac_t *next;
};
u_long aim_newsnac(struct aim_session_t *, struct aim_snac_t *newsnac);
struct aim_snac_t *aim_remsnac(struct aim_session_t *, u_long id);
int aim_cleansnacs(struct aim_session_t *, int maxage);
int aim_putsnac(u_char *, int, int, int, u_long);


void aim_connrst(struct aim_session_t *);
struct aim_conn_t *aim_conn_getnext(struct aim_session_t *);
void aim_conn_close(struct aim_conn_t *deadconn);
struct aim_conn_t *aim_getconn_type(struct aim_session_t *, int type);
struct aim_conn_t *aim_newconn(struct aim_session_t *, int type, char *dest);
int aim_conngetmaxfd(struct aim_session_t *);
struct aim_conn_t *aim_select(struct aim_session_t *, struct timeval *, int *);
int aim_conn_isready(struct aim_conn_t *);
int aim_conn_setstatus(struct aim_conn_t *, int);
void aim_session_init(struct aim_session_t *);

/* aim_misc.c */

#define AIM_VISIBILITYCHANGE_PERMITADD    0x05
#define AIM_VISIBILITYCHANGE_PERMITREMOVE 0x06
#define AIM_VISIBILITYCHANGE_DENYADD      0x07
#define AIM_VISIBILITYCHANGE_DENYREMOVE   0x08

u_long aim_bos_nop(struct aim_session_t *, struct aim_conn_t *);
u_long aim_bos_setidle(struct aim_session_t *, struct aim_conn_t *, u_long);
u_long aim_bos_changevisibility(struct aim_session_t *, struct aim_conn_t *, int, char *);
u_long aim_bos_setbuddylist(struct aim_session_t *, struct aim_conn_t *, char *);
u_long aim_bos_setprofile(struct aim_session_t *, struct aim_conn_t *, char *, char *, unsigned int);
u_long aim_bos_setgroupperm(struct aim_session_t *, struct aim_conn_t *, u_long);
u_long aim_bos_clientready(struct aim_session_t *, struct aim_conn_t *);
u_long aim_bos_reqrate(struct aim_session_t *, struct aim_conn_t *);
u_long aim_bos_ackrateresp(struct aim_session_t *, struct aim_conn_t *);
u_long aim_bos_setprivacyflags(struct aim_session_t *, struct aim_conn_t *, u_long);
u_long aim_bos_reqpersonalinfo(struct aim_session_t *, struct aim_conn_t *);
u_long aim_bos_reqservice(struct aim_session_t *, struct aim_conn_t *, u_short);
u_long aim_bos_reqrights(struct aim_session_t *, struct aim_conn_t *);
u_long aim_bos_reqbuddyrights(struct aim_session_t *, struct aim_conn_t *);
u_long aim_bos_reqlocaterights(struct aim_session_t *, struct aim_conn_t *);
u_long aim_bos_reqicbmparaminfo(struct aim_session_t *, struct aim_conn_t *);
u_long aim_setversions(struct aim_session_t *sess, struct aim_conn_t *conn);

/* aim_rxhandlers.c */
int aim_rxdispatch(struct aim_session_t *);
int aim_authparse(struct aim_session_t *, struct command_rx_struct *);
int aim_handleredirect_middle(struct aim_session_t *, struct command_rx_struct *, ...);
int aim_parse_unknown(struct aim_session_t *, struct command_rx_struct *, ...);
int aim_parse_last_bad(struct aim_session_t *, struct command_rx_struct *, ...);
int aim_parse_generalerrs(struct aim_session_t *, struct command_rx_struct *command, ...);
int aim_parsemotd_middle(struct aim_session_t *sess, struct command_rx_struct *command, ...);

/* aim_im.c */
#define AIM_IMFLAGS_AWAY 0x01 /* mark as an autoreply */
#define AIM_IMFLAGS_ACK  0x02 /* request a receipt notice */

u_long aim_send_im(struct aim_session_t *, struct aim_conn_t *, char *, u_int, char *);
int aim_parse_incoming_im_middle(struct aim_session_t *, struct command_rx_struct *);
int aim_parse_outgoing_im_middle(struct aim_session_t *, struct command_rx_struct *);
u_long aim_seticbmparam(struct aim_session_t *, struct aim_conn_t *conn);
int aim_parse_msgerror_middle(struct aim_session_t *, struct command_rx_struct *);
int aim_negchan_middle(struct aim_session_t *sess, struct command_rx_struct *command);

/* aim_info.c */
#define AIM_CAPS_BUDDYICON 0x01
#define AIM_CAPS_VOICE 0x02
#define AIM_CAPS_IMIMAGE 0x04
#define AIM_CAPS_CHAT 0x08
#define AIM_CAPS_GETFILE 0x10
#define AIM_CAPS_SENDFILE 0x20

extern u_char aim_caps[6][16];
u_short aim_getcap(unsigned char *capblock, int buflen);
int aim_putcap(unsigned char *capblock, int buflen, u_short caps);

#define AIM_GETINFO_GENERALINFO 0x00001
#define AIM_GETINFO_AWAYMESSAGE 0x00003

struct aim_msgcookie_t {
  unsigned char cookie[8];
  int type;
  void *data;
  time_t addtime;
  struct aim_msgcookie_t *next;
};

struct aim_filetransfer_t {
  char sender[MAXSNLEN];	
  char ip[30];
  char *filename;
};
int aim_cachecookie(struct aim_session_t *sess, struct aim_msgcookie_t *cookie);
struct aim_msgcookie_t *aim_uncachecookie(struct aim_session_t *sess, char *cookie);
int aim_purgecookies(struct aim_session_t *sess);

#define AIM_TRANSFER_DENY_NOTSUPPORTED 0x0000
#define AIM_TRANSFER_DENY_DECLINE 0x0001
#define AIM_TRANSFER_DENY_NOTACCEPTING 0x0002
u_long aim_denytransfer(struct aim_session_t *sess, struct aim_conn_t *conn, char *sender, char *cookie, unsigned short code);
u_long aim_accepttransfer(struct aim_session_t *sess, struct aim_conn_t *conn, char *sender, char *cookie, unsigned short rendid);

u_long aim_getinfo(struct aim_session_t *, struct aim_conn_t *, const char *, unsigned short);
int aim_extractuserinfo(u_char *, struct aim_userinfo_s *);
int aim_parse_userinfo_middle(struct aim_session_t *, struct command_rx_struct *);
int aim_parse_oncoming_middle(struct aim_session_t *, struct command_rx_struct *);
int aim_parse_offgoing_middle(struct aim_session_t *, struct command_rx_struct *);
int aim_putuserinfo(u_char *buf, int buflen, struct aim_userinfo_s *info);
int aim_sendbuddyoncoming(struct aim_session_t *sess, struct aim_conn_t *conn, struct aim_userinfo_s *info);
int aim_sendbuddyoffgoing(struct aim_session_t *sess, struct aim_conn_t *conn, char *sn);


/* aim_auth.c */
int aim_auth_sendcookie(struct aim_session_t *, struct aim_conn_t *, u_char *);
u_long aim_auth_clientready(struct aim_session_t *, struct aim_conn_t *);
u_long aim_auth_changepasswd(struct aim_session_t *, struct aim_conn_t *, char *, char *);

/* aim_buddylist.c */
u_long aim_add_buddy(struct aim_session_t *, struct aim_conn_t *, char *);
u_long aim_remove_buddy(struct aim_session_t *, struct aim_conn_t *, char *);

/* aim_search.c */
u_long aim_usersearch_address(struct aim_session_t *, struct aim_conn_t *, char *);
/* u_long aim_usersearch_name(struct aim_session_t *, struct aim_conn_t *, char *); */


struct aim_chat_roominfo {
  u_short exchange;
  char *name;
  u_short instance;
};
int aim_chat_readroominfo(u_char *buf, struct aim_chat_roominfo *outinfo);
int aim_chat_parse_infoupdate(struct aim_session_t *sess, struct command_rx_struct *command);
int aim_chat_parse_joined(struct aim_session_t *sess, struct command_rx_struct *command);
int aim_chat_parse_leave(struct aim_session_t *sess, struct command_rx_struct *command);
int aim_chat_parse_incoming(struct aim_session_t *sess, struct command_rx_struct *command);
u_long aim_chat_send_im(struct aim_session_t *sess, struct aim_conn_t *conn, char *msg);
u_long aim_chat_join(struct aim_session_t *sess, struct aim_conn_t *conn, u_short exchange, const char *roomname);
u_long aim_chat_clientready(struct aim_session_t *sess, struct aim_conn_t *conn);
int aim_chat_attachname(struct aim_conn_t *conn, char *roomname);
char *aim_chat_getname(struct aim_conn_t *conn);
struct aim_conn_t *aim_chat_getconn(struct aim_session_t *, char *name);

u_long aim_chatnav_reqrights(struct aim_session_t *sess, struct aim_conn_t *conn);
u_long aim_chatnav_clientready(struct aim_session_t *sess, struct aim_conn_t *conn);

u_long aim_chat_invite(struct aim_session_t *sess, struct aim_conn_t *conn, char *sn, char *msg, u_short exchange, char *roomname, u_short instance);

struct aim_chat_exchangeinfo {
  u_short number;
  char *name;
  char *charset1;
  char *lang1;
  char *charset2;
  char *lang2;
};
int aim_chatnav_parse_info(struct aim_session_t *sess, struct command_rx_struct *command);
u_long aim_chatnav_createroom(struct aim_session_t *sess, struct aim_conn_t *conn, char *name, u_short exchange);
int aim_chat_leaveroom(struct aim_session_t *sess, char *name);

/* aim_util.c */
#ifdef AIMUTIL_USEMACROS
/*
 * These are really ugly.  You'd think this was LISP.  I wish it was.
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
#else
#warning Not using aimutil macros.  May have performance problems.
int aimutil_put8(u_char *, u_char);
u_char aimutil_get8(u_char *buf);
int aimutil_put16(u_char *, u_short);
u_short aimutil_get16(u_char *);
int aimutil_put32(u_char *, u_long);
u_long aimutil_get32(u_char *);
#endif

int aimutil_putstr(u_char *, const u_char *, int);
int aimutil_tokslen(char *toSearch, int index, char dl);
int aimutil_itemcnt(char *toSearch, char dl);
char *aimutil_itemidx(char *toSearch, int index, char dl);

int aim_snlen(const char *sn);
int aim_sncmp(const char *sn1, const char *sn2);

/* aim_meta.c */
char *aim_getbuilddate(void);
char *aim_getbuildtime(void);
char *aim_getbuildstring(void);

#endif /* __AIM_H__ */

