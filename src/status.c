/*
 * gaim
 *
 * Copyright (C) 2003 Jason Priestly
 * Copyright (C) 2003 Luke Perry
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

#include "status.h"
#include "internal.h"
#include "ui.h"
#include "debug.h"
#include "util.h"

/* for people like myself who are too lazy to add an away msg :) */
/* I don't know who "myself" is in this context.  The exclamation point
 * makes it slightly less boring ;) */
#define BORING_DEFAULT_AWAY_MSG _("Sorry, I ran out for a bit!")

/* XML File Saving */

/* All of this code is adapted from Nathan Walp's.  It's adapted all over the place
 * for accounts, the buddy list, pounces, preferences, and the likes.  It would be
 * neat if we could somehow make this more generic. */
static gboolean status_loaded = FALSE;
static guint    status_save_timer = 0;


typedef enum
{
	TAG_NONE = 0,
	TAG_STATUS,
	TAG_STATE,
	TAG_MESSAGE,

} StatusParserTag;


typedef struct
{
	StatusParserTag tag;
	GString *buffer;
	struct away_message *am;

} StatusParserData;

static void
free_parser_data(gpointer user_data)
{
	StatusParserData *data = user_data;

	if (data->buffer != NULL)
		g_string_free(data->buffer, TRUE);

	g_free(data);
}

static void gaim_status_write(FILE *fp, struct away_message *am)
{
	char *esc = NULL;

	esc = g_markup_escape_text(am->name, -1);
	fprintf(fp, "\t<status name=\"%s\">\n", esc);
	g_free(esc);

	fprintf(fp, "\t\t<state>away</state>\n");

	esc = g_markup_escape_text(am->message, -1);
	fprintf(fp, "\t\t<message>%s</message>\n", esc);
	g_free(esc);

	fprintf(fp, "\t</status>\n");
}

static gboolean
status_save_cb(gpointer unused)
{
	gaim_status_sync();
	status_save_timer = 0;

	return FALSE;
}

static void
schedule_status_save()
{
	if (!status_save_timer)
		status_save_timer = g_timeout_add(5000, status_save_cb, NULL);
}

static void
start_element_handler(GMarkupParseContext *context,
					  const gchar *element_name,
					  const gchar **attribute_names,
					  const gchar **attribute_values,
					  gpointer user_data, GError **error)
{
	const char *value;
	StatusParserData *data = user_data;
	GHashTable *atts;
	int i;

	atts = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	for (i = 0; attribute_names[i] != NULL; i++) {
		g_hash_table_insert(atts, g_strdup(attribute_names[i]),
				    g_strdup(attribute_values[i]));
	}

	if (data->buffer != NULL) {
		g_string_free(data->buffer, TRUE);
		data->buffer = NULL;
	}

	if (!strcmp(element_name, "status")) {
		data->tag = TAG_STATUS;
		if ((value = g_hash_table_lookup(atts, "name")) != NULL) {
			data->am = g_new0(struct away_message, 1);
			g_snprintf(data->am->name, sizeof(data->am->name), "%s", value);
			away_messages = g_slist_append(away_messages, data->am);
		}
	} else if (!strcmp(element_name, "message")) {
		data->tag = TAG_MESSAGE;

	}

	g_hash_table_destroy(atts);
}

static void
end_element_handler(GMarkupParseContext *context, const gchar *element_name,
					gpointer user_data,  GError **error)
{
	StatusParserData *data = user_data;
	gchar *buffer;

	if (data->buffer == NULL)
		return;

	buffer = g_string_free(data->buffer, FALSE);
	data->buffer = NULL;

	if (data->tag == TAG_MESSAGE) {
		if (*buffer != '\0')
			g_snprintf(data->am->message, sizeof(data->am->message), "%s", buffer);
	}

	data->tag = TAG_NONE;

	g_free(buffer);
}

