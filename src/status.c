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
#include "debug.h"
#include "prefs.h"
#include "status.h"
#include "util.h"

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

	unsigned int warning_level;

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


#if 0
static GList *stored_statuses = NULL;

/*
 * XXX This stuff should be removed in a few versions. It stores the
 *     old v1 status stuff so we can write it later. We don't write out
 *     the new status stuff, though. These should all die soon, as the
 *     old status.xml was created before the new status system's design
 *     was created.
 *
 *       -- ChipX86
 */
typedef struct
{
	char *name;
	char *state;
	char *message;

} GaimStatusV1Info;

static GList *v1_statuses = NULL;
#endif


static int primitive_scores[] =
{
	0,      /* unset                    */
	-500,   /* offline                  */
	0,      /* online                   */
	100,    /* available                */
	-75,    /* unavailable              */
	-50,    /* hidden                   */
	-100,   /* away                     */
	-200    /* extended away            */
	-10,    /* idle, special case.      */
	-5      /* idle time, special case. */
};

static GHashTable *buddy_presences = NULL;

#define SCORE_IDLE      5
#define SCORE_IDLE_TIME 6

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
	g_return_val_if_fail(id        != NULL,              NULL);
	g_return_val_if_fail(name      != NULL,              NULL);

	status_type = g_new0(GaimStatusType, 1);

	status_type->primitive     = primitive;
	status_type->id            = g_strdup(id);
	status_type->name          = g_strdup(name);
	status_type->saveable      = saveable;
	status_type->user_settable = user_settable;
	status_type->independent   = independent;

	return status_type;
}

GaimStatusType *
gaim_status_type_new(GaimStatusPrimitive primitive, const char *id,
					 const char *name, gboolean user_settable)
{
	g_return_val_if_fail(primitive != GAIM_STATUS_UNSET, NULL);
	g_return_val_if_fail(id        != NULL,              NULL);
	g_return_val_if_fail(name      != NULL,              NULL);

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
	g_return_val_if_fail(id         != NULL,              NULL);
	g_return_val_if_fail(name       != NULL,              NULL);
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
	GList *l;

	g_return_if_fail(status_type != NULL);

	g_free(status_type->id);
	g_free(status_type->name);

	if (status_type->primary_attr_id != NULL)
		g_free(status_type->primary_attr_id);

	if (status_type->attrs != NULL)
	{
		for (l = status_type->attrs; l != NULL; l = l->next)
			gaim_status_attr_destroy((GaimStatusAttr *)l->data);

		g_list_free(status_type->attrs);
	}

	g_free(status_type);
}

void
gaim_status_type_set_primary_attr(GaimStatusType *status_type, const char *id)
{
	g_return_if_fail(status_type != NULL);

	if (status_type->primary_attr_id != NULL)
		g_free(status_type->primary_attr_id);

	status_type->primary_attr_id = (id == NULL ? NULL : g_strdup(id));
}

void
gaim_status_type_add_attr(GaimStatusType *status_type, const char *id,\
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

	return (primitive == GAIM_STATUS_AVAILABLE ||
			primitive == GAIM_STATUS_HIDDEN);
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
gaim_status_attr_get_value_type(const GaimStatusAttr *attr)
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

	status->type     = status_type;
	status->presence = presence;

	status->attr_values =
		g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
		(GDestroyNotify)gaim_value_destroy);

	for (l = gaim_status_type_get_attrs(status_type); l != NULL; l = l->next)
	{
		GaimStatusAttr *attr = (GaimStatusAttr *)l->data;
		GaimValue *value = gaim_status_attr_get_value_type(attr);
		GaimValue *new_value = gaim_value_dup(value);

		g_hash_table_insert(status->attr_values,
		g_strdup(gaim_status_attr_get_id(attr)), new_value);
	}

	return status;
}

void
gaim_status_destroy(GaimStatus *status)
{
	g_return_if_fail(status != NULL);

	gaim_status_set_active(status, FALSE);

	g_hash_table_destroy(status->attr_values);

	g_free(status);
}

