/**
 * @file utils.c Utility functions
 *
 * gaim
 *
 * Copyright (C) 2003 Christian Hammond <chipx86@gnupdate.org>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "msn.h"

char *
msn_url_decode(const char *str)
{
	static char buf[MSN_BUF_LEN];
	int i, j = 0;
	char *bum;

	g_return_val_if_fail(str != NULL, NULL);

	for (i = 0; i < strlen(str); i++) {
		char hex[3];

		if (str[i] != '%')
			buf[j++] = str[i];
		else {
			strncpy(hex, str + ++i, 2);
			hex[2] = '\0';

			/* i is pointing to the start of the number */
			i++;

			/*
			 * Now it's at the end and at the start of the for loop
			 * will be at the next character.
			 */
			buf[j++] = strtol(hex, NULL, 16);
		}
	}

	buf[j] = '\0';

	if (!g_utf8_validate(buf, -1, (const char **)&bum))
		*bum = '\0';

	return buf;
}

char *
msn_url_encode(const char *str)
{
	static char buf[MSN_BUF_LEN];
	int i, j = 0;

	g_return_val_if_fail(str != NULL, NULL);

	for (i = 0; i < strlen(str); i++) {
		if (isalnum(str[i]))
			buf[j++] = str[i];
		else {
			sprintf(buf + j, "%%%02x", (unsigned char)str[i]);
			j += 3;
		}
	}

	buf[j] = '\0';

	return buf;
}

char *
msn_parse_format(const char *mime)
{
	char *cur;
	GString *ret = g_string_new(NULL);
	guint colorbuf;
	char *colors = (char *)(&colorbuf);

	cur = strstr(mime, "FN=");

	if (cur && (*(cur = cur + 3) != ';')) {
		ret = g_string_append(ret, "<FONT FACE=\"");

		while (*cur && *cur != ';') {
			ret = g_string_append_c(ret, *cur);
			cur++;
		}

		ret = g_string_append(ret, "\">");
	}
	
	cur = strstr(mime, "EF=");

	if (cur && (*(cur = cur + 3) != ';')) {
		while (*cur && *cur != ';') {
			ret = g_string_append_c(ret, '<');
			ret = g_string_append_c(ret, *cur);
			ret = g_string_append_c(ret, '>');
			cur++;
		}
	}

	cur = strstr(mime, "CO=");

	if (cur && (*(cur = cur + 3) != ';')) {
		if (sscanf (cur, "%x;", &colorbuf) == 1) {
			char tag[64];
			g_snprintf(tag, sizeof(tag),
					   "<FONT COLOR=\"#%02hhx%02hhx%02hhx\">",
					   colors[0], colors[1], colors[2]);

			ret = g_string_append(ret, tag);
		}
	}

	cur = msn_url_decode(ret->str);
	g_string_free(ret, TRUE);

	return cur;
}
