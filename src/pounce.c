/**
 * @file pounce.c Buddy Pounce API
 * @ingroup core
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
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
#include "internal.h"
#include "conversation.h"
#include "debug.h"
#include "pounce.h"

#include "debug.h"
#include "pounce.h"
#include "util.h"

typedef struct
{
	GString *buffer;

	GaimPounce *pounce;
	GaimPounceEvent events;
	GaimPounceOption options;

	char *ui_name;
	char *pouncee;
	char *protocol_id;
	char *event_type;
	char *option_type;
	char *action_name;
	char *param_name;
	char *account_name;

} PounceParserData;

typedef struct
{
	char *name;

	gboolean enabled;

	GHashTable *atts;

} GaimPounceActionData;

typedef struct
{
	char *ui;
	GaimPounceCb cb;
	void (*new_pounce)(GaimPounce *);
	void (*free_pounce)(GaimPounce *);

} GaimPounceHandler;


static GHashTable *pounce_handlers = NULL;
static GList      *pounces = NULL;
static guint       save_timer = 0;
static gboolean    pounces_loaded = FALSE;


/*********************************************************************
 * Private utility functions                                         *
 *********************************************************************/

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


/*********************************************************************
 * Writing to disk                                                   *
 *********************************************************************/

static void
action_parameter_to_xmlnode(gpointer key, gpointer value, gpointer user_data)
{
	const char *name, *param_value;
	xmlnode *node, *child;

	name        = (const char *)key;
	param_value = (const char *)value;
	node        = (xmlnode *)user_data;

	child = xmlnode_new_child(node, "param");
	xmlnode_set_attrib(child, "name", name);
	xmlnode_insert_data(child, param_value, -1);
}

static void
action_parameter_list_to_xmlnode(gpointer key, gpointer value, gpointer user_data)
{
	const char *action;
	GaimPounceActionData *action_data;
	xmlnode *node, *child;

	action      = (const char *)key;
	action_data = (GaimPounceActionData *)value;
	node        = (xmlnode *)user_data;

	if (!action_data->enabled)
		return;

	child = xmlnode_new_child(node, "action");
	xmlnode_set_attrib(child, "type", action);

	g_hash_table_foreach(action_data->atts, action_parameter_to_xmlnode, child);
}

static void
add_event_to_xmlnode(xmlnode *node, const char *type)
{
	xmlnode *child;

	child = xmlnode_new_child(node, "event");
	xmlnode_set_attrib(child, "type", type);
}

static void
add_option_to_xmlnode(xmlnode *node, const char *type)
{
	xmlnode *child;

	child = xmlnode_new_child(node, "option");
	xmlnode_set_attrib(child, "type", type);
}

