/* $Id: pubdir50.c 16856 2006-08-19 01:13:25Z evands $ */

/*
 *  (C) Copyright 2003 Wojtek Kaniewski <wojtekka@irc.pl>
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "libgadu.h"

/*
 * gg_pubdir50_new()
 *
 * tworzy now± zmienn± typu gg_pubdir50_t.
 *
 * zaalokowana zmienna lub NULL w przypadku braku pamiêci.
 */
gg_pubdir50_t gg_pubdir50_new(int type)
{
	gg_pubdir50_t res = malloc(sizeof(struct gg_pubdir50_s));

	gg_debug(GG_DEBUG_FUNCTION, "** gg_pubdir50_new(%d);\n", type);

	if (!res) {
		gg_debug(GG_DEBUG_MISC, "// gg_pubdir50_new() out of memory\n");
		return NULL;
	}

	memset(res, 0, sizeof(struct gg_pubdir50_s));

	res->type = type;

	return res;
}

/*
 * gg_pubdir50_add_n()  // funkcja wewnêtrzna
 *
 * funkcja dodaje lub zastêpuje istniej±ce pole do zapytania lub odpowiedzi.
 *
 *  - req - wska¼nik opisu zapytania,
 *  - num - numer wyniku (0 dla zapytania),
 *  - field - nazwa pola,
 *  - value - warto¶æ pola,
 *
 * 0/-1
 */
static int gg_pubdir50_add_n(gg_pubdir50_t req, int num, const char *field, const char *value)
{
	struct gg_pubdir50_entry *tmp = NULL, *entry;
	char *dupfield, *dupvalue;
	int i;

	gg_debug(GG_DEBUG_FUNCTION, "** gg_pubdir50_add_n(%p, %d, \"%s\", \"%s\");\n", req, num, field, value);

	if (!(dupvalue = strdup(value))) {
		gg_debug(GG_DEBUG_MISC, "// gg_pubdir50_add_n() out of memory\n");
		return -1;
	}

	for (i = 0; i < req->entries_count; i++) {
		if (req->entries[i].num != num || strcmp(req->entries[i].field, field))
			continue;

		free(req->entries[i].value);
		req->entries[i].value = dupvalue;

		return 0;
	}
		
	if (!(dupfield = strdup(field))) {
		gg_debug(GG_DEBUG_MISC, "// gg_pubdir50_add_n() out of memory\n");
		free(dupvalue);
		return -1;
	}

	if (!(tmp = realloc(req->entries, sizeof(struct gg_pubdir50_entry) * (req->entries_count + 1)))) {
		gg_debug(GG_DEBUG_MISC, "// gg_pubdir50_add_n() out of memory\n");
		free(dupfield);
		free(dupvalue);
		return -1;
	}

	req->entries = tmp;

	entry = &req->entries[req->entries_count];
	entry->num = num;
	entry->field = dupfield;
	entry->value = dupvalue;

	req->entries_count++;

	return 0;
}

/*
 * gg_pubdir50_add()
 *
 * funkcja dodaje pole do zapytania.
 *
 *  - req - wska¼nik opisu zapytania,
 *  - field - nazwa pola,
 *  - value - warto¶æ pola,
 *
 * 0/-1
 */
int gg_pubdir50_add(gg_pubdir50_t req, const char *field, const char *value)
{
	return gg_pubdir50_add_n(req, 0, field, value);
}

/*
 * gg_pubdir50_seq_set()
 *
 * ustawia numer sekwencyjny zapytania.
 *
 *  - req - zapytanie,
 *  - seq - nowy numer sekwencyjny.
 *
 * 0/-1.
 */
int gg_pubdir50_seq_set(gg_pubdir50_t req, uint32_t seq)
{
	gg_debug(GG_DEBUG_FUNCTION, "** gg_pubdir50_seq_set(%p, %d);\n", req, seq);
	
	if (!req) {
		gg_debug(GG_DEBUG_MISC, "// gg_pubdir50_seq_set() invalid arguments\n");
		errno = EFAULT;
		return -1;
	}

	req->seq = seq;

	return 0;
}

/*
 * gg_pubdir50_free()
 *
 * zwalnia pamiêæ po zapytaniu lub rezultacie szukania u¿ytkownika.
 *
 *  - s - zwalniana zmienna,
 */
