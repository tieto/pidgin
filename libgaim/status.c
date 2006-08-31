/**
 * @file status.c Status API
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

#include "blist.h"
#include "core.h"
#include "dbus-maybe.h"
#include "debug.h"
#include "notify.h"
#include "prefs.h"
#include "status.h"

/**
 * A type of status.
 */
struct _GaimStatusType
{
	GaimStatusPrimitive primitive;

	char *id;
	char *name;
	char *primary_attr_id;

	gboolean saveable;
	gboolean user_settable;
	gboolean independent;

	GList *attrs;
};

/**
 * A status attribute.
 */
struct _GaimStatusAttr
{
	char *id;
	char *name;
	GaimValue *value_type;
};

/**
 * A list of statuses.
 */
struct _GaimPresence
{
	GaimPresenceContext context;

	gboolean idle;
	time_t idle_time;
	time_t login_time;

	GList *statuses;
	GHashTable *status_table;

	GaimStatus *active_status;

	union
	{
		GaimAccount *account;

		struct
		{
			GaimConversation *conv;
			char *user;

		} chat;

		struct
		{
			GaimAccount *account;
			char *name;
			size_t ref_count;
			GList *buddies;

		} buddy;

	} u;
};

/**
 * An active status.
 */
struct _GaimStatus
{
	GaimStatusType *type;
	GaimPresence *presence;

	const char *title;

	gboolean active;

	GHashTable *attr_values;
};

typedef struct
{
	GaimAccount *account;
	char *name;
} GaimStatusBuddyKey;

static int primitive_scores[] =
{
	0,      /* unset                    */
	-500,   /* offline                  */
	100,    /* available                */
	-75,    /* unavailable              */
	-50,    /* invisible                */
	-100,   /* away                     */
	-200,   /* extended away            */
	-400,   /* mobile                   */
	-10,    /* idle, special case.      */
	-5      /* idle time, special case. */
};

static GHashTable *buddy_presences = NULL;

#define SCORE_IDLE      8
#define SCORE_IDLE_TIME 9

/**************************************************************************
 * GaimStatusPrimitive API
 **************************************************************************/
static struct GaimStatusPrimitiveMap
{
	GaimStatusPrimitive type;
	const char *id;
	const char *name;

} const status_primitive_map[] =
{
	{ GAIM_STATUS_UNSET,           "unset",           N_("Unset")           },
	{ GAIM_STATUS_OFFLINE,         "offline",         N_("Offline")         },
	{ GAIM_STATUS_AVAILABLE,       "available",       N_("Available")       },
	{ GAIM_STATUS_UNAVAILABLE,     "unavailable",     N_("Unavailable")     },
	{ GAIM_STATUS_INVISIBLE,       "invisible",       N_("Invisible")       },
	{ GAIM_STATUS_AWAY,            "away",            N_("Away")            },
	{ GAIM_STATUS_EXTENDED_AWAY,   "extended_away",   N_("Extended Away")   },
	{ GAIM_STATUS_MOBILE,          "mobile",          N_("Mobile")          }
};

const char *
gaim_primitive_get_id_from_type(GaimStatusPrimitive type)
{
    int i;

    for (i = 0; i < GAIM_STATUS_NUM_PRIMITIVES; i++)
    {
		if (type == status_primitive_map[i].type)
			return status_primitive_map[i].id;
    }

    return status_primitive_map[0].id;
}

const char *
gaim_primitive_get_name_from_type(GaimStatusPrimitive type)
{
    int i;

    for (i = 0; i < GAIM_STATUS_NUM_PRIMITIVES; i++)
    {
	if (type == status_primitive_map[i].type)
		return _(status_primitive_map[i].name);
    }

    return _(status_primitive_map[0].name);
}

GaimStatusPrimitive
gaim_primitive_get_type_from_id(const char *id)
{
    int i;

    g_return_val_if_fail(id != NULL, GAIM_STATUS_UNSET);

    for (i = 0; i < GAIM_STATUS_NUM_PRIMITIVES; i++)
    {
        if (!strcmp(id, status_primitive_map[i].id))
            return status_primitive_map[i].type;
    }

    return status_primitive_map[0].type;
}


/**************************************************************************
 * GaimStatusType API
 **************************************************************************/
GaimStatusType *
gaim_status_type_new_full(GaimStatusPrimitive primitive, const char *id,
						  const char *name, gboolean saveable,
						  gboolean user_settable, gboolean independent)
{
	GaimStatusType *status_type;

	g_return_val_if_fail(primitive != GAIM_STATUS_UNSET, NULL);

	status_type = g_new0(GaimStatusType, 1);
	GAIM_DBUS_REGISTER_POINTER(status_type, GaimStatusType);

	status_type->primitive     = primitive;
	status_type->saveable      = saveable;
	status_type->user_settable = user_settable;
	status_type->independent   = independent;

	if (id != NULL)
		status_type->id = g_strdup(id);
	else
		status_type->id = g_strdup(gaim_primitive_get_id_from_type(primitive));

	if (name != NULL)
		status_type->name = g_strdup(name);
	else
		status_type->name = g_strdup(gaim_primitive_get_name_from_type(primitive));

	return status_type;
}

GaimStatusType *
gaim_status_type_new(GaimStatusPrimitive primitive, const char *id,
					 const char *name, gboolean user_settable)
{
	g_return_val_if_fail(primitive != GAIM_STATUS_UNSET, NULL);

	return gaim_status_type_new_full(primitive, id, name, FALSE,
			user_settable, FALSE);
}

GaimStatusType *
gaim_status_type_new_with_attrs(GaimStatusPrimitive primitive,
		const char *id, const char *name,
		gboolean saveable, gboolean user_settable,
		gboolean independent, const char *attr_id,
		const char *attr_name, GaimValue *attr_value,
		...)
{
	GaimStatusType *status_type;
	va_list args;

	g_return_val_if_fail(primitive  != GAIM_STATUS_UNSET, NULL);
	g_return_val_if_fail(attr_id    != NULL,              NULL);
	g_return_val_if_fail(attr_name  != NULL,              NULL);
	g_return_val_if_fail(attr_value != NULL,              NULL);

	status_type = gaim_status_type_new_full(primitive, id, name, saveable,
			user_settable, independent);

	/* Add the first attribute */
	gaim_status_type_add_attr(status_type, attr_id, attr_name, attr_value);

	va_start(args, attr_value);
	gaim_status_type_add_attrs_vargs(status_type, args);
	va_end(args);

	return status_type;
}

