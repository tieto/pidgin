/* $Id: libgg.c 2805 2001-11-26 21:22:56Z warmenhoven $ */

/*
 *  (C) Copyright 2001 Wojtek Kaniewski <wojtekka@irc.pl>,
 *                     Robert J. Wo¼ny <speedy@atman.pl>
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <netdb.h>
#include <errno.h>
#ifndef _AIX
#  include <string.h>
#endif
#include <stdarg.h>
#include <pwd.h>
#include <time.h>
#ifdef sun
#include <sys/filio.h>
#endif
#include <glib.h>
#if G_BYTE_ORDER == G_BIG_ENDIAN
#  define WORDS_BIGENDIAN 1
#endif
#include "libgg.h"
#include "config.h"

int gg_debug_level = 0;

#ifndef lint

static char rcsid[]
#ifdef __GNUC__
    __attribute__ ((unused))
#endif
    = "$Id: libgg.c 2805 2001-11-26 21:22:56Z warmenhoven $";

#endif

/*
 * gg_debug()
 *
 * wyrzuca komunikat o danym poziomie, o ile u¿ytkownik sobie tego ¿yczy.
 *
 *  - level - poziom wiadomo¶ci,
 *  - format... - tre¶æ wiadomo¶ci (printf-alike.)
 *
 * niczego nie zwraca.
 */
void gg_debug(int level, char *format, ...)
{
	va_list ap;

	if ((gg_debug_level & level)) {
		va_start(ap, format);
		vprintf(format, ap);
		va_end(ap);
	}
}

/*
 * fix32() // funkcja wewnêtrzna
 *
 * dla maszyn big-endianowych zamienia kolejno¶æ bajtów w ,,long''ach.
 */
static inline unsigned long fix32(unsigned long x)
{
#ifndef WORDS_BIGENDIAN
	return x;
#else
	return (unsigned long)
	    (((x & (unsigned long)0x000000ffU) << 24) |
	     ((x & (unsigned long)0x0000ff00U) << 8) |
	     ((x & (unsigned long)0x00ff0000U) >> 8) | ((x & (unsigned long)0xff000000U) >> 24));
#endif
}

/*
 * fix16() // funkcja wewnêtrzna
 *
 * dla maszyn big-endianowych zamienia kolejno¶æ bajtów w ,,short''ach.
 */
static inline unsigned short fix16(unsigned short x)
{
#ifndef WORDS_BIGENDIAN
	return x;
#else
	return (unsigned short)
	    (((x & (unsigned short)0x00ffU) << 8) | ((x & (unsigned short)0xff00U) >> 8));
#endif
}

/*
 * gg_alloc_sprintf() // funkcja wewnêtrzna
 *
 * robi dok³adnie to samo, co sprintf(), tyle ¿e alokuje sobie wcze¶niej
 * miejsce na dane. powinno dzia³aæ na tych maszynach, które maj± funkcjê
 * vsnprintf() zgodn± z C99, jak i na wcze¶niejszych.
 *
 *  - format, ... - parametry takie same jak w innych funkcjach *printf()
 *
 * zwraca zaalokowany buforek, który wypada³oby pó¼niej zwolniæ, lub NULL
 * je¶li nie uda³o siê wykonaæ zadania.
 */
char *gg_alloc_sprintf(char *format, ...)
{
	va_list ap;
	char *buf = NULL, *tmp;
	int size = 0, res;

	va_start(ap, format);

	if ((size = vsnprintf(buf, 0, format, ap)) < 1) {
		size = 128;
		do {
			size *= 2;
			if (!(tmp = realloc(buf, size))) {
				free(buf);
				return NULL;
			}
			buf = tmp;
			res = vsnprintf(buf, size, format, ap);
		} while (res == size - 1);
	} else {
		if (!(buf = malloc(size + 1)))
			return NULL;
	}

	vsnprintf(buf, size + 1, format, ap);

	va_end(ap);

	return buf;
}

/*
 * gg_get_line() // funkcja wewnêtrzna
 * 
 * podaje kolejn± liniê z bufora tekstowego. psuje co bezpowrotnie, dziel±c
 * na kolejne stringi. zdarza siê, nie ma potrzeby pisania funkcji dubluj±cej
 * bufor ¿eby tylko mieæ nieruszone dane wej¶ciowe, skoro i tak nie bêd± nam
 * po¼niej potrzebne. obcina `\r\n'.
 * 
 *  - ptr - wska¼nik do zmiennej, która przechowuje aktualn± pozycjê
 *    w przemiatanym buforze.
 * 
 * wska¼nik do kolejnej linii tekstu lub NULL, je¶li to ju¿ koniec bufora.
 */
char *gg_get_line(char **ptr)
{
	char *foo, *res;

	if (!ptr || !*ptr || !strcmp(*ptr, ""))
		return NULL;

	res = *ptr;

	if (!(foo = strchr(*ptr, '\n')))
		*ptr += strlen(*ptr);
	else {
		*ptr = foo + 1;
		*foo = 0;
		if (res[strlen(res) - 1] == '\r')
			res[strlen(res) - 1] = 0;
	}

	return res;
}

/*
 * gg_resolve() // funkcja wewnêtrzna
 *
 * tworzy pipe'y, forkuje siê i w drugim procesie zaczyna resolvowaæ 
 * podanego hosta. zapisuje w sesji deskryptor pipe'u. je¶li co¶ tam
 * bêdzie gotowego, znaczy, ¿e mo¿na wczytaæ ,,struct in_addr''. je¶li
 * nie znajdzie, zwraca INADDR_NONE.
 *
 *  - fd - wska¼nik gdzie wrzuciæ deskryptor,
 *  - pid - gdzie wrzuciæ pid dzieciaka,
 *  - hostname - nazwa hosta do zresolvowania.
 *
 * zwraca 0 je¶li uda³o siê odpaliæ proces lub -1 w przypadku b³êdu.
 */
