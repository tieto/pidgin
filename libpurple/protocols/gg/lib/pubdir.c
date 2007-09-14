/* $Id: pubdir.c 16856 2006-08-19 01:13:25Z evands $ */

/*
 *  (C) Copyright 2001-2002 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Dawid Jarosz <dawjar@poczta.onet.pl>
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

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libgadu.h"

/*
 * gg_register3()
 *
 * rozpoczyna rejestracjê u¿ytkownika protoko³em GG 6.0. wymaga wcze¶niejszego
 * pobrania tokenu za pomoc± funkcji gg_token().
 *
 *  - email - adres e-mail klienta
 *  - password - has³o klienta
 *  - tokenid - identyfikator tokenu
 *  - tokenval - warto¶æ tokenu
 *  - async - po³±czenie asynchroniczne
 *
 * zaalokowana struct gg_http, któr± po¼niej nale¿y zwolniæ
 * funkcj± gg_register_free(), albo NULL je¶li wyst±pi³ b³±d.
 */
struct gg_http *gg_register3(const char *email, const char *password, const char *tokenid, const char *tokenval, int async)
{
	struct gg_http *h;
	char *__pwd, *__email, *__tokenid, *__tokenval, *form, *query;

	if (!email || !password || !tokenid || !tokenval) {
		gg_debug(GG_DEBUG_MISC, "=> register, NULL parameter\n");
		errno = EFAULT;
		return NULL;
	}

	__pwd = gg_urlencode(password);
	__email = gg_urlencode(email);
	__tokenid = gg_urlencode(tokenid);
	__tokenval = gg_urlencode(tokenval);

	if (!__pwd || !__email || !__tokenid || !__tokenval) {
		gg_debug(GG_DEBUG_MISC, "=> register, not enough memory for form fields\n");
		free(__pwd);
		free(__email);
		free(__tokenid);
		free(__tokenval);
		return NULL;
	}

	form = gg_saprintf("pwd=%s&email=%s&tokenid=%s&tokenval=%s&code=%u",
			__pwd, __email, __tokenid, __tokenval,
			gg_http_hash("ss", email, password));

	free(__pwd);
	free(__email);
	free(__tokenid);
	free(__tokenval);

	if (!form) {
		gg_debug(GG_DEBUG_MISC, "=> register, not enough memory for form query\n");
		return NULL;
	}

	gg_debug(GG_DEBUG_MISC, "=> register, %s\n", form);

	query = gg_saprintf(
		"Host: " GG_REGISTER_HOST "\r\n"
		"Content-Type: application/x-www-form-urlencoded\r\n"
		"User-Agent: " GG_HTTP_USERAGENT "\r\n"
		"Content-Length: %d\r\n"
		"Pragma: no-cache\r\n"
		"\r\n"
		"%s",
		(int) strlen(form), form);

	free(form);

	if (!query) {
		gg_debug(GG_DEBUG_MISC, "=> register, not enough memory for query\n");
		return NULL;
	}

	if (!(h = gg_http_connect(GG_REGISTER_HOST, GG_REGISTER_PORT, async, "POST", "/appsvc/fmregister3.asp", query))) {
		gg_debug(GG_DEBUG_MISC, "=> register, gg_http_connect() failed mysteriously\n");
		free(query);
		return NULL;
	}

	h->type = GG_SESSION_REGISTER;

	free(query);

	h->callback = gg_pubdir_watch_fd;
	h->destroy = gg_pubdir_free;
	
	if (!async)
		gg_pubdir_watch_fd(h);
	
	return h;
}

/*
 * gg_unregister3()
 *
 * usuwa konto u¿ytkownika z serwera protoko³em GG 6.0
 *
 *  - uin - numerek GG
 *  - password - has³o klienta
 *  - tokenid - identyfikator tokenu
 *  - tokenval - warto¶æ tokenu
 *  - async - po³±czenie asynchroniczne
 *
 * zaalokowana struct gg_http, któr± po¼niej nale¿y zwolniæ
 * funkcj± gg_unregister_free(), albo NULL je¶li wyst±pi³ b³±d.
 */
struct gg_http *gg_unregister3(uin_t uin, const char *password, const char *tokenid, const char *tokenval, int async)
{
	struct gg_http *h;
	char *__fmpwd, *__pwd, *__tokenid, *__tokenval, *form, *query;

	if (!password || !tokenid || !tokenval) {
		gg_debug(GG_DEBUG_MISC, "=> unregister, NULL parameter\n");
		errno = EFAULT;
		return NULL;
	}

