/**
 * @file pounce.c Buddy Pounce API
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
 *
 */
#include "internal.h"
#include "debug.h"
#include "pounce.h"

#include "debug.h"
#include "pounce.h"
#include "util.h"

typedef struct
{
	char *name;

	gboolean enabled;

	GHashTable *atts;

} GaimPounceActionData;


static GList   *pounces = NULL;
static guint    pounces_save_timer = 0;
static gboolean pounces_loaded = FALSE;

static GaimPounceActionData *
find_action_data(const GaimPounce *pounce, const char *name)
{
	GaimPounceActionData *action;

	g_return_val_if_fail(pounce != NULL, NULL);
	g_return_val_if_fail(name   != NULL, NULL);

	action = g_hash_table_lookup(pounce->actions, name);

	return action;
}

static void
free_action_data(gpointer data)
{
	GaimPounceActionData *action_data = data;

	g_free(action_data->name);

	g_hash_table_destroy(action_data->atts);

	g_free(action_data);
}

static gboolean
pounces_save_cb(gpointer unused)
{
	gaim_pounces_sync();
	pounces_save_timer = 0;

	return FALSE;
}

static void
schedule_pounces_save(void)
{
	if (!pounces_save_timer)
		pounces_save_timer = g_timeout_add(5000, pounces_save_cb, NULL);
}

GaimPounce *
gaim_pounce_new(const char *ui_type, GaimAccount *pouncer,
				const char *pouncee, GaimPounceEvent event,
				GaimPounceCb cb, void *data, void (*free)(void *))
{
	GaimPounce *pounce;

	g_return_val_if_fail(ui_type != NULL, NULL);
	g_return_val_if_fail(pouncer != NULL, NULL);
	g_return_val_if_fail(pouncee != NULL, NULL);
	g_return_val_if_fail(event   != 0,    NULL);
	g_return_val_if_fail(cb      != NULL, NULL);

	pounce = g_new0(GaimPounce, 1);

	pounce->ui_type  = g_strdup(ui_type);
	pounce->pouncer  = pouncer;
	pounce->pouncee  = g_strdup(pouncee);
	pounce->events   = event;
	pounce->callback = cb;
	pounce->data     = data;
	pounce->free     = free;

	pounce->actions  = g_hash_table_new_full(g_str_hash, g_str_equal,
											 g_free, free_action_data);

	pounces = g_list_append(pounces, pounce);

	return pounce;
}

void
gaim_pounce_destroy(GaimPounce *pounce)
{
	g_return_if_fail(pounce != NULL);

	pounces = g_list_remove(pounces, pounce);

	if (pounce->ui_type != NULL) g_free(pounce->ui_type);
	if (pounce->pouncee != NULL) g_free(pounce->pouncee);

	g_hash_table_destroy(pounce->actions);

	if (pounce->free != NULL && pounce->data != NULL)
		pounce->free(pounce->data);

	g_free(pounce);

	schedule_pounces_save();
}

void
gaim_pounce_set_events(GaimPounce *pounce, GaimPounceEvent events)
{
	g_return_if_fail(pounce != NULL);
	g_return_if_fail(pounce != GAIM_POUNCE_NONE);

	pounce->events = events;

	schedule_pounces_save();
}

void
gaim_pounce_set_pouncer(GaimPounce *pounce, GaimAccount *pouncer)
{
	g_return_if_fail(pounce  != NULL);
	g_return_if_fail(pouncer != NULL);

	pounce->pouncer = pouncer;

	schedule_pounces_save();
}

void
gaim_pounce_set_pouncee(GaimPounce *pounce, const char *pouncee)
{
	g_return_if_fail(pounce  != NULL);
	g_return_if_fail(pouncee != NULL);

	if (pounce->pouncee != NULL)
		g_free(pounce->pouncee);

	pounce->pouncee = (pouncee == NULL ? NULL : g_strdup(pouncee));

	schedule_pounces_save();
}

void
gaim_pounce_set_save(GaimPounce *pounce, gboolean save)
{
	g_return_if_fail(pounce != NULL);

	pounce->save = save;

	schedule_pounces_save();
}

void
gaim_pounce_action_register(GaimPounce *pounce, const char *name)
{
	GaimPounceActionData *action_data;

	g_return_if_fail(pounce != NULL);
	g_return_if_fail(name   != NULL);

	action_data = g_new0(GaimPounceActionData, 1);

	action_data->name    = g_strdup(name);
	action_data->enabled = FALSE;
	action_data->atts    = g_hash_table_new_full(g_str_hash, g_str_equal,
												 g_free, g_free);

	g_hash_table_insert(pounce->actions, g_strdup(name), action_data);

	schedule_pounces_save();
}