int gg_resolve(int *fd, int *pid, char *hostname)
{
	int pipes[2], res;
	struct in_addr a;

	gg_debug(GG_DEBUG_FUNCTION, "** gg_resolve(..., \"%s\");\n", hostname);

	if (!fd | !pid) {
		errno = EFAULT;
		return -1;
	}

	if (pipe(pipes) == -1)
		return -1;

	if ((res = fork()) == -1)
		return -1;

	if (!res) {
		if ((a.s_addr = inet_addr(hostname)) == INADDR_NONE) {
			struct hostent *he;

			if (!(he = gethostbyname(hostname)))
				a.s_addr = INADDR_NONE;
			else
				memcpy((char *)&a, he->h_addr, sizeof(a));
		}

		write(pipes[1], &a, sizeof(a));

		exit(0);
	}

	close(pipes[1]);

	*fd = pipes[0];
	*pid = res;

	return 0;
}

/*
 * gg_connect() // funkcja wewnêtrzna
 *
 * ³±czy siê z serwerem. pierwszy argument jest typu (void *), ¿eby nie
 * musieæ niczego inkludowaæ w libgg.h i nie psuæ jaki¶ g³upich zale¿no¶ci
 * na dziwnych systemach.
 *
 *  - addr - adres serwera (struct in_addr *),
 *  - port - port serwera,
 *  - async - ma byæ asynchroniczne po³±czenie?
 *
 * zwraca po³±czonego socketa lub -1 w przypadku b³êdu. zobacz errno.
 */
int gg_connect(void *addr, int port, int async)
{
	int sock, one = 1;
	struct sockaddr_in sin;
	struct in_addr *a = addr;

	gg_debug(GG_DEBUG_FUNCTION, "** gg_connect(%s, %d, %d);\n", inet_ntoa(*a), port, async);

	if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		gg_debug(GG_DEBUG_MISC, "-- socket() failed. errno = %d (%s)\n", errno, strerror(errno));
		return -1;
	}

	if (async) {
		if (ioctl(sock, FIONBIO, &one) == -1) {
			gg_debug(GG_DEBUG_MISC, "-- ioctl() failed. errno = %d (%s)\n", errno,
				 strerror(errno));
			return -1;
		}
	}

	sin.sin_port = htons(port);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = a->s_addr;

	if (connect(sock, (struct sockaddr *)&sin, sizeof(sin)) == -1) {
		if (errno && (!async || errno != EINPROGRESS)) {
			gg_debug(GG_DEBUG_MISC, "-- connect() failed. errno = %d (%s)\n", errno,
				 strerror(errno));
			return -1;
		}
		gg_debug(GG_DEBUG_MISC, "-- connect() in progress\n");
	}

	return sock;
}

/*
 * gg_read_line() // funkcja wewnêtrzna
 *
 * czyta jedn± liniê tekstu z socketa.
 *
 *  - sock - socket,
 *  - buf - wska¼nik bufora,
 *  - length - d³ugo¶æ bufora.
 *
 * olewa b³êdy. je¶li na jaki¶ trafi, potraktuje go jako koniec linii.
 */
static void gg_read_line(int sock, char *buf, int length)
{
	int ret;

	gg_debug(GG_DEBUG_FUNCTION, "** gg_read_line(...);\n");

	for (; length > 1; buf++, length--) {
		do {
			if ((ret = read(sock, buf, 1)) == -1 && errno != EINTR) {
				*buf = 0;
				return;
			}
		} while (ret == -1 && errno == EINTR);

		if (*buf == '\n') {
			buf++;
			break;
		}
	}

	*buf = 0;
	return;
}

/*
 * gg_recv_packet() // funkcja wewnêtrzna
 *
 * odbiera jeden pakiet gg i zwraca wska¼nik do niego. pamiêæ po nim
 * wypada³oby uwolniæ.
 *
 *  - sock - po³±czony socket.
 *
 * je¶li wyst±pi³ b³±d, zwraca NULL. reszta w errno.
 */
