/* $Id: common.c 16856 2006-08-19 01:13:25Z evands $ */

/*
 *  (C) Copyright 2001-2002 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Wo¼ny <speedy@ziew.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License Version
 *  2.1 as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,
 *  USA.
 */

#ifndef _WIN32
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef sun
#  include <sys/filio.h>
#endif
#endif

#include <errno.h>
#include <fcntl.h>
#ifndef _WIN32
#include <netdb.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libgadu.h"

FILE *gg_debug_file = NULL;

#ifndef GG_DEBUG_DISABLE

/*
 * gg_debug() // funkcja wewnêtrzna
 *
 * wy¶wietla komunikat o danym poziomie, o ile u¿ytkownik sobie tego ¿yczy.
 *
 *  - level - poziom wiadomo¶ci
 *  - format... - tre¶æ wiadomo¶ci (kompatybilna z printf())
 */
void gg_debug(int level, const char *format, ...)
{
	va_list ap;
	int old_errno = errno;
	
	if (gg_debug_handler) {
		va_start(ap, format);
		(*gg_debug_handler)(level, format, ap);
		va_end(ap);

		goto cleanup;
	}
	
	if ((gg_debug_level & level)) {
		va_start(ap, format);
		vfprintf((gg_debug_file) ? gg_debug_file : stderr, format, ap);
		va_end(ap);
	}

cleanup:
	errno = old_errno;
}

#endif

/*
 * gg_vsaprintf() // funkcja pomocnicza
 *
 * robi dok³adnie to samo, co vsprintf(), tyle ¿e alokuje sobie wcze¶niej
 * miejsce na dane. powinno dzia³aæ na tych maszynach, które maj± funkcjê
 * vsnprintf() zgodn± z C99, jak i na wcze¶niejszych.
 *
 *  - format - opis wy¶wietlanego tekstu jak dla printf()
 *  - ap - lista argumentów dla printf()
 *
 * zaalokowany bufor, który nale¿y pó¼niej zwolniæ, lub NULL
 * je¶li nie uda³o siê wykonaæ zadania.
 */
char *gg_vsaprintf(const char *format, va_list ap)
{
	int size = 0;
	const char *start;
	char *buf = NULL;
	
#ifdef __GG_LIBGADU_HAVE_VA_COPY
	va_list aq;

	va_copy(aq, ap);
#else
#  ifdef __GG_LIBGADU_HAVE___VA_COPY
	va_list aq;

	__va_copy(aq, ap);
#  endif
#endif

	start = format; 

#ifndef __GG_LIBGADU_HAVE_C99_VSNPRINTF
	{
		int res;
		char *tmp;
		
		size = 128;
		do {
			size *= 2;
			if (!(tmp = realloc(buf, size))) {
				free(buf);
				return NULL;
			}
			buf = tmp;
			res = vsnprintf(buf, size, format, ap);
		} while (res == size - 1 || res == -1);
	}
#else
	{
		char tmp[2];
		
		/* libce Solarisa przy buforze NULL zawsze zwracaj± -1, wiêc
		 * musimy podaæ co¶ istniej±cego jako cel printf()owania. */
		size = vsnprintf(tmp, sizeof(tmp), format, ap);
		if (!(buf = malloc(size + 1)))
			return NULL;
	}
#endif

	format = start;
	
#ifdef __GG_LIBGADU_HAVE_VA_COPY
	vsnprintf(buf, size + 1, format, aq);
	va_end(aq);
#else
#  ifdef __GG_LIBGADU_HAVE___VA_COPY
	vsnprintf(buf, size + 1, format, aq);
	va_end(aq);
#  else
	vsnprintf(buf, size + 1, format, ap);
#  endif
#endif
	
	return buf;
}

/*
 * gg_saprintf() // funkcja pomocnicza
 *
 * robi dok³adnie to samo, co sprintf(), tyle ¿e alokuje sobie wcze¶niej
 * miejsce na dane. powinno dzia³aæ na tych maszynach, które maj± funkcjê
 * vsnprintf() zgodn± z C99, jak i na wcze¶niejszych.
 *
 *  - format... - tre¶æ taka sama jak w funkcji printf()
 *
 * zaalokowany bufor, który nale¿y pó¼niej zwolniæ, lub NULL
 * je¶li nie uda³o siê wykonaæ zadania.
 */
char *gg_saprintf(const char *format, ...)
{
	va_list ap;
	char *res;

	va_start(ap, format);
	res = gg_vsaprintf(format, ap);
	va_end(ap);

	return res;
}