void
gaim_status_type_destroy(GaimStatusType *status_type)
{
	g_return_if_fail(status_type != NULL);

	g_free(status_type->id);
	g_free(status_type->name);
	g_free(status_type->primary_attr_id);

	g_list_foreach(status_type->attrs, (GFunc)gaim_status_attr_destroy, NULL);
	g_list_free(status_type->attrs);

	GAIM_DBUS_UNREGISTER_POINTER(status_type);
	g_free(status_type);
}

void
gaim_status_type_set_primary_attr(GaimStatusType *status_type, const char *id)
{
	g_return_if_fail(status_type != NULL);

	g_free(status_type->primary_attr_id);
	status_type->primary_attr_id = g_strdup(id);
}

void
gaim_status_type_add_attr(GaimStatusType *status_type, const char *id,
		const char *name, GaimValue *value)
{
	GaimStatusAttr *attr;

	g_return_if_fail(status_type != NULL);
	g_return_if_fail(id          != NULL);
	g_return_if_fail(name        != NULL);
	g_return_if_fail(value       != NULL);

	attr = gaim_status_attr_new(id, name, value);

	status_type->attrs = g_list_append(status_type->attrs, attr);
}

void
gaim_status_type_add_attrs_vargs(GaimStatusType *status_type, va_list args)
{
	const char *id, *name;
	GaimValue *value;

	g_return_if_fail(status_type != NULL);

	while ((id = va_arg(args, const char *)) != NULL)
	{
		name = va_arg(args, const char *);
		g_return_if_fail(name != NULL);

		value = va_arg(args, GaimValue *);
		g_return_if_fail(value != NULL);

		gaim_status_type_add_attr(status_type, id, name, value);
	}
}

void
gaim_status_type_add_attrs(GaimStatusType *status_type, const char *id,
		const char *name, GaimValue *value, ...)
{
	va_list args;

	g_return_if_fail(status_type != NULL);
	g_return_if_fail(id          != NULL);
	g_return_if_fail(name        != NULL);
	g_return_if_fail(value       != NULL);

	/* Add the first attribute */
	gaim_status_type_add_attr(status_type, id, name, value);

	va_start(args, value);
	gaim_status_type_add_attrs_vargs(status_type, args);
	va_end(args);
}

GaimStatusPrimitive
gaim_status_type_get_primitive(const GaimStatusType *status_type)
{
	g_return_val_if_fail(status_type != NULL, GAIM_STATUS_UNSET);

	return status_type->primitive;
}

const char *
gaim_status_type_get_id(const GaimStatusType *status_type)
{
	g_return_val_if_fail(status_type != NULL, NULL);

	return status_type->id;
}

const char *
gaim_status_type_get_name(const GaimStatusType *status_type)
{
	g_return_val_if_fail(status_type != NULL, NULL);

	return status_type->name;
}

gboolean
gaim_status_type_is_saveable(const GaimStatusType *status_type)
{
	g_return_val_if_fail(status_type != NULL, FALSE);

	return status_type->saveable;
}

gboolean
gaim_status_type_is_user_settable(const GaimStatusType *status_type)
{
	g_return_val_if_fail(status_type != NULL, FALSE);

	return status_type->user_settable;
}

gboolean
gaim_status_type_is_independent(const GaimStatusType *status_type)
{
	g_return_val_if_fail(status_type != NULL, FALSE);

	return status_type->independent;
}

gboolean
gaim_status_type_is_exclusive(const GaimStatusType *status_type)
{
	g_return_val_if_fail(status_type != NULL, FALSE);

	return !status_type->independent;
}

gboolean
gaim_status_type_is_available(const GaimStatusType *status_type)
{
	GaimStatusPrimitive primitive;

	g_return_val_if_fail(status_type != NULL, FALSE);

	primitive = gaim_status_type_get_primitive(status_type);

	return (primitive == GAIM_STATUS_AVAILABLE);
}

const char *
gaim_status_type_get_primary_attr(const GaimStatusType *status_type)
{
	g_return_val_if_fail(status_type != NULL, NULL);

	return status_type->primary_attr_id;
}

GaimStatusAttr *
gaim_status_type_get_attr(const GaimStatusType *status_type, const char *id)
{
	GList *l;

	g_return_val_if_fail(status_type != NULL, NULL);
	g_return_val_if_fail(id          != NULL, NULL);

	for (l = status_type->attrs; l != NULL; l = l->next)
	{
		GaimStatusAttr *attr = (GaimStatusAttr *)l->data;

		if (!strcmp(gaim_status_attr_get_id(attr), id))
			return attr;
	}

	return NULL;
}

const GList *
gaim_status_type_get_attrs(const GaimStatusType *status_type)
{
	g_return_val_if_fail(status_type != NULL, NULL);

	return status_type->attrs;
}

const GaimStatusType *
gaim_status_type_find_with_id(GList *status_types, const char *id)
{
	GaimStatusType *status_type;

	g_return_val_if_fail(id != NULL, NULL);

	while (status_types != NULL)
	{
		status_type = status_types->data;

		if (!strcmp(id, status_type->id))
			return status_type;

		status_types = status_types->next;
	}

	return NULL;
}


/**************************************************************************
* GaimStatusAttr API
**************************************************************************/
GaimStatusAttr *
gaim_status_attr_new(const char *id, const char *name, GaimValue *value_type)
{
	GaimStatusAttr *attr;

	g_return_val_if_fail(id         != NULL, NULL);
	g_return_val_if_fail(name       != NULL, NULL);
	g_return_val_if_fail(value_type != NULL, NULL);

	attr = g_new0(GaimStatusAttr, 1);
	GAIM_DBUS_REGISTER_POINTER(attr, GaimStatusAttr);

	attr->id         = g_strdup(id);
	attr->name       = g_strdup(name);
	attr->value_type = value_type;

	return attr;
}