static xmlnode *
pounce_to_xmlnode(GaimPounce *pounce)
{
	xmlnode *node, *child;
	GaimAccount *pouncer;
	GaimPounceEvent events;
	GaimPounceOption options;

	pouncer = gaim_pounce_get_pouncer(pounce);
	events  = gaim_pounce_get_events(pounce);
	options = gaim_pounce_get_options(pounce);

	node = xmlnode_new("pounce");
	xmlnode_set_attrib(node, "ui", pounce->ui_type);

	child = xmlnode_new_child(node, "account");
	xmlnode_set_attrib(child, "protocol", pouncer->protocol_id);
	xmlnode_insert_data(child, gaim_account_get_username(pouncer), -1);

	child = xmlnode_new_child(node, "pouncee");
	xmlnode_insert_data(child, gaim_pounce_get_pouncee(pounce), -1);

	/* Write pounce options */
	child = xmlnode_new_child(node, "options");
	if (options & GAIM_POUNCE_OPTION_AWAY)
		add_option_to_xmlnode(child, "on-away");

	/* Write pounce events */
	child = xmlnode_new_child(node, "events");
	if (events & GAIM_POUNCE_SIGNON)
		add_event_to_xmlnode(child, "sign-on");
	if (events & GAIM_POUNCE_SIGNOFF)
		add_event_to_xmlnode(child, "sign-off");
	if (events & GAIM_POUNCE_AWAY)
		add_event_to_xmlnode(child, "away");
	if (events & GAIM_POUNCE_AWAY_RETURN)
		add_event_to_xmlnode(child, "return-from-away");
	if (events & GAIM_POUNCE_IDLE)
		add_event_to_xmlnode(child, "idle");
	if (events & GAIM_POUNCE_IDLE_RETURN)
		add_event_to_xmlnode(child, "return-from-idle");
	if (events & GAIM_POUNCE_TYPING)
		add_event_to_xmlnode(child, "start-typing");
	if (events & GAIM_POUNCE_TYPED)
		add_event_to_xmlnode(child, "typed");
	if (events & GAIM_POUNCE_TYPING_STOPPED)
		add_event_to_xmlnode(child, "stop-typing");
	if (events & GAIM_POUNCE_MESSAGE_RECEIVED)
		add_event_to_xmlnode(child, "message-received");

	/* Write pounce actions */
	child = xmlnode_new_child(node, "actions");
	g_hash_table_foreach(pounce->actions, action_parameter_list_to_xmlnode, child);

	if (gaim_pounce_get_save(pounce))
		child = xmlnode_new_child(node, "save");

	return node;
}

static xmlnode *
pounces_to_xmlnode(void)
{
	xmlnode *node, *child;
	GList *cur;

	node = xmlnode_new("pounces");
	xmlnode_set_attrib(node, "version", "1.0");

	for (cur = gaim_pounces_get_all(); cur != NULL; cur = cur->next)
	{
		child = pounce_to_xmlnode(cur->data);
		xmlnode_insert_child(node, child);
	}

	return node;
}

static void
sync_pounces(void)
{
	xmlnode *node;
	char *data;

	if (!pounces_loaded)
	{
		gaim_debug_error("pounce", "Attempted to save buddy pounces before "
						 "they were read!\n");
		return;
	}

	node = pounces_to_xmlnode();
	data = xmlnode_to_formatted_str(node, NULL);
	gaim_util_write_data_to_file("pounces.xml", data, -1);
	g_free(data);
	xmlnode_free(node);
}

static gboolean
save_cb(gpointer data)
{
	sync_pounces();
	save_timer = 0;
	return FALSE;
}

static void
schedule_pounces_save(void)
{
	if (save_timer == 0)
		save_timer = gaim_timeout_add(5000, save_cb, NULL);
}


/*********************************************************************
 * Reading from disk                                                 *
 *********************************************************************/

static void
free_parser_data(gpointer user_data)
{
	PounceParserData *data = user_data;

	if (data->buffer != NULL)
		g_string_free(data->buffer, TRUE);

	g_free(data->ui_name);
	g_free(data->pouncee);
	g_free(data->protocol_id);
	g_free(data->event_type);
	g_free(data->option_type);
	g_free(data->action_name);
	g_free(data->param_name);
	g_free(data->account_name);

	g_free(data);
}