static void
notify_buddy_status_update(GaimBuddy *buddy, GaimPresence *presence,
		GaimStatus *old_status, GaimStatus *new_status)
{
	GaimBlistUiOps *ops = gaim_blist_get_ui_ops();

	if (gaim_prefs_get_bool("/core/logging/log_system") &&
		gaim_prefs_get_bool("/core/logging/log_away_state"))
	{
		time_t current_time = time(NULL);
		const char *buddy_alias = gaim_buddy_get_alias(buddy);
		char *tmp = NULL;

		if (!gaim_status_is_available(old_status) &&
				gaim_status_is_available(new_status))
		{
			tmp = g_strdup_printf(_("%s came back"), buddy_alias);
		}
		else if (gaim_status_is_available(old_status) &&
				!gaim_status_is_available(new_status))
		{
			tmp = g_strdup_printf(_("%s went away"), buddy_alias);
		}

		if (tmp != NULL)
		{
			GaimLog *log = gaim_account_get_log(buddy->account);

			gaim_log_write(log, GAIM_MESSAGE_SYSTEM, buddy_alias,
			current_time, tmp);
			g_free(tmp);
		}
	}



	if (ops != NULL && ops->update != NULL)
		ops->update(gaim_get_blist(), (GaimBlistNode*)buddy);
}