void
gaim_status_attr_destroy(GaimStatusAttr *attr)
{
	g_return_if_fail(attr != NULL);

	g_free(attr->id);
	g_free(attr->name);

	gaim_value_destroy(attr->value_type);

	GAIM_DBUS_UNREGISTER_POINTER(attr);
	g_free(attr);
}

const char *
gaim_status_attr_get_id(const GaimStatusAttr *attr)
{
	g_return_val_if_fail(attr != NULL, NULL);

	return attr->id;
}

const char *
gaim_status_attr_get_name(const GaimStatusAttr *attr)
{
	g_return_val_if_fail(attr != NULL, NULL);

	return attr->name;
}

GaimValue *
gaim_status_attr_get_value(const GaimStatusAttr *attr)
{
	g_return_val_if_fail(attr != NULL, NULL);

	return attr->value_type;
}


/**************************************************************************
* GaimStatus API
**************************************************************************/
GaimStatus *
gaim_status_new(GaimStatusType *status_type, GaimPresence *presence)
{
	GaimStatus *status;
	const GList *l;

	g_return_val_if_fail(status_type != NULL, NULL);
	g_return_val_if_fail(presence    != NULL, NULL);

	status = g_new0(GaimStatus, 1);
	GAIM_DBUS_REGISTER_POINTER(status, GaimStatus);

	status->type     = status_type;
	status->presence = presence;

	status->attr_values =
		g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
		(GDestroyNotify)gaim_value_destroy);

	for (l = gaim_status_type_get_attrs(status_type); l != NULL; l = l->next)
	{
		GaimStatusAttr *attr = (GaimStatusAttr *)l->data;
		GaimValue *value = gaim_status_attr_get_value(attr);
		GaimValue *new_value = gaim_value_dup(value);

		g_hash_table_insert(status->attr_values,
							g_strdup(gaim_status_attr_get_id(attr)),
							new_value);
	}

	return status;
}

/*
 * TODO: If the GaimStatus is in a GaimPresence, then
 *       remove it from the GaimPresence?
 */
void
gaim_status_destroy(GaimStatus *status)
{
	g_return_if_fail(status != NULL);

	g_hash_table_destroy(status->attr_values);

	GAIM_DBUS_UNREGISTER_POINTER(status);
	g_free(status);
}

static void
notify_buddy_status_update(GaimBuddy *buddy, GaimPresence *presence,
		GaimStatus *old_status, GaimStatus *new_status)
{
	GaimBlistUiOps *ops = gaim_blist_get_ui_ops();

	if (gaim_prefs_get_bool("/core/logging/log_system"))
	{
		time_t current_time = time(NULL);
		const char *buddy_alias = gaim_buddy_get_alias(buddy);
		char *tmp;
		GaimLog *log;

		if (old_status != NULL)
		{
			tmp = g_strdup_printf(_("%s changed status from %s to %s"), buddy_alias,
			                      gaim_status_get_name(old_status),
			                      gaim_status_get_name(new_status));
		}
		else
		{
			/* old_status == NULL when an independent status is toggled. */

			if (gaim_status_is_active(new_status))
			{
				tmp = g_strdup_printf(_("%s is now %s"), buddy_alias,
				                      gaim_status_get_name(new_status));
			}
			else
			{
				tmp = g_strdup_printf(_("%s is no longer %s"), buddy_alias,
				                      gaim_status_get_name(new_status));
			}
		}

		log = gaim_account_get_log(buddy->account, FALSE);
		if (log != NULL)
		{
			gaim_log_write(log, GAIM_MESSAGE_SYSTEM, buddy_alias,
			               current_time, tmp);
		}

		g_free(tmp);
	}

	if (ops != NULL && ops->update != NULL)
		ops->update(gaim_get_blist(), (GaimBlistNode*)buddy);
}

static void
notify_status_update(GaimPresence *presence, GaimStatus *old_status,
					 GaimStatus *new_status)
{
	GaimPresenceContext context = gaim_presence_get_context(presence);

	if (context == GAIM_PRESENCE_CONTEXT_ACCOUNT)
	{
		GaimAccount *account = gaim_presence_get_account(presence);
		GaimAccountUiOps *ops = gaim_accounts_get_ui_ops();

		if (gaim_account_get_enabled(account, gaim_core_get_ui()))
			gaim_prpl_change_account_status(account, old_status, new_status);

		if (ops != NULL && ops->status_changed != NULL)
		{
			ops->status_changed(account, new_status);
		}
	}
	else if (context == GAIM_PRESENCE_CONTEXT_BUDDY)
	{
		const GList *l;

		for (l = gaim_presence_get_buddies(presence); l != NULL; l = l->next)
		{
			notify_buddy_status_update((GaimBuddy *)l->data, presence,
					old_status, new_status);
		}
	}
}

static void
status_has_changed(GaimStatus *status)
{
	GaimPresence *presence;
	GaimStatus *old_status;

	presence   = gaim_status_get_presence(status);

	/*
	 * If this status is exclusive, then we must be setting it to "active."
	 * Since we are setting it to active, we want to set the currently
	 * active status to "inactive."
	 */
	if (gaim_status_is_exclusive(status))
	{
		old_status = gaim_presence_get_active_status(presence);
		if (old_status != NULL && (old_status != status))
			old_status->active = FALSE;
		presence->active_status = status;
	}
	else
		old_status = NULL;

	notify_status_update(presence, old_status, status);
}

void
gaim_status_set_active(GaimStatus *status, gboolean active)
{
	gaim_status_set_active_with_attrs_list(status, active, NULL);
}

/*
 * This used to parse the va_list directly, but now it creates a GList
 * and passes it to gaim_status_set_active_with_attrs_list().  That
 * function was created because accounts.c needs to pass a GList of
 * attributes to the status API.
 */
void
gaim_status_set_active_with_attrs(GaimStatus *status, gboolean active, va_list args)
{
	GList *attrs = NULL;
	const gchar *id;
	gpointer data;

	if (args != NULL)
	{
		while ((id = va_arg(args, const char *)) != NULL)
		{
			attrs = g_list_append(attrs, (char *)id);
			data = va_arg(args, void *);
			attrs = g_list_append(attrs, data);
		}
	}
	gaim_status_set_active_with_attrs_list(status, active, attrs);
	g_list_free(attrs);
}