static void
start_element_handler(GMarkupParseContext *context,
					  const gchar *element_name,
					  const gchar **attribute_names,
					  const gchar **attribute_values,
					  gpointer user_data, GError **error)
{
	PounceParserData *data = user_data;
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

	if (!strcmp(element_name, "pounce")) {
		const char *ui = g_hash_table_lookup(atts, "ui");

		if (ui == NULL) {
			gaim_debug(GAIM_DEBUG_ERROR, "pounce",
					   "Unset 'ui' parameter for pounce!\n");
		}
		else
			data->ui_name = g_strdup(ui);

		data->events = 0;
	}
	else if (!strcmp(element_name, "account")) {
		const char *protocol_id = g_hash_table_lookup(atts, "protocol");

		if (protocol_id == NULL) {
			gaim_debug(GAIM_DEBUG_ERROR, "pounce",
					   "Unset 'protocol' parameter for account!\n");
		}
		else
			data->protocol_id = g_strdup(protocol_id);
	}
	else if (!strcmp(element_name, "option")) {
		const char *type = g_hash_table_lookup(atts, "type");

		if (type == NULL) {
			gaim_debug(GAIM_DEBUG_ERROR, "pounce",
					   "Unset 'type' parameter for option!\n");
		}
		else
			data->option_type = g_strdup(type);
	}
	else if (!strcmp(element_name, "event")) {
		const char *type = g_hash_table_lookup(atts, "type");

		if (type == NULL) {
			gaim_debug(GAIM_DEBUG_ERROR, "pounce",
					   "Unset 'type' parameter for event!\n");
		}
		else
			data->event_type = g_strdup(type);
	}
	else if (!strcmp(element_name, "action")) {
		const char *type = g_hash_table_lookup(atts, "type");

		if (type == NULL) {
			gaim_debug(GAIM_DEBUG_ERROR, "pounce",
					   "Unset 'type' parameter for action!\n");
		}
		else
			data->action_name = g_strdup(type);
	}
	else if (!strcmp(element_name, "param")) {
		const char *param_name = g_hash_table_lookup(atts, "name");

		if (param_name == NULL) {
			gaim_debug(GAIM_DEBUG_ERROR, "pounce",
					   "Unset 'name' parameter for param!\n");
		}
		else
			data->param_name = g_strdup(param_name);
	}

	g_hash_table_destroy(atts);
}

