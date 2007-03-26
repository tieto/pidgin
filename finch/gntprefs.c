/**
 * @file gntprefs.c GNT Preferences API
 * @ingroup gntui
 *
 * finch
 *
 * Finch is the legal property of its developers, whose names are too numerous
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
#include <prefs.h>
#include <savedstatuses.h>

#include "finch.h"
#include "gntprefs.h"
#include "gntrequest.h"

#include <string.h>

static GList *freestrings;  /* strings to be freed when the pref-window is closed */

void finch_prefs_init()
{
	purple_prefs_add_none("/purple");
	purple_prefs_add_none("/purple/gnt");

	purple_prefs_add_none("/purple/gnt/plugins");
	purple_prefs_add_path_list("/purple/gnt/plugins/loaded", NULL);

	purple_prefs_add_none("/purple/gnt/conversations");
	purple_prefs_add_bool("/purple/gnt/conversations/timestamps", TRUE);
	purple_prefs_add_bool("/purple/gnt/conversations/notify_typing", FALSE); /* XXX: Not functional yet */
}

typedef struct
{
	PurplePrefType type;
	const char *pref;
	const char *label;
	GList *(*lv)();   /* If the value is to be selected from a number of choices */
} Prefs;

static GList *
get_log_options()
{
	return purple_log_logger_get_options();
}

static GList *
get_idle_options()
{
	GList *list = NULL;
	list = g_list_append(list, "Based on keyboard use"); /* XXX: string freeze */
	list = g_list_append(list, "system");
	list = g_list_append(list, (char*)_("From last sent message"));
	list = g_list_append(list, "purple");
	list = g_list_append(list, (char*)_("Never"));
	list = g_list_append(list, "never");
	return list;
}

static GList *
get_status_titles()
{
	GList *list = NULL;
	const GList *iter;
	for (iter = purple_savedstatuses_get_all(); iter; iter = iter->next) {
		char *str;
		if (purple_savedstatus_is_transient(iter->data))
			continue;
		str = g_strdup_printf("%ld", purple_savedstatus_get_creation_time(iter->data));
		list = g_list_append(list, (char*)purple_savedstatus_get_title(iter->data));
		list = g_list_append(list, str);
		freestrings = g_list_prepend(freestrings, str);
	}
	return list;
}

static PurpleRequestField *
get_pref_field(Prefs *prefs)
{
	PurpleRequestField *field = NULL;

	if (prefs->lv == NULL)
	{
		switch (prefs->type)
		{
			case PURPLE_PREF_BOOLEAN:
				field = purple_request_field_bool_new(prefs->pref, _(prefs->label),
						purple_prefs_get_bool(prefs->pref));
				break;
			case PURPLE_PREF_INT:
				field = purple_request_field_int_new(prefs->pref, _(prefs->label),
						purple_prefs_get_int(prefs->pref));
				break;
			case PURPLE_PREF_STRING:
				field = purple_request_field_string_new(prefs->pref, _(prefs->label),
						purple_prefs_get_string(prefs->pref), FALSE);
				break;
			default:
				break;
		}
	}
	else
	{
		GList *list = prefs->lv(), *iter;
		if (list)
			field = purple_request_field_list_new(prefs->pref, _(prefs->label));
		for (iter = list; iter; iter = iter->next)
		{
			gboolean select = FALSE;
			const char *data = iter->data;
			int idata;
			iter = iter->next;
			switch (prefs->type)
			{
				case PURPLE_PREF_BOOLEAN:
					sscanf(iter->data, "%d", &idata);
					if (purple_prefs_get_bool(prefs->pref) == idata)
						select = TRUE;
					break;
				case PURPLE_PREF_INT:
					sscanf(iter->data, "%d", &idata);
					if (purple_prefs_get_int(prefs->pref) == idata)
						select = TRUE;
					break;
				case PURPLE_PREF_STRING:
					if (strcmp(purple_prefs_get_string(prefs->pref), iter->data) == 0)
						select = TRUE;
					break;
				default:
					break;
			}
			purple_request_field_list_add(field, data, iter->data);
			if (select)
				purple_request_field_list_add_selected(field, data);
		}
		g_list_free(list);
	}
	return field;
}

static Prefs blist[] = 
{
	{PURPLE_PREF_BOOLEAN, "/purple/gnt/blist/idletime", N_("Show Idle Time"), NULL},
	{PURPLE_PREF_BOOLEAN, "/purple/gnt/blist/showoffline", N_("Show Offline Buddies"), NULL},
	{PURPLE_PREF_NONE, NULL, NULL, NULL}
};