static void *gg_recv_packet(struct gg_session *sess)
{
	struct gg_header h;
	char *buf = NULL;
	int ret = 0, offset, size = 0;

	gg_debug(GG_DEBUG_FUNCTION, "** gg_recv_packet(...);\n");

	if (!sess) {
		errno = EFAULT;
		return NULL;
	}

	if (sess->recv_left < 1) {
		while (ret != sizeof(h)) {
			ret = read(sess->fd, &h, sizeof(h));
			gg_debug(GG_DEBUG_MISC, "-- header recv(..., %d) = %d\n", sizeof(h), ret);
			if (ret < sizeof(h)) {
				if (errno != EINTR) {
					gg_debug(GG_DEBUG_MISC, "-- errno = %d (%s)\n", errno,
						 strerror(errno));
					return NULL;
				}
			}
		}

		h.type = fix32(h.type);
		h.length = fix32(h.length);
	} else {
		memcpy(&h, sess->recv_buf, sizeof(h));
	}

	/* jakie¶ sensowne limity na rozmiar pakietu */
	if (h.length < 0 || h.length > 65535) {
		gg_debug(GG_DEBUG_MISC, "-- invalid packet length (%d)\n", h.length);
		errno = ERANGE;
		return NULL;
	}

	if (sess->recv_left > 0) {
		gg_debug(GG_DEBUG_MISC, "-- resuming last gg_recv_packet()\n");
		size = sess->recv_left;
		offset = sess->recv_done;
		buf = sess->recv_buf;
	} else {
		if (!(buf = malloc(sizeof(h) + h.length + 1))) {
			gg_debug(GG_DEBUG_MISC, "-- not enough memory\n");
			return NULL;
		}

		memcpy(buf, &h, sizeof(h));

		offset = 0;
		size = h.length;
	}

	while (size > 0) {
		ret = read(sess->fd, buf + sizeof(h) + offset, size);
		gg_debug(GG_DEBUG_MISC, "-- body recv(..., %d) = %d\n", size, ret);
		if (ret > -1 && ret <= size) {
			offset += ret;
			size -= ret;
		} else if (ret == -1) {
			gg_debug(GG_DEBUG_MISC, "-- errno = %d (%s)\n", errno, strerror(errno));
			if (errno == EAGAIN) {
				gg_debug(GG_DEBUG_MISC, "-- %d bytes received, %d left\n", offset, size);
				sess->recv_buf = buf;
				sess->recv_left = size;
				sess->recv_done = offset;
				return NULL;
			}
			if (errno != EINTR) {
//                              errno = EINVAL;
				free(buf);
				return NULL;
			}
		}
	}

	sess->recv_left = 0;

	if ((gg_debug_level & GG_DEBUG_DUMP)) {
		int i;

		gg_debug(GG_DEBUG_DUMP, ">> received packet (type=%.2x):", h.type);
		for (i = 0; i < sizeof(h) + h.length; i++)
			gg_debug(GG_DEBUG_DUMP, " %.2x", (unsigned char)buf[i]);
		gg_debug(GG_DEBUG_DUMP, "\n");
	}

	return buf;
}

/*
 * gg_send_packet() // funkcja wewnêtrzna
 *
 * konstruuje pakiet i wysy³a go w do serwera.
 *
 *  - sock - po³±czony socket,
 *  - type - typ pakietu,
 *  - packet - wska¼nik do struktury pakietu,
 *  - length - d³ugo¶æ struktury pakietu,
 *  - payload - dodatkowy tekst doklejany do pakietu (np. wiadomo¶æ),
 *  - payload_length - d³ugo¶æ dodatkowego tekstu.
 *
 * je¶li posz³o dobrze, zwraca 0. w przypadku b³êdu -1. je¶li errno=ENOMEM,
 * zabrak³o pamiêci. inaczej by³ b³±d przy wysy³aniu pakietu. dla errno=0
 * nie wys³ano ca³ego pakietu.
 */
static int gg_send_packet(int sock, int type, void *packet, int length, void *payload,
			  int payload_length)
{
	struct gg_header *h;
	int res, plen;
	char *tmp;

	gg_debug(GG_DEBUG_FUNCTION, "** gg_send_packet(0x%.2x, %d, %d);\n", type, length,
		 payload_length);

	if (length < 0 || payload_length < 0) {
		gg_debug(GG_DEBUG_MISC, "-- invalid packet/payload length\n");
		errno = ERANGE;
		return -1;
	}

	if (!(tmp = malloc(sizeof(struct gg_header) + length + payload_length))) {
		gg_debug(GG_DEBUG_MISC, "-- not enough memory\n");
		return -1;
	}

	h = (struct gg_header *)tmp;
	h->type = fix32(type);
	h->length = fix32(length + payload_length);

	if (packet)
		memcpy(tmp + sizeof(struct gg_header), packet, length);
	if (payload)
		memcpy(tmp + sizeof(struct gg_header) + length, payload, payload_length);

	if ((gg_debug_level & GG_DEBUG_DUMP)) {
		int i;

		gg_debug(GG_DEBUG_DUMP, "%%%% sending packet (type=%.2x):", fix32(h->type));
		for (i = 0; i < sizeof(struct gg_header) + fix32(h->length); i++)
			gg_debug(GG_DEBUG_DUMP, " %.2x", (unsigned char)tmp[i]);
		gg_debug(GG_DEBUG_DUMP, "\n");
	}

	plen = sizeof(struct gg_header) + length + payload_length;

	if ((res = write(sock, tmp, plen)) < plen) {
		gg_debug(GG_DEBUG_MISC, "-- write() failed. res = %d, errno = %d (%s)\n", res, errno,
			 strerror(errno));
		free(tmp);
		return -1;
	}

	free(tmp);
	return 0;
}


/*
 * gg_login()
 *
 * rozpoczyna procedurê ³±czenia siê z serwerem. resztê obs³guje siê przez
 * gg_watch_event.
 *
 *  - uin - numerek usera,
 *  - password - jego hase³ko,
 *  - async - ma byæ asynchronicznie?
 *
 * UWAGA! program musi obs³u¿yæ SIGCHLD, je¶li ³±czy siê asynchronicznie,
 * ¿eby zrobiæ pogrzeb zmar³emu procesowi resolvera.
 *
 * w przypadku b³êdu zwraca NULL, je¶li idzie dobrze (async) albo posz³o
 * dobrze (sync), zwróci wska¼nik do zaalokowanej struktury `gg_session'.
 */
struct gg_session *gg_login(uin_t uin, char *password, int async)
{
	struct gg_session *sess;

	gg_debug(GG_DEBUG_FUNCTION, "** gg_login(%u, \"...\", %d);\n", uin, async);

