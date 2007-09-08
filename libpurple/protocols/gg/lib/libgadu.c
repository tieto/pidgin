/* $Id: libgadu.c 16856 2006-08-19 01:13:25Z evands $ */

/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Wo¼ny <speedy@ziew.org>
 *                          Arkadiusz Mi¶kiewicz <arekm@pld-linux.org>
 *                          Tomasz Chiliñski <chilek@chilan.com>
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301,
 *  USA.
 */

#include <sys/types.h>
#ifndef _WIN32
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef sun
#  include <sys/filio.h>
#endif
#else
#include <io.h>
#include <fcntl.h>
#include <errno.h>
#define SHUT_RDWR SD_BOTH
#endif

#include "libgadu-config.h"

#include <errno.h>
#ifndef _WIN32
#include <netdb.h>
#endif
#ifdef __GG_LIBGADU_HAVE_PTHREAD
#  include <pthread.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef __GG_LIBGADU_HAVE_OPENSSL
#  include <openssl/err.h>
#  include <openssl/rand.h>
#endif

#include "compat.h"
#include "libgadu.h"

int gg_debug_level = 0;
void (*gg_debug_handler)(int level, const char *format, va_list ap) = NULL;

int gg_dcc_port = 0;
unsigned long gg_dcc_ip = 0;

unsigned long gg_local_ip = 0;
/*
 * zmienne opisuj±ce parametry proxy http.
 */
char *gg_proxy_host = NULL;
int gg_proxy_port = 0;
int gg_proxy_enabled = 0;
int gg_proxy_http_only = 0;
char *gg_proxy_username = NULL;
char *gg_proxy_password = NULL;

#ifndef lint 
static char rcsid[]
#ifdef __GNUC__
__attribute__ ((unused))
#endif
= "$Id: libgadu.c 16856 2006-08-19 01:13:25Z evands $";
#endif 

#ifdef _WIN32
/**
 *  Deal with the fact that you can't select() on a win32 file fd.
 *  This makes it practically impossible to tie into purple's event loop.
 *
 *  -This is thanks to Tor Lillqvist.
 *  XXX - Move this to where the rest of the the win32 compatiblity stuff goes when we push the changes back to libgadu.
 */