static Prefs convs[] = 
{
	{PURPLE_PREF_BOOLEAN, "/purple/gnt/conversations/timestamps", N_("Show Timestamps"), NULL},
	{PURPLE_PREF_BOOLEAN, "/purple/gnt/conversations/notify_typing", N_("Notify buddies when you are typing"), NULL},
	{PURPLE_PREF_NONE, NULL, NULL, NULL}
};

static Prefs logging[] = 
{
	{PURPLE_PREF_STRING, "/core/logging/format", N_("Log format"), get_log_options},
	{PURPLE_PREF_BOOLEAN, "/core/logging/log_ims", N_("Log IMs"), NULL},
	{PURPLE_PREF_BOOLEAN, "/core/logging/log_chats", N_("Log chats"), NULL},
	{PURPLE_PREF_BOOLEAN, "/core/logging/log_system", N_("Log status change events"), NULL},
	{PURPLE_PREF_NONE, NULL, NULL, NULL},
};

/* XXX: Translate after the freeze */
static Prefs idle[] =
{
	{PURPLE_PREF_STRING, "/core/away/idle_reporting", "Report Idle time", get_idle_options},
	{PURPLE_PREF_BOOLEAN, "/core/away/away_when_idle", "Change status when idle", NULL},
	{PURPLE_PREF_INT, "/core/away/mins_before_away", "Minutes before changing status", NULL},
	{PURPLE_PREF_INT, "/core/savedstatus/idleaway", "Change status to", get_status_titles},
	{PURPLE_PREF_NONE, NULL, NULL, NULL},
};

static void
free_strings()
{
	g_list_foreach(freestrings, (GFunc)g_free, NULL);
	g_list_free(freestrings);
	freestrings = NULL;
}

static void
save_cb(void *data, PurpleRequestFields *allfields)
{
	GList *list;
	for (list = purple_request_fields_get_groups(allfields); list; list = list->next)
	{
		PurpleRequestFieldGroup *group = list->data;
		GList *fields = purple_request_field_group_get_fields(group);
		
		for (; fields ; fields = fields->next)
		{
			PurpleRequestField *field = fields->data;
			PurpleRequestFieldType type = purple_request_field_get_type(field);
			PurplePrefType pt;
			gpointer val = NULL;
			const char *id = purple_request_field_get_id(field);

			switch (type)
			{
				case PURPLE_REQUEST_FIELD_LIST:
					val = purple_request_field_list_get_selected(field)->data;
					break;
				case PURPLE_REQUEST_FIELD_BOOLEAN:
					val = GINT_TO_POINTER(purple_request_field_bool_get_value(field));
					break;
				case PURPLE_REQUEST_FIELD_INTEGER:
					val = GINT_TO_POINTER(purple_request_field_int_get_value(field));
					break;
				case PURPLE_REQUEST_FIELD_STRING:
					val = (gpointer)purple_request_field_string_get_value(field);
					break;
				default:
					break;
			}

			pt = purple_prefs_get_type(id);
			switch (pt)
			{
				case PURPLE_PREF_INT:
					if (type == PURPLE_REQUEST_FIELD_LIST) /* Lists always return string */
						sscanf(val, "%ld", (long int *)&val);
					purple_prefs_set_int(id, GPOINTER_TO_INT(val));
					break;
				case PURPLE_PREF_BOOLEAN:
					purple_prefs_set_bool(id, GPOINTER_TO_INT(val));
					break;
				case PURPLE_PREF_STRING:
					purple_prefs_set_string(id, val);
					break;
				default:
					break;
			}
		}
	}
	free_strings();
}

static void
add_pref_group(PurpleRequestFields *fields, const char *title, Prefs *prefs)
{
	PurpleRequestField *field;
	PurpleRequestFieldGroup *group;
	int i;

	group = purple_request_field_group_new(title);
	purple_request_fields_add_group(fields, group);
	for (i = 0; prefs[i].pref; i++)
	{
		field = get_pref_field(prefs + i);
		if (field)
			purple_request_field_group_add_field(group, field);
	}
}

void finch_prefs_show_all()
{
	PurpleRequestFields *fields;

	fields = purple_request_fields_new();

	add_pref_group(fields, _("Buddy List"), blist);
	add_pref_group(fields, _("Conversations"), convs);
	add_pref_group(fields, _("Logging"), logging);
	add_pref_group(fields, _("Idle"), idle);

	purple_request_fields(NULL, _("Preferences"), NULL, NULL, fields,
			_("Save"), G_CALLBACK(save_cb), _("Cancel"), free_strings, NULL);
}

