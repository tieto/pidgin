/*
 * Gaim
 *
 * Gaim is the legal property of its developers, whose names are too
 * numerous to list here. Please refer to the COPYRIGHT file distributed
 * with this source distribution
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 * USA.
 */

#include <stdio.h>
#include <string.h>

#include <glib.h>
#include <glib/ghash.h>
#include <glib/glist.h>
#include <glib/gstring.h>

/* this should become "util.h" if we ever get this into gaim proper */
#include "debug.h"
#include "mime.h"
#include "util.h"

/**
 * @struct mime_fields
 *
 * Utility structure used in both MIME document and parts, which maps
 * field names to their values, and keeps an easily accessible list of
 * keys.
 */
struct mime_fields {
	GHashTable *map;
	GList *keys;
};

struct _GaimMimeDocument {
	struct mime_fields fields;
	GList *parts;
};

struct _GaimMimePart {
	struct mime_fields fields;
	struct _GaimMimeDocument *doc;
	GString *data;
};

static void
fields_set(struct mime_fields *mf, const char *key, const char *val)
{
	char *k, *v;

	g_return_if_fail(mf != NULL);
	g_return_if_fail(mf->map != NULL);

	k = g_ascii_strdown(key, -1);
	v = g_strdup(val);

	/* append to the keys list only if it's not already there */
	if(! g_hash_table_lookup(mf->map, k)) {
		mf->keys = g_list_append(mf->keys, k);
	}

	/* important to use insert. If the key is already in the table, then
		 it's already in the keys list. Insert will free the new instance
		 of the key rather than the old instance. */
	g_hash_table_insert(mf->map, k, v);
}


static const char *
fields_get(struct mime_fields *mf, const char *key)
{
	char *kdown;
	const char *ret;

	g_return_val_if_fail(mf != NULL, NULL);
	g_return_val_if_fail(mf->map != NULL, NULL);

	kdown = g_ascii_strdown(key, -1);
	ret = g_hash_table_lookup(mf->map, kdown);
	g_free(kdown);

	return ret;
}


static void
fields_init(struct mime_fields *mf)
{
	g_return_if_fail(mf != NULL);

	mf->map = g_hash_table_new_full(g_str_hash, g_str_equal,
					g_free, g_free);
}


static void
fields_loadline(struct mime_fields *mf, const char *line, gsize len)
{
	/* split the line into key: value */
	char *key, *val;
	char **tokens;

	/* feh, need it to be NUL terminated */
	key = g_strndup(line, len);

	/* split */
	val = strchr(key, ':');
	if(! val) {
		g_free(key);
		return;
	}
	*val++ = '\0';

	/* normalize whitespace (sorta) and trim on key and value */
	tokens = g_strsplit(key, "\t\r\n", 0);
	key = g_strjoinv("", tokens);
	key = g_strstrip(key);
	g_strfreev(tokens);

	tokens = g_strsplit(val, "\t\r\n", 0);
	val = g_strjoinv("", tokens);
	val = g_strstrip(val);
	g_strfreev(tokens);

	fields_set(mf, key, val);

	g_free(key);
	g_free(val);
}


static void
fields_load(struct mime_fields *mf, char **buf, gsize *len)
{
	char *tail;

	while ((tail = g_strstr_len(*buf, *len, "\r\n")))
	{
		char *line;
		gsize ln;

		/* determine the current line */
		line = *buf;
		ln = tail - line;

		/* advance our search space past the CRLF */
		*buf = tail + 2;
		*len -= (ln + 2);

		/* empty line, end of headers */
		if(! ln) return;

		/* look out for line continuations */
		if(line[ln-1] == ';') {
			tail = g_strstr_len(*buf, *len, "\r\n");
			if(tail) {
				gsize cln;

				cln = tail - *buf;
				ln = tail - line;

				/* advance our search space past the CRLF (again) */
				*buf = tail + 2;
				*len -= (cln + 2);
			}
		}

		/* process our super-cool line */
		fields_loadline(mf, line, ln);
	}
}


static void
field_write(const char *key, const char *val, GString *str)
{
	g_string_append_printf(str, "%s: %s\r\n", key, val);
}