static void
end_element_handler(GMarkupParseContext *context, const gchar *element_name,
					gpointer user_data,  GError **error)
{
	PounceParserData *data = user_data;
	gchar *buffer = NULL;

	if (data->buffer != NULL) {
		buffer = g_string_free(data->buffer, FALSE);
		data->buffer = NULL;
	}

	if (!strcmp(element_name, "account")) {
		g_free(data->account_name);
		data->account_name = g_strdup(buffer);
	}
	else if (!strcmp(element_name, "pouncee")) {
		g_free(data->pouncee);
		data->pouncee = g_strdup(buffer);
	}
	else if (!strcmp(element_name, "option")) {
		if (!strcmp(data->option_type, "on-away"))
			data->options |= GAIM_POUNCE_OPTION_AWAY;

		g_free(data->option_type);
		data->option_type = NULL;
	}
	else if (!strcmp(element_name, "event")) {
		if (!strcmp(data->event_type, "sign-on"))
			data->events |= GAIM_POUNCE_SIGNON;
		else if (!strcmp(data->event_type, "sign-off"))
			data->events |= GAIM_POUNCE_SIGNOFF;
		else if (!strcmp(data->event_type, "away"))
			data->events |= GAIM_POUNCE_AWAY;
		else if (!strcmp(data->event_type, "return-from-away"))
			data->events |= GAIM_POUNCE_AWAY_RETURN;
		else if (!strcmp(data->event_type, "idle"))
			data->events |= GAIM_POUNCE_IDLE;
		else if (!strcmp(data->event_type, "return-from-idle"))
			data->events |= GAIM_POUNCE_IDLE_RETURN;
		else if (!strcmp(data->event_type, "start-typing"))
			data->events |= GAIM_POUNCE_TYPING;
		else if (!strcmp(data->event_type, "typed"))
			data->events |= GAIM_POUNCE_TYPED;
		else if (!strcmp(data->event_type, "stop-typing"))
			data->events |= GAIM_POUNCE_TYPING_STOPPED;
		else if (!strcmp(data->event_type, "message-received"))
			data->events |= GAIM_POUNCE_MESSAGE_RECEIVED;

		g_free(data->event_type);
		data->event_type = NULL;
	}
	else if (!strcmp(element_name, "action")) {
		if (data->pounce != NULL) {
			gaim_pounce_action_register(data->pounce, data->action_name);
			gaim_pounce_action_set_enabled(data->pounce, data->action_name, TRUE);
		}

		g_free(data->action_name);
		data->action_name = NULL;
	}
	else if (!strcmp(element_name, "param")) {
		if (data->pounce != NULL) {
			gaim_pounce_action_set_attribute(data->pounce, data->action_name,
											 data->param_name, buffer);
		}

		g_free(data->param_name);
		data->param_name = NULL;
	}
	else if (!strcmp(element_name, "events")) {
		GaimAccount *account;

		account = gaim_accounts_find(data->account_name, data->protocol_id);

		g_free(data->account_name);
		g_free(data->protocol_id);

		data->account_name = NULL;
		data->protocol_id  = NULL;

		if (account == NULL) {
			gaim_debug(GAIM_DEBUG_ERROR, "pounce",
					   "Account for pounce not found!\n");
			/*
			 * This pounce has effectively been removed, so make
			 * sure that we save the changes to pounces.xml
			 */
			schedule_pounces_save();
		}
		else {
			gaim_debug(GAIM_DEBUG_INFO, "pounce",
					   "Creating pounce: %s, %s\n", data->ui_name,
					   data->pouncee);

			data->pounce = gaim_pounce_new(data->ui_name, account,
										   data->pouncee, data->events,
										   data->options);
		}

		g_free(data->pouncee);
		data->pouncee = NULL;
	}
	else if (!strcmp(element_name, "save")) {
		if (data->pounce != NULL)
			gaim_pounce_set_save(data->pounce, TRUE);
	}
	else if (!strcmp(element_name, "pounce")) {
		data->pounce  = NULL;
		data->events  = 0;
		data->options = 0;

		g_free(data->ui_name);
		g_free(data->pouncee);
		g_free(data->protocol_id);
		g_free(data->event_type);
		g_free(data->option_type);
		g_free(data->action_name);
		g_free(data->param_name);
		g_free(data->account_name);

		data->ui_name      = NULL;
		data->pounce       = NULL;
		data->protocol_id  = NULL;
		data->event_type   = NULL;
		data->option_type  = NULL;
		data->action_name  = NULL;
		data->param_name   = NULL;
		data->account_name = NULL;
	}

	if (buffer != NULL)
		g_free(buffer);
}

static void
text_handler(GMarkupParseContext *context, const gchar *text,
			 gsize text_len, gpointer user_data, GError **error)
{
	PounceParserData *data = user_data;

	if (data->buffer == NULL)
		data->buffer = g_string_new_len(text, text_len);
	else
		g_string_append_len(data->buffer, text, text_len);
}

static GMarkupParser pounces_parser =
{
	start_element_handler,
	end_element_handler,
	text_handler,
	NULL,
	NULL
};

gboolean
gaim_pounces_load(void)
{
	gchar *filename = g_build_filename(gaim_user_dir(), "pounces.xml", NULL);
	gchar *contents = NULL;
	gsize length;
	GMarkupParseContext *context;
	GError *error = NULL;
	PounceParserData *parser_data;

	if (filename == NULL) {
		pounces_loaded = TRUE;
		return FALSE;
	}

	if (!g_file_get_contents(filename, &contents, &length, &error)) {
		gaim_debug(GAIM_DEBUG_ERROR, "pounce",
				   "Error reading pounces: %s\n", error->message);

		g_free(filename);
		g_error_free(error);

		pounces_loaded = TRUE;
		return FALSE;
	}

	parser_data = g_new0(PounceParserData, 1);

	context = g_markup_parse_context_new(&pounces_parser, 0,
										 parser_data, free_parser_data);

	if (!g_markup_parse_context_parse(context, contents, length, NULL)) {
		g_markup_parse_context_free(context);
		g_free(contents);
		g_free(filename);

		pounces_loaded = TRUE;

		return FALSE;
	}

	if (!g_markup_parse_context_end_parse(context, NULL)) {
		gaim_debug(GAIM_DEBUG_ERROR, "pounce", "Error parsing %s\n",
				   filename);

		g_markup_parse_context_free(context);
		g_free(contents);
		g_free(filename);
		pounces_loaded = TRUE;

		return FALSE;
	}

	g_markup_parse_context_free(context);
	g_free(contents);
	g_free(filename);

	pounces_loaded = TRUE;

	return TRUE;
}


