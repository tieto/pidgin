#ifndef __AIM_H__
#define __AIM_H__

#include <faimconfig.h>

/* some global includes */
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>

#define CONNECT_SIG_LEN 10 /* not used anymore, hopefully */
#define LOGIN_RESP_LEN 512 /* should only be 334b but no segfault for us! */


/*
 * Error codes
 */
#define AIM_CONNECT_ERROR	-0x1
#define AIM_SIGNON_TOO_SOON	-0x4
#define AIM_SERVICE_FULL	-0x6f


struct login_phase1_struct {
  char *screen_name;
  char *BOSIP;
  char *cookie;
  char *email;
  ushort regstatus;
};

extern struct login_phase1_struct aim_logininfo;

struct client_info_s {
  char clientstring[100]; /* arbitrary number */
  int major;
  int minor;
  int build;
  char country[3];
  char lang[3];
};

struct connection_info_struct {
  unsigned int local_seq_num_origin; /* our first seq num */
  int local_command_count;

  unsigned int remote_seq_num_origin; /* oscar's first seqnum */
  int remote_command_count; /* command_count + seq_num_origin = cur_seq_num */

  char *sn; /* our screen name */

  int fd;                   /* socket descriptor */
};

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#define AIM_CONN_MAX 5 
/* these could be arbitrary, but its easier to use the actual AIM values */
#define AIM_CONN_TYPE_AUTH 0x0007
#define AIM_CONN_TYPE_ADS 0x0005
#define AIM_CONN_TYPE_BOS 2
#define AIM_CONN_TYPE_CHAT 0x000e
#define AIM_CONN_TYPE_CHATNAV 0x000d

#define AIM_CONN_STATUS_READY 0x0001
#define AIM_CONN_STATUS_INTERNALERR 0x0002
#define AIM_CONN_STATUS_RESOLVERR 0x80
#define AIM_CONN_STATUS_CONNERR 0x40


struct aim_conn_t {
  int fd;
  int type;
  int seqnum;
  int status;
};
struct aim_conn_t aim_conns[AIM_CONN_MAX];


/* struct for incoming commands */
struct command_rx_struct {
                            /* byte 1 assumed to always be 0x2a */
  char type;                /* type code (byte 2) */
  unsigned int seqnum;      /* sequence number (bytes 3 and 4) */
  unsigned int commandlen;  /* total packet len - 6 (bytes 5 and 6) */
  char *data;               /* packet data (from 7 byte on) */
  unsigned int lock;        /* 1 = locked, 0 = open */
  unsigned int handled;     /* 1 = been handled, 0 = new */
  struct aim_conn_t *conn;  /* the connection it came in on... */
  struct command_rx_struct *next; /* ptr to next struct in list */
};

/* struct for outgoing commands */
struct command_tx_struct {
                            /* byte 1 assumed to be 0x2a */
  char type;                /* type/family code */
  unsigned int seqnum;      /* seqnum dynamically assigned on tx */
  unsigned int commandlen;  /* SNAC length */
  char *data;               /* packet data */
  unsigned int lock;        /* 1 = locked, 0 = open */
  unsigned int sent;        /* 1 = has been sent, 0 = new */
  struct aim_conn_t *conn; 
  struct command_tx_struct *next; /* ptr to next struct in list */
};

/* TLV-related tidbits */
struct aim_tlv_t {
  u_short type;
  u_short length;
  u_char *value;
};

struct aim_tlv_t *aim_grabtlv(u_char *src);
struct aim_tlv_t *aim_grabtlvstr(u_char *src);
int aim_puttlv (u_char *dest, struct aim_tlv_t *newtlv);
struct aim_tlv_t *aim_createtlv(void);
int aim_freetlv(struct aim_tlv_t **oldtlv);
int aim_puttlv_16(u_char *, u_short, u_short);

/* some prototypes... */

/*   implicitly or explicitly called */
int aim_get_command(void);
int aim_rxdispatch(void);
int aim_logoff(void);

typedef int (*rxcallback_t)(struct command_rx_struct *, ...);
int aim_register_callbacks(rxcallback_t *);

u_long aim_genericreq_n(struct aim_conn_t *conn, u_short family, u_short subtype);
u_long aim_genericreq_l(struct aim_conn_t *conn, u_short family, u_short subtype, u_long *);
u_long aim_genericreq_s(struct aim_conn_t *conn, u_short family, u_short subtype, u_short *);

/* aim_login.c */
int aim_send_login (struct aim_conn_t *, char *, char *, struct client_info_s *);
int aim_encode_password(const char *, char *);


struct command_rx_struct *aim_purge_rxqueue(struct command_rx_struct *queue);


int aim_parse_unknown(struct command_rx_struct *command, ...);
int aim_parse_missed_im(struct command_rx_struct *, ...);
int aim_parse_last_bad(struct command_rx_struct *, ...);

int aim_tx_enqueue(struct command_tx_struct *);
unsigned int aim_get_next_txseqnum(struct aim_conn_t *);
int aim_tx_flushqueue(void);
int aim_tx_printqueue(void);
int aim_tx_purgequeue(void);

/* queue (linked list) pointers */
extern struct command_tx_struct *aim_queue_outgoing; /* incoming commands */
extern struct command_rx_struct *aim_queue_incoming; /* outgoing commands */

/* The default callback handler array */
extern rxcallback_t aim_callbacks[];

extern struct aim_snac_t *aim_outstanding_snacs;
extern u_long aim_snac_nextid;