void gg_pubdir50_free(gg_pubdir50_t s)
{
	int i;

	if (!s)
		return;
	
	for (i = 0; i < s->entries_count; i++) {
		free(s->entries[i].field);
		free(s->entries[i].value);
	}

	free(s->entries);
	free(s);
}

/*
 * gg_pubdir50()
 *
 * wysy³a zapytanie katalogu publicznego do serwera.
 *
 *  - sess - sesja,
 *  - req - zapytanie.
 *
 * numer sekwencyjny wyszukiwania lub 0 w przypadku b³êdu.
 */
uint32_t gg_pubdir50(struct gg_session *sess, gg_pubdir50_t req)
{
	int i, size = 5;
	uint32_t res;
	char *buf, *p;
	struct gg_pubdir50_request *r;

	gg_debug(GG_DEBUG_FUNCTION, "** gg_pubdir50(%p, %p);\n", sess, req);
	
	if (!sess || !req) {
		gg_debug(GG_DEBUG_MISC, "// gg_pubdir50() invalid arguments\n");
		errno = EFAULT;
		return 0;
	}

	if (sess->state != GG_STATE_CONNECTED) {
		gg_debug(GG_DEBUG_MISC, "// gg_pubdir50() not connected\n");
		errno = ENOTCONN;
		return 0;
	}

	for (i = 0; i < req->entries_count; i++) {
		/* wyszukiwanie bierze tylko pierwszy wpis */
		if (req->entries[i].num)
			continue;
		
		size += strlen(req->entries[i].field) + 1;
		size += strlen(req->entries[i].value) + 1;
	}

	if (!(buf = malloc(size))) {
		gg_debug(GG_DEBUG_MISC, "// gg_pubdir50() out of memory (%d bytes)\n", size);
		return 0;
	}

	r = (struct gg_pubdir50_request*) buf;
	res = time(NULL);
	r->type = req->type;
	r->seq = (req->seq) ? gg_fix32(req->seq) : gg_fix32(time(NULL));
	req->seq = gg_fix32(r->seq);

	for (i = 0, p = buf + 5; i < req->entries_count; i++) {
		if (req->entries[i].num)
			continue;

		strcpy(p, req->entries[i].field);
		p += strlen(p) + 1;

		strcpy(p, req->entries[i].value);
		p += strlen(p) + 1;
	}

	if (gg_send_packet(sess, GG_PUBDIR50_REQUEST, buf, size, NULL, 0) == -1)
		res = 0;

	free(buf);

	return res;
}

/*
 * gg_pubdir50_handle_reply()  // funkcja wewnêtrzna
 *
 * analizuje przychodz±cy pakiet odpowiedzi i zapisuje wynik w struct gg_event.
 *
 *  - e - opis zdarzenia
 *  - packet - zawarto¶æ pakietu odpowiedzi
 *  - length - d³ugo¶æ pakietu odpowiedzi
 *
 * 0/-1
 */
