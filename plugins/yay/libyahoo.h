#ifndef LIBYAHOO_H
#define LIBYAHOO_H

/* Service constants */
#define YAHOO_SERVICE_LOGON		1
#define YAHOO_SERVICE_LOGOFF		2
#define YAHOO_SERVICE_ISAWAY		3
#define YAHOO_SERVICE_ISBACK		4
#define YAHOO_SERVICE_IDLE		5
#define YAHOO_SERVICE_MESSAGE		6
#define YAHOO_SERVICE_IDACT		7
#define YAHOO_SERVICE_IDDEACT		8
#define YAHOO_SERVICE_MAILSTAT	9
#define YAHOO_SERVICE_USERSTAT	10
#define YAHOO_SERVICE_NEWMAIL		11
#define YAHOO_SERVICE_CHATINVITE	12
#define YAHOO_SERVICE_CALENDAR	13
#define YAHOO_SERVICE_NEWPERSONALMAIL		14
#define YAHOO_SERVICE_NEWCONTACT	15
#define YAHOO_SERVICE_ADDIDENT	16
#define YAHOO_SERVICE_ADDIGNORE	17
#define YAHOO_SERVICE_PING		18
#define YAHOO_SERVICE_GROUPRENAME	19
#define YAHOO_SERVICE_SYSMESSAGE	20
#define YAHOO_SERVICE_PASSTHROUGH2	22
#define YAHOO_SERVICE_CONFINVITE 24
#define YAHOO_SERVICE_CONFLOGON	25
#define YAHOO_SERVICE_CONFDECLINE 26
#define YAHOO_SERVICE_CONFLOGOFF		27
#define YAHOO_SERVICE_CONFADDINVITE 28
#define YAHOO_SERVICE_CONFMSG 29
#define YAHOO_SERVICE_CHATLOGON	30
#define YAHOO_SERVICE_CHATLOGOFF	31
#define YAHOO_SERVICE_CHATMSG 32
#define YAHOO_SERVICE_GAMELOGON 40
#define YAHOO_SERVICE_GAMELOGOFF 41
#define YAHOO_SERVICE_FILETRANSFER 70

/* Yahoo style/color directives */
#define YAHOO_COLOR_BLACK "\033[30m"
#define YAHOO_COLOR_BLUE "\033[31m"
#define YAHOO_COLOR_LIGHTBLUE "\033[32m"
#define YAHOO_COLOR_GRAY "\033[33m"
#define YAHOO_COLOR_GREEN "\033[34m"
#define YAHOO_COLOR_PINK "\033[35m"
#define YAHOO_COLOR_PURPLE "\033[36m"
#define YAHOO_COLOR_ORANGE "\033[37m"
#define YAHOO_COLOR_RED "\033[38m"
#define YAHOO_COLOR_OLIVE "\033[39m"
#define YAHOO_STYLE_ITALICON "\033[2m"
#define YAHOO_STYLE_ITALICOFF "\033[x2m"
#define YAHOO_STYLE_BOLDON "\033[1m"
#define YAHOO_STYLE_BOLDOFF "\033[x1m"
#define YAHOO_STYLE_UNDERLINEON "\033[4m"
#define YAHOO_STYLE_UNDERLINEOFF "\033[x4m"
#define YAHOO_STYLE_URLON "\033[lm"
#define YAHOO_STYLE_URLOFF "\033[xlm"

/* Message flags */
#define YAHOO_MSGTYPE_ERROR 		-1	/* 0xFFFFFFFF */
#define YAHOO_MSGTYPE_NONE 		0	/* ok */
#define YAHOO_MSGTYPE_NORMAL 		1	/* notify */
#define YAHOO_MSGTYPE_BOUNCE 		2	/* not available */
#define YAHOO_MSGTYPE_STATUS	 	4	/* user away */
#define YAHOO_MSGTYPE_OFFLINE 		5
#define YAHOO_MSGTYPE_INVISIBLE 	12

#define YAHOO_MSGTYPE_KNOWN_USER 	1515563606	/* 0x5A55AA56 */
#define YAHOO_MSGTYPE_UNKNOWN_USER 	1515563605	/* 0x5A55AA55 */

#define YAHOO_CONF_LEVEL_0		0

/* Structure definitions */

enum phone { home = 0, work };

struct yahoo_address
{
	char *id;
	char *firstname;
	char *lastname;
	char *emailnickname;
	char *email;
	char *workphone;
	char *homephone;
	enum phone primary_phone;
	unsigned int entryid;
};

struct yahoo_context
{
	/* Input parameters from calling application */
	char *user;
	char *password;
	int connect_mode;			/* connection mode */
	int proxy_port;
	char *proxy_host;

	/* Semi-public */
	int sockfd;					/* pager server socket */

	/* IO buffer parameters */
	char *io_buf;				/* Buffer for storing incoming packets */
	int io_buf_curlen;
	int io_buf_maxlen;