static void
text_handler(GMarkupParseContext *context, const gchar *text,
			 gsize text_len, gpointer user_data, GError **error)
{
	StatusParserData *data = user_data;

	if (data->buffer == NULL)
		data->buffer = g_string_new_len(text, text_len);
	else
		g_string_append_len(data->buffer, text, text_len);
}

static GMarkupParser status_parser =
{
	start_element_handler,
	end_element_handler,
	text_handler,
	NULL,
	NULL
};

void gaim_status_sync()
{
	FILE *fp;
	const char *user_dir = gaim_user_dir();
	char *filename, *filename_real;

	if (!status_loaded) {
		gaim_debug(GAIM_DEBUG_WARNING, "status", "Writing status to disk.\n");
		schedule_status_save();
		return;
	}

	if (user_dir == NULL)
		return;

	gaim_debug(GAIM_DEBUG_INFO, "status", "Saving statuses to disk\n");

	fp = fopen(user_dir, "r");

	if (fp == NULL)
		mkdir(user_dir, S_IRUSR | S_IWUSR | S_IXUSR);
	else
		fclose(fp);

	filename = g_build_filename(user_dir, "status.xml.save", NULL);

	if ((fp = fopen(filename, "w")) != NULL) {
		GSList *l;

		fprintf(fp, "<?xml version='1.0' encoding='UTF-8' ?>\n\n");
		fprintf(fp, "<statuses>\n");

		for (l = away_messages; l != NULL; l = l->next)
			gaim_status_write(fp, l->data);

		fprintf(fp, "</statuses>\n");

		fclose(fp);
		chmod(filename, S_IRUSR | S_IWUSR);
	}
	else {
		gaim_debug(GAIM_DEBUG_ERROR, "status", "Unable to write %s\n",
				   filename);
	}

	filename_real = g_build_filename(user_dir, "status.xml", NULL);

	if (rename(filename, filename_real) < 0) {
		gaim_debug(GAIM_DEBUG_ERROR, "status", "Error renaming %s to %s\n",
				   filename, filename_real);
	}

	g_free(filename);
	g_free(filename_real);

}

void gaim_status_load()
{
	gchar *filename = g_build_filename(gaim_user_dir(), "status.xml", NULL);
	gchar *contents = NULL;
	gsize length;
	GMarkupParseContext *context;
	GError *error = NULL;
	StatusParserData *parser_data;

	if (filename == NULL) {
		status_loaded = TRUE;
		return;
	}

	if (!g_file_get_contents(filename, &contents, &length, &error)) {
		gaim_debug(GAIM_DEBUG_ERROR, "status",
				   "Error reading statuses: %s\n", error->message);
		g_error_free(error);
		g_free(filename);
		status_loaded = TRUE;
		if (!away_messages) {
			struct away_message *a = g_new0(struct away_message, 1);
			g_snprintf(a->name, sizeof(a->name), _("Slightly less boring default"));
			g_snprintf(a->message, sizeof(a->message), "%s", BORING_DEFAULT_AWAY_MSG);
			away_messages = g_slist_append(away_messages, a);
		}
		return;
	}

	parser_data = g_new0(StatusParserData, 1);

	context = g_markup_parse_context_new(&status_parser, 0,
					     parser_data, free_parser_data);

	if (!g_markup_parse_context_parse(context, contents, length, NULL)) {
		g_markup_parse_context_free(context);
		g_free(contents);
		g_free(filename);
		status_loaded = TRUE;
		return;
	}

	if (!g_markup_parse_context_end_parse(context, NULL)) {
		gaim_debug(GAIM_DEBUG_ERROR, "status", "Error parsing %s\n",
				   filename);
		g_markup_parse_context_free(context);
		g_free(contents);
		g_free(filename);
		status_loaded = TRUE;
		return;
	}

	g_markup_parse_context_free(context);
	g_free(contents);
	g_free(filename);
	status_loaded = TRUE;
	return;
}
