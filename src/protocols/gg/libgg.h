/* $Id: libgg.h 2406 2001-09-29 23:06:30Z warmenhoven $ */

/*
 *  (C) Copyright 2001 Wojtek Kaniewski <wojtekka@irc.pl>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __LIBGG_H
#define __LIBGG_H

#ifdef __cplusplus
extern "C" {
#endif

#define GG_DEBUG 1

/*
 * typ zmiennej okre¶laj±cej numerek danej osoby.
 */
typedef unsigned int uin_t;

/*
 * co¶tam.
 */
struct gg_session {
	int state, check;
	int fd, pid;
	int port;
	int seq, async;

	uin_t uin;
	char *password;

	char *recv_buf;
	int recv_done, recv_left;
};

/*
 * ró¿ne stany asynchronicznej maszynki.
 */
enum {
	GG_STATE_IDLE = 0,		/* wspólne */
	GG_STATE_RESOLVING,
	GG_STATE_CONNECTING_HTTP,

	GG_STATE_WRITING_HTTP,		/* gg_login */	
	GG_STATE_CONNECTING_GG,
	GG_STATE_WAITING_FOR_KEY,
	GG_STATE_SENDING_KEY,
	GG_STATE_CONNECTED,

	GG_STATE_READING_HEADER,	/* gg_search */
	GG_STATE_READING_DATA,
	GG_STATE_PARSING,
	GG_STATE_FINISHED,
};

/*
 * co proces klienta powinien sprawdzaæ w deskryptorach?
 */
enum {
	GG_CHECK_NONE = 0,
	GG_CHECK_WRITE = 1,
	GG_CHECK_READ = 2,
};

struct gg_session *gg_login(uin_t uin, char *password, int async);
void gg_free_session(struct gg_session *sess);
void gg_logoff(struct gg_session *sess);
int gg_change_status(struct gg_session *sess, int status);
int gg_send_message(struct gg_session *sess, int msgclass, uin_t recipient, unsigned char *message);
int gg_ping(struct gg_session *sess);

struct gg_notify_reply {
	uin_t uin;		/* numerek */
	int status;		/* status danej osoby */
	int remote_ip;		/* adres ip delikwenta */
	short remote_port;	/* port, na którym s³ucha klient */
	int dunno1;		/* == 0x0b */
	short dunno2;		/* znowu port? */
} __attribute__ ((packed));

struct gg_status {
	uin_t uin;		/* numerek */
	int status;		/* nowy stan */
} __attribute__ ((packed));

enum {
	GG_EVENT_NONE = 0,
	GG_EVENT_MSG,
	GG_EVENT_NOTIFY,
	GG_EVENT_STATUS,
	GG_EVENT_ACK,
	GG_EVENT_CONN_FAILED,
	GG_EVENT_CONN_SUCCESS,
};

enum {
	GG_FAILURE_RESOLVING = 1,
	GG_FAILURE_CONNECTING,
	GG_FAILURE_INVALID,
	GG_FAILURE_READING,
	GG_FAILURE_WRITING,
	GG_FAILURE_PASSWORD,
};

struct gg_event {
        int type;
        union {
                struct {
                        uin_t sender;
			int msgclass;
                        unsigned char *message;
                } msg;
                struct gg_notify_reply *notify;
                struct gg_status status;
                struct {
                        uin_t recipient;
                        int status;
                        int seq;
                } ack;
		int failure;
        } event;
};

struct gg_event *gg_watch_fd(struct gg_session *sess);
void gg_free_event(struct gg_event *e);

int gg_notify(struct gg_session *sess, uin_t *userlist, int count);
int gg_add_notify(struct gg_session *sess, uin_t uin);
int gg_remove_notify(struct gg_session *sess, uin_t uin);

/*
 * jakie¶tam bzdurki dotycz±ce szukania userów.
 */

struct gg_search_result {
	uin_t uin;
	char *first_name;
	char *last_name;
	char *nickname;
	int born;
	int gender;
	char *city;
	int active;
};

struct gg_search_request {
	/* czy ma szukaæ tylko aktywnych? */
	int active;
	/* mode 0 */
	char *nickname, *first_name, *last_name, *city;
	int gender, min_birth, max_birth;
	/* mode 1 */
	char *email;
	/* mode 2 */
	char *phone;
	/* mode 3 */
	uin_t uin;
};

struct gg_search {
	struct gg_search_request request;

	/* bzdurki */
	int mode, fd, async, state, check, error, pid;
	char *header_buf, *data_buf;
	int header_size, data_size;

	/* wyniki */
	int count;
	struct gg_search_result *results;
};

#define GG_GENDER_NONE 0
#define GG_GENDER_FEMALE 1
#define GG_GENDER_MALE 2