static int
socket_pipe (int *fds)
{
	SOCKET temp, socket1 = -1, socket2 = -1;
	struct sockaddr_in saddr;
	int len;
	u_long arg;
	fd_set read_set, write_set;
	struct timeval tv;

	temp = socket(AF_INET, SOCK_STREAM, 0);

	if (temp == INVALID_SOCKET) {
		goto out0;
	}

	arg = 1;
	if (ioctlsocket(temp, FIONBIO, &arg) == SOCKET_ERROR) {
		goto out0;
	}

	memset(&saddr, 0, sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_port = 0;
	saddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (bind(temp, (struct sockaddr *)&saddr, sizeof (saddr))) {
		goto out0;
	}

	if (listen(temp, 1) == SOCKET_ERROR) {
		goto out0;
	}

	len = sizeof(saddr);
	if (getsockname(temp, (struct sockaddr *)&saddr, &len)) {
		goto out0;
	}

	socket1 = socket(AF_INET, SOCK_STREAM, 0);

	if (socket1 == INVALID_SOCKET) {
		goto out0;
	}

	arg = 1;
	if (ioctlsocket(socket1, FIONBIO, &arg) == SOCKET_ERROR) {
		goto out1;
	}

	if (connect(socket1, (struct sockaddr  *)&saddr, len) != SOCKET_ERROR ||
			WSAGetLastError() != WSAEWOULDBLOCK) {
		goto out1;
	}

	FD_ZERO(&read_set);
	FD_SET(temp, &read_set);

	tv.tv_sec = 0;
	tv.tv_usec = 0;

	if (select(0, &read_set, NULL, NULL, NULL) == SOCKET_ERROR) {
		goto out1;
	}

	if (!FD_ISSET(temp, &read_set)) {
		goto out1;
	}

	socket2 = accept(temp, (struct sockaddr *) &saddr, &len);
	if (socket2 == INVALID_SOCKET) {
		goto out1;
	}

	FD_ZERO(&write_set);
	FD_SET(socket1, &write_set);

	tv.tv_sec = 0;
	tv.tv_usec = 0;

	if (select(0, NULL, &write_set, NULL, NULL) == SOCKET_ERROR) {
		goto out2;
	}

	if (!FD_ISSET(socket1, &write_set)) {
		goto out2;
	}

	arg = 0;
	if (ioctlsocket(socket1, FIONBIO, &arg) == SOCKET_ERROR) {
		goto out2;
	}

	arg = 0;
	if (ioctlsocket(socket2, FIONBIO, &arg) == SOCKET_ERROR) {
		goto out2;
	}

	fds[0] = socket1;
	fds[1] = socket2;

	closesocket (temp);

	return 0;

out2:
	closesocket (socket2);
out1:
	closesocket (socket1);
out0:
	closesocket (temp);
	errno = EIO;            /* XXX */

	return -1;
}
#endif

/*
 * gg_libgadu_version()
 *
 * zwraca wersjê libgadu.
 *
 *  - brak
 *
 * wersja libgadu.
 */
const char *gg_libgadu_version()
{
	return GG_LIBGADU_VERSION;
}

/*
 * gg_fix32()
 *
 * zamienia kolejno¶æ bajtów w liczbie 32-bitowej tak, by odpowiada³a
 * kolejno¶ci bajtów w protokole GG. ze wzglêdu na LE-owo¶æ serwera,
 * zamienia tylko na maszynach BE-wych.
 *
 *  - x - liczba do zamiany
 *
 * liczba z odpowiedni± kolejno¶ci± bajtów.
 */
uint32_t gg_fix32(uint32_t x)
{
#ifndef __GG_LIBGADU_BIGENDIAN
	return x;
#else
	return (uint32_t)
		(((x & (uint32_t) 0x000000ffU) << 24) |
		((x & (uint32_t) 0x0000ff00U) << 8) |
		((x & (uint32_t) 0x00ff0000U) >> 8) |
		((x & (uint32_t) 0xff000000U) >> 24));
#endif
}

/*
 * gg_fix16()
 *
 * zamienia kolejno¶æ bajtów w liczbie 16-bitowej tak, by odpowiada³a
 * kolejno¶ci bajtów w protokole GG. ze wzglêdu na LE-owo¶æ serwera,
 * zamienia tylko na maszynach BE-wych.
 *
 *  - x - liczba do zamiany
 *
 * liczba z odpowiedni± kolejno¶ci± bajtów.
 */
uint16_t gg_fix16(uint16_t x)
{
#ifndef __GG_LIBGADU_BIGENDIAN
	return x;
#else
	return (uint16_t)
		(((x & (uint16_t) 0x00ffU) << 8) |
		((x & (uint16_t) 0xff00U) >> 8));
#endif
}

/* 
 * gg_login_hash() // funkcja wewnêtrzna
 * 
 * liczy hash z has³a i danego seeda.
 * 
 *  - password - has³o do hashowania
 *  - seed - warto¶æ podana przez serwer
 *
 * hash.
 */
unsigned int gg_login_hash(const unsigned char *password, unsigned int seed)
{
	unsigned int x, y, z;

	y = seed;

	for (x = 0; *password; password++) {
		x = (x & 0xffffff00) | *password;
		y ^= x;
		y += x;
		x <<= 8;
		y ^= x;
		x <<= 8;
		y -= x;
		x <<= 8;
		y ^= x;

		z = y & 0x1F;
		y = (y << z) | (y >> (32 - z));
	}

	return y;
}

#ifndef _WIN32

/*
 * gg_resolve() // funkcja wewnêtrzna
 *
 * tworzy potok, forkuje siê i w drugim procesie zaczyna resolvowaæ 
 * podanego hosta. zapisuje w sesji deskryptor potoku. je¶li co¶ tam
 * bêdzie gotowego, znaczy, ¿e mo¿na wczytaæ struct in_addr. je¶li
 * nie znajdzie, zwraca INADDR_NONE.
 *
 *  - fd - wska¼nik gdzie wrzuciæ deskryptor
 *  - pid - gdzie wrzuciæ pid procesu potomnego
 *  - hostname - nazwa hosta do zresolvowania
 *
 * 0, -1.
 */
int gg_resolve(int *fd, int *pid, const char *hostname)
{
	int pipes[2], res;
	struct in_addr a;
	int errno2;

	gg_debug(GG_DEBUG_FUNCTION, "** gg_resolve(%p, %p, \"%s\");\n", fd, pid, hostname);
	
	if (!fd || !pid) {
		errno = EFAULT;
		return -1;
	}

	if (pipe(pipes) == -1)
		return -1;

	if ((res = fork()) == -1) {
		errno2 = errno;
		close(pipes[0]);
		close(pipes[1]);
		errno = errno2;
		return -1;
	}

	if (!res) {
		if ((a.s_addr = inet_addr(hostname)) == INADDR_NONE) {
			struct in_addr *hn;
		
			if (!(hn = gg_gethostbyname(hostname)))
				a.s_addr = INADDR_NONE;
			else {
				a.s_addr = hn->s_addr;
				free(hn);
			}
		}

		write(pipes[1], &a, sizeof(a));

		_exit(0);
	}

	close(pipes[1]);

	*fd = pipes[0];
	*pid = res;

	return 0;
}
#endif

#ifdef __GG_LIBGADU_HAVE_PTHREAD

struct gg_resolve_pthread_data {
	char *hostname;
	int fd;
};

static void *gg_resolve_pthread_thread(void *arg)
{
	struct gg_resolve_pthread_data *d = arg;
	struct in_addr a;

	pthread_detach(pthread_self());

	if ((a.s_addr = inet_addr(d->hostname)) == INADDR_NONE) {
		struct in_addr *hn;
		
		if (!(hn = gg_gethostbyname(d->hostname)))
			a.s_addr = INADDR_NONE;
		else {
			a.s_addr = hn->s_addr;
			free(hn);
		}
	}

	write(d->fd, &a, sizeof(a));
	close(d->fd);

	free(d->hostname);
	d->hostname = NULL;

	free(d);

	pthread_exit(NULL);

	return NULL;	/* ¿eby kompilator nie marudzi³ */
}

/*
 * gg_resolve_pthread() // funkcja wewnêtrzna
 *
 * tworzy potok, nowy w±tek i w nim zaczyna resolvowaæ podanego hosta.
 * zapisuje w sesji deskryptor potoku. je¶li co¶ tam bêdzie gotowego,
 * znaczy, ¿e mo¿na wczytaæ struct in_addr. je¶li nie znajdzie, zwraca
 * INADDR_NONE.
 *
 *  - fd - wska¼nik do zmiennej przechowuj±cej desktyptor resolvera
 *  - resolver - wska¼nik do wska¼nika resolvera
 *  - hostname - nazwa hosta do zresolvowania
 *
 * 0, -1.
 */
int gg_resolve_pthread(int *fd, void **resolver, const char *hostname)
{
	struct gg_resolve_pthread_data *d = NULL;
	pthread_t *tmp;
	int pipes[2], new_errno;

	gg_debug(GG_DEBUG_FUNCTION, "** gg_resolve_pthread(%p, %p, \"%s\");\n", fd, resolver, hostname);
	
	if (!resolver || !fd || !hostname) {
		gg_debug(GG_DEBUG_MISC, "// gg_resolve_pthread() invalid arguments\n");
		errno = EFAULT;
		return -1;
	}

	if (!(tmp = malloc(sizeof(pthread_t)))) {
		gg_debug(GG_DEBUG_MISC, "// gg_resolve_pthread() out of memory for pthread id\n");
		return -1;
	}
	
	if (pipe(pipes) == -1) {
		gg_debug(GG_DEBUG_MISC, "// gg_resolve_pthread() unable to create pipes (errno=%d, %s)\n", errno, strerror(errno));
		free(tmp);
		return -1;
	}

	if (!(d = malloc(sizeof(*d)))) {
		gg_debug(GG_DEBUG_MISC, "// gg_resolve_pthread() out of memory\n");
		new_errno = errno;
		goto cleanup;
	}
	
	d->hostname = NULL;

	if (!(d->hostname = strdup(hostname))) {
		gg_debug(GG_DEBUG_MISC, "// gg_resolve_pthread() out of memory\n");
		new_errno = errno;
		goto cleanup;
	}

	d->fd = pipes[1];

	if (pthread_create(tmp, NULL, gg_resolve_pthread_thread, d)) {
		gg_debug(GG_DEBUG_MISC, "// gg_resolve_phread() unable to create thread\n");
		new_errno = errno;
		goto cleanup;
	}

	gg_debug(GG_DEBUG_MISC, "// gg_resolve_pthread() %p\n", tmp);

	*resolver = tmp;

	*fd = pipes[0];

	return 0;

cleanup:
	if (d) {
		free(d->hostname);
		free(d);
	}

	close(pipes[0]);
	close(pipes[1]);

	free(tmp);

	errno = new_errno;

	return -1;
}

#elif defined _WIN32

struct gg_resolve_win32thread_data {
	char *hostname;
	int fd;
};

static DWORD WINAPI gg_resolve_win32thread_thread(LPVOID arg)
{
	struct gg_resolve_win32thread_data *d = arg;
	struct in_addr a;

	if ((a.s_addr = inet_addr(d->hostname)) == INADDR_NONE) {
		struct in_addr *hn;
		
		if (!(hn = gg_gethostbyname(d->hostname)))
			a.s_addr = INADDR_NONE;
		else {
			a.s_addr = hn->s_addr;
			free(hn);
		}
	}

	write(d->fd, &a, sizeof(a));
	close(d->fd);

	free(d->hostname);
	d->hostname = NULL;

	free(d);

	return 0;
}


int gg_resolve_win32thread(int *fd, void **resolver, const char *hostname)
{
	struct gg_resolve_win32thread_data *d = NULL;
	HANDLE h;
	DWORD dwTId;
	int pipes[2], new_errno;

	gg_debug(GG_DEBUG_FUNCTION, "** gg_resolve_win32thread(%p, %p, \"%s\");\n", fd, resolver, hostname);
	
	if (!resolver || !fd || !hostname) {
		gg_debug(GG_DEBUG_MISC, "// gg_resolve_win32thread() invalid arguments\n");
		errno = EFAULT;
		return -1;
	}

	if (socket_pipe(pipes) == -1) {
		gg_debug(GG_DEBUG_MISC, "// gg_resolve_win32thread() unable to create pipes (errno=%d, %s)\n", errno, strerror(errno));
		return -1;
	}

	if (!(d = malloc(sizeof(*d)))) {
		gg_debug(GG_DEBUG_MISC, "// gg_resolve_win32thread() out of memory\n");
		new_errno = GetLastError();
		goto cleanup;
	}

	d->hostname = NULL;

	if (!(d->hostname = strdup(hostname))) {
		gg_debug(GG_DEBUG_MISC, "// gg_resolve_win32thread() out of memory\n");
		new_errno = GetLastError();
		goto cleanup;
	}

	d->fd = pipes[1];

	h = CreateThread(NULL, 0, gg_resolve_win32thread_thread,
		d, 0, &dwTId);

	if (h == NULL) {
		gg_debug(GG_DEBUG_MISC, "// gg_resolve_win32thread() unable to create thread\n");
		new_errno = GetLastError();
		goto cleanup;
	}

	*resolver = h;
	*fd = pipes[0];

	return 0;

cleanup:
	if (d) {
		free(d->hostname);
		free(d);
	}

	close(pipes[0]);
	close(pipes[1]);

	errno = new_errno;

	return -1;

}
#endif

/*
 * gg_read() // funkcja pomocnicza
 *
 * czyta z gniazda okre¶lon± ilo¶æ bajtów. bierze pod uwagê, czy mamy
 * po³±czenie zwyk³e czy TLS.
 *
 *  - sess - sesja,
 *  - buf - bufor,
 *  - length - ilo¶æ bajtów,
 *
 * takie same warto¶ci jak read().
 */
int gg_read(struct gg_session *sess, char *buf, int length)
{
	int res;

#ifdef __GG_LIBGADU_HAVE_OPENSSL
	if (sess->ssl) {
		int err;

		res = SSL_read(sess->ssl, buf, length);

		if (res < 0) {
			err = SSL_get_error(sess->ssl, res);

			if (err == SSL_ERROR_WANT_READ)
				errno = EAGAIN;

			return -1;
		}
	} else
#endif
		res = read(sess->fd, buf, length);

	return res;
}

/*
 * gg_write() // funkcja pomocnicza
 *
 * zapisuje do gniazda okre¶lon± ilo¶æ bajtów. bierze pod uwagê, czy mamy
 * po³±czenie zwyk³e czy TLS.
 *
 *  - sess - sesja,
 *  - buf - bufor,
 *  - length - ilo¶æ bajtów,
 *
 * takie same warto¶ci jak write().
 */
int gg_write(struct gg_session *sess, const char *buf, int length)
{
	int res = 0;

#ifdef __GG_LIBGADU_HAVE_OPENSSL
	if (sess->ssl) {
		int err;

		res = SSL_write(sess->ssl, buf, length);

		if (res < 0) {
			err = SSL_get_error(sess->ssl, res);

			if (err == SSL_ERROR_WANT_WRITE)
				errno = EAGAIN;

			return -1;
		}
	} else
#endif
	{
		int written = 0;
		
		while (written < length) {
			res = write(sess->fd, buf + written, length - written);

			if (res == -1) {
				if (errno == EAGAIN)
					continue;
				else
					break;
			} else {
				written += res;
				res = written;
			}
		}
	}

	return res;
}

/*
 * gg_recv_packet() // funkcja wewnêtrzna
 *
 * odbiera jeden pakiet i zwraca wska¼nik do niego. pamiêæ po nim
 * nale¿y zwolniæ za pomoc± free().
 *
 *  - sess - opis sesji
 *
 * w przypadku b³êdu NULL, kod b³êdu w errno. nale¿y zwróciæ uwagê, ¿e gdy
 * po³±czenie jest nieblokuj±ce, a kod b³êdu wynosi EAGAIN, nie uda³o siê
 * odczytaæ ca³ego pakietu i nie nale¿y tego traktowaæ jako b³±d.
 */
void *gg_recv_packet(struct gg_session *sess)
{
	struct gg_header h;
	char *buf = NULL;
	int ret = 0, offset, size = 0;

	gg_debug(GG_DEBUG_FUNCTION, "** gg_recv_packet(%p);\n", sess);
	
	if (!sess) {
		errno = EFAULT;
		return NULL;
	}

	if (sess->recv_left < 1) {
		if (sess->header_buf) {
			memcpy(&h, sess->header_buf, sess->header_done);
			gg_debug(GG_DEBUG_MISC, "// gg_recv_packet() header recv: resuming last read (%d bytes left)\n", sizeof(h) - sess->header_done);
			free(sess->header_buf);
			sess->header_buf = NULL;
		} else
			sess->header_done = 0;

		while (sess->header_done < sizeof(h)) {
			ret = gg_read(sess, (char*) &h + sess->header_done, sizeof(h) - sess->header_done);

			gg_debug(GG_DEBUG_MISC, "// gg_recv_packet() header recv(%d,%p,%d) = %d\n", sess->fd, &h + sess->header_done, sizeof(h) - sess->header_done, ret);

			if (!ret) {
				errno = ECONNRESET;
				gg_debug(GG_DEBUG_MISC, "// gg_recv_packet() header recv() failed: connection broken\n");
				return NULL;
			}

			if (ret == -1) {
				if (errno == EINTR) {
					gg_debug(GG_DEBUG_MISC, "// gg_recv_packet() header recv() interrupted system call, resuming\n");
					continue;
				}

				if (errno == EAGAIN) {
					gg_debug(GG_DEBUG_MISC, "// gg_recv_packet() header recv() incomplete header received\n");

					if (!(sess->header_buf = malloc(sess->header_done))) {
						gg_debug(GG_DEBUG_MISC, "// gg_recv_packet() header recv() not enough memory\n");
						return NULL;
					}

					memcpy(sess->header_buf, &h, sess->header_done);

					return NULL;
				}

				gg_debug(GG_DEBUG_MISC, "// gg_recv_packet() header recv() failed: errno=%d, %s\n", errno, strerror(errno));

				return NULL;
			}

			sess->header_done += ret;

		}

		h.type = gg_fix32(h.type);
		h.length = gg_fix32(h.length);
	} else
		memcpy(&h, sess->recv_buf, sizeof(h));
	
	/* jakie¶ sensowne limity na rozmiar pakietu */
	if (h.length > 65535) {
		gg_debug(GG_DEBUG_MISC, "// gg_recv_packet() invalid packet length (%d)\n", h.length);
		errno = ERANGE;
		return NULL;
	}

	if (sess->recv_left > 0) {
		gg_debug(GG_DEBUG_MISC, "// gg_recv_packet() resuming last gg_recv_packet()\n");
		size = sess->recv_left;
		offset = sess->recv_done;
		buf = sess->recv_buf;
	} else {
		if (!(buf = malloc(sizeof(h) + h.length + 1))) {
			gg_debug(GG_DEBUG_MISC, "// gg_recv_packet() not enough memory for packet data\n");
			return NULL;
		}

		memcpy(buf, &h, sizeof(h));

		offset = 0;
		size = h.length;
	}

	while (size > 0) {
		ret = gg_read(sess, buf + sizeof(h) + offset, size);
		gg_debug(GG_DEBUG_MISC, "// gg_recv_packet() body recv(%d,%p,%d) = %d\n", sess->fd, buf + sizeof(h) + offset, size, ret);
		if (!ret) {
			gg_debug(GG_DEBUG_MISC, "// gg_recv_packet() body recv() failed: connection broken\n");
			errno = ECONNRESET;
			return NULL;
		}
		if (ret > -1 && ret <= size) {
			offset += ret;
			size -= ret;
		} else if (ret == -1) {	
			int errno2 = errno;

			gg_debug(GG_DEBUG_MISC, "// gg_recv_packet() body recv() failed (errno=%d, %s)\n", errno, strerror(errno));
			errno = errno2;

			if (errno == EAGAIN) {
				gg_debug(GG_DEBUG_MISC, "// gg_recv_packet() %d bytes received, %d left\n", offset, size);
				sess->recv_buf = buf;
				sess->recv_left = size;
				sess->recv_done = offset;
				return NULL;
			}
			if (errno != EINTR) {
				free(buf);
				return NULL;
			}
		}
	}

	sess->recv_left = 0;

	if ((gg_debug_level & GG_DEBUG_DUMP)) {
		unsigned int i;

		gg_debug(GG_DEBUG_DUMP, "// gg_recv_packet(%.2x)", h.type);
		for (i = 0; i < sizeof(h) + h.length; i++) 
			gg_debug(GG_DEBUG_DUMP, " %.2x", (unsigned char) buf[i]);
		gg_debug(GG_DEBUG_DUMP, "\n");
	}

	return buf;
}

/*
 * gg_send_packet() // funkcja wewnêtrzna
 *
 * konstruuje pakiet i wysy³a go do serwera.
 *
 *  - sock - deskryptor gniazda
 *  - type - typ pakietu
 *  - payload_1 - pierwsza czê¶æ pakietu
 *  - payload_length_1 - d³ugo¶æ pierwszej czê¶ci
 *  - payload_2 - druga czê¶æ pakietu
 *  - payload_length_2 - d³ugo¶æ drugiej czê¶ci
 *  - ... - kolejne czê¶ci pakietu i ich d³ugo¶ci
 *  - NULL - koñcowym parametr (konieczny!)
 *
 * je¶li siê powiod³o, zwraca 0, w przypadku b³êdu -1. je¶li errno == ENOMEM,
 * zabrak³o pamiêci. inaczej by³ b³±d przy wysy³aniu pakietu. dla errno == 0
 * nie wys³ano ca³ego pakietu.
 */
int gg_send_packet(struct gg_session *sess, int type, ...)
{
	struct gg_header *h;
	char *tmp;
	int tmp_length;
	void *payload;
	unsigned int payload_length;
	va_list ap;
	int res;

	gg_debug(GG_DEBUG_FUNCTION, "** gg_send_packet(%p, 0x%.2x, ...)\n", sess, type);

	tmp_length = sizeof(struct gg_header);

	if (!(tmp = malloc(tmp_length))) {
		gg_debug(GG_DEBUG_MISC, "// gg_send_packet() not enough memory for packet header\n");
		return -1;
	}

	va_start(ap, type);

	payload = va_arg(ap, void *);

	while (payload) {
		char *tmp2;

		payload_length = va_arg(ap, unsigned int);

		if (!(tmp2 = realloc(tmp, tmp_length + payload_length))) {
			gg_debug(GG_DEBUG_MISC, "// gg_send_packet() not enough memory for payload\n");
			free(tmp);
			va_end(ap);
			return -1;
		}

		tmp = tmp2;
		
		memcpy(tmp + tmp_length, payload, payload_length);
		tmp_length += payload_length;

		payload = va_arg(ap, void *);
	}

	va_end(ap);

	h = (struct gg_header*) tmp;
	h->type = gg_fix32(type);
	h->length = gg_fix32(tmp_length - sizeof(struct gg_header));

	if ((gg_debug_level & GG_DEBUG_DUMP)) {
		int i;

		gg_debug(GG_DEBUG_DUMP, "// gg_send_packet(0x%.2x)", gg_fix32(h->type));
		for (i = 0; i < tmp_length; ++i)
			gg_debug(GG_DEBUG_DUMP, " %.2x", (unsigned char) tmp[i]);
		gg_debug(GG_DEBUG_DUMP, "\n");
	}
	
	if ((res = gg_write(sess, tmp, tmp_length)) < tmp_length) {
		gg_debug(GG_DEBUG_MISC, "// gg_send_packet() write() failed. res = %d, errno = %d (%s)\n", res, errno, strerror(errno));
		free(tmp);
		return -1;
	}
	
	free(tmp);	
	return 0;
}

/*
 * gg_session_callback() // funkcja wewnêtrzna
 *
 * wywo³ywany z gg_session->callback, wykonuje gg_watch_fd() i pakuje
 * do gg_session->event jego wynik.
 */
static int gg_session_callback(struct gg_session *s)
{
	if (!s) {
		errno = EFAULT;
		return -1;
	}

	return ((s->event = gg_watch_fd(s)) != NULL) ? 0 : -1;
}

/*
 * gg_login()
 *
 * rozpoczyna procedurê ³±czenia siê z serwerem. resztê obs³uguje siê przez
 * gg_watch_fd().
 *
 * UWAGA! program musi obs³u¿yæ SIGCHLD, je¶li ³±czy siê asynchronicznie,
 * ¿eby poprawnie zamkn±æ proces resolvera.
 *
 *  - p - struktura opisuj±ca pocz±tkowy stan. wymagane pola: uin, 
 *    password
 *
 * w przypadku b³êdu NULL, je¶li idzie dobrze (async) albo posz³o
 * dobrze (sync), zwróci wska¼nik do zaalokowanej struct gg_session.
 */
struct gg_session *gg_login(const struct gg_login_params *p)
{
	struct gg_session *sess = NULL;
	char *hostname;
	int port;

	if (!p) {
		gg_debug(GG_DEBUG_FUNCTION, "** gg_login(%p);\n", p);
		errno = EFAULT;
		return NULL;
	}

	gg_debug(GG_DEBUG_FUNCTION, "** gg_login(%p: [uin=%u, async=%d, ...]);\n", p, p->uin, p->async);

	if (!(sess = malloc(sizeof(struct gg_session)))) {
		gg_debug(GG_DEBUG_MISC, "// gg_login() not enough memory for session data\n");
		goto fail;
	}

	memset(sess, 0, sizeof(struct gg_session));

	if (!p->password || !p->uin) {
		gg_debug(GG_DEBUG_MISC, "// gg_login() invalid arguments. uin and password needed\n");
		errno = EFAULT;
		goto fail;
	}

	if (!(sess->password = strdup(p->password))) {
		gg_debug(GG_DEBUG_MISC, "// gg_login() not enough memory for password\n");
		goto fail;
	}

	if (p->status_descr && !(sess->initial_descr = strdup(p->status_descr))) {
		gg_debug(GG_DEBUG_MISC, "// gg_login() not enough memory for status\n");
		goto fail;
	}

	sess->uin = p->uin;
	sess->state = GG_STATE_RESOLVING;
	sess->check = GG_CHECK_READ;
	sess->timeout = GG_DEFAULT_TIMEOUT;
	sess->async = p->async;
	sess->type = GG_SESSION_GG;
	sess->initial_status = p->status;
	sess->callback = gg_session_callback;
	sess->destroy = gg_free_session;
	sess->port = (p->server_port) ? p->server_port : ((gg_proxy_enabled) ? GG_HTTPS_PORT : GG_DEFAULT_PORT);
	sess->server_addr = p->server_addr;
	sess->external_port = p->external_port;
	sess->external_addr = p->external_addr;
	sess->protocol_version = (p->protocol_version) ? p->protocol_version : GG_DEFAULT_PROTOCOL_VERSION;
	if (p->era_omnix)
		sess->protocol_version |= GG_ERA_OMNIX_MASK;
	if (p->has_audio)
		sess->protocol_version |= GG_HAS_AUDIO_MASK;
	sess->client_version = (p->client_version) ? strdup(p->client_version) : NULL;
	sess->last_sysmsg = p->last_sysmsg;
	sess->image_size = p->image_size;
	sess->pid = -1;

	if (p->tls == 1) {
#ifdef __GG_LIBGADU_HAVE_OPENSSL
		char buf[1024];

		OpenSSL_add_ssl_algorithms();

		if (!RAND_status()) {
			char rdata[1024];
			struct {
				time_t time;
				void *ptr;
			} rstruct;

			time(&rstruct.time);
			rstruct.ptr = (void *) &rstruct;			

			RAND_seed((void *) rdata, sizeof(rdata));
			RAND_seed((void *) &rstruct, sizeof(rstruct));
		}

		sess->ssl_ctx = SSL_CTX_new(TLSv1_client_method());

		if (!sess->ssl_ctx) {
			ERR_error_string_n(ERR_get_error(), buf, sizeof(buf));
			gg_debug(GG_DEBUG_MISC, "// gg_login() SSL_CTX_new() failed: %s\n", buf);
			goto fail;
		}

		SSL_CTX_set_verify(sess->ssl_ctx, SSL_VERIFY_NONE, NULL);

		sess->ssl = SSL_new(sess->ssl_ctx);

		if (!sess->ssl) {
			ERR_error_string_n(ERR_get_error(), buf, sizeof(buf));
			gg_debug(GG_DEBUG_MISC, "// gg_login() SSL_new() failed: %s\n", buf);
			goto fail;
		}
#else
		gg_debug(GG_DEBUG_MISC, "// gg_login() client requested TLS but no support compiled in\n");
#endif
	}
	
	if (gg_proxy_enabled) {
		hostname = gg_proxy_host;
		sess->proxy_port = port = gg_proxy_port;
	} else {
		hostname = GG_APPMSG_HOST;
		port = GG_APPMSG_PORT;
	}

	if (!p->async) {
		struct in_addr a;

		if (!p->server_addr || !p->server_port) {
			if ((a.s_addr = inet_addr(hostname)) == INADDR_NONE) {
				struct in_addr *hn;
	
				if (!(hn = gg_gethostbyname(hostname))) {
					gg_debug(GG_DEBUG_MISC, "// gg_login() host \"%s\" not found\n", hostname);
					goto fail;
				} else {
					a.s_addr = hn->s_addr;
					free(hn);
				}
			}
		} else {
			a.s_addr = p->server_addr;
			port = p->server_port;
		}

		sess->hub_addr = a.s_addr;

		if (gg_proxy_enabled)
			sess->proxy_addr = a.s_addr;

		if ((sess->fd = gg_connect(&a, port, 0)) == -1) {
			gg_debug(GG_DEBUG_MISC, "// gg_login() connection failed (errno=%d, %s)\n", errno, strerror(errno));
			goto fail;
		}

		if (p->server_addr && p->server_port)
			sess->state = GG_STATE_CONNECTING_GG;
		else
			sess->state = GG_STATE_CONNECTING_HUB;

		while (sess->state != GG_STATE_CONNECTED) {
			struct gg_event *e;

			if (!(e = gg_watch_fd(sess))) {
				gg_debug(GG_DEBUG_MISC, "// gg_login() critical error in gg_watch_fd()\n");
				goto fail;
			}

			if (e->type == GG_EVENT_CONN_FAILED) {
				errno = EACCES;
				gg_debug(GG_DEBUG_MISC, "// gg_login() could not login\n");
				gg_event_free(e);
				goto fail;
			}

			gg_event_free(e);
		}

		return sess;
	}
	
	if (!sess->server_addr || gg_proxy_enabled) {
#ifdef __GG_LIBGADU_HAVE_PTHREAD
		if (gg_resolve_pthread(&sess->fd, &sess->resolver, hostname)) {
#elif defined _WIN32
		if (gg_resolve_win32thread(&sess->fd, &sess->resolver, hostname)) {
#else
		if (gg_resolve(&sess->fd, &sess->pid, hostname)) {
#endif
			gg_debug(GG_DEBUG_MISC, "// gg_login() resolving failed (errno=%d, %s)\n", errno, strerror(errno));
			goto fail;
		}
	} else {
		if ((sess->fd = gg_connect(&sess->server_addr, sess->port, sess->async)) == -1) {
			gg_debug(GG_DEBUG_MISC, "// gg_login() direct connection failed (errno=%d, %s)\n", errno, strerror(errno));
			goto fail;
		}
		sess->state = GG_STATE_CONNECTING_GG;
		sess->check = GG_CHECK_WRITE;
	}

	return sess;

fail:
	if (sess) {
		if (sess->password)
			free(sess->password);
		if (sess->initial_descr)
			free(sess->initial_descr);
		free(sess);
	}
	
	return NULL;
}

/* 
 * gg_free_session()
 *
 * próbuje zamkn±æ po³±czenia i zwalnia pamiêæ zajmowan± przez sesjê.
 *
 *  - sess - opis sesji
 */
void gg_free_session(struct gg_session *sess)
{
	if (!sess)
		return;

	/* XXX dopisaæ zwalnianie i zamykanie wszystkiego, co mog³o zostaæ */

	if (sess->password)
		free(sess->password);
	
	if (sess->initial_descr)
		free(sess->initial_descr);

	if (sess->client_version)
		free(sess->client_version);

	if (sess->header_buf)
		free(sess->header_buf);

#ifdef __GG_LIBGADU_HAVE_OPENSSL
	if (sess->ssl)
		SSL_free(sess->ssl);

	if (sess->ssl_ctx)
		SSL_CTX_free(sess->ssl_ctx);
#endif

#ifdef __GG_LIBGADU_HAVE_PTHREAD
	if (sess->resolver) {
		pthread_cancel(*((pthread_t*) sess->resolver));
		free(sess->resolver);
		sess->resolver = NULL;
	}
#elif defined _WIN32
	if (sess->resolver) {
		HANDLE h = sess->resolver;
		TerminateThread(h, 0);
		CloseHandle(h);
		sess->resolver = NULL;
	}
#else
	if (sess->pid != -1)
		waitpid(sess->pid, NULL, WNOHANG);
#endif

	if (sess->fd != -1)
		close(sess->fd);

	while (sess->images)
		gg_image_queue_remove(sess, sess->images, 1);

	free(sess);
}

/*
 * gg_change_status()
 *
 * zmienia status u¿ytkownika. przydatne do /away i /busy oraz /quit.
 *
 *  - sess - opis sesji
 *  - status - nowy status u¿ytkownika
 *
 * 0, -1.
 */
int gg_change_status(struct gg_session *sess, int status)
{
	struct gg_new_status p;

	gg_debug(GG_DEBUG_FUNCTION, "** gg_change_status(%p, %d);\n", sess, status);

	if (!sess) {
		errno = EFAULT;
		return -1;
	}

	if (sess->state != GG_STATE_CONNECTED) {
		errno = ENOTCONN;
		return -1;
	}

	p.status = gg_fix32(status);

	sess->status = status;

	return gg_send_packet(sess, GG_NEW_STATUS, &p, sizeof(p), NULL);
}

/*
 * gg_change_status_descr()
 *
 * zmienia status u¿ytkownika na opisowy.
 *
 *  - sess - opis sesji
 *  - status - nowy status u¿ytkownika
 *  - descr - opis statusu
 *
 * 0, -1.
 */
int gg_change_status_descr(struct gg_session *sess, int status, const char *descr)
{
	struct gg_new_status p;

	gg_debug(GG_DEBUG_FUNCTION, "** gg_change_status_descr(%p, %d, \"%s\");\n", sess, status, descr);

	if (!sess || !descr) {
		errno = EFAULT;
		return -1;
	}

	if (sess->state != GG_STATE_CONNECTED) {
		errno = ENOTCONN;
		return -1;
	}

	p.status = gg_fix32(status);

	sess->status = status;

	return gg_send_packet(sess, GG_NEW_STATUS, &p, sizeof(p), descr, (strlen(descr) > GG_STATUS_DESCR_MAXSIZE) ? GG_STATUS_DESCR_MAXSIZE : strlen(descr), NULL);
}

/*
 * gg_change_status_descr_time()
 *
 * zmienia status u¿ytkownika na opisowy z godzin± powrotu.
 *
 *  - sess - opis sesji
 *  - status - nowy status u¿ytkownika
 *  - descr - opis statusu
 *  - time - czas w formacie uniksowym
 *
 * 0, -1.
 */
int gg_change_status_descr_time(struct gg_session *sess, int status, const char *descr, int time)
{
	struct gg_new_status p;
	uint32_t newtime;

	gg_debug(GG_DEBUG_FUNCTION, "** gg_change_status_descr_time(%p, %d, \"%s\", %d);\n", sess, status, descr, time);

	if (!sess || !descr || !time) {
		errno = EFAULT;
		return -1;
	}

	if (sess->state != GG_STATE_CONNECTED) {
		errno = ENOTCONN;
		return -1;
	}

	p.status = gg_fix32(status);

	sess->status = status;

	newtime = gg_fix32(time);

	return gg_send_packet(sess, GG_NEW_STATUS, &p, sizeof(p), descr, (strlen(descr) > GG_STATUS_DESCR_MAXSIZE) ? GG_STATUS_DESCR_MAXSIZE : strlen(descr), &newtime, sizeof(newtime), NULL);
}

/*
 * gg_logoff()
 *
 * wylogowuje u¿ytkownika i zamyka po³±czenie, ale nie zwalnia pamiêci.
 *
 *  - sess - opis sesji
 */
void gg_logoff(struct gg_session *sess)
{
	if (!sess)
		return;

	gg_debug(GG_DEBUG_FUNCTION, "** gg_logoff(%p);\n", sess);

	if (GG_S_NA(sess->status & ~GG_STATUS_FRIENDS_MASK))
		gg_change_status(sess, GG_STATUS_NOT_AVAIL);

#ifdef __GG_LIBGADU_HAVE_OPENSSL
	if (sess->ssl)
		SSL_shutdown(sess->ssl);
#endif

#ifdef __GG_LIBGADU_HAVE_PTHREAD
	if (sess->resolver) {
		pthread_cancel(*((pthread_t*) sess->resolver));
		free(sess->resolver);
		sess->resolver = NULL;
	}
#elif defined _WIN32
	if (sess->resolver) {
		HANDLE h = sess->resolver;
		TerminateThread(h, 0);
		CloseHandle(h);
		sess->resolver = NULL;
	}
#else
	if (sess->pid != -1) {
		waitpid(sess->pid, NULL, WNOHANG);
		sess->pid = -1;
	}
#endif
	
	if (sess->fd != -1) {
		shutdown(sess->fd, SHUT_RDWR);
		close(sess->fd);
		sess->fd = -1;
	}
}

/*
 * gg_image_request()
 *
 * wysy³a ¿±danie wys³ania obrazka o podanych parametrach.
 *
 *  - sess - opis sesji
 *  - recipient - numer adresata
 *  - size - rozmiar obrazka
 *  - crc32 - suma kontrolna obrazka
 *
 * 0/-1
 */
int gg_image_request(struct gg_session *sess, uin_t recipient, int size, uint32_t crc32)
{
	struct gg_send_msg s;
	struct gg_msg_image_request r;
	char dummy = 0;
	int res;

	gg_debug(GG_DEBUG_FUNCTION, "** gg_image_request(%p, %d, %u, 0x%.4x);\n", sess, recipient, size, crc32);

	if (!sess) {
		errno = EFAULT;
		return -1;
	}
	
	if (sess->state != GG_STATE_CONNECTED) {
		errno = ENOTCONN;
		return -1;
	}

	if (size < 0) {
		errno = EINVAL;
		return -1;
	}

	s.recipient = gg_fix32(recipient);
	s.seq = gg_fix32(0);
	s.msgclass = gg_fix32(GG_CLASS_MSG);

	r.flag = 0x04;
	r.size = gg_fix32(size);
	r.crc32 = gg_fix32(crc32);
	
	res = gg_send_packet(sess, GG_SEND_MSG, &s, sizeof(s), &dummy, 1, &r, sizeof(r), NULL);

	if (!res) {
		struct gg_image_queue *q = malloc(sizeof(*q));
		char *buf;

		if (!q) {
			gg_debug(GG_DEBUG_MISC, "// gg_image_request() not enough memory for image queue\n");
			return -1;
		}

		buf = malloc(size);
		if (size && !buf)
		{
			gg_debug(GG_DEBUG_MISC, "// gg_image_request() not enough memory for image\n");
			free(q);
			return -1;
		}

		memset(q, 0, sizeof(*q));

		q->sender = recipient;
		q->size = size;
		q->crc32 = crc32;
		q->image = buf;

		if (!sess->images)
			sess->images = q;
		else {
			struct gg_image_queue *qq;

			for (qq = sess->images; qq->next; qq = qq->next)
				;

			qq->next = q;
		}
	}

	return res;
}

/*
 * gg_image_reply()
 *
 * wysy³a ¿±dany obrazek.
 *
 *  - sess - opis sesji
 *  - recipient - numer adresata
 *  - filename - nazwa pliku
 *  - image - bufor z obrazkiem
 *  - size - rozmiar obrazka
 *
 * 0/-1
 */
int gg_image_reply(struct gg_session *sess, uin_t recipient, const char *filename, const unsigned char *image, int size)
{
	struct gg_msg_image_reply *r;
	struct gg_send_msg s;
	const char *tmp;
	char buf[1910];
	int res = -1;

	gg_debug(GG_DEBUG_FUNCTION, "** gg_image_reply(%p, %d, \"%s\", %p, %d);\n", sess, recipient, filename, image, size);

	if (!sess || !filename || !image) {
		errno = EFAULT;
		return -1;
	}

	if (sess->state != GG_STATE_CONNECTED) {
		errno = ENOTCONN;
		return -1;
	}

	if (size < 0) {
		errno = EINVAL;
		return -1;
	}

	/* wytnij ¶cie¿ki, zostaw tylko nazwê pliku */
	while ((tmp = strrchr(filename, '/')) || (tmp = strrchr(filename, '\\')))
		filename = tmp + 1;

	if (strlen(filename) < 1 || strlen(filename) > 1024) {
		errno = EINVAL;
		return -1;
	}
	
	s.recipient = gg_fix32(recipient);
	s.seq = gg_fix32(0);
	s.msgclass = gg_fix32(GG_CLASS_MSG);

	buf[0] = 0;
	r = (void*) &buf[1];

	r->flag = 0x05;
	r->size = gg_fix32(size);
	r->crc32 = gg_fix32(gg_crc32(0, image, size));

	while (size > 0) {
		size_t buflen, chunklen;
		
		/* \0 + struct gg_msg_image_reply */
		buflen = sizeof(struct gg_msg_image_reply) + 1;

		/* w pierwszym kawa³ku jest nazwa pliku */
		if (r->flag == 0x05) {
			strcpy(buf + buflen, filename);
			buflen += strlen(filename) + 1;
		}

		chunklen = ((size_t)size >= sizeof(buf) - buflen) ? (sizeof(buf) - buflen) : (size_t)size;

		memcpy(buf + buflen, image, chunklen);
		size -= chunklen;
		image += chunklen;
		
		res = gg_send_packet(sess, GG_SEND_MSG, &s, sizeof(s), buf, buflen + chunklen, NULL);

		if (res == -1)
			break;

		r->flag = 0x06;
	}

	return res;
}

/*
 * gg_send_message_ctcp()
 *
 * wysy³a wiadomo¶æ do innego u¿ytkownika. zwraca losowy numer
 * sekwencyjny, który mo¿na zignorowaæ albo wykorzystaæ do potwierdzenia.
 *
 *  - sess - opis sesji
 *  - msgclass - rodzaj wiadomo¶ci
 *  - recipient - numer adresata
 *  - message - tre¶æ wiadomo¶ci
 *  - message_len - d³ugo¶æ
 *
 * numer sekwencyjny wiadomo¶ci lub -1 w przypadku b³êdu.
 */
int gg_send_message_ctcp(struct gg_session *sess, int msgclass, uin_t recipient, const unsigned char *message, int message_len)
{
	struct gg_send_msg s;

	gg_debug(GG_DEBUG_FUNCTION, "** gg_send_message_ctcp(%p, %d, %u, ...);\n", sess, msgclass, recipient);

	if (!sess) {
		errno = EFAULT;
		return -1;
	}
	
	if (sess->state != GG_STATE_CONNECTED) {
		errno = ENOTCONN;
		return -1;
	}

	s.recipient = gg_fix32(recipient);
	s.seq = gg_fix32(0);
	s.msgclass = gg_fix32(msgclass);
	
	return gg_send_packet(sess, GG_SEND_MSG, &s, sizeof(s), message, message_len, NULL);
}

/*
 * gg_send_message()
 *
 * wysy³a wiadomo¶æ do innego u¿ytkownika. zwraca losowy numer
 * sekwencyjny, który mo¿na zignorowaæ albo wykorzystaæ do potwierdzenia.
 *
 *  - sess - opis sesji
 *  - msgclass - rodzaj wiadomo¶ci
 *  - recipient - numer adresata
 *  - message - tre¶æ wiadomo¶ci
 *
 * numer sekwencyjny wiadomo¶ci lub -1 w przypadku b³êdu.
 */
int gg_send_message(struct gg_session *sess, int msgclass, uin_t recipient, const unsigned char *message)
{
	gg_debug(GG_DEBUG_FUNCTION, "** gg_send_message(%p, %d, %u, %p)\n", sess, msgclass, recipient, message);

	return gg_send_message_richtext(sess, msgclass, recipient, message, NULL, 0);
}

/*
 * gg_send_message_richtext()
 *
 * wysy³a kolorow± wiadomo¶æ do innego u¿ytkownika. zwraca losowy numer
 * sekwencyjny, który mo¿na zignorowaæ albo wykorzystaæ do potwierdzenia.
 *
 *  - sess - opis sesji
 *  - msgclass - rodzaj wiadomo¶ci
 *  - recipient - numer adresata
 *  - message - tre¶æ wiadomo¶ci
 *  - format - informacje o formatowaniu
 *  - formatlen - d³ugo¶æ informacji o formatowaniu
 *
 * numer sekwencyjny wiadomo¶ci lub -1 w przypadku b³êdu.
 */
int gg_send_message_richtext(struct gg_session *sess, int msgclass, uin_t recipient, const unsigned char *message, const unsigned char *format, int formatlen)
{
	struct gg_send_msg s;

	gg_debug(GG_DEBUG_FUNCTION, "** gg_send_message_richtext(%p, %d, %u, %p, %p, %d);\n", sess, msgclass, recipient, message, format, formatlen);

	if (!sess) {
		errno = EFAULT;
		return -1;
	}

	if (sess->state != GG_STATE_CONNECTED) {
		errno = ENOTCONN;
		return -1;
	}

	if (!message) {
		errno = EFAULT;
		return -1;
	}
	
	s.recipient = gg_fix32(recipient);
	if (!sess->seq)
		sess->seq = 0x01740000 | (rand() & 0xffff);
	s.seq = gg_fix32(sess->seq);
	s.msgclass = gg_fix32(msgclass);
	sess->seq += (rand() % 0x300) + 0x300;
	
	if (gg_send_packet(sess, GG_SEND_MSG, &s, sizeof(s), message, strlen((const char *)message) + 1, format, formatlen, NULL) == -1)
		return -1;

	return gg_fix32(s.seq);
}

/*
 * gg_send_message_confer()
 *
 * wysy³a wiadomo¶æ do kilku u¿ytkownikow (konferencja). zwraca losowy numer
 * sekwencyjny, który mo¿na zignorowaæ albo wykorzystaæ do potwierdzenia.
 *
 *  - sess - opis sesji
 *  - msgclass - rodzaj wiadomo¶ci
 *  - recipients_count - ilo¶æ adresatów
 *  - recipients - numerki adresatów
 *  - message - tre¶æ wiadomo¶ci
 *
 * numer sekwencyjny wiadomo¶ci lub -1 w przypadku b³êdu.
 */
int gg_send_message_confer(struct gg_session *sess, int msgclass, int recipients_count, uin_t *recipients, const unsigned char *message)
{
	gg_debug(GG_DEBUG_FUNCTION, "** gg_send_message_confer(%p, %d, %d, %p, %p);\n", sess, msgclass, recipients_count, recipients, message);

	return gg_send_message_confer_richtext(sess, msgclass, recipients_count, recipients, message, NULL, 0);
}

/*
 * gg_send_message_confer_richtext()
 *
 * wysy³a kolorow± wiadomo¶æ do kilku u¿ytkownikow (konferencja). zwraca
 * losowy numer sekwencyjny, który mo¿na zignorowaæ albo wykorzystaæ do
 * potwierdzenia.
 *
 *  - sess - opis sesji
 *  - msgclass - rodzaj wiadomo¶ci
 *  - recipients_count - ilo¶æ adresatów
 *  - recipients - numerki adresatów
 *  - message - tre¶æ wiadomo¶ci
 *  - format - informacje o formatowaniu
 *  - formatlen - d³ugo¶æ informacji o formatowaniu
 *
 * numer sekwencyjny wiadomo¶ci lub -1 w przypadku b³êdu.
 */
int gg_send_message_confer_richtext(struct gg_session *sess, int msgclass, int recipients_count, uin_t *recipients, const unsigned char *message, const unsigned char *format, int formatlen)
{
	struct gg_send_msg s;
	struct gg_msg_recipients r;
	int i, j, k;
	uin_t *recps;
		
	gg_debug(GG_DEBUG_FUNCTION, "** gg_send_message_confer_richtext(%p, %d, %d, %p, %p, %p, %d);\n", sess, msgclass, recipients_count, recipients, message, format, formatlen);

	if (!sess) {
		errno = EFAULT;
		return -1;
	}

	if (sess->state != GG_STATE_CONNECTED) {
		errno = ENOTCONN;
		return -1;
	}

	if (!message || recipients_count <= 0 || recipients_count > 0xffff || !recipients) {
		errno = EINVAL;
		return -1;
	}
	
	r.flag = 0x01;
	r.count = gg_fix32(recipients_count - 1);
	
	if (!sess->seq)
		sess->seq = 0x01740000 | (rand() & 0xffff);
	s.seq = gg_fix32(sess->seq);
	s.msgclass = gg_fix32(msgclass);

	recps = malloc(sizeof(uin_t) * recipients_count);
	if (!recps)
		return -1;

	for (i = 0; i < recipients_count; i++) {
	 
		s.recipient = gg_fix32(recipients[i]);
		
		for (j = 0, k = 0; j < recipients_count; j++)
			if (recipients[j] != recipients[i]) {
				recps[k] = gg_fix32(recipients[j]);
				k++;
			}
				
		if (!i)
			sess->seq += (rand() % 0x300) + 0x300;
		
		if (gg_send_packet(sess, GG_SEND_MSG, &s, sizeof(s), message, strlen((const char *)message) + 1, &r, sizeof(r), recps, (recipients_count - 1) * sizeof(uin_t), format, formatlen, NULL) == -1) {
			free(recps);
			return -1;
		}
	}

	free(recps);
	
	return gg_fix32(s.seq);
}

/*
 * gg_ping()
 *
 * wysy³a do serwera pakiet ping.
 *
 *  - sess - opis sesji
 *
 * 0, -1.
 */
int gg_ping(struct gg_session *sess)
{
	gg_debug(GG_DEBUG_FUNCTION, "** gg_ping(%p);\n", sess);

	if (!sess) {
		errno = EFAULT;
		return -1;
	}

	if (sess->state != GG_STATE_CONNECTED) {
		errno = ENOTCONN;
		return -1;
	}

	return gg_send_packet(sess, GG_PING, NULL);
}

/*
 * gg_notify_ex()
 *
 * wysy³a serwerowi listê kontaktów (wraz z odpowiadaj±cymi im typami userów),
 * dziêki czemu wie, czyj stan nas interesuje.
 *
 *  - sess - opis sesji
 *  - userlist - wska¼nik do tablicy numerów
 *  - types - wska¼nik do tablicy typów u¿ytkowników
 *  - count - ilo¶æ numerków
 *
 * 0, -1.
 */
int gg_notify_ex(struct gg_session *sess, uin_t *userlist, char *types, int count)
{
	struct gg_notify *n;
	uin_t *u;
	char *t;
	int i, res = 0;

	gg_debug(GG_DEBUG_FUNCTION, "** gg_notify_ex(%p, %p, %p, %d);\n", sess, userlist, types, count);
	
	if (!sess) {
		errno = EFAULT;
		return -1;
	}
	
	if (sess->state != GG_STATE_CONNECTED) {
		errno = ENOTCONN;
		return -1;
	}

	if (!userlist || !count)
		return gg_send_packet(sess, GG_LIST_EMPTY, NULL);
	
	while (count > 0) {
		int part_count, packet_type;
		
		if (count > 400) {
			part_count = 400;
			packet_type = GG_NOTIFY_FIRST;
		} else {
			part_count = count;
			packet_type = GG_NOTIFY_LAST;
		}

		if (!(n = (struct gg_notify*) malloc(sizeof(*n) * part_count)))
			return -1;
	
		for (u = userlist, t = types, i = 0; i < part_count; u++, t++, i++) { 
			n[i].uin = gg_fix32(*u);
			n[i].dunno1 = *t;
		}
	
		if (gg_send_packet(sess, packet_type, n, sizeof(*n) * part_count, NULL) == -1) {
			free(n);
			res = -1;
			break;
		}

		count -= part_count;
		userlist += part_count;
		types += part_count;

		free(n);
	}

	return res;
}

/*
 * gg_notify()
 *
 * wysy³a serwerowi listê kontaktów, dziêki czemu wie, czyj stan nas
 * interesuje.
 *
 *  - sess - opis sesji
 *  - userlist - wska¼nik do tablicy numerów
 *  - count - ilo¶æ numerków
 *
 * 0, -1.
 */
int gg_notify(struct gg_session *sess, uin_t *userlist, int count)
{
	struct gg_notify *n;
	uin_t *u;
	int i, res = 0;

	gg_debug(GG_DEBUG_FUNCTION, "** gg_notify(%p, %p, %d);\n", sess, userlist, count);
	
	if (!sess) {
		errno = EFAULT;
		return -1;
	}
	
	if (sess->state != GG_STATE_CONNECTED) {
		errno = ENOTCONN;
		return -1;
	}

	if (!userlist || !count)
		return gg_send_packet(sess, GG_LIST_EMPTY, NULL);
	
	while (count > 0) {
		int part_count, packet_type;
		
		if (count > 400) {
			part_count = 400;
			packet_type = GG_NOTIFY_FIRST;
		} else {
			part_count = count;
			packet_type = GG_NOTIFY_LAST;
		}
			
		if (!(n = (struct gg_notify*) malloc(sizeof(*n) * part_count)))
			return -1;
	
		for (u = userlist, i = 0; i < part_count; u++, i++) { 
			n[i].uin = gg_fix32(*u);
			n[i].dunno1 = GG_USER_NORMAL;
		}
	
		if (gg_send_packet(sess, packet_type, n, sizeof(*n) * part_count, NULL) == -1) {
			res = -1;
			free(n);
			break;
		}

		free(n);

		userlist += part_count;
		count -= part_count;
	}

	return res;
}

/*
 * gg_add_notify_ex()
 *
 * dodaje do listy kontaktów dany numer w trakcie po³±czenia.
 * dodawanemu u¿ytkownikowi okre¶lamy jego typ (patrz protocol.html)
 *
 *  - sess - opis sesji
 *  - uin - numer
 *  - type - typ
 *
 * 0, -1.
 */
int gg_add_notify_ex(struct gg_session *sess, uin_t uin, char type)
{
	struct gg_add_remove a;

	gg_debug(GG_DEBUG_FUNCTION, "** gg_add_notify_ex(%p, %u, %d);\n", sess, uin, type);
	
	if (!sess) {
		errno = EFAULT;
		return -1;
	}

	if (sess->state != GG_STATE_CONNECTED) {
		errno = ENOTCONN;
		return -1;
	}
	
	a.uin = gg_fix32(uin);
	a.dunno1 = type;
	
	return gg_send_packet(sess, GG_ADD_NOTIFY, &a, sizeof(a), NULL);
}

/*
 * gg_add_notify()
 *
 * dodaje do listy kontaktów dany numer w trakcie po³±czenia.
 *
 *  - sess - opis sesji
 *  - uin - numer
 *
 * 0, -1.
 */
int gg_add_notify(struct gg_session *sess, uin_t uin)
{
	return gg_add_notify_ex(sess, uin, GG_USER_NORMAL);
}

/*
 * gg_remove_notify_ex()
 *
 * usuwa z listy kontaktów w trakcie po³±czenia.
 * usuwanemu u¿ytkownikowi okre¶lamy jego typ (patrz protocol.html)
 *
 *  - sess - opis sesji
 *  - uin - numer
 *  - type - typ
 *
 * 0, -1.
 */
int gg_remove_notify_ex(struct gg_session *sess, uin_t uin, char type)
{
	struct gg_add_remove a;

	gg_debug(GG_DEBUG_FUNCTION, "** gg_remove_notify_ex(%p, %u, %d);\n", sess, uin, type);
	
	if (!sess) {
		errno = EFAULT;
		return -1;
	}

	if (sess->state != GG_STATE_CONNECTED) {
		errno = ENOTCONN;
		return -1;
	}

	a.uin = gg_fix32(uin);
	a.dunno1 = type;
	
	return gg_send_packet(sess, GG_REMOVE_NOTIFY, &a, sizeof(a), NULL);
}

/*
 * gg_remove_notify()
 *
 * usuwa z listy kontaktów w trakcie po³±czenia.
 *
 *  - sess - opis sesji
 *  - uin - numer
 *
 * 0, -1.
 */
int gg_remove_notify(struct gg_session *sess, uin_t uin)
{
	return gg_remove_notify_ex(sess, uin, GG_USER_NORMAL);
}

/*
 * gg_userlist_request()
 *
 * wysy³a ¿±danie/zapytanie listy kontaktów na serwerze.
 *
 *  - sess - opis sesji
 *  - type - rodzaj zapytania/¿±dania
 *  - request - tre¶æ zapytania/¿±dania (mo¿e byæ NULL)
 *
 * 0, -1
 */
int gg_userlist_request(struct gg_session *sess, char type, const char *request)
{
	int len;

	if (!sess) {
		errno = EFAULT;
		return -1;
	}
	
	if (sess->state != GG_STATE_CONNECTED) {
		errno = ENOTCONN;
		return -1;
	}

	if (!request) {
		sess->userlist_blocks = 1;
		return gg_send_packet(sess, GG_USERLIST_REQUEST, &type, sizeof(type), NULL);
	}
	
	len = strlen(request);

	sess->userlist_blocks = 0;

	while (len > 2047) {
		sess->userlist_blocks++;

		if (gg_send_packet(sess, GG_USERLIST_REQUEST, &type, sizeof(type), request, 2047, NULL) == -1)
			return -1;

		if (type == GG_USERLIST_PUT)
			type = GG_USERLIST_PUT_MORE;

		request += 2047;
		len -= 2047;
	}

	sess->userlist_blocks++;

	return gg_send_packet(sess, GG_USERLIST_REQUEST, &type, sizeof(type), request, len, NULL);
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