GaimPounce *
gaim_pounce_new(const char *ui_type, GaimAccount *pouncer,
				const char *pouncee, GaimPounceEvent event,
				GaimPounceOption option)
{
	GaimPounce *pounce;
	GaimPounceHandler *handler;

	g_return_val_if_fail(ui_type != NULL, NULL);
	g_return_val_if_fail(pouncer != NULL, NULL);
	g_return_val_if_fail(pouncee != NULL, NULL);
	g_return_val_if_fail(event   != 0,    NULL);

	pounce = g_new0(GaimPounce, 1);

	pounce->ui_type  = g_strdup(ui_type);
	pounce->pouncer  = pouncer;
	pounce->pouncee  = g_strdup(pouncee);
	pounce->events   = event;
	pounce->options  = option;

	pounce->actions  = g_hash_table_new_full(g_str_hash, g_str_equal,
											 g_free, free_action_data);

	handler = g_hash_table_lookup(pounce_handlers, pounce->ui_type);

	if (handler != NULL && handler->new_pounce != NULL)
		handler->new_pounce(pounce);

	pounces = g_list_append(pounces, pounce);

	schedule_pounces_save();

	return pounce;
}

void
gaim_pounce_destroy(GaimPounce *pounce)
{
	GaimPounceHandler *handler;

	g_return_if_fail(pounce != NULL);

	handler = g_hash_table_lookup(pounce_handlers, pounce->ui_type);

	pounces = g_list_remove(pounces, pounce);

	g_free(pounce->ui_type);
	g_free(pounce->pouncee);

	g_hash_table_destroy(pounce->actions);

	if (handler != NULL && handler->free_pounce != NULL)
		handler->free_pounce(pounce);

	g_free(pounce);

	schedule_pounces_save();
}

void
gaim_pounce_destroy_all_by_account(GaimAccount *account)
{
	GaimAccount *pouncer;
	GaimPounce *pounce;
	GList *l, *l_next;

	g_return_if_fail(account != NULL);

	for (l = gaim_pounces_get_all(); l != NULL; l = l_next)
	{
		pounce = (GaimPounce *)l->data;
		l_next = l->next;

		pouncer = gaim_pounce_get_pouncer(pounce);
		if (pouncer == account)
			gaim_pounce_destroy(pounce);
	}
}

void
gaim_pounce_set_events(GaimPounce *pounce, GaimPounceEvent events)
{
	g_return_if_fail(pounce != NULL);
	g_return_if_fail(events != GAIM_POUNCE_NONE);

	pounce->events = events;

	schedule_pounces_save();
}

void
gaim_pounce_set_options(GaimPounce *pounce, GaimPounceOption options)
{
	g_return_if_fail(pounce  != NULL);

	pounce->options = options;

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

	if (g_hash_table_lookup(pounce->actions, name) != NULL)
		return;

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

	schedule_pounces_save();
}

GaimPounceEvent
gaim_pounce_get_events(const GaimPounce *pounce)
{
	g_return_val_if_fail(pounce != NULL, GAIM_POUNCE_NONE);

	return pounce->events;
}