/*
 * gg_get_line() // funkcja pomocnicza
 * 
 * podaje kolejn± liniê z bufora tekstowego. niszczy go bezpowrotnie, dziel±c
 * na kolejne stringi. zdarza siê, nie ma potrzeby pisania funkcji dubluj±cej
 * bufor ¿eby tylko mieæ nieruszone dane wej¶ciowe, skoro i tak nie bêd± nam
 * po¼niej potrzebne. obcina `\r\n'.
 * 
 *  - ptr - wska¼nik do zmiennej, która przechowuje aktualn± pozycjê
 *    w przemiatanym buforze
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
		if (strlen(res) > 1 && res[strlen(res) - 1] == '\r')
			res[strlen(res) - 1] = 0;
	}

	return res;
}

/*
 * gg_connect() // funkcja pomocnicza
 *
 * ³±czy siê z serwerem. pierwszy argument jest typu (void *), ¿eby nie
 * musieæ niczego inkludowaæ w libgadu.h i nie psuæ jaki¶ g³upich zale¿no¶ci
 * na dziwnych systemach.
 *
 *  - addr - adres serwera (struct in_addr *)
 *  - port - port serwera
 *  - async - asynchroniczne po³±czenie
 *
 * deskryptor gniazda lub -1 w przypadku b³êdu (kod b³êdu w zmiennej errno).
 */
int gg_connect(void *addr, int port, int async)
{
	int sock, one = 1, errno2;
	struct sockaddr_in sin;
	struct in_addr *a = addr;
	struct sockaddr_in myaddr;

	gg_debug(GG_DEBUG_FUNCTION, "** gg_connect(%s, %d, %d);\n", inet_ntoa(*a), port, async);
	
	if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		gg_debug(GG_DEBUG_MISC, "// gg_connect() socket() failed (errno=%d, %s)\n", errno, strerror(errno));
		return -1;
	}

	memset(&myaddr, 0, sizeof(myaddr));
	myaddr.sin_family = AF_INET;

	myaddr.sin_addr.s_addr = gg_local_ip;

	if (bind(sock, (struct sockaddr *) &myaddr, sizeof(myaddr)) == -1) {
		gg_debug(GG_DEBUG_MISC, "// gg_connect() bind() failed (errno=%d, %s)\n", errno, strerror(errno));
		return -1;
	}

#ifdef ASSIGN_SOCKETS_TO_THREADS
	gg_win32_thread_socket(0, sock);
#endif

	if (async) {
#ifdef FIONBIO
		if (ioctl(sock, FIONBIO, &one) == -1) {
#else
		if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1) {
#endif
			gg_debug(GG_DEBUG_MISC, "// gg_connect() ioctl() failed (errno=%d, %s)\n", errno, strerror(errno));
			errno2 = errno;
			close(sock);
			errno = errno2;
			return -1;
		}
	}

	sin.sin_port = htons(port);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = a->s_addr;
	
	if (connect(sock, (struct sockaddr*) &sin, sizeof(sin)) == -1) {
		if (errno && (!async || errno != EINPROGRESS)) {
			gg_debug(GG_DEBUG_MISC, "// gg_connect() connect() failed (errno=%d, %s)\n", errno, strerror(errno));
			errno2 = errno;
			close(sock);
			errno = errno2;
			return -1;
		}
		gg_debug(GG_DEBUG_MISC, "// gg_connect() connect() in progress\n");
	}
	
	return sock;
}

/*
 * gg_read_line() // funkcja pomocnicza
 *
 * czyta jedn± liniê tekstu z gniazda.
 *
 *  - sock - deskryptor gniazda
 *  - buf - wska¼nik do bufora
 *  - length - d³ugo¶æ bufora
 *
 * je¶li trafi na b³±d odczytu lub podano nieprawid³owe parametry, zwraca NULL.
 * inaczej zwraca buf.
 */
char *gg_read_line(int sock, char *buf, int length)
{
	int ret;

	if (!buf || length < 0)
		return NULL;

	for (; length > 1; buf++, length--) {
		do {
			if ((ret = read(sock, buf, 1)) == -1 && errno != EINTR) {
				gg_debug(GG_DEBUG_MISC, "// gg_read_line() error on read (errno=%d, %s)\n", errno, strerror(errno));
				*buf = 0;
				return NULL;
			} else if (ret == 0) {
				gg_debug(GG_DEBUG_MISC, "// gg_read_line() eof reached\n");
				*buf = 0;
				return NULL;
			}
		} while (ret == -1 && errno == EINTR);

		if (*buf == '\n') {
			buf++;
			break;
		}
	}

	*buf = 0;
	return buf;
}