static void
fields_write(struct mime_fields *mf, GString *str)
{
	g_return_if_fail(mf != NULL);

	g_hash_table_foreach(mf->map, (GHFunc) field_write, str);
	g_string_append(str, "\r\n");
}


static void
fields_destroy(struct mime_fields *mf)
{
	g_return_if_fail(mf != NULL);

	g_hash_table_destroy(mf->map);
	g_list_free(mf->keys);

	mf->map = NULL;
	mf->keys = NULL;
}


static GaimMimePart *
part_new(GaimMimeDocument *doc)
{
	GaimMimePart *part;

	part = g_new0(GaimMimePart, 1);
	fields_init(&part->fields);
	part->doc = doc;
	part->data = g_string_new(NULL);

	doc->parts = g_list_prepend(doc->parts, part);

	return part;
}


static void
part_load(GaimMimePart *part, const char *buf, gsize len)
{

	char *b = (char *) buf;
	gsize n = len;

	fields_load(&part->fields, &b, &n);

	/* the remainder will have a blank line, if there's anything at all,
		 so check if there's anything then trim off the trailing four
		 bytes, \r\n\r\n */
	if(n > 4) n -= 4;
	g_string_append_len(part->data, b, n);
}


static void
part_write(GaimMimePart *part, GString *str)
{
	fields_write(&part->fields, str);
	g_string_append_printf(str, "%s\r\n\r\n", part->data->str);
}


static void
part_free(GaimMimePart *part)
{

	fields_destroy(&part->fields);

	g_string_free(part->data, TRUE);
	part->data = NULL;

	g_free(part);
}


GaimMimePart *
gaim_mime_part_new(GaimMimeDocument *doc)
{
	g_return_val_if_fail(doc != NULL, NULL);
	return part_new(doc);
}


const GList *
gaim_mime_part_get_fields(GaimMimePart *part)
{
	g_return_val_if_fail(part != NULL, NULL);
	return part->fields.keys;
}


const char *
gaim_mime_part_get_field(GaimMimePart *part, const char *field)
{
	g_return_val_if_fail(part != NULL, NULL);
	return fields_get(&part->fields, field);
}


char *
gaim_mime_part_get_field_decoded(GaimMimePart *part, const char *field)
{
	const char *f;

	g_return_val_if_fail(part != NULL, NULL);

	f = fields_get(&part->fields, field);
	return gaim_mime_decode_field(f);
}


void
gaim_mime_part_set_field(GaimMimePart *part, const char *field, const char *value)
{
	g_return_if_fail(part != NULL);
	fields_set(&part->fields, field, value);
}


const char *
gaim_mime_part_get_data(GaimMimePart *part)
{
	g_return_val_if_fail(part != NULL, NULL);
	g_return_val_if_fail(part->data != NULL, NULL);

	return part->data->str;
}


void
gaim_mime_part_get_data_decoded(GaimMimePart *part, guchar **data, gsize *len)
{
	const char *enc;

	g_return_if_fail(part != NULL);
	g_return_if_fail(data != NULL);
	g_return_if_fail(len != NULL);

	g_return_if_fail(part->data != NULL);

	enc = gaim_mime_part_get_field(part, "content-transfer-encoding");

	if(! enc) {
		*data = (guchar *)g_strdup(part->data->str);
		*len = part->data->len;

	} else if(! g_ascii_strcasecmp(enc, "7bit")) {
		*data = (guchar *)g_strdup(part->data->str);
		*len = part->data->len;

	} else if(! g_ascii_strcasecmp(enc, "8bit")) {
		*data = (guchar *)g_strdup(part->data->str);
		*len = part->data->len;

	} else if(! g_ascii_strcasecmp(enc, "base16")) {
		*data = gaim_base16_decode(part->data->str, len);

	} else if(! g_ascii_strcasecmp(enc, "base64")) {
	*data = gaim_base64_decode(part->data->str, len);

	} else if(! g_ascii_strcasecmp(enc, "quoted-printable")) {
		*data = gaim_quotedp_decode(part->data->str, len);

	} else {
		gaim_debug_warning("mime", "gaim_mime_part_get_data_decoded:"
					 " unknown encoding '%s'\n", enc);
		*data = NULL;
		*len = 0;
	}
}