	if (!(sess = malloc(sizeof(*sess))))
		return NULL;

	sess->uin = uin;
	if (!(sess->password = strdup(password))) {
		free(sess);
		return NULL;
	}
	sess->state = GG_STATE_RESOLVING;
	sess->check = GG_CHECK_READ;
	sess->async = async;
	sess->seq = 0;
	sess->recv_left = 0;
	sess->last_pong = 0;
	sess->server_ip = 0;
	sess->initial_status = 0;

	if (async) {
		if (gg_resolve(&sess->fd, &sess->pid, GG_APPMSG_HOST)) {
			gg_debug(GG_DEBUG_MISC, "-- resolving failed\n");
			free(sess);
			return NULL;
		}
	} else {
		struct in_addr a;

		if ((a.s_addr = inet_addr(GG_APPMSG_HOST)) == INADDR_NONE) {
			struct hostent *he;

			if (!(he = gethostbyname(GG_APPMSG_HOST))) {
				gg_debug(GG_DEBUG_MISC, "-- host %s not found\n", GG_APPMSG_HOST);
				free(sess);
				return NULL;
			} else
				memcpy((char *)&a, he->h_addr, sizeof(a));
		}

		if (!(sess->fd = gg_connect(&a, GG_APPMSG_PORT, 0)) == -1) {
			gg_debug(GG_DEBUG_MISC, "-- connection failed\n");
			free(sess);
			return NULL;
		}

		sess->state = GG_STATE_CONNECTING_HTTP;

		while (sess->state != GG_STATE_CONNECTED) {
			struct gg_event *e;

			if (!(e = gg_watch_fd(sess))) {
				gg_debug(GG_DEBUG_MISC, "-- some nasty error in gg_watch_fd()\n");
				free(sess);
				return NULL;
			}

			if (e->type == GG_EVENT_CONN_FAILED) {
				errno = EACCES;
				gg_debug(GG_DEBUG_MISC, "-- could not login\n");
				gg_free_event(e);
				free(sess);
				return NULL;
			}

			gg_free_event(e);
		}
	}

	return sess;
}

/* 
 * gg_free_session()
 *
 * zwalnia pamiêæ zajmowan± przez opis sesji.
 *
 *  - sess - opis sesji.
 *
 * nie zwraca niczego, bo i po co?
 */
void gg_free_session(struct gg_session *sess)
{
	if (!sess)
		return;

	free(sess->password);
	free(sess);
}

/*
 * gg_change_status()
 *
 * zmienia status u¿ytkownika. przydatne do /away i /busy oraz /quit.
 *
 *  - sess - opis sesji,
 *  - status - nowy status u¿ytkownika.
 *
 * je¶li wys³a³ pakiet zwraca 0, je¶li nie uda³o siê, zwraca -1.
 */
int gg_change_status(struct gg_session *sess, int status)
{
	struct gg_new_status p;

	if (!sess) {
		errno = EFAULT;
		return -1;
	}

	if (sess->state != GG_STATE_CONNECTED) {
		errno = ENOTCONN;
		return -1;
	}

	gg_debug(GG_DEBUG_FUNCTION, "** gg_change_status(..., %d);\n", status);

	p.status = fix32(status);

	return gg_send_packet(sess->fd, GG_NEW_STATUS, &p, sizeof(p), NULL, 0);
}

/*
 * gg_logoff()
 *
 * wylogowuje u¿ytkownika i zamyka po³±czenie.
 *
 *  - sock - deskryptor socketu.
 *
 * nie zwraca b³êdów. skoro siê ¿egnamy, to olewamy wszystko.
 */
void gg_logoff(struct gg_session *sess)
{
	if (!sess)
		return;

	gg_debug(GG_DEBUG_FUNCTION, "** gg_logoff(...);\n");

	if (sess->state == GG_STATE_CONNECTED)
		gg_change_status(sess, GG_STATUS_NOT_AVAIL);

	if (sess->fd) {
		shutdown(sess->fd, 2);
		close(sess->fd);
	}
}

/*
 * gg_send_message()
 *
 * wysy³a wiadomo¶æ do innego u¿ytkownika. zwraca losowy numer
 * sekwencyjny, który mo¿na olaæ albo wykorzystaæ do potwierdzenia.
 *
 *  - sess - opis sesji,
 *  - msgclass - rodzaj wiadomo¶ci,
 *  - recipient - numer adresata,
 *  - message - tre¶æ wiadomo¶ci.
 *
 * w przypadku b³êdu zwraca -1, inaczej numer sekwencyjny.
 */
int gg_send_message(struct gg_session *sess, int msgclass, uin_t recipient, unsigned char *message)
{
	struct gg_send_msg s;

	if (!sess) {
		errno = EFAULT;
		return -1;
	}

	if (sess->state != GG_STATE_CONNECTED) {
		errno = ENOTCONN;
		return -1;
	}

	gg_debug(GG_DEBUG_FUNCTION, "** gg_send_message(..., %d, %u, \"...\");\n", msgclass, recipient);

	s.recipient = fix32(recipient);
	if (!sess->seq)
		sess->seq = 0x01740000 | (rand() & 0xffff);
	s.seq = fix32(sess->seq);
	s.msgclass = fix32(msgclass);
	sess->seq += (rand() % 0x300) + 0x300;

	if (gg_send_packet(sess->fd, GG_SEND_MSG, &s, sizeof(s), message, strlen(message) + 1) == -1)
		return -1;

	return fix32(s.seq);
}