/*
 * gg_chomp() // funkcja pomocnicza
 *
 * ucina "\r\n" lub "\n" z koñca linii.
 *
 *  - line - linia do przyciêcia
 */
void gg_chomp(char *line)
{
	int len;
	
	if (!line)
		return;

	len = strlen(line);
	
	if (len > 0 && line[len - 1] == '\n')
		line[--len] = 0;
	if (len > 0 && line[len - 1] == '\r')
		line[--len] = 0;
}

/*
 * gg_urlencode() // funkcja wewnêtrzna
 *
 * zamienia podany tekst na ci±g znaków do formularza http. przydaje siê
 * przy ró¿nych us³ugach katalogu publicznego.
 *
 *  - str - ci±g znaków do zakodowania
 *
 * zaalokowany bufor, który nale¿y pó¼niej zwolniæ albo NULL
 * w przypadku b³êdu.
 */
char *gg_urlencode(const char *str)
{
	char *q, *buf, hex[] = "0123456789abcdef";
	const char *p;
	unsigned int size = 0;

	if (!str)
		str = "";

	for (p = str; *p; p++, size++) {
		if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == ' ') || (*p == '@') || (*p == '.') || (*p == '-'))
			size += 2;
	}

	if (!(buf = malloc(size + 1)))
		return NULL;

	for (p = str, q = buf; *p; p++, q++) {
		if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || (*p == '@') || (*p == '.') || (*p == '-'))
			*q = *p;
		else {
			if (*p == ' ')
				*q = '+';
			else {
				*q++ = '%';
				*q++ = hex[*p >> 4 & 15];
				*q = hex[*p & 15];
			}
		}
	}

	*q = 0;

	return buf;
}

/*
 * gg_http_hash() // funkcja wewnêtrzna
 *
 * funkcja licz±ca hash dla adresu e-mail, has³a i paru innych.
 *
 *  - format... - format kolejnych parametrów ('s' je¶li dany parametr jest
 *                ci±giem znaków lub 'u' je¶li numerem GG)
 *
 * hash wykorzystywany przy rejestracji i wszelkich manipulacjach w³asnego
 * wpisu w katalogu publicznym.
 */
int gg_http_hash(const char *format, ...)
{
	unsigned int a, c, i, j;
	va_list ap;
	int b = -1;

	va_start(ap, format);

	for (j = 0; j < strlen(format); j++) {
		char *arg, buf[16];

		if (format[j] == 'u') {
			snprintf(buf, sizeof(buf), "%d", va_arg(ap, uin_t));
			arg = buf;
		} else {
			if (!(arg = va_arg(ap, char*)))
				arg = "";
		}	

		i = 0;
		while ((c = (unsigned char) arg[i++]) != 0) {
			a = (c ^ b) + (c << 8);
			b = (a >> 24) | (a << 8);
		}
	}

	va_end(ap);

	return (b < 0 ? -b : b);
}

/*
 * gg_gethostbyname() // funkcja pomocnicza
 *
 * odpowiednik gethostbyname() troszcz±cy siê o wspó³bie¿no¶æ, gdy mamy do
 * dyspozycji funkcjê gethostbyname_r().
 *
 *  - hostname - nazwa serwera
 *
 * zwraca wska¼nik na strukturê in_addr, któr± nale¿y zwolniæ.
 */
struct in_addr *gg_gethostbyname(const char *hostname)
{
	struct in_addr *addr = NULL;

#ifdef HAVE_GETHOSTBYNAME_R
	char *tmpbuf = NULL, *buf = NULL;
	struct hostent *hp = NULL, *hp2 = NULL;
	int h_errnop, ret;
	size_t buflen = 1024;
	int new_errno;
	
	new_errno = ENOMEM;
	
	if (!(addr = malloc(sizeof(struct in_addr))))
		goto cleanup;
	
	if (!(hp = calloc(1, sizeof(*hp))))
		goto cleanup;

	if (!(buf = malloc(buflen)))
		goto cleanup;

	tmpbuf = buf;
	
	while ((ret = gethostbyname_r(hostname, hp, buf, buflen, &hp2, &h_errnop)) == ERANGE) {
		buflen *= 2;
		
		if (!(tmpbuf = realloc(buf, buflen)))
			break;
		
		buf = tmpbuf;
	}
	