void
gaim_pounce_action_set_enabled(GaimPounce *pounce, const char *action,
							   gboolean enabled)
{
	GaimPounceActionData *action_data;

	g_return_if_fail(pounce != NULL);
	g_return_if_fail(action != NULL);

	action_data = find_action_data(pounce, action);

	g_return_if_fail(action_data != NULL);

	action_data->enabled = enabled;

	schedule_pounces_save();
}

void
gaim_pounce_action_set_attribute(GaimPounce *pounce, const char *action,
								 const char *attr, const char *value)
{
	GaimPounceActionData *action_data;

	g_return_if_fail(pounce != NULL);
	g_return_if_fail(action != NULL);
	g_return_if_fail(attr   != NULL);

	action_data = find_action_data(pounce, action);

	g_return_if_fail(action_data != NULL);

	if (value == NULL)
		g_hash_table_remove(action_data->atts, attr);
	else
		g_hash_table_insert(action_data->atts, g_strdup(attr),
							g_strdup(value));

	schedule_pounces_save();
}

void
gaim_pounce_set_data(GaimPounce *pounce, void *data)
{
	g_return_if_fail(pounce != NULL);

	pounce->data = data;
}

GaimPounceEvent
gaim_pounce_get_events(const GaimPounce *pounce)
{
	g_return_val_if_fail(pounce != NULL, GAIM_POUNCE_NONE);

	return pounce->events;
}

GaimAccount *
gaim_pounce_get_pouncer(const GaimPounce *pounce)
{
	g_return_val_if_fail(pounce != NULL, NULL);

	return pounce->pouncer;
}

const char *
gaim_pounce_get_pouncee(const GaimPounce *pounce)
{
	g_return_val_if_fail(pounce != NULL, NULL);

	return pounce->pouncee;
}

gboolean
gaim_pounce_get_save(const GaimPounce *pounce)
{
	g_return_val_if_fail(pounce != NULL, FALSE);

	return pounce->save;
}

gboolean
gaim_pounce_action_is_enabled(const GaimPounce *pounce, const char *action)
{
	GaimPounceActionData *action_data;

	g_return_val_if_fail(pounce != NULL, FALSE);
	g_return_val_if_fail(action != NULL, FALSE);

	action_data = find_action_data(pounce, action);

	g_return_val_if_fail(action_data != NULL, FALSE);

	return action_data->enabled;
}

const char *
gaim_pounce_action_get_attribute(const GaimPounce *pounce,
								 const char *action, const char *attr)
{
	GaimPounceActionData *action_data;

	g_return_val_if_fail(pounce != NULL, NULL);
	g_return_val_if_fail(action != NULL, NULL);
	g_return_val_if_fail(attr   != NULL, NULL);

	action_data = find_action_data(pounce, action);

	g_return_val_if_fail(action_data != NULL, NULL);

	return g_hash_table_lookup(action_data->atts, attr);
}

void *
gaim_pounce_get_data(const GaimPounce *pounce)
{
	g_return_val_if_fail(pounce != NULL, NULL);

	return pounce->data;
}

void
gaim_pounce_execute(const GaimAccount *pouncer, const char *pouncee,
					GaimPounceEvent events)
{
	GaimPounce *pounce;
	GList *l, *l_next;

	g_return_if_fail(pouncer != NULL);
	g_return_if_fail(pouncee != NULL);
	g_return_if_fail(events  != GAIM_POUNCE_NONE);

	for (l = gaim_pounces_get_all(); l != NULL; l = l_next) {
		pounce = (GaimPounce *)l->data;
		l_next = l->next;

		if ((gaim_pounce_get_events(pounce) & events) &&
			(gaim_pounce_get_pouncer(pounce) == pouncer) &&
			!strcmp(gaim_pounce_get_pouncee(pounce), pouncee)) {

			if (pounce->callback != NULL) {
				pounce->callback(pounce, events, gaim_pounce_get_data(pounce));

				if (!gaim_pounce_get_save(pounce))
					gaim_pounce_destroy(pounce);
			}
		}
	}
}

GaimPounce *
gaim_find_pounce(const GaimAccount *pouncer, const char *pouncee,
				 GaimPounceEvent events)
{
	GaimPounce *pounce;
	GList *l;

	g_return_val_if_fail(pouncer != NULL, NULL);
	g_return_val_if_fail(pouncee != NULL, NULL);
	g_return_val_if_fail(events  != GAIM_POUNCE_NONE, NULL);

	for (l = gaim_pounces_get_all(); l != NULL; l = l->next) {
		pounce = (GaimPounce *)l->data;

		if ((gaim_pounce_get_events(pounce) & events) &&
			(gaim_pounce_get_pouncer(pounce) == pouncer) &&
			!strcmp(gaim_pounce_get_pouncee(pounce), pouncee)) {

			return pounce;
		}
	}

	return NULL;
}

gboolean
gaim_pounces_load(void)
{
	pounces_loaded = TRUE;

	return TRUE;
}