gsize
gaim_mime_part_get_length(GaimMimePart *part)
{
	g_return_val_if_fail(part != NULL, 0);
	g_return_val_if_fail(part->data != NULL, 0);

	return part->data->len;
}


void
gaim_mime_part_set_data(GaimMimePart *part, const char *data)
{
	g_return_if_fail(part != NULL);
	g_string_free(part->data, TRUE);
	part->data = g_string_new(data);
}


GaimMimeDocument *
gaim_mime_document_new()
{
	GaimMimeDocument *doc;

	doc = g_new0(GaimMimeDocument, 1);
	fields_init(&doc->fields);

	return doc;
}


static void
doc_parts_load(GaimMimeDocument *doc, const char *boundary, const char *buf, gsize len)
{
	char *b = (char *) buf;
	gsize n = len;

	const char *bnd;
	gsize bl;

	bnd = g_strdup_printf("--%s", boundary);
	bl = strlen(bnd);

	for(b = g_strstr_len(b, n, bnd); b; ) {
		char *tail;

		/* skip the boundary */
		b += bl;
		n -= bl;

		/* skip the trailing \r\n or -- as well */
		if(n >= 2) {
			b += 2;
			n -= 2;
		}

		/* find the next boundary */
		tail = g_strstr_len(b, n, bnd);

		if(tail) {
			gsize sl;

			sl = tail - b;
			if(sl) {
				GaimMimePart *part = part_new(doc);
				part_load(part, b, sl);
			}
		}

		b = tail;
	}
}


GaimMimeDocument *
gaim_mime_document_parsen(const char *buf, gsize len)
{
	GaimMimeDocument *doc;

	char *b = (char *) buf;
	gsize n = len;

	g_return_val_if_fail(buf != NULL, NULL);

	doc = gaim_mime_document_new();

	if (!len)
		return doc;

	fields_load(&doc->fields, &b, &n);

	{
		const char *ct = fields_get(&doc->fields, "content-type");
		if(ct && gaim_str_has_prefix(ct, "multipart")) {
			char *bd = strrchr(ct, '=');
			if(bd++) {
				doc_parts_load(doc, bd, b, n);
			}
		}
	}

	return doc;
}


GaimMimeDocument *
gaim_mime_document_parse(const char *buf)
{
	g_return_val_if_fail(buf != NULL, NULL);
	return gaim_mime_document_parsen(buf, strlen(buf));
}


void
gaim_mime_document_write(GaimMimeDocument *doc, GString *str)
{
	const char *bd = NULL;

	g_return_if_fail(doc != NULL);
	g_return_if_fail(str != NULL);

	{
		const char *ct = fields_get(&doc->fields, "content-type");
		if(ct && gaim_str_has_prefix(ct, "multipart")) {
			char *b = strrchr(ct, '=');
			if(b++) bd = b;
		}
	}

	fields_write(&doc->fields, str);

	if(bd) {
		GList *l;

		for(l = doc->parts; l; l = l->next) {
			g_string_append_printf(str, "--%s\r\n", bd);

			part_write(l->data, str);

			if(! l->next) {
				g_string_append_printf(str, "--%s--\r\n", bd);
			}
		}
	}
}


const GList *
gaim_mime_document_get_fields(GaimMimeDocument *doc)
{
	g_return_val_if_fail(doc != NULL, NULL);
	return doc->fields.keys;
}


const char *
gaim_mime_document_get_field(GaimMimeDocument *doc, const char *field)
{
	g_return_val_if_fail(doc != NULL, NULL);
	return fields_get(&doc->fields, field);
}


void
gaim_mime_document_set_field(GaimMimeDocument *doc, const char *field, const char *value)
{
	g_return_if_fail(doc != NULL);
	fields_set(&doc->fields, field, value);
}


const GList *
gaim_mime_document_get_parts(GaimMimeDocument *doc)
{
	g_return_val_if_fail(doc != NULL, NULL);
	return doc->parts;
}


void
gaim_mime_document_free(GaimMimeDocument *doc)
{
	if (!doc)
		return;

	fields_destroy(&doc->fields);

	while(doc->parts) {
		part_free(doc->parts->data);
		doc->parts = g_list_delete_link(doc->parts, doc->parts);
	}

	g_free(doc);
}