	if (ret)
		new_errno = h_errnop;

	if (ret || !hp2 || !tmpbuf)
		goto cleanup;
	
	memcpy(addr, hp->h_addr, sizeof(struct in_addr));
	
	free(buf);
	free(hp);
	
	return addr;
	
cleanup:
	errno = new_errno;
	
	if (addr)
		free(addr);
	if (hp)
		free(hp);
	if (buf)
		free(buf);
	
	return NULL;
#else
	struct hostent *hp;

	if (!(addr = malloc(sizeof(struct in_addr)))) {
		goto cleanup;
	}

	if (!(hp = gethostbyname(hostname)))
		goto cleanup;

	memcpy(addr, hp->h_addr, sizeof(struct in_addr));

	return addr;
	
cleanup:
	if (addr)
		free(addr);

	return NULL;
#endif
}

#ifdef ASSIGN_SOCKETS_TO_THREADS

typedef struct gg_win32_thread {
	int id;
	int socket;
	struct gg_win32_thread *next;
} gg_win32_thread;

struct gg_win32_thread *gg_win32_threads = 0;

/*
 * gg_win32_thread_socket() // funkcja pomocnicza, tylko dla win32
 *
 * zwraca deskryptor gniazda, które by³o ostatnio tworzone dla w±tku
 * o podanym identyfikatorze.
 *
 * je¶li na win32 przy po³±czeniach synchronicznych zapamiêtamy w jakim
 * w±tku uruchomili¶my funkcjê, która siê z czymkolwiek ³±czy, to z osobnego
 * w±tku mo¿emy anulowaæ po³±czenie poprzez gg_win32_thread_socket(watek, -1);
 * 
 * - thread_id - id w±tku. je¶li jest równe 0, brany jest aktualny w±tek,
 *               je¶li równe -1, usuwa wpis o podanym sockecie.
 * - socket - deskryptor gniazda. je¶li równe 0, zwraca deskryptor gniazda
 *            dla podanego w±tku, je¶li równe -1, usuwa wpis, je¶li co¶
 *            innego, ustawia dla podanego w±tku dany numer deskryptora.
 *
 * je¶li socket jest równe 0, zwraca deskryptor gniazda dla podanego w±tku.
 */
int gg_win32_thread_socket(int thread_id, int socket)
{
	char close = (thread_id == -1) || socket == -1;
	gg_win32_thread *wsk = gg_win32_threads;
	gg_win32_thread **p_wsk = &gg_win32_threads;

	if (!thread_id)
		thread_id = GetCurrentThreadId();
	
	while (wsk) {
		if ((thread_id == -1 && wsk->socket == socket) || wsk->id == thread_id) {
			if (close) {
				/* socket zostaje usuniety */
				closesocket(wsk->socket);
				*p_wsk = wsk->next;
				free(wsk);
				return 1;
			} else if (!socket) {
				/* socket zostaje zwrocony */
				return wsk->socket;
			} else {
				/* socket zostaje ustawiony */
				wsk->socket = socket;
				return socket;
			}
		}
		p_wsk = &(wsk->next);
		wsk = wsk->next;
	}

	if (close && socket != -1)
		closesocket(socket);
	if (close || !socket)
		return 0;
	
	/* Dodaje nowy element */
	wsk = malloc(sizeof(gg_win32_thread));
	wsk->id = thread_id;
	wsk->socket = socket;
	wsk->next = 0;
	*p_wsk = wsk;

	return socket;
}

#endif /* ASSIGN_SOCKETS_TO_THREADS */