static void
write_action_parameter(gpointer key, gpointer value, gpointer user_data)
{
	const char *name, *param_value;
	FILE *fp;

	param_value = (const char *)value;
	name        = (const char *)key;
	fp          = (FILE *)user_data;

	fprintf(fp, "    <param name='%s'>%s</param>\n", name, param_value);
}

static void
write_action_parameter_list(gpointer key, gpointer value, gpointer user_data)
{
	GaimPounceActionData *action_data;
	const char *action;
	FILE *fp;

	action_data = (GaimPounceActionData *)value;
	action      = (const char *)key;
	fp          = (FILE *)user_data;

	if (!action_data->enabled)
		return;

	fprintf(fp, "   <action type='%s'", action);

	if (g_hash_table_size(action_data->atts) == 0) {
		fprintf(fp, "/>\n");
	}
	else {
		fprintf(fp, ">\n");

		g_hash_table_foreach(action_data->atts, write_action_parameter, fp);

		fprintf(fp, "   </action>\n");
	}
}

static void
gaim_pounces_write(FILE *fp, GaimPounce *pounce)
{
	GaimAccount *pouncer;
	GaimPounceEvent events;
	char *pouncer_name;

	pouncer = gaim_pounce_get_pouncer(pounce);
	events  = gaim_pounce_get_events(pounce);

	pouncer_name = g_markup_escape_text(gaim_account_get_username(pouncer), -1);

	fprintf(fp, " <pounce ui='%s'>\n", pounce->ui_type);
	fprintf(fp, "  <account protocol='%s'>%s</account>\n",
			pouncer->protocol_id, pouncer_name);
	fprintf(fp, "  <pouncee>%s</pouncee>\n", gaim_pounce_get_pouncee(pounce));
	fprintf(fp, "  <events>\n");

	if (events & GAIM_POUNCE_SIGNON)
		fprintf(fp, "   <event type='sign-on'>\n");
	if (events & GAIM_POUNCE_SIGNOFF)
		fprintf(fp, "   <event type='sign-off'>\n");
	if (events & GAIM_POUNCE_AWAY)
		fprintf(fp, "   <event type='away'>\n");
	if (events & GAIM_POUNCE_AWAY_RETURN)
		fprintf(fp, "   <event type='return-from-away'>\n");
	if (events & GAIM_POUNCE_IDLE)
		fprintf(fp, "   <event type='idle'>\n");
	if (events & GAIM_POUNCE_IDLE_RETURN)
		fprintf(fp, "   <event type='return-from-idle'>\n");
	if (events & GAIM_POUNCE_TYPING)
		fprintf(fp, "   <event type='start-typing'>\n");
	if (events & GAIM_POUNCE_TYPING_STOPPED)
		fprintf(fp, "   <event type='stop-typing'>\n");

	fprintf(fp, "  </events>\n");
	fprintf(fp, "  <actions>\n");

	g_hash_table_foreach(pounce->actions, write_action_parameter_list, fp);

	fprintf(fp, "  </actions>\n");
	fprintf(fp, " </pounce>\n");

	g_free(pouncer_name);
}

void
gaim_pounces_sync(void)
{
	FILE *fp;
	const char *user_dir = gaim_user_dir();
	char *filename;
	char *filename_real;

	if (!pounces_loaded) {
		gaim_debug(GAIM_DEBUG_WARNING, "pounces",
				   "Writing pounces to disk.\n");
		schedule_pounces_save();
		return;
	}

	if (user_dir == NULL)
		return;

	gaim_debug(GAIM_DEBUG_INFO, "pounces", "Writing pounces to disk.\n");

	fp = fopen(user_dir, "r");

	if (fp == NULL)
		mkdir(user_dir, S_IRUSR | S_IWUSR | S_IXUSR);
	else
		fclose(fp);

	filename = g_build_filename(user_dir, "pounces.xml.save", NULL);

	if ((fp = fopen(filename, "w")) != NULL) {
		GList *l;

		fprintf(fp, "<?xml version='1.0' encoding='UTF-8' ?>\n\n");
		fprintf(fp, "<pounces>\n");

		for (l = gaim_pounces_get_all(); l != NULL; l = l->next)
			gaim_pounces_write(fp, l->data);

		fprintf(fp, "</pounces>\n");

		fclose(fp);
		chmod(filename, S_IRUSR | S_IWUSR);
	}
	else {
		gaim_debug(GAIM_DEBUG_ERROR, "pounces", "Unable to write %s\n",
				   filename);
	}

	filename_real = g_build_filename(user_dir, "pounces.xml", NULL);

	if (rename(filename, filename_real) < 0) {
		gaim_debug(GAIM_DEBUG_ERROR, "pounces", "Error renaming %s to %s\n",
				   filename, filename_real);
	}

	g_free(filename);
	g_free(filename_real);
}

GList *
gaim_pounces_get_all(void)
{
	return pounces;
}