void
gaim_status_set_active_with_attrs_list(GaimStatus *status, gboolean active,
									   const GList *attrs)
{
	gboolean changed = FALSE;
	const GList *l;
	GList *specified_attr_ids = NULL;
	GaimStatusType *status_type;

	g_return_if_fail(status != NULL);

	if (!active && gaim_status_is_exclusive(status))
	{
		gaim_debug_error("status",
				   "Cannot deactivate an exclusive status (%s).\n",
				   gaim_status_get_id(status));
		return;
	}

	if (status->active != active)
	{
		changed = TRUE;
	}

	status->active = active;

	/* Set any attributes */
	l = attrs;
	while (l != NULL)
	{
		const gchar *id;
		GaimValue *value;

		id = l->data;
		l = l->next;
		value = gaim_status_get_attr_value(status, id);
		if (value == NULL)
		{
			gaim_debug_warning("status", "The attribute \"%s\" on the status \"%s\" is "
							   "not supported.\n", id, status->type->name);
			/* Skip over the data and move on to the next attribute */
			l = l->next;
			continue;
		}

		specified_attr_ids = g_list_prepend(specified_attr_ids, (gpointer)id);

		if (value->type == GAIM_TYPE_STRING)
		{
			const gchar *string_data = l->data;
			l = l->next;
			if (((string_data == NULL) && (value->data.string_data == NULL)) ||
				((string_data != NULL) && (value->data.string_data != NULL) &&
				!strcmp(string_data, value->data.string_data)))
			{
				continue;
			}
			gaim_status_set_attr_string(status, id, string_data);
			changed = TRUE;
		}
		else if (value->type == GAIM_TYPE_INT)
		{
			int int_data = GPOINTER_TO_INT(l->data);
			l = l->next;
			if (int_data == value->data.int_data)
				continue;
			gaim_status_set_attr_int(status, id, int_data);
			changed = TRUE;
		}
		else if (value->type == GAIM_TYPE_BOOLEAN)
		{
			gboolean boolean_data = GPOINTER_TO_INT(l->data);
			l = l->next;
			if (boolean_data == value->data.boolean_data)
				continue;
			gaim_status_set_attr_boolean(status, id, boolean_data);
			changed = TRUE;
		}
		else
		{
			/* We don't know what the data is--skip over it */
			l = l->next;
		}
	}

	/* Reset any unspecified attributes to their default value */
	status_type = gaim_status_get_type(status);
	l = gaim_status_type_get_attrs(status_type);
	while (l != NULL)
	{
		GaimStatusAttr *attr;

		attr = l->data;
		if (!g_list_find_custom(specified_attr_ids, attr->id, (GCompareFunc)strcmp))
		{
			GaimValue *default_value;
			default_value = gaim_status_attr_get_value(attr);
			if (default_value->type == GAIM_TYPE_STRING)
				gaim_status_set_attr_string(status, attr->id,
						gaim_value_get_string(default_value));
			else if (default_value->type == GAIM_TYPE_INT)
				gaim_status_set_attr_int(status, attr->id,
						gaim_value_get_int(default_value));
			else if (default_value->type == GAIM_TYPE_BOOLEAN)
				gaim_status_set_attr_boolean(status, attr->id,
						gaim_value_get_boolean(default_value));
			changed = TRUE;
		}

		l = l->next;
	}
	g_list_free(specified_attr_ids);

	if (!changed)
		return;
	status_has_changed(status);
}

void
gaim_status_set_attr_boolean(GaimStatus *status, const char *id,
		gboolean value)
{
	GaimValue *attr_value;

	g_return_if_fail(status != NULL);
	g_return_if_fail(id     != NULL);

	/* Make sure this attribute exists and is the correct type. */
	attr_value = gaim_status_get_attr_value(status, id);
	g_return_if_fail(attr_value != NULL);
	g_return_if_fail(gaim_value_get_type(attr_value) == GAIM_TYPE_BOOLEAN);

	gaim_value_set_boolean(attr_value, value);
}

void
gaim_status_set_attr_int(GaimStatus *status, const char *id, int value)
{
	GaimValue *attr_value;

	g_return_if_fail(status != NULL);
	g_return_if_fail(id     != NULL);

	/* Make sure this attribute exists and is the correct type. */
	attr_value = gaim_status_get_attr_value(status, id);
	g_return_if_fail(attr_value != NULL);
	g_return_if_fail(gaim_value_get_type(attr_value) == GAIM_TYPE_INT);

	gaim_value_set_int(attr_value, value);
}

void
gaim_status_set_attr_string(GaimStatus *status, const char *id,
		const char *value)
{
	GaimValue *attr_value;

	g_return_if_fail(status != NULL);
	g_return_if_fail(id     != NULL);

	/* Make sure this attribute exists and is the correct type. */
	attr_value = gaim_status_get_attr_value(status, id);
	/* This used to be g_return_if_fail, but it's failing a LOT, so
	 * let's generate a log error for now. */
	/* g_return_if_fail(attr_value != NULL); */
	if (attr_value == NULL) {
		gaim_debug_error("status",
				 "Attempted to set status attribute '%s' for "
				 "status '%s', which is not legal.  Fix "
                                 "this!\n", id,
				 gaim_status_type_get_name(gaim_status_get_type(status)));
		return;
	}
	g_return_if_fail(gaim_value_get_type(attr_value) == GAIM_TYPE_STRING);

	gaim_value_set_string(attr_value, value);
}

GaimStatusType *
gaim_status_get_type(const GaimStatus *status)
{
	g_return_val_if_fail(status != NULL, NULL);

	return status->type;
}

GaimPresence *
gaim_status_get_presence(const GaimStatus *status)
{
	g_return_val_if_fail(status != NULL, NULL);

	return status->presence;
}

const char *
gaim_status_get_id(const GaimStatus *status)
{
	g_return_val_if_fail(status != NULL, NULL);

	return gaim_status_type_get_id(gaim_status_get_type(status));
}