static char gg_base64_charset[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/*
 * gg_base64_encode()
 *
 * zapisuje ci±g znaków w base64.
 *
 *  - buf - ci±g znaków.
 *
 * zaalokowany bufor.
 */
char *gg_base64_encode(const char *buf)
{
	char *out, *res;
	unsigned int i = 0, j = 0, k = 0, len = strlen(buf);
	
	res = out = malloc((len / 3 + 1) * 4 + 2);

	if (!res)
		return NULL;
	
	while (j <= len) {
		switch (i % 4) {
			case 0:
				k = (buf[j] & 252) >> 2;
				break;
			case 1:
				if (j < len)
					k = ((buf[j] & 3) << 4) | ((buf[j + 1] & 240) >> 4);
				else
					k = (buf[j] & 3) << 4;

				j++;
				break;
			case 2:
				if (j < len)
					k = ((buf[j] & 15) << 2) | ((buf[j + 1] & 192) >> 6);
				else
					k = (buf[j] & 15) << 2;

				j++;
				break;
			case 3:
				k = buf[j++] & 63;
				break;
		}
		*out++ = gg_base64_charset[k];
		i++;
	}

	if (i % 4)
		for (j = 0; j < 4 - (i % 4); j++, out++)
			*out = '=';
	
	*out = 0;
	
	return res;
}

/*
 * gg_base64_decode()
 *
 * dekoduje ci±g znaków z base64.
 *
 *  - buf - ci±g znaków.
 *
 * zaalokowany bufor.
 */
char *gg_base64_decode(const char *buf)
{
	char *res, *save, *foo, val;
	const char *end;
	unsigned int index = 0;

	if (!buf)
		return NULL;
	
	save = res = calloc(1, (strlen(buf) / 4 + 1) * 3 + 2);

	if (!save)
		return NULL;

	end = buf + strlen(buf);

	while (*buf && buf < end) {
		if (*buf == '\r' || *buf == '\n') {
			buf++;
			continue;
		}
		if (!(foo = strchr(gg_base64_charset, *buf)))
			foo = gg_base64_charset;
		val = (int)(foo - gg_base64_charset);
		buf++;
		switch (index) {
			case 0:
				*res |= val << 2;
				break;
			case 1:
				*res++ |= val >> 4;
				*res |= val << 4;
				break;
			case 2:
				*res++ |= val >> 2;
				*res |= val << 6;
				break;
			case 3:
				*res++ |= val;
				break;
		}
		index++;
		index %= 4;
	}
	*res = 0;
	
	return save;
}

/*
 * gg_proxy_auth() // funkcja wewnêtrzna
 *
 * tworzy nag³ówek autoryzacji dla proxy.
 * 
 * zaalokowany tekst lub NULL, je¶li proxy nie jest w³±czone lub nie wymaga
 * autoryzacji.
 */
char *gg_proxy_auth()
{
	char *tmp, *enc, *out;
	unsigned int tmp_size;
	
	if (!gg_proxy_enabled || !gg_proxy_username || !gg_proxy_password)
		return NULL;

	if (!(tmp = malloc((tmp_size = strlen(gg_proxy_username) + strlen(gg_proxy_password) + 2))))
		return NULL;

	snprintf(tmp, tmp_size, "%s:%s", gg_proxy_username, gg_proxy_password);

	if (!(enc = gg_base64_encode(tmp))) {
		free(tmp);
		return NULL;
	}
	
	free(tmp);

	if (!(out = malloc(strlen(enc) + 40))) {
		free(enc);
		return NULL;
	}
	
	snprintf(out, strlen(enc) + 40,  "Proxy-Authorization: Basic %s\r\n", enc);

	free(enc);

	return out;
}

static uint32_t gg_crc32_table[256];
static int gg_crc32_initialized = 0;

/*
 * gg_crc32_make_table()  // funkcja wewnêtrzna
 */
static void gg_crc32_make_table()
{
	uint32_t h = 1;
	unsigned int i, j;

	memset(gg_crc32_table, 0, sizeof(gg_crc32_table));

	for (i = 128; i; i >>= 1) {
		h = (h >> 1) ^ ((h & 1) ? 0xedb88320L : 0);

		for (j = 0; j < 256; j += 2 * i)
			gg_crc32_table[i + j] = gg_crc32_table[j] ^ h;
	}

	gg_crc32_initialized = 1;
}

/*
 * gg_crc32()
 *
 * wyznacza sumê kontroln± CRC32 danego bloku danych.
 *
 *  - crc - suma kontrola poprzedniego bloku danych lub 0 je¶li pierwszy
 *  - buf - bufor danych
 *  - size - ilo¶æ danych
 *
 * suma kontrolna CRC32.
 */
uint32_t gg_crc32(uint32_t crc, const unsigned char *buf, int len)
{
	if (!gg_crc32_initialized)
		gg_crc32_make_table();

	if (!buf || len < 0)
		return crc;

	crc ^= 0xffffffffL;

	while (len--)
		crc = (crc >> 8) ^ gg_crc32_table[(crc ^ *buf++) & 0xff];

	return crc ^ 0xffffffffL;
}


/*
 * Local variables:
 * c-indentation-style: k&r
 * c-basic-offset: 8
 * indent-tabs-mode: notnil
 * End:
 *
 * vim: shiftwidth=8:
 */