	__pwd = gg_saprintf("%ld", random());
	__fmpwd = gg_urlencode(password);
	__tokenid = gg_urlencode(tokenid);
	__tokenval = gg_urlencode(tokenval);

	if (!__fmpwd || !__pwd || !__tokenid || !__tokenval) {
		gg_debug(GG_DEBUG_MISC, "=> unregister, not enough memory for form fields\n");
		free(__pwd);
		free(__fmpwd);
		free(__tokenid);
		free(__tokenval);
		return NULL;
	}

	form = gg_saprintf("fmnumber=%d&fmpwd=%s&delete=1&pwd=%s&email=deletedaccount@gadu-gadu.pl&tokenid=%s&tokenval=%s&code=%u", uin, __fmpwd, __pwd, __tokenid, __tokenval, gg_http_hash("ss", "deletedaccount@gadu-gadu.pl", __pwd));

	free(__fmpwd);
	free(__pwd);
	free(__tokenid);
	free(__tokenval);

	if (!form) {
		gg_debug(GG_DEBUG_MISC, "=> unregister, not enough memory for form query\n");
		return NULL;
	}

	gg_debug(GG_DEBUG_MISC, "=> unregister, %s\n", form);

	query = gg_saprintf(
		"Host: " GG_REGISTER_HOST "\r\n"
		"Content-Type: application/x-www-form-urlencoded\r\n"
		"User-Agent: " GG_HTTP_USERAGENT "\r\n"
		"Content-Length: %d\r\n"
		"Pragma: no-cache\r\n"
		"\r\n"
		"%s",
		(int) strlen(form), form);

	free(form);

	if (!query) {
		gg_debug(GG_DEBUG_MISC, "=> unregister, not enough memory for query\n");
		return NULL;
	}

	if (!(h = gg_http_connect(GG_REGISTER_HOST, GG_REGISTER_PORT, async, "POST", "/appsvc/fmregister3.asp", query))) {
		gg_debug(GG_DEBUG_MISC, "=> unregister, gg_http_connect() failed mysteriously\n");
		free(query);
		return NULL;
	}

	h->type = GG_SESSION_UNREGISTER;

	free(query);

	h->callback = gg_pubdir_watch_fd;
	h->destroy = gg_pubdir_free;
	
	if (!async)
		gg_pubdir_watch_fd(h);
	
	return h;
}

/*
 * gg_change_passwd4()
 *
 * wysy³a ¿±danie zmiany has³a zgodnie z protoko³em GG 6.0. wymaga
 * wcze¶niejszego pobrania tokenu za pomoc± funkcji gg_token().
 *
 *  - uin - numer
 *  - email - adres e-mail
 *  - passwd - stare has³o
 *  - newpasswd - nowe has³o
 *  - tokenid - identyfikator tokenu
 *  - tokenval - warto¶æ tokenu
 *  - async - po³±czenie asynchroniczne
 *
 * zaalokowana struct gg_http, któr± po¼niej nale¿y zwolniæ
 * funkcj± gg_change_passwd_free(), albo NULL je¶li wyst±pi³ b³±d.
 */
struct gg_http *gg_change_passwd4(uin_t uin, const char *email, const char *passwd, const char *newpasswd, const char *tokenid, const char *tokenval, int async)
{
	struct gg_http *h;
	char *form, *query, *__email, *__fmpwd, *__pwd, *__tokenid, *__tokenval;

	if (!uin || !email || !passwd || !newpasswd || !tokenid || !tokenval) {
		gg_debug(GG_DEBUG_MISC, "=> change, NULL parameter\n");
		errno = EFAULT;
		return NULL;
	}
	
	__fmpwd = gg_urlencode(passwd);
	__pwd = gg_urlencode(newpasswd);
	__email = gg_urlencode(email);
	__tokenid = gg_urlencode(tokenid);
	__tokenval = gg_urlencode(tokenval);

	if (!__fmpwd || !__pwd || !__email || !__tokenid || !__tokenval) {
		gg_debug(GG_DEBUG_MISC, "=> change, not enough memory for form fields\n");
		free(__fmpwd);
		free(__pwd);
		free(__email);
		free(__tokenid);
		free(__tokenval);
		return NULL;
	}
	
	if (!(form = gg_saprintf("fmnumber=%d&fmpwd=%s&pwd=%s&email=%s&tokenid=%s&tokenval=%s&code=%u", uin, __fmpwd, __pwd, __email, __tokenid, __tokenval, gg_http_hash("ss", email, newpasswd)))) {
		gg_debug(GG_DEBUG_MISC, "=> change, not enough memory for form fields\n");
		free(__fmpwd);
		free(__pwd);
		free(__email);
		free(__tokenid);
		free(__tokenval);

		return NULL;
	}
	