const char *
gaim_status_get_name(const GaimStatus *status)
{
	g_return_val_if_fail(status != NULL, NULL);

	return gaim_status_type_get_name(gaim_status_get_type(status));
}

gboolean
gaim_status_is_independent(const GaimStatus *status)
{
	g_return_val_if_fail(status != NULL, FALSE);

	return gaim_status_type_is_independent(gaim_status_get_type(status));
}

gboolean
gaim_status_is_exclusive(const GaimStatus *status)
{
	g_return_val_if_fail(status != NULL, FALSE);

	return gaim_status_type_is_exclusive(gaim_status_get_type(status));
}

gboolean
gaim_status_is_available(const GaimStatus *status)
{
	g_return_val_if_fail(status != NULL, FALSE);

	return gaim_status_type_is_available(gaim_status_get_type(status));
}

gboolean
gaim_status_is_active(const GaimStatus *status)
{
	g_return_val_if_fail(status != NULL, FALSE);

	return status->active;
}

gboolean
gaim_status_is_online(const GaimStatus *status)
{
	GaimStatusPrimitive primitive;

	g_return_val_if_fail( status != NULL, FALSE);

	primitive = gaim_status_type_get_primitive(gaim_status_get_type(status));

	return (primitive != GAIM_STATUS_UNSET &&
			primitive != GAIM_STATUS_OFFLINE);
}

GaimValue *
gaim_status_get_attr_value(const GaimStatus *status, const char *id)
{
	g_return_val_if_fail(status != NULL, NULL);
	g_return_val_if_fail(id     != NULL, NULL);

	return (GaimValue *)g_hash_table_lookup(status->attr_values, id);
}

gboolean
gaim_status_get_attr_boolean(const GaimStatus *status, const char *id)
{
	const GaimValue *value;

	g_return_val_if_fail(status != NULL, FALSE);
	g_return_val_if_fail(id     != NULL, FALSE);

	if ((value = gaim_status_get_attr_value(status, id)) == NULL)
		return FALSE;

	g_return_val_if_fail(gaim_value_get_type(value) == GAIM_TYPE_BOOLEAN, FALSE);

	return gaim_value_get_boolean(value);
}

int
gaim_status_get_attr_int(const GaimStatus *status, const char *id)
{
	const GaimValue *value;

	g_return_val_if_fail(status != NULL, 0);
	g_return_val_if_fail(id     != NULL, 0);

	if ((value = gaim_status_get_attr_value(status, id)) == NULL)
		return 0;

	g_return_val_if_fail(gaim_value_get_type(value) == GAIM_TYPE_INT, 0);

	return gaim_value_get_int(value);
}

const char *
gaim_status_get_attr_string(const GaimStatus *status, const char *id)
{
	const GaimValue *value;

	g_return_val_if_fail(status != NULL, NULL);
	g_return_val_if_fail(id     != NULL, NULL);

	if ((value = gaim_status_get_attr_value(status, id)) == NULL)
		return NULL;

	g_return_val_if_fail(gaim_value_get_type(value) == GAIM_TYPE_STRING, NULL);

	return gaim_value_get_string(value);
}

gint
gaim_status_compare(const GaimStatus *status1, const GaimStatus *status2)
{
	GaimStatusType *type1, *type2;
	int score1 = 0, score2 = 0;

	if ((status1 == NULL && status2 == NULL) ||
			(status1 == status2))
	{
		return 0;
	}
	else if (status1 == NULL)
		return 1;
	else if (status2 == NULL)
		return -1;

	type1 = gaim_status_get_type(status1);
	type2 = gaim_status_get_type(status2);

	if (gaim_status_is_active(status1))
		score1 = primitive_scores[gaim_status_type_get_primitive(type1)];

	if (gaim_status_is_active(status2))
		score2 = primitive_scores[gaim_status_type_get_primitive(type2)];

	if (score1 > score2)
		return -1;
	else if (score1 < score2)
		return 1;

	return 0;
}


/**************************************************************************
* GaimPresence API
**************************************************************************/
GaimPresence *
gaim_presence_new(GaimPresenceContext context)
{
	GaimPresence *presence;

	g_return_val_if_fail(context != GAIM_PRESENCE_CONTEXT_UNSET, NULL);

	presence = g_new0(GaimPresence, 1);
	GAIM_DBUS_REGISTER_POINTER(presence, GaimPresence);

	presence->context = context;

	presence->status_table =
		g_hash_table_new_full(g_str_hash, g_str_equal,
							  g_free, NULL);

	return presence;
}

GaimPresence *
gaim_presence_new_for_account(GaimAccount *account)
{
	GaimPresence *presence = NULL;
	g_return_val_if_fail(account != NULL, NULL);

	presence = gaim_presence_new(GAIM_PRESENCE_CONTEXT_ACCOUNT);
	presence->u.account = account;
	presence->statuses = gaim_prpl_get_statuses(account, presence);

	return presence;
}

GaimPresence *
gaim_presence_new_for_conv(GaimConversation *conv)
{
	GaimPresence *presence;

	g_return_val_if_fail(conv != NULL, NULL);

	presence = gaim_presence_new(GAIM_PRESENCE_CONTEXT_CONV);
	presence->u.chat.conv = conv;
	/* presence->statuses = gaim_prpl_get_statuses(conv->account, presence); ? */

	return presence;
}

GaimPresence *
gaim_presence_new_for_buddy(GaimBuddy *buddy)
{
	GaimPresence *presence;
	GaimStatusBuddyKey *key;
	GaimAccount *account;

	g_return_val_if_fail(buddy != NULL, NULL);
	account = buddy->account;

	key = g_new0(GaimStatusBuddyKey, 1);
	key->account = buddy->account;
	key->name    = g_strdup(buddy->name);

	presence = g_hash_table_lookup(buddy_presences, key);
	if (presence == NULL)
	{
		presence = gaim_presence_new(GAIM_PRESENCE_CONTEXT_BUDDY);

		presence->u.buddy.name    = g_strdup(buddy->name);
		presence->u.buddy.account = buddy->account;
		presence->statuses = gaim_prpl_get_statuses(buddy->account, presence);

		g_hash_table_insert(buddy_presences, key, presence);
	}
	else
	{
		g_free(key->name);
		g_free(key);
	}

	presence->u.buddy.ref_count++;
	presence->u.buddy.buddies = g_list_append(presence->u.buddy.buddies,
			buddy);

	return presence;
}