/*
 * gg_ping()
 *
 * wysy³a do serwera pakiet typu yeah-i'm-still-alive.
 *
 *  - sess - zgadnij.
 *
 * je¶li nie powiod³o siê wys³anie pakietu, zwraca -1. otherwise 0.
 */
int gg_ping(struct gg_session *sess)
{
	if (!sess) {
		errno = EFAULT;
		return -1;
	}

	if (sess->state != GG_STATE_CONNECTED) {
		errno = ENOTCONN;
		return -1;
	}

	gg_debug(GG_DEBUG_FUNCTION, "** gg_ping(...);\n");

	return gg_send_packet(sess->fd, GG_PING, NULL, 0, NULL, 0);
}

/*
 * gg_free_event()
 *
 * zwalnia pamiêæ zajmowan± przez informacjê o zdarzeniu
 *
 *  - event - wska¼nik do informacji o zdarzeniu
 *
 * nie ma czego zwracaæ.
 */
void gg_free_event(struct gg_event *e)
{
	if (!e)
		return;
	if (e->type == GG_EVENT_MSG)
		free(e->event.msg.message);
	if (e->type == GG_EVENT_NOTIFY)
		free(e->event.notify);
	free(e);
}

/*
 * gg_notify()
 *
 * wysy³a serwerowi listê ludków, za którymi têsknimy.
 *
 *  - sess - identyfikator sesji,
 *  - userlist - wska¼nik do tablicy numerów,
 *  - count - ilo¶æ numerków.
 *
 * je¶li uda³o siê, zwraca 0. je¶li b³±d, dostajemy -1.
 */
int gg_notify(struct gg_session *sess, uin_t * userlist, int count)
{
	struct gg_notify *n;
	uin_t *u;
	int i, res = 0;

	if (!sess) {
		errno = EFAULT;
		return -1;
	}

	if (sess->state != GG_STATE_CONNECTED) {
		errno = ENOTCONN;
		return -1;
	}

	gg_debug(GG_DEBUG_FUNCTION, "** gg_notify(..., %d);\n", count);

	if (!userlist || !count)
		return 0;

	if (!(n = (struct gg_notify *)malloc(sizeof(*n) * count)))
		return -1;

	for (u = userlist, i = 0; i < count; u++, i++) {
		n[i].uin = fix32(*u);
		n[i].dunno1 = 3;
	}

	if (gg_send_packet(sess->fd, GG_NOTIFY, n, sizeof(*n) * count, NULL, 0) == -1)
		res = -1;

	free(n);

	return res;
}

/*
 * gg_add_notify()
 *
 * dodaje w locie do listy ukochanych dany numerek.
 *
 *  - sess - identyfikator sesji,
 *  - uin - numerek ukochanej.
 *
 * je¶li uda³o siê wys³aæ, daje 0. inaczej -1.
 */
int gg_add_notify(struct gg_session *sess, uin_t uin)
{
	struct gg_add_remove a;

	if (!sess) {
		errno = EFAULT;
		return -1;
	}

	if (sess->state != GG_STATE_CONNECTED) {
		errno = ENOTCONN;
		return -1;
	}

	gg_debug(GG_DEBUG_FUNCTION, "** gg_add_notify(..., %u);\n", uin);

	a.uin = fix32(uin);
	a.dunno1 = 3;

	return gg_send_packet(sess->fd, GG_ADD_NOTIFY, &a, sizeof(a), NULL, 0);
}

/*
 * gg_remove_notify()
 *
 * w locie usuwa z listy zainteresowanych.
 *
 *  - sess - id sesji,
 *  - uin - numerek.
 *
 * zwraca -1 je¶li by³ b³±d, 0 je¶li siê uda³o wys³aæ pakiet.
 */
int gg_remove_notify(struct gg_session *sess, uin_t uin)
{
	struct gg_add_remove a;

	if (!sess) {
		errno = EFAULT;
		return -1;
	}

	if (sess->state != GG_STATE_CONNECTED) {
		errno = ENOTCONN;
		return -1;
	}

	gg_debug(GG_DEBUG_FUNCTION, "** gg_remove_notify(..., %u);\n", uin);

	a.uin = fix32(uin);
	a.dunno1 = 3;

	return gg_send_packet(sess->fd, GG_REMOVE_NOTIFY, &a, sizeof(a), NULL, 0);
}

/*
 * gg_watch_fd_connected() // funkcja wewnêtrzna
 *
 * patrzy na socketa, odbiera pakiet i wype³nia strukturê zdarzenia.
 *
 *  - sock - lalala, trudno zgadn±æ.
 *
 * je¶li b³±d -1, je¶li dobrze 0.
 */