static void
notify_status_update(GaimPresence *presence, GaimStatus *old_status,
		GaimStatus *new_status)
{
	GaimPresenceContext context = gaim_presence_get_context(presence);

	gaim_debug_info("notify_status_update", "Context is %d\n", context);

	if (context == GAIM_PRESENCE_CONTEXT_ACCOUNT)
	{
		GaimAccountUiOps *ops = gaim_accounts_get_ui_ops();

		if (ops != NULL && ops->status_changed != NULL)
		{
			ops->status_changed(gaim_presence_get_account(presence),
					new_status);
		}
	}
	else if (context == GAIM_PRESENCE_CONTEXT_CONV)
	{
/* TODO */
#if 0
		GaimConversationUiOps *ops;
		GaimConversation *conv;

		conv = gaim_status_get_conversation(new_status);
#endif
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

void
gaim_status_set_active(GaimStatus *status, gboolean active)
{
	GaimStatusType *status_type;
	GaimPresence *presence;
	GaimStatus *old_status;

	g_return_if_fail(status != NULL);

	if (status->active == active)
		return;

	status_type = gaim_status_get_type(status);

	if (!active && gaim_status_type_is_exclusive(status_type))
	{
		gaim_debug_error("status",
				   "Cannot deactivate an exclusive status (%s).\n",
				   gaim_status_type_get_id(status_type));
		return;
	}

	presence   = gaim_status_get_presence(status);
	old_status = gaim_presence_get_active_status(presence);

	if (gaim_status_type_is_exclusive(status_type))
	{
		const GList *l;

		for (l = gaim_presence_get_statuses(presence); l != NULL; l = l->next)
		{
			GaimStatus *temp_status = l->data;
			GaimStatusType *temp_type;

			temp_type = gaim_status_get_type(temp_status);

			if (gaim_status_type_is_independent(temp_type))
				continue;

			if (gaim_status_is_active(temp_status))
			{
				/*
				 * Since we don't want an infinite loop, we have to set
				 * the active variable ourself.
				 */
				temp_status->active = FALSE;

				notify_status_update(presence, old_status, temp_status);

				break;
			}
		}
	}

	status->active = active;

	notify_status_update(presence, old_status, status);
}

void
gaim_status_set_attr_boolean(GaimStatus *status, const char *id,
		gboolean value)
{
	GaimStatusType *status_type;
	GaimStatusAttr *attr;
	GaimValue *attr_value;

	g_return_if_fail(status != NULL);
	g_return_if_fail(id     != NULL);

	status_type = gaim_status_get_type(status);

	/* Make sure this attribute exists. */
	attr = gaim_status_type_get_attr(status_type, id);
	g_return_if_fail(attr != NULL);

	attr_value = gaim_status_attr_get_value_type(attr);
	g_return_if_fail(gaim_value_get_type(attr_value) == GAIM_TYPE_BOOLEAN);

	gaim_value_set_boolean(attr_value, value);
}

void
gaim_status_set_attr_int(GaimStatus *status, const char *id, int value)
{
	GaimStatusType *status_type;
	GaimStatusAttr *attr;
	GaimValue *attr_value;

	g_return_if_fail(status != NULL);
	g_return_if_fail(id     != NULL);

	status_type = gaim_status_get_type(status);

	/* Make sure this attribute exists. */
	attr = gaim_status_type_get_attr(status_type, id);
	g_return_if_fail(attr != NULL);

	attr_value = gaim_status_attr_get_value_type(attr);
	g_return_if_fail(gaim_value_get_type(attr_value) == GAIM_TYPE_INT);

	gaim_value_set_int(attr_value, value);
}

void
gaim_status_set_attr_string(GaimStatus *status, const char *id,
		const char *value)
{
	GaimStatusType *status_type;
	GaimStatusAttr *attr;
	GaimValue *attr_value;

	g_return_if_fail(status != NULL);
	g_return_if_fail(id     != NULL);

	status_type = gaim_status_get_type(status);

	/* Make sure this attribute exists. */
	attr = gaim_status_type_get_attr(status_type, id);
	g_return_if_fail(attr != NULL);

	attr_value = gaim_status_attr_get_value_type(attr);
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
	GaimStatusType *status_type;
	GaimStatusAttr *attr;

	g_return_val_if_fail(status != NULL, NULL);
	g_return_val_if_fail(id     != NULL, NULL);

	status_type = gaim_status_get_type(status);

	/* Make sure this attribute exists. */
	attr = gaim_status_type_get_attr(status_type, id);
	g_return_val_if_fail(attr != NULL, NULL);

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

	g_return_val_if_fail(gaim_value_get_type(value) == GAIM_TYPE_STRING, FALSE);

	return gaim_value_get_boolean(value);
}

int
gaim_status_get_attr_int(const GaimStatus *status, const char *id)
{
	const GaimValue *value;

	g_return_val_if_fail(status != NULL, FALSE);
	g_return_val_if_fail(id     != NULL, FALSE);

	if ((value = gaim_status_get_attr_value(status, id)) == NULL)
		return FALSE;

	g_return_val_if_fail(gaim_value_get_type(value) == GAIM_TYPE_INT, 0);

	return gaim_value_get_int(value);
}

const char *
gaim_status_get_attr_string(const GaimStatus *status, const char *id)
{
	const GaimValue *value;

	g_return_val_if_fail(status != NULL, FALSE);
	g_return_val_if_fail(id     != NULL, FALSE);

	if ((value = gaim_status_get_attr_value(status, id)) == NULL)
		return FALSE;

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

	presence->context = context;

	presence->status_table =
		g_hash_table_new_full(g_str_hash, g_str_equal,
							  g_free, (GFreeFunc)gaim_status_destroy);

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

		presence->u.buddy.ref_count--;

		if(presence->u.buddy.ref_count != 0)
			return;

		key.account = presence->u.buddy.account;
		key.name    = presence->u.buddy.name;

		g_hash_table_remove(buddy_presences, &key);

		if (presence->u.buddy.name != NULL)
			g_free(presence->u.buddy.name);
	}
	else if (gaim_presence_get_context(presence) == GAIM_PRESENCE_CONTEXT_CONV)
	{
		if (presence->u.chat.user != NULL)
			g_free(presence->u.chat.user);
	}

	if (presence->statuses != NULL)
		g_list_free(presence->statuses);

	g_hash_table_destroy(presence->status_table);

	g_free(presence);
}

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
gaim_presence_add_presence(GaimPresence *presence, const GList *source_list)
{
	const GList *l;

	g_return_if_fail(presence    != NULL);
	g_return_if_fail(source_list != NULL);

	for (l = source_list; l != NULL; l = l->next)
		gaim_presence_add_status(presence, (GaimStatus *)l->data);
}

/*
 * TODO: Should we g_return_if_fail(active == status_id->active); ?
 *
 * TODO: If a buddy signed on or off, should we do any of the following?
 *       (Note: We definitely need to do some of this somewhere, I'm just 
 *        not sure if here is the correct place.)
 *
 *           char *tmp = g_strdup_printf(_("%s logged in."), alias);
 *           gaim_conversation_write(c, NULL, tmp, GAIM_MESSAGE_SYSTEM, time(NULL));
 *           g_free(tmp);
 *
 *           GaimLog *log = gaim_account_get_log(account);
 *           char *tmp = g_strdup_printf(_("%s signed on"), alias);
 *           gaim_log_write(log, GAIM_MESSAGE_SYSTEM, (alias ? alias : name), current_time, tmp);
 *           g_free(tmp);
 *
 *           gaim_sound_play_event(GAIM_SOUND_BUDDY_ARRIVE); 
 *
 *           char *tmp = g_strdup_printf(_("%s logged out."), alias);
 *           gaim_conversation_write(c, NULL, tmp, GAIM_MESSAGE_SYSTEM, time(NULL));
 *           g_free(tmp); 
 *
 *           char *tmp = g_strdup_printf(_("%s signed off"), alias);
 *           gaim_log_write(log, GAIM_MESSAGE_SYSTEM, (alias ? alias : name), current_time, tmp);
 *           g_free(tmp);
 *
 *           serv_got_typing_stopped(gc, name);
 *
 *           gaim_sound_play_event(GAIM_SOUND_BUDDY_LEAVE); 
 *
 *           gaim_conversation_update(c, GAIM_CONV_UPDATE_AWAY);
 *
 */
void
gaim_presence_set_status_active(GaimPresence *presence, const char *status_id,
		gboolean active)
{
	GaimStatus *status;

	g_return_if_fail(presence  != NULL);
	g_return_if_fail(status_id != NULL);

	status = gaim_presence_get_status(presence, status_id);

	g_return_if_fail(status != NULL);

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

	} else if (presence->active_status != NULL) {
		gaim_status_set_active(presence->active_status, FALSE);

	}

	gaim_status_set_active(status, active);
	presence->active_status = status;
}