void
gaim_presence_destroy(GaimPresence *presence)
{
	g_return_if_fail(presence != NULL);

	if (gaim_presence_get_context(presence) == GAIM_PRESENCE_CONTEXT_BUDDY)
	{
		GaimStatusBuddyKey key;

		if(presence->u.buddy.ref_count != 0)
			return;

		key.account = presence->u.buddy.account;
		key.name    = presence->u.buddy.name;

		g_hash_table_remove(buddy_presences, &key);

		g_free(presence->u.buddy.name);
	}
	else if (gaim_presence_get_context(presence) == GAIM_PRESENCE_CONTEXT_CONV)
	{
		g_free(presence->u.chat.user);
	}

	g_list_foreach(presence->statuses, (GFunc)gaim_status_destroy, NULL);
	g_list_free(presence->statuses);

	g_hash_table_destroy(presence->status_table);

	GAIM_DBUS_UNREGISTER_POINTER(presence);
	g_free(presence);
}

/*
 * TODO: Maybe we should cal gaim_presence_destroy() after we
 *       decrement the ref count?  I don't see why we should
 *       make other places do it manually when we can do it here.
 */
void
gaim_presence_remove_buddy(GaimPresence *presence, GaimBuddy *buddy)
{
	g_return_if_fail(presence != NULL);
	g_return_if_fail(buddy    != NULL);
	g_return_if_fail(gaim_presence_get_context(presence) ==
			GAIM_PRESENCE_CONTEXT_BUDDY);

	if (g_list_find(presence->u.buddy.buddies, buddy) != NULL)
	{
		presence->u.buddy.buddies = g_list_remove(presence->u.buddy.buddies,
				buddy);
		presence->u.buddy.ref_count--;
	}
}

void
gaim_presence_add_status(GaimPresence *presence, GaimStatus *status)
{
	g_return_if_fail(presence != NULL);
	g_return_if_fail(status   != NULL);

	presence->statuses = g_list_append(presence->statuses, status);

	g_hash_table_insert(presence->status_table,
	g_strdup(gaim_status_get_id(status)), status);
}

void
gaim_presence_add_list(GaimPresence *presence, const GList *source_list)
{
	const GList *l;

	g_return_if_fail(presence    != NULL);
	g_return_if_fail(source_list != NULL);

	for (l = source_list; l != NULL; l = l->next)
		gaim_presence_add_status(presence, (GaimStatus *)l->data);
}

void
gaim_presence_set_status_active(GaimPresence *presence, const char *status_id,
		gboolean active)
{
	GaimStatus *status;

	g_return_if_fail(presence  != NULL);
	g_return_if_fail(status_id != NULL);

	status = gaim_presence_get_status(presence, status_id);

	g_return_if_fail(status != NULL);
	/* TODO: Should we do the following? */
	/* g_return_if_fail(active == status->active); */

	if (gaim_status_is_exclusive(status))
	{
		if (!active)
		{
			gaim_debug_warning("status",
					"Attempted to set a non-independent status "
					"(%s) inactive. Only independent statuses "
					"can be specifically marked inactive.",
					status_id);
			return;
		}
	}

	gaim_status_set_active(status, active);
}

void
gaim_presence_switch_status(GaimPresence *presence, const char *status_id)
{
	gaim_presence_set_status_active(presence, status_id, TRUE);
}

static void
update_buddy_idle(GaimBuddy *buddy, GaimPresence *presence,
		time_t current_time, gboolean old_idle, gboolean idle)
{
	GaimBlistUiOps *ops = gaim_blist_get_ui_ops();

	if (!old_idle && idle)
	{
		if (gaim_prefs_get_bool("/core/logging/log_system"))
		{
			GaimLog *log = gaim_account_get_log(buddy->account, FALSE);

			if (log != NULL)
			{
				char *tmp = g_strdup_printf(_("%s became idle"),
				gaim_buddy_get_alias(buddy));

				gaim_log_write(log, GAIM_MESSAGE_SYSTEM,
				gaim_buddy_get_alias(buddy), current_time, tmp);
				g_free(tmp);
			}
		}
	}
	else if (old_idle && !idle)
	{
		if (gaim_prefs_get_bool("/core/logging/log_system"))
		{
			GaimLog *log = gaim_account_get_log(buddy->account, FALSE);

			if (log != NULL)
			{
				char *tmp = g_strdup_printf(_("%s became unidle"),
				gaim_buddy_get_alias(buddy));

				gaim_log_write(log, GAIM_MESSAGE_SYSTEM,
				gaim_buddy_get_alias(buddy), current_time, tmp);
				g_free(tmp);
			}
		}
	}

	if (old_idle != idle)
		gaim_signal_emit(gaim_blist_get_handle(), "buddy-idle-changed", buddy,
		                 old_idle, idle);

	gaim_contact_invalidate_priority_buddy(gaim_buddy_get_contact(buddy));

	/* Should this be done here? It'd perhaps make more sense to
	 * connect to buddy-[un]idle signals and update from there
	 */

	if (ops != NULL && ops->update != NULL)
		ops->update(gaim_get_blist(), (GaimBlistNode *)buddy);
}