int gg_pubdir50_handle_reply(struct gg_event *e, const char *packet, int length)
{
	const char *end = packet + length, *p;
	struct gg_pubdir50_reply *r = (struct gg_pubdir50_reply*) packet;
	gg_pubdir50_t res;
	int num = 0;
	
	gg_debug(GG_DEBUG_FUNCTION, "** gg_pubdir50_handle_reply(%p, %p, %d);\n", e, packet, length);

	if (!e || !packet) {
		gg_debug(GG_DEBUG_MISC, "// gg_pubdir50_handle_reply() invalid arguments\n");
		errno = EFAULT;
		return -1;
	}

	if (length < 5) {
		gg_debug(GG_DEBUG_MISC, "// gg_pubdir50_handle_reply() packet too short\n");
		errno = EINVAL;
		return -1;
	}

	if (!(res = gg_pubdir50_new(r->type))) {
		gg_debug(GG_DEBUG_MISC, "// gg_pubdir50_handle_reply() unable to allocate reply\n");
		return -1;
	}

	e->event.pubdir50 = res;

	res->seq = gg_fix32(r->seq);

	switch (res->type) {
		case GG_PUBDIR50_READ:
			e->type = GG_EVENT_PUBDIR50_READ;
			break;

		case GG_PUBDIR50_WRITE:
			e->type = GG_EVENT_PUBDIR50_WRITE;
			break;

		default:
			e->type = GG_EVENT_PUBDIR50_SEARCH_REPLY;
			break;
	}

	/* brak wyników? */
	if (length == 5)
		return 0;

	/* pomiñ pocz±tek odpowiedzi */
	p = packet + 5;

	while (p < end) {
		const char *field, *value;

		field = p;

		/* sprawd¼, czy nie mamy podzia³u na kolejne pole */
		if (!*field) {
			num++;
			field++;
		}

		value = NULL;
		
		for (p = field; p < end; p++) {
			/* je¶li mamy koniec tekstu... */
			if (!*p) {
				/* ...i jeszcze nie mieli¶my warto¶ci pola to
				 * wiemy, ¿e po tym zerze jest warto¶æ... */
				if (!value)
					value = p + 1;
				else
					/* ...w przeciwym wypadku koniec
					 * warto¶ci i mo¿emy wychodziæ
					 * grzecznie z pêtli */
					break;
			}
		}
		
		/* sprawd¼my, czy pole nie wychodzi poza pakiet, ¿eby nie
		 * mieæ segfaultów, je¶li serwer przestanie zakañczaæ pakietów
		 * przez \0 */

		if (p == end) {
			gg_debug(GG_DEBUG_MISC, "// gg_pubdir50_handle_reply() premature end of packet\n");
			goto failure;
		}

		p++;

		/* je¶li dostali¶my namier na nastêpne wyniki, to znaczy ¿e
		 * mamy koniec wyników i nie jest to kolejna osoba. */
		if (!strcasecmp(field, "nextstart")) {
			res->next = atoi(value);
			num--;
		} else {
			if (gg_pubdir50_add_n(res, num, field, value) == -1)
				goto failure;
		}
	}	

	res->count = num + 1;
	
	return 0;

failure:
	gg_pubdir50_free(res);
	return -1;
}

/*
 * gg_pubdir50_get()
 *
 * pobiera informacjê z rezultatu wyszukiwania.
 *
 *  - res - rezultat wyszukiwania,
 *  - num - numer odpowiedzi,
 *  - field - nazwa pola (wielko¶æ liter nie ma znaczenia).
 *
 * warto¶æ pola lub NULL, je¶li nie znaleziono.
 */
const char *gg_pubdir50_get(gg_pubdir50_t res, int num, const char *field)
{
	char *value = NULL;
	int i;

	gg_debug(GG_DEBUG_FUNCTION, "** gg_pubdir50_get(%p, %d, \"%s\");\n", res, num, field);

	if (!res || num < 0 || !field) {
		gg_debug(GG_DEBUG_MISC, "// gg_pubdir50_get() invalid arguments\n");
		errno = EINVAL;
		return NULL;
	}

	for (i = 0; i < res->entries_count; i++) {
		if (res->entries[i].num == num && !strcasecmp(res->entries[i].field, field)) {
			value = res->entries[i].value;
			break;
		}
	}

	return value;
}

/*
 * gg_pubdir50_count()
 *
 * zwraca ilo¶æ wyników danego zapytania.
 *
 *  - res - odpowied¼
 *
 * ilo¶æ lub -1 w przypadku b³êdu.
 */
int gg_pubdir50_count(gg_pubdir50_t res)
{
	return (!res) ? -1 : res->count;
}

/*
 * gg_pubdir50_type()
 *
 * zwraca rodzaj zapytania lub odpowiedzi.
 *
 *  - res - zapytanie lub odpowied¼
 *
 * ilo¶æ lub -1 w przypadku b³êdu.
 */
int gg_pubdir50_type(gg_pubdir50_t res)
{
	return (!res) ? -1 : res->type;
}

/*
 * gg_pubdir50_next()
 *
 * zwraca numer, od którego nale¿y rozpocz±æ kolejne wyszukiwanie, je¶li
 * zale¿y nam na kolejnych wynikach.
 *
 *  - res - odpowied¼
 *
 * numer lub -1 w przypadku b³êdu.
 */
uin_t gg_pubdir50_next(gg_pubdir50_t res)
{
	return (!res) ? (unsigned) -1 : res->next;
}

/*
 * gg_pubdir50_seq()
 *
 * zwraca numer sekwencyjny zapytania lub odpowiedzi.
 *
 *  - res - zapytanie lub odpowied¼
 *
 * numer lub -1 w przypadku b³êdu.
 */
uint32_t gg_pubdir50_seq(gg_pubdir50_t res)
{
	return (!res) ? (unsigned) -1 : res->seq;
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