void
gaim_presence_switch_status(GaimPresence *presence, const char *status_id)
{
	GaimStatus *status;

	g_return_if_fail(presence  != NULL);
	g_return_if_fail(status_id != NULL);

	status = gaim_presence_get_status(presence, status_id);

	g_return_if_fail(status != NULL);

	if (gaim_status_is_independent(status))
		return;

	if (presence->active_status != NULL)
		gaim_status_set_active(presence->active_status, FALSE);

	gaim_status_set_active(status, TRUE);
}

static void
update_buddy_idle(GaimBuddy *buddy, GaimPresence *presence,
		time_t current_time, gboolean old_idle, gboolean idle)
{
	GaimBlistUiOps *ops = gaim_get_blist()->ui_ops;

	if (!old_idle && idle)
	{
		gaim_signal_emit(gaim_blist_get_handle(), "buddy-idle", buddy);

		if (gaim_prefs_get_bool("/core/logging/log_system") &&
				gaim_prefs_get_bool("/core/logging/log_idle_state"))
		{
			GaimLog *log = gaim_account_get_log(buddy->account);
			char *tmp = g_strdup_printf(_("%s became idle"),
			gaim_buddy_get_alias(buddy));

			gaim_log_write(log, GAIM_MESSAGE_SYSTEM,
			gaim_buddy_get_alias(buddy), current_time, tmp);
			g_free(tmp);
		}
	}
	else if (old_idle && !idle)
	{
		gaim_signal_emit(gaim_blist_get_handle(), "buddy-unidle", buddy);

		if (gaim_prefs_get_bool("/core/logging/log_system") &&
				gaim_prefs_get_bool("/core/logging/log_idle_state"))
		{
			GaimLog *log = gaim_account_get_log(buddy->account);
			char *tmp = g_strdup_printf(_("%s became unidle"),
			gaim_buddy_get_alias(buddy));

			gaim_log_write(log, GAIM_MESSAGE_SYSTEM,
			gaim_buddy_get_alias(buddy), current_time, tmp);
			g_free(tmp);
		}
	}

	gaim_contact_compute_priority_buddy(gaim_buddy_get_contact(buddy));

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
}

void
gaim_presence_set_login_time(GaimPresence *presence, time_t login_time)
{
	g_return_if_fail(presence != NULL);

	if (presence->login_time == login_time)
		return;

	presence->login_time = login_time;
}

