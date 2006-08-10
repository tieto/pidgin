#include <prefs.h>

#include "gntgaim.h"
#include "gntprefs.h"
#include "gntrequest.h"

#include <string.h>

void gg_prefs_init()
{
	gaim_prefs_add_none("/gaim");
	gaim_prefs_add_none("/gaim/gnt");

	gaim_prefs_add_none("/gaim/gnt/plugins");
	gaim_prefs_add_string_list("/gaim/gnt/plugins/loaded", NULL);

	gaim_prefs_add_none("/gaim/gnt/blist");
	gaim_prefs_add_bool("/gaim/gnt/blist/idletime", TRUE);
	
	gaim_prefs_add_none("/gaim/gnt/conversations");
	gaim_prefs_add_bool("/gaim/gnt/conversations/timestamps", TRUE);
	gaim_prefs_add_bool("/gaim/gnt/conversations/notify_typing", FALSE); /* XXX: Not functional yet */
}

typedef struct
{
	GaimPrefType type;
	const char *pref;
	const char *label;
	GList *(*lv)();   /* If the value is to be selected from a number of choices */
} Prefs;

static GList *
get_log_options()
{
	return gaim_log_logger_get_options();
}

static GaimRequestField *
get_pref_field(Prefs *prefs)
{
	GaimRequestField *field = NULL;

	if (prefs->lv == NULL)
	{
		switch (prefs->type)
		{
			case GAIM_PREF_BOOLEAN:
				field = gaim_request_field_bool_new(prefs->pref, prefs->label,
						gaim_prefs_get_bool(prefs->pref));
				break;
			case GAIM_PREF_INT:
				field = gaim_request_field_int_new(prefs->pref, prefs->label,
						gaim_prefs_get_int(prefs->pref));
				break;
			case GAIM_PREF_STRING:
				field = gaim_request_field_string_new(prefs->pref, prefs->label,
						gaim_prefs_get_string(prefs->pref), FALSE);
				break;
			default:
				break;
		}
	}
	else
	{
		GList *list = prefs->lv(), *iter;
		field = gaim_request_field_list_new(prefs->pref, prefs->label);
		for (iter = list; iter; iter = iter->next)
		{
			gboolean select = FALSE;
			const char *data = iter->data;
			iter = iter->next;
			switch (prefs->type)
			{
				case GAIM_PREF_BOOLEAN:
					if (gaim_prefs_get_bool(prefs->pref) == GPOINTER_TO_INT(iter->data))
						select = TRUE;
					break;
				case GAIM_PREF_INT:
					if (gaim_prefs_get_int(prefs->pref) == GPOINTER_TO_INT(iter->data))
						select = TRUE;
					break;
				case GAIM_PREF_STRING:
					if (strcmp(gaim_prefs_get_string(prefs->pref), iter->data) == 0)
						select = TRUE;
					break;
				default:
					break;
			}
			gaim_request_field_list_add(field, data, iter->data);
			if (select)
				gaim_request_field_list_add_selected(field, data);
		}
		g_list_free(list);
	}
	return field;
}

static Prefs blist[] = 
{
	{GAIM_PREF_BOOLEAN, "/gaim/gnt/blist/idletime", _("Show Idle Time"), NULL},
	{GAIM_PREF_NONE, NULL, NULL, NULL}
};

static Prefs convs[] = 
{
	{GAIM_PREF_BOOLEAN, "/gaim/gnt/conversations/timestamps", _("Show Timestamps"), NULL},
	{GAIM_PREF_BOOLEAN, "/gaim/gnt/conversations/notify_typing", _("Notify buddies when you are typing"), NULL},
	{GAIM_PREF_NONE, NULL, NULL, NULL}
};

static Prefs logging[] = 
{
	{GAIM_PREF_STRING, "/core/logging/format", _("Log format"), get_log_options},
	{GAIM_PREF_BOOLEAN, "/core/logging/log_ims", _("Log IMs"), NULL},
	{GAIM_PREF_BOOLEAN, "/core/logging/log_chats", _("Log chats"), NULL},
	{GAIM_PREF_BOOLEAN, "/core/logging/log_system", _("Log status change events"), NULL},
	{GAIM_PREF_NONE, NULL, NULL, NULL},
};

static void
save_cb(void *data, GaimRequestFields *allfields)
{
	GList *list;
	for (list = gaim_request_fields_get_groups(allfields); list; list = list->next)
	{
		GaimRequestFieldGroup *group = list->data;
		GList *fields = gaim_request_field_group_get_fields(group);
		
		for (; fields ; fields = fields->next)
		{
			GaimRequestField *field = fields->data;
			GaimRequestFieldType type = gaim_request_field_get_type(field);
			GaimPrefType pt;
			gpointer val = NULL;
			const char *id = gaim_request_field_get_id(field);

			switch (type)
			{
				case GAIM_REQUEST_FIELD_LIST:
					val = gaim_request_field_list_get_selected(field)->data;
					break;
				case GAIM_REQUEST_FIELD_BOOLEAN:
					val = GINT_TO_POINTER(gaim_request_field_bool_get_value(field));
					break;
				case GAIM_REQUEST_FIELD_INTEGER:
					val = GINT_TO_POINTER(gaim_request_field_int_get_value(field));
					break;
				case GAIM_REQUEST_FIELD_STRING:
					val = (gpointer)gaim_request_field_string_get_value(field);
					break;
				default:
					break;
			}

			pt = gaim_prefs_get_type(id);
			switch (pt)
			{
				case GAIM_PREF_INT:
					gaim_prefs_set_int(id, GPOINTER_TO_INT(val));
					break;
				case GAIM_PREF_BOOLEAN:
					gaim_prefs_set_bool(id, GPOINTER_TO_INT(val));
					break;
				case GAIM_PREF_STRING:
					gaim_prefs_set_string(id, val);
					break;
				default:
					break;
			}
		}
	}
}

static void
add_pref_group(GaimRequestFields *fields, const char *title, Prefs *prefs)
{
	GaimRequestField *field;
	GaimRequestFieldGroup *group;
	int i;

	group = gaim_request_field_group_new(title);
	gaim_request_fields_add_group(fields, group);
	for (i = 0; prefs[i].pref; i++)
	{
		field = get_pref_field(prefs + i);
		gaim_request_field_group_add_field(group, field);
	}
}

void gg_prefs_show_all()
{
	GaimRequestFields *fields;

	fields = gaim_request_fields_new();

	add_pref_group(fields, _("Buddy List"), blist);
	add_pref_group(fields, _("Conversations"), convs);
	add_pref_group(fields, _("Logging"), logging);

	gaim_request_fields(NULL, _("Preferences"), NULL, NULL, fields,
			_("Save"), G_CALLBACK(save_cb), _("Cancel"), NULL, NULL);
}