	free(__fmpwd);
	free(__pwd);
	free(__email);
	free(__tokenid);
	free(__tokenval);
	
	gg_debug(GG_DEBUG_MISC, "=> change, %s\n", form);

	query = gg_saprintf(
		"Host: " GG_REGISTER_HOST "\r\n"
		"Content-Type: application/x-www-form-urlencoded\r\n"
		"User-Agent: " GG_HTTP_USERAGENT "\r\n"
		"Content-Length: %d\r\n"
		"Pragma: no-cache\r\n"
		"\r\n"
		"%s",
		(int) strlen(form), form);

	free(form);

	if (!query) {
		gg_debug(GG_DEBUG_MISC, "=> change, not enough memory for query\n");
		return NULL;
	}

	if (!(h = gg_http_connect(GG_REGISTER_HOST, GG_REGISTER_PORT, async, "POST", "/appsvc/fmregister3.asp", query))) {
		gg_debug(GG_DEBUG_MISC, "=> change, gg_http_connect() failed mysteriously\n");
		free(query);
		return NULL;
	}

	h->type = GG_SESSION_PASSWD;

	free(query);

	h->callback = gg_pubdir_watch_fd;
	h->destroy = gg_pubdir_free;

	if (!async)
		gg_pubdir_watch_fd(h);

	return h;
}

/*
 * gg_remind_passwd3()
 *
 * wysy³a ¿±danie przypomnienia has³a e-mailem.
 *
 *  - uin - numer
 *  - email - adres e-mail taki, jak ten zapisany na serwerze
 *  - async - po³±czenie asynchroniczne
 *  - tokenid - identyfikator tokenu
 *  - tokenval - warto¶æ tokenu
 *
 * zaalokowana struct gg_http, któr± po¼niej nale¿y zwolniæ
 * funkcj± gg_remind_passwd_free(), albo NULL je¶li wyst±pi³ b³±d.
 */
struct gg_http *gg_remind_passwd3(uin_t uin, const char *email, const char *tokenid, const char *tokenval, int async)
{
	struct gg_http *h;
	char *form, *query, *__tokenid, *__tokenval, *__email;

	if (!tokenid || !tokenval || !email) {
		gg_debug(GG_DEBUG_MISC, "=> remind, NULL parameter\n");
		errno = EFAULT;
		return NULL;
	}
	
	__tokenid = gg_urlencode(tokenid);
	__tokenval = gg_urlencode(tokenval);
	__email = gg_urlencode(email);

	if (!__tokenid || !__tokenval || !__email) {
		gg_debug(GG_DEBUG_MISC, "=> remind, not enough memory for form fields\n");
		free(__tokenid);
		free(__tokenval);
		free(__email);
		return NULL;
	}

	if (!(form = gg_saprintf("userid=%d&code=%u&tokenid=%s&tokenval=%s&email=%s", uin, gg_http_hash("u", uin), __tokenid, __tokenval, __email))) {
		gg_debug(GG_DEBUG_MISC, "=> remind, not enough memory for form fields\n");
		free(__tokenid);
		free(__tokenval);
		free(__email);
		return NULL;
	}

	free(__tokenid);
	free(__tokenval);
	free(__email);
	
	gg_debug(GG_DEBUG_MISC, "=> remind, %s\n", form);

	query = gg_saprintf(
		"Host: " GG_REMIND_HOST "\r\n"
		"Content-Type: application/x-www-form-urlencoded\r\n"
		"User-Agent: " GG_HTTP_USERAGENT "\r\n"
		"Content-Length: %d\r\n"
		"Pragma: no-cache\r\n"
		"\r\n"
		"%s",
		(int) strlen(form), form);

	free(form);

	if (!query) {
		gg_debug(GG_DEBUG_MISC, "=> remind, not enough memory for query\n");
		return NULL;
	}

	if (!(h = gg_http_connect(GG_REMIND_HOST, GG_REMIND_PORT, async, "POST", "/appsvc/fmsendpwd3.asp", query))) {
		gg_debug(GG_DEBUG_MISC, "=> remind, gg_http_connect() failed mysteriously\n");
		free(query);
		return NULL;
	}

	h->type = GG_SESSION_REMIND;

	free(query);

	h->callback = gg_pubdir_watch_fd;
	h->destroy = gg_pubdir_free;