void
gaim_presence_set_warning_level(GaimPresence *presence, unsigned int level)
{
	g_return_if_fail(presence != NULL);
	g_return_if_fail(level <= 100);

	if (presence->warning_level == level)
		return;

	presence->warning_level = level;

	if (gaim_presence_get_context(presence) == GAIM_PRESENCE_CONTEXT_BUDDY)
	{
		GaimBlistUiOps *ops = gaim_get_blist()->ui_ops;

		if (ops != NULL && ops->update != NULL)
		{
			const GList *l;

			for (l = gaim_presence_get_buddies(presence);
					l != NULL;
					l = l->next)
			{
				ops->update(gaim_get_blist(), (GaimBlistNode *)l->data);
			}
		}
	}
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

	return presence->idle;
}

time_t
gaim_presence_get_idle_time(const GaimPresence *presence)
{
	g_return_val_if_fail(presence != NULL, 0);

	return presence->idle_time;
}

unsigned int
gaim_presence_get_warning_level(const GaimPresence *presence)
{
	g_return_val_if_fail(presence != NULL, 0);

	return presence->warning_level;
}

gint
gaim_presence_compare(const GaimPresence *presence1,
		const GaimPresence *presence2)
{
	gboolean idle1, idle2;
	size_t idle_time_1, idle_time_2;
	int score1 = 0, score2 = 0;
	const GList *l;

	if ((presence1 == NULL && presence2 == NULL) || (presence1 == presence2))
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

	/* Compute the score of the second set of statuses. */
	for (l = gaim_presence_get_statuses(presence2); l != NULL; l = l->next)
	{
		GaimStatus *status = (GaimStatus *)l->data;
		GaimStatusType *type = gaim_status_get_type(status);

		if (gaim_status_is_active(status))
			score2 += primitive_scores[gaim_status_type_get_primitive(type)];
	}

	idle1 = gaim_presence_is_idle(presence1);
	idle2 = gaim_presence_is_idle(presence2);

	if (idle1)
		score1 += primitive_scores[SCORE_IDLE];

	if (idle2)
		score2 += primitive_scores[SCORE_IDLE];

	idle_time_1 = gaim_presence_get_idle_time(presence1);
	idle_time_2 = gaim_presence_get_idle_time(presence2);

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
score_pref_changed_cb(const char *name, GaimPrefType type, gpointer value,
		gpointer data)
{
	int index = GPOINTER_TO_INT(data);

	primitive_scores[index] = GPOINTER_TO_INT(value);
}

guint
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

gboolean
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

void *
gaim_statuses_get_handle() {
	static int handle;

	return &handle;
}

void
gaim_statuses_init(void)
{
	void *handle = gaim_statuses_get_handle;

	gaim_prefs_add_none("/core/status");
	gaim_prefs_add_none("/core/status/scores");

	gaim_prefs_add_int("/core/status/scores/offline",
			primitive_scores[GAIM_STATUS_OFFLINE]);
	gaim_prefs_add_int("/core/status/scores/available",
			primitive_scores[GAIM_STATUS_AVAILABLE]);
	gaim_prefs_add_int("/core/status/scores/hidden",
			primitive_scores[GAIM_STATUS_HIDDEN]);
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
	gaim_prefs_connect_callback(handle, "/core/status/scores/hidden",
			score_pref_changed_cb,
			GINT_TO_POINTER(GAIM_STATUS_HIDDEN));
	gaim_prefs_connect_callback(handle, "/core/status/scores/away",
			score_pref_changed_cb,
			GINT_TO_POINTER(GAIM_STATUS_AWAY));
	gaim_prefs_connect_callback(handle, "/core/status/scores/extended_away",
			score_pref_changed_cb,
			GINT_TO_POINTER(GAIM_STATUS_EXTENDED_AWAY));
	gaim_prefs_connect_callback(handle, "/core/status/scores/idle",
			score_pref_changed_cb,
			GINT_TO_POINTER(SCORE_IDLE));

	buddy_presences = g_hash_table_new(gaim_buddy_presences_hash,
									   gaim_buddy_presences_equal);
}

void
gaim_statuses_uninit(void)
{
	if(buddy_presences != NULL)
		g_hash_table_destroy(buddy_presences);
	buddy_presences = NULL;
}

void
gaim_statuses_sync(void)
{
}

void
gaim_statuses_load(void)
{
}