void
gaim_presence_set_idle(GaimPresence *presence, gboolean idle, time_t idle_time)
{
	gboolean old_idle;

	g_return_if_fail(presence != NULL);

	if (presence->idle == idle && presence->idle_time == idle_time)
		return;

	old_idle            = presence->idle;
	presence->idle      = idle;
	presence->idle_time = (idle ? idle_time : 0);

	if (gaim_presence_get_context(presence) == GAIM_PRESENCE_CONTEXT_BUDDY)
	{
		const GList *l;
		time_t current_time = time(NULL);

		for (l = gaim_presence_get_buddies(presence); l != NULL; l = l->next)
		{
			update_buddy_idle((GaimBuddy *)l->data, presence, current_time,
			old_idle, idle);
		}
	}
	else if(gaim_presence_get_context(presence) == GAIM_PRESENCE_CONTEXT_ACCOUNT)
	{
		GaimAccount *account;
		GaimConnection *gc;
		GaimPluginProtocolInfo *prpl_info = NULL;

		account = gaim_presence_get_account(presence);

		if (gaim_prefs_get_bool("/core/logging/log_system"))
		{
			GaimLog *log = gaim_account_get_log(account, FALSE);

			if (log != NULL)
			{
				char *msg;

				if (idle)
					msg = g_strdup_printf(_("+++ %s became idle"), gaim_account_get_username(account));
				else
					msg = g_strdup_printf(_("+++ %s became unidle"), gaim_account_get_username(account));
				gaim_log_write(log, GAIM_MESSAGE_SYSTEM,
							   gaim_account_get_username(account),
							   idle_time, msg);
				g_free(msg);
			}
		}

		gc = gaim_account_get_connection(account);

		if (gc != NULL && GAIM_CONNECTION_IS_CONNECTED(gc) &&
				gc->prpl != NULL)
			prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

		if (prpl_info && prpl_info->set_idle)
			prpl_info->set_idle(gc, (idle ? (time(NULL) - idle_time) : 0));
	}
}

void
gaim_presence_set_login_time(GaimPresence *presence, time_t login_time)
{
	g_return_if_fail(presence != NULL);

	if (presence->login_time == login_time)
		return;

	presence->login_time = login_time;
}

GaimPresenceContext
gaim_presence_get_context(const GaimPresence *presence)
{
	g_return_val_if_fail(presence != NULL, GAIM_PRESENCE_CONTEXT_UNSET);

	return presence->context;
}

GaimAccount *
gaim_presence_get_account(const GaimPresence *presence)
{
	GaimPresenceContext context;

	g_return_val_if_fail(presence != NULL, NULL);

	context = gaim_presence_get_context(presence);

	g_return_val_if_fail(context == GAIM_PRESENCE_CONTEXT_ACCOUNT ||
			context == GAIM_PRESENCE_CONTEXT_BUDDY, NULL);

	return presence->u.account;
}

GaimConversation *
gaim_presence_get_conversation(const GaimPresence *presence)
{
	g_return_val_if_fail(presence != NULL, NULL);
	g_return_val_if_fail(gaim_presence_get_context(presence) ==
			GAIM_PRESENCE_CONTEXT_CONV, NULL);

	return presence->u.chat.conv;
}

const char *
gaim_presence_get_chat_user(const GaimPresence *presence)
{
	g_return_val_if_fail(presence != NULL, NULL);
	g_return_val_if_fail(gaim_presence_get_context(presence) ==
			GAIM_PRESENCE_CONTEXT_CONV, NULL);

	return presence->u.chat.user;
}

const GList *
gaim_presence_get_buddies(const GaimPresence *presence)
{
	g_return_val_if_fail(presence != NULL, NULL);
	g_return_val_if_fail(gaim_presence_get_context(presence) ==
			GAIM_PRESENCE_CONTEXT_BUDDY, NULL);

	return presence->u.buddy.buddies;
}

const GList *
gaim_presence_get_statuses(const GaimPresence *presence)
{
	g_return_val_if_fail(presence != NULL, NULL);

	return presence->statuses;
}

GaimStatus *
gaim_presence_get_status(const GaimPresence *presence, const char *status_id)
{
	GaimStatus *status;
	const GList *l = NULL;

	g_return_val_if_fail(presence  != NULL, NULL);
	g_return_val_if_fail(status_id != NULL, NULL);

	/* What's the purpose of this hash table? */
	status = (GaimStatus *)g_hash_table_lookup(presence->status_table,
						   status_id);

	if (status == NULL) {
		for (l = gaim_presence_get_statuses(presence);
			 l != NULL && status == NULL; l = l->next)
		{
			GaimStatus *temp_status = l->data;

			if (!strcmp(status_id, gaim_status_get_id(temp_status)))
				status = temp_status;
		}

		if (status != NULL)
			g_hash_table_insert(presence->status_table,
								g_strdup(gaim_status_get_id(status)), status);
	}

	return status;
}

GaimStatus *
gaim_presence_get_active_status(const GaimPresence *presence)
{
	g_return_val_if_fail(presence != NULL, NULL);

	return presence->active_status;
}

gboolean
gaim_presence_is_available(const GaimPresence *presence)
{
	GaimStatus *status;

	g_return_val_if_fail(presence != NULL, FALSE);

	status = gaim_presence_get_active_status(presence);

	return ((status != NULL && gaim_status_is_available(status)) &&
			!gaim_presence_is_idle(presence));
}

gboolean
gaim_presence_is_online(const GaimPresence *presence)
{
	GaimStatus *status;

	g_return_val_if_fail(presence != NULL, FALSE);

	if ((status = gaim_presence_get_active_status(presence)) == NULL)
		return FALSE;

	return gaim_status_is_online(status);
}

gboolean
gaim_presence_is_status_active(const GaimPresence *presence,
		const char *status_id)
{
	GaimStatus *status;

	g_return_val_if_fail(presence  != NULL, FALSE);
	g_return_val_if_fail(status_id != NULL, FALSE);

	status = gaim_presence_get_status(presence, status_id);

	return (status != NULL && gaim_status_is_active(status));
}

gboolean
gaim_presence_is_status_primitive_active(const GaimPresence *presence,
		GaimStatusPrimitive primitive)
{
	GaimStatus *status;
	GaimStatusType *status_type;

	g_return_val_if_fail(presence  != NULL,              FALSE);
	g_return_val_if_fail(primitive != GAIM_STATUS_UNSET, FALSE);

	status      = gaim_presence_get_active_status(presence);
	status_type = gaim_status_get_type(status);

	if (gaim_status_type_get_primitive(status_type) == primitive)
		return TRUE;

	return FALSE;
}

gboolean
gaim_presence_is_idle(const GaimPresence *presence)
{
	g_return_val_if_fail(presence != NULL, FALSE);

	return gaim_presence_is_online(presence) && presence->idle;
}

time_t
gaim_presence_get_idle_time(const GaimPresence *presence)
{
	g_return_val_if_fail(presence != NULL, 0);

	return presence->idle_time;
}