static int gg_watch_fd_connected(struct gg_session *sess, struct gg_event *e)
{
	struct gg_header *h;
	void *p;

	if (!sess) {
		errno = EFAULT;
		return -1;
	}

	gg_debug(GG_DEBUG_FUNCTION, "** gg_watch_fd_connected(...);\n");

	if (!(h = gg_recv_packet(sess))) {
		gg_debug(GG_DEBUG_MISC, "-- gg_recv_packet failed. errno = %d (%d)\n", errno,
			 strerror(errno));
		return -1;
	}

	p = (void *)h + sizeof(struct gg_header);

	if (h->type == GG_RECV_MSG) {
		struct gg_recv_msg *r = p;

		gg_debug(GG_DEBUG_MISC, "-- received a message\n");

		if (h->length >= sizeof(*r)) {
			e->type = GG_EVENT_MSG;
			e->event.msg.msgclass = fix32(r->msgclass);
			e->event.msg.sender = fix32(r->sender);
			e->event.msg.message = strdup((char *)r + sizeof(*r));
			e->event.msg.time = fix32(r->time);
		}
	}

	if (h->type == GG_NOTIFY_REPLY) {
		struct gg_notify_reply *n = p;
		int count, i;

		gg_debug(GG_DEBUG_MISC, "-- received a notify reply\n");

		e->type = GG_EVENT_NOTIFY;
		if (!(e->event.notify = (void *)malloc(h->length + 2 * sizeof(*n)))) {
			gg_debug(GG_DEBUG_MISC, "-- not enough memory\n");
			free(h);
			return -1;
		}
		count = h->length / sizeof(*n);
		memcpy(e->event.notify, p, h->length);
		e->event.notify[count].uin = 0;
		for (i = 0; i < count; i++) {
			e->event.notify[i].uin = fix32(e->event.notify[i].uin);
			e->event.notify[i].status = fix32(e->event.notify[i].status);
		}
	}

	if (h->type == GG_STATUS) {
		struct gg_status *s = p;

		gg_debug(GG_DEBUG_MISC, "-- received a status change\n");

		if (h->length >= sizeof(*s)) {
			e->type = GG_EVENT_STATUS;
			memcpy(&e->event.status, p, h->length);
			e->event.status.uin = fix32(e->event.status.uin);
			e->event.status.status = fix32(e->event.status.status);
		}
	}

	if (h->type == GG_SEND_MSG_ACK) {
		struct gg_send_msg_ack *s = p;

		gg_debug(GG_DEBUG_MISC, "-- received a message ack\n");

		if (h->length >= sizeof(*s)) {
			e->type = GG_EVENT_ACK;
			e->event.ack.status = fix32(s->status);
			e->event.ack.recipient = fix32(s->recipient);
			e->event.ack.seq = fix32(s->seq);
		}
	}

	if (h->type == GG_PONG) {
		gg_debug(GG_DEBUG_MISC, "-- received a pong\n");

		sess->last_pong = time(NULL);
	}

	free(h);

	return 0;
}

/*
 * gg_chomp() // funkcja wewnêtrzna
 *
 * ucina "\r\n" lub "\n" z koñca linii.
 *
 *  - line - ofiara operacji plastycznej.
 *
 * niczego nie zwraca.
 */
static void gg_chomp(char *line)
{
	if (!line || strlen(line) < 1)
		return;

	if (line[strlen(line) - 1] == '\n')
		line[strlen(line) - 1] = 0;
	if (line[strlen(line) - 1] == '\r')
		line[strlen(line) - 1] = 0;
}

/*
 * gg_watch_fd()
 *
 * funkcja wywo³ywana, gdy co¶ siê stanie na obserwowanym deskryptorze.
 * zwraca klientowi informacjê o tym, co siê dzieje.
 *
 *  - sess - identyfikator sesji.
 *
 * zwraca wska¼nik do struktury gg_event, któr± trzeba zwolniæ pó¼niej
 * za pomoc± gg_free_event(). jesli rodzaj zdarzenia jest równy
 * GG_EVENT_NONE, nale¿y je olaæ kompletnie. je¶li zwróci³o NULL,
 * sta³o siê co¶ niedobrego -- albo brak³o pamiêci albo zerwa³o
 * po³±czenie albo co¶ takiego.
 */
struct gg_event *gg_watch_fd(struct gg_session *sess)
{
	struct gg_event *e;
	int res = 0;

	if (!sess) {
		errno = EFAULT;
		return NULL;
	}

	gg_debug(GG_DEBUG_FUNCTION, "** gg_watch_fd(...);\n");

	if (!(e = (void *)malloc(sizeof(*e)))) {
		gg_debug(GG_DEBUG_MISC, "-- not enough memory\n");
		return NULL;
	}

	e->type = GG_EVENT_NONE;