	if (!async)
		gg_pubdir_watch_fd(h);

	return h;
}

/*
 * gg_pubdir_watch_fd()
 *
 * przy asynchronicznych operacjach na katalogu publicznym nale¿y wywo³ywaæ
 * tê funkcjê przy zmianach na obserwowanym deskryptorze.
 *
 *  - h - struktura opisuj±ca po³±czenie
 *
 * je¶li wszystko posz³o dobrze to 0, inaczej -1. operacja bêdzie
 * zakoñczona, je¶li h->state == GG_STATE_DONE. je¶li wyst±pi jaki¶
 * b³±d, to bêdzie tam GG_STATE_ERROR i odpowiedni kod b³êdu w h->error.
 */
int gg_pubdir_watch_fd(struct gg_http *h)
{
	struct gg_pubdir *p;
	char *tmp;

	if (!h) {
		errno = EFAULT;
		return -1;
	}

	if (h->state == GG_STATE_ERROR) {
		gg_debug(GG_DEBUG_MISC, "=> pubdir, watch_fd issued on failed session\n");
		errno = EINVAL;
		return -1;
	}
	
	if (h->state != GG_STATE_PARSING) {
		if (gg_http_watch_fd(h) == -1) {
			gg_debug(GG_DEBUG_MISC, "=> pubdir, http failure\n");
			errno = EINVAL;
			return -1;
		}
	}

	if (h->state != GG_STATE_PARSING)
		return 0;
	
	h->state = GG_STATE_DONE;
	
	if (!(h->data = p = malloc(sizeof(struct gg_pubdir)))) {
		gg_debug(GG_DEBUG_MISC, "=> pubdir, not enough memory for results\n");
		return -1;
	}

	p->success = 0;
	p->uin = 0;
	
	gg_debug(GG_DEBUG_MISC, "=> pubdir, let's parse \"%s\"\n", h->body);

	if ((tmp = strstr(h->body, "success")) || (tmp = strstr(h->body, "results"))) {
		p->success = 1;
		if (tmp[7] == ':')
			p->uin = strtol(tmp + 8, NULL, 0);
		gg_debug(GG_DEBUG_MISC, "=> pubdir, success (uin=%d)\n", p->uin);
	} else
		gg_debug(GG_DEBUG_MISC, "=> pubdir, error.\n");

	return 0;
}

/*
 * gg_pubdir_free()
 *
 * zwalnia pamiêæ po efektach operacji na katalogu publicznym.
 *
 *  - h - zwalniana struktura
 */
void gg_pubdir_free(struct gg_http *h)
{
	if (!h)
		return;
	
	free(h->data);
	gg_http_free(h);
}

/*
 * gg_token()
 *
 * pobiera z serwera token do autoryzacji zak³adania konta, usuwania
 * konta i zmiany has³a.
 *
 * zaalokowana struct gg_http, któr± po¼niej nale¿y zwolniæ
 * funkcj± gg_token_free(), albo NULL je¶li wyst±pi³ b³±d.
 */
struct gg_http *gg_token(int async)
{
	struct gg_http *h;
	const char *query;

	query = "Host: " GG_REGISTER_HOST "\r\n"
		"Content-Type: application/x-www-form-urlencoded\r\n"
		"User-Agent: " GG_HTTP_USERAGENT "\r\n"
		"Content-Length: 0\r\n"
		"Pragma: no-cache\r\n"
		"\r\n";

	if (!(h = gg_http_connect(GG_REGISTER_HOST, GG_REGISTER_PORT, async, "POST", "/appsvc/regtoken.asp", query))) {
		gg_debug(GG_DEBUG_MISC, "=> token, gg_http_connect() failed mysteriously\n");
		return NULL;
	}

	h->type = GG_SESSION_TOKEN;

	h->callback = gg_token_watch_fd;
	h->destroy = gg_token_free;
	
	if (!async)
		gg_token_watch_fd(h);
	
	return h;
}

/*
 * gg_token_watch_fd()
 *
 * przy asynchronicznych operacjach zwi±zanych z tokenem nale¿y wywo³ywaæ
 * tê funkcjê przy zmianach na obserwowanym deskryptorze.
 *
 *  - h - struktura opisuj±ca po³±czenie
 *
 * je¶li wszystko posz³o dobrze to 0, inaczej -1. operacja bêdzie
 * zakoñczona, je¶li h->state == GG_STATE_DONE. je¶li wyst±pi jaki¶
 * b³±d, to bêdzie tam GG_STATE_ERROR i odpowiedni kod b³êdu w h->error.
 */