time_t
gaim_presence_get_login_time(const GaimPresence *presence)
{
	g_return_val_if_fail(presence != NULL, 0);

	return gaim_presence_is_online(presence) ? presence->login_time : 0;
}

gint
gaim_presence_compare(const GaimPresence *presence1,
		const GaimPresence *presence2)
{
	gboolean idle1, idle2;
	time_t idle_time_1, idle_time_2;
	int score1 = 0, score2 = 0;
	const GList *l;

	if (presence1 == presence2)
		return 0;
	else if (presence1 == NULL)
		return 1;
	else if (presence2 == NULL)
		return -1;

	/* Compute the score of the first set of statuses. */
	for (l = gaim_presence_get_statuses(presence1); l != NULL; l = l->next)
	{
		GaimStatus *status = (GaimStatus *)l->data;
		GaimStatusType *type = gaim_status_get_type(status);

		if (gaim_status_is_active(status))
			score1 += primitive_scores[gaim_status_type_get_primitive(type)];
	}
	score1 += gaim_account_get_int(gaim_presence_get_account(presence1), "score", 0);

	/* Compute the score of the second set of statuses. */
	for (l = gaim_presence_get_statuses(presence2); l != NULL; l = l->next)
	{
		GaimStatus *status = (GaimStatus *)l->data;
		GaimStatusType *type = gaim_status_get_type(status);

		if (gaim_status_is_active(status))
			score2 += primitive_scores[gaim_status_type_get_primitive(type)];
	}
	score2 += gaim_account_get_int(gaim_presence_get_account(presence2), "score", 0);

	idle1 = gaim_presence_is_idle(presence1);
	idle2 = gaim_presence_is_idle(presence2);

	if (idle1)
		score1 += primitive_scores[SCORE_IDLE];

	if (idle2)
		score2 += primitive_scores[SCORE_IDLE];

	idle_time_1 = time(NULL) - gaim_presence_get_idle_time(presence1);
	idle_time_2 = time(NULL) - gaim_presence_get_idle_time(presence2);

	if (idle_time_1 > idle_time_2)
		score1 += primitive_scores[SCORE_IDLE_TIME];
	else if (idle_time_1 < idle_time_2)
		score2 += primitive_scores[SCORE_IDLE_TIME];

	if (score1 < score2)
		return 1;
	else if (score1 > score2)
		return -1;

	return 0;
}


/**************************************************************************
* Status subsystem
**************************************************************************/
static void
score_pref_changed_cb(const char *name, GaimPrefType type,
					  gconstpointer value, gpointer data)
{
	int index = GPOINTER_TO_INT(data);

	primitive_scores[index] = GPOINTER_TO_INT(value);
}

static guint
gaim_buddy_presences_hash(gconstpointer key)
{
	const GaimStatusBuddyKey *me = key;
	guint ret;
	char *str;

	str = g_strdup_printf("%p%s", me->account, me->name);
	ret = g_str_hash(str);
	g_free(str);

	return ret;
}

static gboolean
gaim_buddy_presences_equal(gconstpointer a, gconstpointer b)
{
	GaimStatusBuddyKey *key_a = (GaimStatusBuddyKey *)a;
	GaimStatusBuddyKey *key_b = (GaimStatusBuddyKey *)b;

	if(key_a->account == key_b->account &&
			!strcmp(key_a->name, key_b->name))
		return TRUE;
	else
		return FALSE;
}

static void
gaim_buddy_presences_key_free(gpointer a)
{
	GaimStatusBuddyKey *key = (GaimStatusBuddyKey *)a;
	g_free(key->name);
	g_free(key);
}

void *
gaim_status_get_handle(void) {
	static int handle;

	return &handle;
}

void
gaim_status_init(void)
{
	void *handle = gaim_status_get_handle;

	gaim_prefs_add_none("/core/status");
	gaim_prefs_add_none("/core/status/scores");

	gaim_prefs_add_int("/core/status/scores/offline",
			primitive_scores[GAIM_STATUS_OFFLINE]);
	gaim_prefs_add_int("/core/status/scores/available",
			primitive_scores[GAIM_STATUS_AVAILABLE]);
	gaim_prefs_add_int("/core/status/scores/invisible",
			primitive_scores[GAIM_STATUS_INVISIBLE]);
	gaim_prefs_add_int("/core/status/scores/away",
			primitive_scores[GAIM_STATUS_AWAY]);
	gaim_prefs_add_int("/core/status/scores/extended_away",
			primitive_scores[GAIM_STATUS_EXTENDED_AWAY]);
	gaim_prefs_add_int("/core/status/scores/idle",
			primitive_scores[SCORE_IDLE]);

	gaim_prefs_connect_callback(handle, "/core/status/scores/offline",
			score_pref_changed_cb,
			GINT_TO_POINTER(GAIM_STATUS_OFFLINE));
	gaim_prefs_connect_callback(handle, "/core/status/scores/available",
			score_pref_changed_cb,
			GINT_TO_POINTER(GAIM_STATUS_AVAILABLE));
	gaim_prefs_connect_callback(handle, "/core/status/scores/invisible",
			score_pref_changed_cb,
			GINT_TO_POINTER(GAIM_STATUS_INVISIBLE));
	gaim_prefs_connect_callback(handle, "/core/status/scores/away",
			score_pref_changed_cb,
			GINT_TO_POINTER(GAIM_STATUS_AWAY));
	gaim_prefs_connect_callback(handle, "/core/status/scores/extended_away",
			score_pref_changed_cb,
			GINT_TO_POINTER(GAIM_STATUS_EXTENDED_AWAY));
	gaim_prefs_connect_callback(handle, "/core/status/scores/idle",
			score_pref_changed_cb,
			GINT_TO_POINTER(SCORE_IDLE));

	buddy_presences = g_hash_table_new_full(gaim_buddy_presences_hash,
											gaim_buddy_presences_equal,
											gaim_buddy_presences_key_free, NULL);
}

void
gaim_status_uninit(void)
{
	if (buddy_presences != NULL)
	{
		g_hash_table_destroy(buddy_presences);

		buddy_presences = NULL;
	}
}