	switch (sess->state) {
	case GG_STATE_RESOLVING:
		{
			struct in_addr a;

			gg_debug(GG_DEBUG_MISC, "== GG_STATE_RESOLVING\n");

			if (read(sess->fd, &a, sizeof(a)) < sizeof(a) || a.s_addr == INADDR_NONE) {
				gg_debug(GG_DEBUG_MISC, "-- resolving failed\n");

				errno = ENOENT;
				e->type = GG_EVENT_CONN_FAILED;
				e->event.failure = GG_FAILURE_RESOLVING;
				sess->state = GG_STATE_IDLE;

				close(sess->fd);

				break;
			}

			sess->server_ip = a.s_addr;

			close(sess->fd);

			waitpid(sess->pid, NULL, 0);

			gg_debug(GG_DEBUG_MISC, "-- resolved, now connecting\n");

			if ((sess->fd = gg_connect(&a, GG_APPMSG_PORT, sess->async)) == -1) {
				struct in_addr *addr = (struct in_addr *)&sess->server_ip;

				gg_debug(GG_DEBUG_MISC,
					 "-- connection failed, trying direct connection\n");

				if ((sess->fd = gg_connect(addr, GG_DEFAULT_PORT, sess->async)) == -1) {
					gg_debug(GG_DEBUG_MISC,
						 "-- connection failed, trying https connection\n");
					if ((sess->fd =
					     gg_connect(&a, GG_HTTPS_PORT, sess->async)) == -1) {
						gg_debug(GG_DEBUG_MISC,
							 "-- connect() failed. errno = %d (%s)\n", errno,
							 strerror(errno));

						e->type = GG_EVENT_CONN_FAILED;
						e->event.failure = GG_FAILURE_CONNECTING;
						sess->state = GG_STATE_IDLE;
						break;
					}
				}
				sess->state = GG_STATE_CONNECTING_GG;
				sess->check = GG_CHECK_WRITE;
			} else {
				sess->state = GG_STATE_CONNECTING_HTTP;
				sess->check = GG_CHECK_WRITE;
			}

			break;
		}

	case GG_STATE_CONNECTING_HTTP:
		{
			char buf[1024];
			int res, res_size = sizeof(res);

			gg_debug(GG_DEBUG_MISC, "== GG_STATE_CONNECTING_HTTP\n");

			if (sess->async
			    && (getsockopt(sess->fd, SOL_SOCKET, SO_ERROR, &res, &res_size) || res)) {
				struct in_addr *addr = (struct in_addr *)&sess->server_ip;
				gg_debug(GG_DEBUG_MISC,
					 "-- http connection failed, errno = %d (%s), trying direct connection\n",
					 res, strerror(res));

				if ((sess->fd = gg_connect(addr, GG_DEFAULT_PORT, sess->async)) == -1) {
					gg_debug(GG_DEBUG_MISC,
						 "-- connection failed, trying https connection\n");
					if ((sess->fd =
					     gg_connect(addr, GG_HTTPS_PORT, sess->async)) == -1) {
						gg_debug(GG_DEBUG_MISC,
							 "-- connect() failed. errno = %d (%s)\n", errno,
							 strerror(errno));

						e->type = GG_EVENT_CONN_FAILED;
						e->event.failure = GG_FAILURE_CONNECTING;
						sess->state = GG_STATE_IDLE;
						break;
					}
				}

				sess->state = GG_STATE_CONNECTING_GG;
				sess->check = GG_CHECK_WRITE;
				break;
			}

			gg_debug(GG_DEBUG_MISC, "-- http connection succeded, sending query\n");

			snprintf(buf, sizeof(buf) - 1,
				 "GET /appsvc/appmsg.asp?fmnumber=%lu HTTP/1.0\r\n"
				 "Host: " GG_APPMSG_HOST "\r\n"
				 "User-Agent: " GG_HTTP_USERAGENT "\r\n"
				 "Pragma: no-cache\r\n" "\r\n", sess->uin);

			if (write(sess->fd, buf, strlen(buf)) < strlen(buf)) {
				gg_debug(GG_DEBUG_MISC, "-- sending query failed\n");

				errno = EIO;
				e->type = GG_EVENT_CONN_FAILED;
				e->event.failure = GG_FAILURE_WRITING;
				sess->state = GG_STATE_IDLE;
				break;
			}

			sess->state = GG_STATE_WRITING_HTTP;
			sess->check = GG_CHECK_READ;

			break;
		}

	case GG_STATE_WRITING_HTTP:
		{
			char buf[1024], *tmp, *host;
			int port = GG_DEFAULT_PORT;
			struct in_addr a;

			gg_debug(GG_DEBUG_MISC, "== GG_STATE_WRITING_HTTP\n");

			gg_read_line(sess->fd, buf, sizeof(buf) - 1);
			gg_chomp(buf);

			gg_debug(GG_DEBUG_TRAFFIC, "-- got http response (%s)\n", buf);

			if (strncmp(buf, "HTTP/1.", 7) || strncmp(buf + 9, "200", 3)) {
				gg_debug(GG_DEBUG_MISC, "-- but that's not what we've expected\n");

				errno = EINVAL;
				e->type = GG_EVENT_CONN_FAILED;
				e->event.failure = GG_FAILURE_INVALID;
				sess->state = GG_STATE_IDLE;
				break;
			}

			while (strcmp(buf, "\r\n") && strcmp(buf, ""))
				gg_read_line(sess->fd, buf, sizeof(buf) - 1);

			gg_read_line(sess->fd, buf, sizeof(buf) - 1);
			gg_chomp(buf);

			close(sess->fd);

			gg_debug(GG_DEBUG_TRAFFIC, "-- received http data (%s)\n", buf);

			tmp = buf;
			while (*tmp && *tmp != ' ')
				tmp++;
			while (*tmp && *tmp == ' ')
				tmp++;
			while (*tmp && *tmp != ' ')
				tmp++;
			while (*tmp && *tmp == ' ')
				tmp++;
			while (*tmp && *tmp != ' ')
				tmp++;
			while (*tmp && *tmp == ' ')
				tmp++;
			host = tmp;
			while (*tmp && *tmp != ' ')
				tmp++;
			*tmp = 0;

			if ((tmp = strchr(host, ':'))) {
				*tmp = 0;
				port = atoi(tmp + 1);
			}

			a.s_addr = inet_addr(host);
			sess->server_ip = a.s_addr;

			if ((sess->fd = gg_connect(&a, port, sess->async)) == -1) {
				gg_debug(GG_DEBUG_MISC,
					 "-- connection failed, trying https connection\n");
				if ((sess->fd = gg_connect(&a, GG_HTTPS_PORT, sess->async)) == -1) {
					gg_debug(GG_DEBUG_MISC,
						 "-- connection failed, errno = %d (%s)\n", errno,
						 strerror(errno));

					e->type = GG_EVENT_CONN_FAILED;
					e->event.failure = GG_FAILURE_CONNECTING;
					sess->state = GG_STATE_IDLE;
					break;
				}
			}

			sess->state = GG_STATE_CONNECTING_GG;
			sess->check = GG_CHECK_WRITE;

			break;
		}

	case GG_STATE_CONNECTING_GG:
		{
			int res, res_size = sizeof(res);

			gg_debug(GG_DEBUG_MISC, "== GG_STATE_CONNECTING_GG\n");

			if (sess->async
			    && (getsockopt(sess->fd, SOL_SOCKET, SO_ERROR, &res, &res_size) || res)) {
				struct in_addr *addr = (struct in_addr *)&sess->server_ip;

				gg_debug(GG_DEBUG_MISC,
					 "-- connection failed, trying https connection\n");
				if ((sess->fd = gg_connect(addr, GG_HTTPS_PORT, sess->async)) == -1) {
					gg_debug(GG_DEBUG_MISC,
						 "-- connection failed, errno = %d (%s)\n", errno,
						 strerror(errno));

					e->type = GG_EVENT_CONN_FAILED;
					e->event.failure = GG_FAILURE_CONNECTING;
					sess->state = GG_STATE_IDLE;
					break;
				}
			}

			gg_debug(GG_DEBUG_MISC, "-- connected\n");

			sess->state = GG_STATE_WAITING_FOR_KEY;
			sess->check = GG_CHECK_READ;

			break;
		}

	case GG_STATE_WAITING_FOR_KEY:
		{
			struct gg_header *h;
			struct gg_welcome *w;
			struct gg_login l;
			unsigned int hash;
			char *password = sess->password;

			gg_debug(GG_DEBUG_MISC, "== GG_STATE_WAITING_FOR_KEY\n");

			if (!(h = gg_recv_packet(sess))) {
				gg_debug(GG_DEBUG_MISC, "-- gg_recv_packet() failed. errno = %d (%s)\n",
					 errno, strerror(errno));

				e->type = GG_EVENT_CONN_FAILED;
				e->event.failure = GG_FAILURE_READING;
				sess->state = GG_STATE_IDLE;
				close(sess->fd);
				break;
			}

			if (h->type != GG_WELCOME) {
				gg_debug(GG_DEBUG_MISC, "-- invalid packet received\n");

				free(h);
				close(sess->fd);
				errno = EINVAL;
				e->type = GG_EVENT_CONN_FAILED;
				e->event.failure = GG_FAILURE_INVALID;
				sess->state = GG_STATE_IDLE;
				break;
			}

			w = (void *)h + sizeof(struct gg_header);
			w->key = fix32(w->key);

			for (hash = 1; *password; password++)
				hash *= (*password) + 1;
			hash *= w->key;

			gg_debug(GG_DEBUG_DUMP, "%%%% klucz serwera %.4x, hash has³a %.8x\n", w->key,
				 hash);

			free(h);

			free(sess->password);
			sess->password = NULL;

			l.uin = fix32(sess->uin);
			l.hash = fix32(hash);
			l.status = fix32(sess->initial_status ? sess->initial_status : GG_STATUS_AVAIL);
			l.dunno = fix32(0x0b);
			l.local_ip = 0;
			l.local_port = 0;

			gg_debug(GG_DEBUG_TRAFFIC, "-- sending GG_LOGIN packet\n");

			if (gg_send_packet(sess->fd, GG_LOGIN, &l, sizeof(l), NULL, 0) == -1) {
				gg_debug(GG_DEBUG_TRAFFIC, "-- oops, failed. errno = %d (%s)\n", errno,
					 strerror(errno));

				close(sess->fd);
				e->type = GG_EVENT_CONN_FAILED;
				e->event.failure = GG_FAILURE_WRITING;
				sess->state = GG_STATE_IDLE;
				break;
			}

			sess->state = GG_STATE_SENDING_KEY;

			break;
		}

	case GG_STATE_SENDING_KEY:
		{
			struct gg_header *h;

			gg_debug(GG_DEBUG_MISC, "== GG_STATE_SENDING_KEY\n");

			if (!(h = gg_recv_packet(sess))) {
				gg_debug(GG_DEBUG_MISC, "-- recv_packet failed\n");
				e->type = GG_EVENT_CONN_FAILED;
				e->event.failure = GG_FAILURE_READING;
				sess->state = GG_STATE_IDLE;
				close(sess->fd);
				break;
			}

			if (h->type == GG_LOGIN_OK) {
				gg_debug(GG_DEBUG_MISC, "-- login succeded\n");
				e->type = GG_EVENT_CONN_SUCCESS;
				sess->state = GG_STATE_CONNECTED;
				free(h);
				break;
			}

			if (h->type == GG_LOGIN_FAILED) {
				gg_debug(GG_DEBUG_MISC, "-- login failed\n");
				e->event.failure = GG_FAILURE_PASSWORD;
				errno = EACCES;
			} else {
				gg_debug(GG_DEBUG_MISC, "-- invalid packet\n");
				e->event.failure = GG_FAILURE_INVALID;
				errno = EINVAL;
			}

			e->type = GG_EVENT_CONN_FAILED;
			sess->state = GG_STATE_IDLE;
			close(sess->fd);
			free(h);

			break;
		}

	case GG_STATE_CONNECTED:
		{
			gg_debug(GG_DEBUG_MISC, "== GG_STATE_CONNECTED\n");

			if ((res = gg_watch_fd_connected(sess, e)) == -1) {

				gg_debug(GG_DEBUG_MISC,
					 "-- watch_fd_connected failed. errno = %d (%s)\n", errno,
					 strerror(errno));

				if (errno == EAGAIN) {
					e->type = GG_EVENT_NONE;
					res = 0;
				} else
					res = -1;
			}
			break;
		}
	}

	if (res == -1) {
		free(e);
		e = NULL;
	}

	return e;
}