	/* Cookie data */
	char *cookie;
	char *login_cookie;

	/* Buddy list parameters */
	struct yahoo_buddy **buddies;	/* list of groups and buddies */
	char **identities;			/* list of identities */
	char *login_id;				/* what id should be specified as the primary id */
	int mail;					/* I think this indicates if user has a yahoo mail id */

	/* Temporary to hold the magic id for outbound packets */
	unsigned int magic_id;
	unsigned int connection_id;
	unsigned int address_count;
	struct yahoo_address *addresses;
};

struct yahoo_options
{
	int connect_mode;

	char *proxy_host;
	int proxy_port;
};

struct yahoo_rawpacket
{
	char version[8];			/* 7 chars and trailing null */
	unsigned char len[4];		/* length - little endian */
	unsigned char service[4];	/* service - little endian */

/* 3 X 4bytes - host, port, ip_address */
/* not sure what diff is between host and ip addr */
	unsigned char connection_id[4];	/* connection number - little endian */
	unsigned char magic_id[4];	/* magic number used for http session */
	unsigned char unknown1[4];

	unsigned char msgtype[4];
	char nick1[36];
	char nick2[36];
	char content[1];			/* was zero, had problems with aix xlc */
};

/*
 * Structure for returning the status/flags/etc. of a particular id
 */
struct yahoo_idstatus
{
	char *id;
	int status;
	char *status_msg;
	char *connection_id;
	int in_pager;
	int in_chat;
	int in_game;				/* not sure */
};

/*
 * Structure for returning a buddy entry
 */
struct yahoo_buddy
{
	char *group;				/* member of what group */
	char *id;					/* the buddy's id */
};

/*
 * Generic packet type for returning from the parse routine
 * The fields in this packet are not all used and are defined
 * so that a single type of packet can be returned from the parse routine
 */

struct yahoo_packet
{
	/* Common info */
	int service;				/* Service type */
	int connection_id;			/* Connection ID */
	char *real_id;				/* What ID is logged on */
	char *active_id;			/* What ID is active */

	/* Flags for the unknown portion of the data */
	unsigned int magic_id;
	unsigned int unknown1;
	unsigned int msgtype;		/* flag for indicating/requesting msg type */

	/* Status flag, I think used at login */
	int flag;					/* Used at logon for success/alerts? */

	/* Status entries */
	int idstatus_count;
	struct yahoo_idstatus **idstatus;	/* Array of status entries for id's */

	/* Conferencing */
	char *conf_id;				/* id for the conference */
	char *conf_host;			/* who is hosting the conference */
	char *conf_user;			/* single username ( used in */
	/* declined conference/ */
	/* addinvite / message / */
	/* logon / logoff ) */

	char **conf_userlist;		/* user list */
	char *conf_inviter;			/* user who invited you */
	/* (conference addinvite) */

	char *conf_msg;				/* conference message */

	int conf_type;				/* text(0) or */
	/* voice(1) conference */

	/* Mail status */
	int mail_status;

	/* Calendar data */
	char *cal_url;
	int cal_type;
	char *cal_timestamp;
	char *cal_title;
	char *cal_description;

	/* Chat invite data */
	char *chat_invite_content;

	/* Received message */
	char *msg_id;				/* Originator of message */
	int msg_status;				/* Status update from the message */
	char *msg_timestamp;		/* Timestamp of offline message */
	char *msg;					/* Content of message */

	/* File transfer request */
	char *file_from;
	char *file_flag;
	char *file_url;
	char *file_description;
	int file_expires;

	/* Group names for renaming */
	char *group_old;			/* Old group name */
	char *group_new;			/* New group name */
};

/* Misc contants */
#define YAHOO_PACKET_HEADER_SIZE 104	/* size of a standard header */

/* Status codes */
struct yahoo_idlabel
{
	int id;
	char *label;
};

/* Constants for connect mode selection */
enum
{
	YAHOO_CONNECT_NORMAL, YAHOO_CONNECT_HTTP, YAHOO_CONNECT_HTTPPROXY,
	YAHOO_CONNECT_SOCKS4, YAHOO_CONNECT_SOCKS5
};

/* Constants for status codes */
enum
{
	YAHOO_STATUS_AVAILABLE,
	YAHOO_STATUS_BRB,
	YAHOO_STATUS_BUSY,
	YAHOO_STATUS_NOTATHOME,
	YAHOO_STATUS_NOTATDESK,
	YAHOO_STATUS_NOTINOFFICE,
	YAHOO_STATUS_ONPHONE,
	YAHOO_STATUS_ONVACATION,
	YAHOO_STATUS_OUTTOLUNCH,
	YAHOO_STATUS_STEPPEDOUT,
	YAHOO_STATUS_INVISIBLE = 12,
	YAHOO_STATUS_CUSTOM = 99,
	YAHOO_STATUS_IDLE = 999
};

/* Function prototypes */
#include "libyahoo-proto.h"
#endif