#define AIM_CB_INCOMING_IM 0
#define AIM_CB_ONCOMING_BUDDY 1
#define AIM_CB_OFFGOING_BUDDY 2
#define AIM_CB_MISSED_IM 3
#define AIM_CB_MISSED_CALL 4
#define AIM_CB_LOGIN_P4_C1 5
#define AIM_CB_LOGIN_P4_C2 6
#define AIM_CB_LOGIN_P2_1 7
#define AIM_CB_LOGIN_P2_2 8
#define AIM_CB_LOGIN_P3_B 9
#define AIM_CB_LOGIN_P3D_A 10
#define AIM_CB_LOGIN_P3D_B 11
#define AIM_CB_LOGIN_P3D_C 12
#define AIM_CB_LOGIN_P3D_D 13
#define AIM_CB_LOGIN_P3D_E 14
#define AIM_CB_LOGIN_P3D_F 15
#define AIM_CB_RATECHANGE 16
#define AIM_CB_USERERROR 17
#define AIM_CB_UNKNOWN 18
#define AIM_CB_USERINFO 19
#define AIM_CB_SEARCH_ADDRESS 20
#define AIM_CB_SEARCH_NAME 21
#define AIM_CB_SEARCH_FAIL 22
#define AIM_CB_AUTH_ERROR 23
#define AIM_CB_AUTH_SUCCESS 24
#define AIM_CB_AUTH_SVRREADY 25
#define AIM_CB_AUTH_OTHER 26
#define AIM_CB_AUTH_INFOCHNG_REPLY 27
#define AIM_CB_CHATNAV_SVRREADY 28

int Read(int, u_char *, int);

struct aim_snac_t {
  u_long id;
  u_short family;
  u_short type;
  u_short flags;
  void *data;
  time_t issuetime;
  struct aim_snac_t *next;
};
u_long aim_newsnac(struct aim_snac_t *newsnac);
struct aim_snac_t *aim_remsnac(u_long id);
int aim_cleansnacs(int maxage);
int aim_putsnac(u_char *, int, int, int, u_long);

void aim_connrst(void);
struct aim_conn_t *aim_conn_getnext(void);
void aim_conn_close(struct aim_conn_t *deadconn);
struct aim_conn_t *aim_getconn_type(int type);
struct aim_conn_t *aim_newconn(int type, char *dest);
int aim_conngetmaxfd(void);
struct aim_conn_t *aim_select(struct timeval *);
int aim_conn_isready(struct aim_conn_t *);
int aim_conn_setstatus(struct aim_conn_t *, int);

/* aim_misc.c */

#define AIM_VISIBILITYCHANGE_PERMITADD 0x05
#define AIM_VISIBILITYCHANGE_PERMITREMOVE 0x06
#define AIM_VISIBILITYCHANGE_DENYADD 0x07
#define AIM_VISIBILITYCHANGE_DENYREMOVE 0x08

u_long aim_bos_setidle(struct aim_conn_t *, u_long);
u_long aim_bos_changevisibility(struct aim_conn_t *, int, char *);
u_long aim_bos_setbuddylist(struct aim_conn_t *, char *);
u_long aim_bos_setprofile(struct aim_conn_t *, char *);
u_long aim_bos_setgroupperm(struct aim_conn_t *, u_long);
u_long aim_bos_clientready(struct aim_conn_t *);
u_long aim_bos_reqrate(struct aim_conn_t *);
u_long aim_bos_ackrateresp(struct aim_conn_t *);
u_long aim_bos_setprivacyflags(struct aim_conn_t *, u_long);
u_long aim_bos_reqpersonalinfo(struct aim_conn_t *);
u_long aim_bos_reqservice(struct aim_conn_t *, u_short);
u_long aim_bos_reqrights(struct aim_conn_t *);
u_long aim_bos_reqbuddyrights(struct aim_conn_t *);
u_long aim_bos_reqlocaterights(struct aim_conn_t *);
u_long aim_bos_reqicbmparaminfo(struct aim_conn_t *);

/* aim_rxhandlers.c */
int aim_register_callbacks(rxcallback_t *);
int aim_rxdispatch(void);
int aim_authparse(struct command_rx_struct *);
int aim_handleredirect_middle(struct command_rx_struct *, ...);
int aim_parse_unknown(struct command_rx_struct *, ...);
int aim_parse_missed_im(struct command_rx_struct *, ...);
int aim_parse_last_bad(struct command_rx_struct *, ...);
int aim_parse_generalerrs(struct command_rx_struct *command, ...);

/* aim_im.c */
#define AIM_IMFLAGS_AWAY 0x01 /* mark as an autoreply */
#define AIM_IMFLAGS_ACK 0x02 /* request a receipt notice */
u_long aim_send_im(struct aim_conn_t *, char *, int, char *);
int aim_parse_incoming_im_middle(struct command_rx_struct *);

/* aim_info.c */
u_long aim_getinfo(struct aim_conn_t *, const char *);
int aim_parse_userinfo_middle(struct command_rx_struct *);

/* aim_auth.c */
int aim_auth_sendcookie(struct aim_conn_t *, char *);
u_long aim_auth_clientready(struct aim_conn_t *);
u_long aim_auth_changepasswd(struct aim_conn_t *, char *, char *);

/* aim_buddylist.c */
u_long aim_add_buddy(struct aim_conn_t *, char *);
u_long aim_remove_buddy(struct aim_conn_t *, char *);

/* aim_search.c */
u_long aim_usersearch_address(struct aim_conn_t *, char *);
/* u_long aim_usersearch_name(struct aim_conn_t *, char *); */

/* aim_util.c */
int aimutil_put8(u_char *, u_short);
int aimutil_put16(u_char *, u_short);
int aimutil_put32(u_char *, u_long);
int aimutil_putstr(u_char *, u_char *, int);

/* proxy support */
#ifdef ENABLE_PROXY_SUPPORT
#include "proxy.h"
#endif

#endif /* __AIM_H__ */