GaimPounceOption
gaim_pounce_get_options(const GaimPounce *pounce)
{
	g_return_val_if_fail(pounce != NULL, GAIM_POUNCE_OPTION_NONE);

	return pounce->options;
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
	GaimPounceHandler *handler;
	GaimPresence *presence;
	GList *l, *l_next;
	char *norm_pouncee;

	g_return_if_fail(pouncer != NULL);
	g_return_if_fail(pouncee != NULL);
	g_return_if_fail(events  != GAIM_POUNCE_NONE);

	norm_pouncee = g_strdup(gaim_normalize(pouncer, pouncee));

	for (l = gaim_pounces_get_all(); l != NULL; l = l_next)
	{
		pounce = (GaimPounce *)l->data;
		l_next = l->next;

		presence = gaim_account_get_presence(pouncer);

		if ((gaim_pounce_get_events(pounce) & events) &&
			(gaim_pounce_get_pouncer(pounce) == pouncer) &&
			!gaim_utf8_strcasecmp(gaim_normalize(pouncer, gaim_pounce_get_pouncee(pounce)),
								  norm_pouncee) &&
			(pounce->options == GAIM_POUNCE_OPTION_NONE ||
			 (pounce->options & GAIM_POUNCE_OPTION_AWAY && 
			  !gaim_presence_is_available(presence))))
		{
			handler = g_hash_table_lookup(pounce_handlers, pounce->ui_type);

			if (handler != NULL && handler->cb != NULL)
			{
				handler->cb(pounce, events, gaim_pounce_get_data(pounce));

				if (!gaim_pounce_get_save(pounce))
					gaim_pounce_destroy(pounce);
			}
		}
	}

	g_free(norm_pouncee);
}

GaimPounce *
gaim_find_pounce(const GaimAccount *pouncer, const char *pouncee,
				 GaimPounceEvent events)
{
	GaimPounce *pounce = NULL;
	GList *l;
	char *norm_pouncee;

	g_return_val_if_fail(pouncer != NULL, NULL);
	g_return_val_if_fail(pouncee != NULL, NULL);
	g_return_val_if_fail(events  != GAIM_POUNCE_NONE, NULL);

	norm_pouncee = g_strdup(gaim_normalize(pouncer, pouncee));

	for (l = gaim_pounces_get_all(); l != NULL; l = l->next)
	{
		pounce = (GaimPounce *)l->data;

		if ((gaim_pounce_get_events(pounce) & events) &&
			(gaim_pounce_get_pouncer(pounce) == pouncer) &&
			!gaim_utf8_strcasecmp(gaim_normalize(pouncer, gaim_pounce_get_pouncee(pounce)),
								  norm_pouncee))
		{
			break;
		}

		pounce = NULL;
	}

	g_free(norm_pouncee);

	return pounce;
}

void
gaim_pounces_register_handler(const char *ui, GaimPounceCb cb,
							  void (*new_pounce)(GaimPounce *pounce),
							  void (*free_pounce)(GaimPounce *pounce))
{
	GaimPounceHandler *handler;

	g_return_if_fail(ui != NULL);
	g_return_if_fail(cb != NULL);

	handler = g_new0(GaimPounceHandler, 1);

	handler->ui          = g_strdup(ui);
	handler->cb          = cb;
	handler->new_pounce  = new_pounce;
	handler->free_pounce = free_pounce;

	g_hash_table_insert(pounce_handlers, g_strdup(ui), handler);
}

void
gaim_pounces_unregister_handler(const char *ui)
{
	g_return_if_fail(ui != NULL);

	g_hash_table_remove(pounce_handlers, ui);
}

GList *
gaim_pounces_get_all(void)
{
	return pounces;
}

static void
free_pounce_handler(gpointer user_data)
{
	GaimPounceHandler *handler = (GaimPounceHandler *)user_data;

	g_free(handler->ui);
	g_free(handler);
}

static void
buddy_state_cb(GaimBuddy *buddy, GaimPounceEvent event)
{
	gaim_pounce_execute(buddy->account, buddy->name, event);
}

