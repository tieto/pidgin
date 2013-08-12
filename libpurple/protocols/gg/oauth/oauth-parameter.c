/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * Code adapted from libgadu (C) 2008 Wojtek Kaniewski <wojtekka@irc.pl>
 * (http://toxygen.net/libgadu/) during Google Summer of Code 2012
 * by Tomek Wasilczyk (http://www.wasilczyk.pl).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#include "oauth-parameter.h"

struct gg_oauth_parameter {
	char *key;
	char *value;
	struct gg_oauth_parameter *next;
};

int gg_oauth_parameter_set(gg_oauth_parameter_t **list, const char *key, const char *value)
{
	gg_oauth_parameter_t *p, *new_p;
	char *new_key;
	char *new_value;

	if (value == NULL)
		return 0;

	if (list == NULL)
		return -1;

	new_key = strdup(key);

	if (new_key == NULL)
		return -1;

	new_value = strdup(value);

	if (new_value == NULL) {
		free(new_key);
		return -1;
	}

	new_p = malloc(sizeof(gg_oauth_parameter_t));

	if (new_p == NULL) {
		free(new_key);
		free(new_value);
		return -1;
	}

	memset(new_p, 0, sizeof(gg_oauth_parameter_t));
	new_p->key = new_key;
	new_p->value = new_value;

	if (*list != NULL) {
		p = *list;

		while (p != NULL && p->next != NULL)
			p = p->next;

		p->next = new_p;
	} else {
		*list = new_p;
	}

	return 0;
}

char *gg_oauth_parameter_join(gg_oauth_parameter_t *list, int header)
{
	gg_oauth_parameter_t *p;
	int len = 0;
	char *res, *out;

	if (header)
		len += strlen("OAuth ");

	for (p = list; p; p = p->next) {
		gchar *escaped;
		len += strlen(p->key);

		len += (header) ? 3 : 1;

		escaped = g_uri_escape_string(p->value, NULL, FALSE);
		len += strlen(escaped);
		g_free(escaped);

		if (p->next)
			len += 1;
	}

	res = malloc(len + 1);

	if (res == NULL)
		return NULL;

	out = res;

	*out = 0;

	if (header) {
		strcpy(out, "OAuth ");
		out += strlen(out);
	}

	for (p = list; p; p = p->next) {
		gchar *escaped;
		strcpy(out, p->key);
		out += strlen(p->key);

		strcpy(out++, "=");

		if (header)
			strcpy(out++, "\"");

		escaped = g_uri_escape_string(p->value, NULL, FALSE);
		strcpy(out, escaped);
		out += strlen(escaped);
		g_free(escaped);

		if (header)
			strcpy(out++, "\"");

		if (p->next != NULL)
			strcpy(out++, (header) ? "," : "&");
	}

	return res;
}

void gg_oauth_parameter_free(gg_oauth_parameter_t *list)
{
	while (list != NULL) {
		gg_oauth_parameter_t *next;

		next = list->next;

		free(list->key);
		free(list->value);
		free(list);

		list = next;
	}
}