struct gg_search *gg_search(struct gg_search_request *r, int async);
int gg_search_watch_fd(struct gg_search *f);
void gg_free_search(struct gg_search *f);
void gg_search_cancel(struct gg_search *f);

/*
 * je¶li chcemy sobie podebugowaæ, wystarczy zdefiniowaæ GG_DEBUG.
 */

int gg_debug_level;

#ifdef GG_DEBUG

#	define GG_DEBUG_NET 1
#	define GG_DEBUG_TRAFFIC 2
#	define GG_DEBUG_DUMP 4
#	define GG_DEBUG_FUNCTION 8
#	define GG_DEBUG_MISC 16

	void gg_debug_real(int level, char *format, ...);
#	define gg_debug(x, y...) gg_debug_real(x, y)

#else

#	define gg_debug(x, y...) while(0) { };

#endif /* GG_DEBUG */

/*
 * -------------------------------------------------------------------------
 * poni¿ej znajduj± siê wewnêtrzne sprawy biblioteki. zwyk³y klient nie
 * powinien ich w ogóle ruszaæ, bo i nie ma po co. wszystko mo¿na za³atwiæ
 * procedurami wy¿szego poziomu, których definicje znajduj± siê na pocz±tku
 * tego pliku.
 * -------------------------------------------------------------------------
 */

int gg_resolve(int *fd, int *pid, char *hostname);
int gg_connect(void *addr, int port, int async);
char *gg_alloc_sprintf(char *format, ...);

#define GG_APPMSG_HOST "appmsg.gadu-gadu.pl"
#define GG_APPMSG_PORT 80
#define GG_PUBDIR_HOST "pubdir.gadu-gadu.pl"
#define GG_PUBDIR_PORT 80
#define GG_DEFAULT_PORT 8074

struct gg_header {
	int type;		/* typ pakietu */
	int length;		/* d³ugo¶æ reszty pakietu */
} __attribute__ ((packed));

#define GG_WELCOME 0x0001

struct gg_welcome {
	int key;		/* klucz szyfrowania has³a */
} __attribute__ ((packed));
	
#define GG_LOGIN 0x000c

struct gg_login {
	uin_t uin;		/* twój numerek */
	int hash;		/* hash has³a */
	int status;		/* status na dzieñ dobry */
	int dunno;		/* == 0x0b */
	int local_ip;		/* mój adres ip */
	short local_port;	/* port, na którym s³ucham */
} __attribute__ ((packed));

#define GG_LOGIN_OK 0x0003

#define GG_LOGIN_FAILED 0x0009

#define GG_NEW_STATUS 0x0002

#define GG_STATUS_NOT_AVAIL 0x0001	/* roz³±czony */
#define GG_STATUS_AVAIL 0x0002		/* dostêpny */
#define GG_STATUS_BUSY 0x0003		/* zajêty */
#define GG_STATUS_INVISIBLE 0x0014	/* niewidoczny (GG 4.6) */

#define GG_STATUS_FRIENDS_MASK 0x8000	/* tylko dla znajomych (GG 4.6) */

struct gg_new_status {
	int status;			/* na jaki zmieniæ? */
} __attribute__ ((packed));

#define GG_NOTIFY 0x0010
	
struct gg_notify {
	uin_t uin;		/* numerek danej osoby */
	char dunno1;		/* == 3 */
} __attribute__ ((packed));
	
#define GG_NOTIFY_REPLY 0x000c	/* tak, to samo co GG_LOGIN */
	
/* struct gg_notify_reply zadeklarowane wy¿ej */

#define GG_ADD_NOTIFY 0x000d
#define GG_REMOVE_NOTIFY 0x000e
	
struct gg_add_remove {
	uin_t uin;		/* numerek */
	char dunno1;		/* == 3 */
} __attribute__ ((packed));

#define GG_STATUS 0x0002

/* struct gg_status zadeklarowane wcze¶niej */
	
#define GG_SEND_MSG 0x000b

#define GG_CLASS_MSG 0x0004
#define GG_CLASS_CHAT 0x0008

struct gg_send_msg {
	int recipient;
	int seq;
	int msgclass;
} __attribute__ ((packed));

#define GG_SEND_MSG_ACK 0x0005

#define GG_ACK_DELIVERED 0x0002
#define GG_ACK_QUEUED 0x0003
	
struct gg_send_msg_ack {
	int status;
	int recipient;
	int seq;
} __attribute__ ((packed));

#define GG_RECV_MSG 0x000a
	
struct gg_recv_msg {
	int sender;
	int dunno1;
	int dunno2;
	int msgclass;
} __attribute__ ((packed));

#define GG_PING 0x0008
	
#define GG_PONG 0x0007

#ifdef __cplusplus
}
#endif

#endif