static void
buddy_status_changed_cb(GaimBuddy *buddy, GaimStatus *old_status,
                        GaimStatus *status)
{
	gboolean old_available, available;

	available = gaim_status_is_available(status);
	old_available = gaim_status_is_available(old_status);

	if (available && !old_available)
		gaim_pounce_execute(buddy->account, buddy->name,
		                    GAIM_POUNCE_AWAY_RETURN);
	else if (!available && old_available)
		gaim_pounce_execute(buddy->account, buddy->name,
		                    GAIM_POUNCE_AWAY);
}

static void
buddy_idle_changed_cb(GaimBuddy *buddy, gboolean old_idle, gboolean idle)
{
	if (idle && !old_idle)
		gaim_pounce_execute(buddy->account, buddy->name,
		                    GAIM_POUNCE_IDLE);
	else if (!idle && old_idle)
		gaim_pounce_execute(buddy->account, buddy->name,
		                    GAIM_POUNCE_IDLE_RETURN);
}

static void
buddy_typing_cb(GaimAccount *account, const char *name, void *data)
{
	GaimConversation *conv;

	conv = gaim_find_conversation_with_account(GAIM_CONV_TYPE_IM, name, account);
	if (conv != NULL)
	{
		GaimTypingState state;
		GaimPounceEvent event;

		state = gaim_conv_im_get_typing_state(GAIM_CONV_IM(conv));
		if (state == GAIM_TYPED)
			event = GAIM_POUNCE_TYPED;
		else if (state == GAIM_NOT_TYPING)
			event = GAIM_POUNCE_TYPING_STOPPED;
		else
			event = GAIM_POUNCE_TYPING;

		gaim_pounce_execute(account, name, event);
	}
}

static void
received_message_cb(GaimAccount *account, const char *name, void *data)
{
	gaim_pounce_execute(account, name, GAIM_POUNCE_MESSAGE_RECEIVED);
}

void *
gaim_pounces_get_handle(void)
{
	static int pounce_handle;

	return &pounce_handle;
}

void
gaim_pounces_init(void)
{
	void *handle       = gaim_pounces_get_handle();
	void *blist_handle = gaim_blist_get_handle();
	void *conv_handle  = gaim_conversations_get_handle();

	pounce_handlers = g_hash_table_new_full(g_str_hash, g_str_equal,
											g_free, free_pounce_handler);

	gaim_signal_connect(blist_handle, "buddy-idle-changed",
	                    handle, GAIM_CALLBACK(buddy_idle_changed_cb), NULL);
	gaim_signal_connect(blist_handle, "buddy-status-changed",
	                    handle, GAIM_CALLBACK(buddy_status_changed_cb), NULL);
	gaim_signal_connect(blist_handle, "buddy-signed-on",
						handle, GAIM_CALLBACK(buddy_state_cb),
						GINT_TO_POINTER(GAIM_POUNCE_SIGNON));
	gaim_signal_connect(blist_handle, "buddy-signed-off",
						handle, GAIM_CALLBACK(buddy_state_cb),
						GINT_TO_POINTER(GAIM_POUNCE_SIGNOFF));

	gaim_signal_connect(conv_handle, "buddy-typing",
						handle, GAIM_CALLBACK(buddy_typing_cb), NULL);
	gaim_signal_connect(conv_handle, "buddy-typed",
						handle, GAIM_CALLBACK(buddy_typing_cb), NULL);
	gaim_signal_connect(conv_handle, "buddy-typing-stopped",
						handle, GAIM_CALLBACK(buddy_typing_cb), NULL);

	gaim_signal_connect(conv_handle, "received-im-msg",
						handle, GAIM_CALLBACK(received_message_cb), NULL);
}

void
gaim_pounces_uninit()
{
	if (save_timer != 0)
	{
		gaim_timeout_remove(save_timer);
		save_timer = 0;
		sync_pounces();
	}

	gaim_signals_disconnect_by_handle(gaim_pounces_get_handle());
}