int gg_token_watch_fd(struct gg_http *h)
{
	if (!h) {
		errno = EFAULT;
		return -1;
	}

	if (h->state == GG_STATE_ERROR) {
		gg_debug(GG_DEBUG_MISC, "=> token, watch_fd issued on failed session\n");
		errno = EINVAL;
		return -1;
	}
	
	if (h->state != GG_STATE_PARSING) {
		if (gg_http_watch_fd(h) == -1) {
			gg_debug(GG_DEBUG_MISC, "=> token, http failure\n");
			errno = EINVAL;
			return -1;
		}
	}

	if (h->state != GG_STATE_PARSING)
		return 0;
	
	/* je¶li h->data jest puste, to ¶ci±gali¶my tokenid i url do niego,
	 * ale je¶li co¶ tam jest, to znaczy, ¿e mamy drugi etap polegaj±cy
	 * na pobieraniu tokenu. */
	if (!h->data) {
		int width, height, length;
		char *url = NULL, *tokenid = NULL, *path, *headers;
		const char *host;
		struct gg_http *h2;
		struct gg_token *t;

		gg_debug(GG_DEBUG_MISC, "=> token body \"%s\"\n", h->body);

		if (h->body && (!(url = malloc(strlen(h->body))) || !(tokenid = malloc(strlen(h->body))))) {
			gg_debug(GG_DEBUG_MISC, "=> token, not enough memory for results\n");
			free(url);
			return -1;
		}
		
		if (!h->body || sscanf(h->body, "%d %d %d\r\n%s\r\n%s", &width, &height, &length, tokenid, url) != 5) {
			gg_debug(GG_DEBUG_MISC, "=> token, parsing failed\n");
			free(url);
			free(tokenid);
			errno = EINVAL;
			return -1;
		}
		
		/* dostali¶my tokenid i wszystkie niezbêdne informacje,
		 * wiêc pobierzmy obrazek z tokenem */

		if (strncmp(url, "http://", 7)) {
			path = gg_saprintf("%s?tokenid=%s", url, tokenid);
			host = GG_REGISTER_HOST;
		} else {
			char *slash = strchr(url + 7, '/');

			if (slash) {
				path = gg_saprintf("%s?tokenid=%s", slash, tokenid);
				*slash = 0;
				host = url + 7;
			} else {
				gg_debug(GG_DEBUG_MISC, "=> token, url parsing failed\n");
				free(url);
				free(tokenid);
				errno = EINVAL;
				return -1;
			}
		}

		if (!path) {
			gg_debug(GG_DEBUG_MISC, "=> token, not enough memory for token url\n");
			free(url);
			free(tokenid);
			return -1;
		}

		if (!(headers = gg_saprintf("Host: %s\r\nUser-Agent: " GG_HTTP_USERAGENT "\r\n\r\n", host))) {
			gg_debug(GG_DEBUG_MISC, "=> token, not enough memory for token url\n");
			free(path);
			free(url);
			free(tokenid);
			return -1;
		}			

		if (!(h2 = gg_http_connect(host, GG_REGISTER_PORT, h->async, "GET", path, headers))) {
			gg_debug(GG_DEBUG_MISC, "=> token, gg_http_connect() failed mysteriously\n");
			free(headers);
			free(url);
			free(path);
			free(tokenid);
			return -1;
		}

		free(headers);
		free(path);
		free(url);

		memcpy(h, h2, sizeof(struct gg_http));
		free(h2);

		h->type = GG_SESSION_TOKEN;

		h->callback = gg_token_watch_fd;
		h->destroy = gg_token_free;
	
		if (!h->async)
			gg_token_watch_fd(h);

		if (!(h->data = t = malloc(sizeof(struct gg_token)))) {
			gg_debug(GG_DEBUG_MISC, "=> token, not enough memory for token data\n");
			free(tokenid);
			return -1;
		}

		t->width = width;
		t->height = height;
		t->length = length;
		t->tokenid = tokenid;
	} else {
		/* obrazek mamy w h->body */
		h->state = GG_STATE_DONE;
	}
	
	return 0;
}

/*
 * gg_token_free()
 *
 * zwalnia pamiêæ po efektach pobierania tokenu.
 *
 *  - h - zwalniana struktura
 */
void gg_token_free(struct gg_http *h)
{
	struct gg_token *t;

	if (!h)
		return;

	if ((t = h->data))
		free(t->tokenid);
	
	free(h->data);
	gg_http_free(h);
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
