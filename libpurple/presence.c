/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */
#include "internal.h"
#include "debug.h"
#include "dbus-maybe.h"
#include "presence.h"

#define PURPLE_PRESENCE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_PRESENCE, PurplePresencePrivate))

/** @copydoc _PurplePresencePrivate */
typedef struct _PurplePresencePrivate  PurplePresencePrivate;

#define PURPLE_ACCOUNT_PRESENCE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_ACCOUNT_PRESENCE, PurpleAccountPresencePrivate))

/** @copydoc _PurpleAccountPresencePrivate */
typedef struct _PurpleAccountPresencePrivate  PurpleAccountPresencePrivate;

#define PURPLE_BUDDY_PRESENCE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_BUDDY_PRESENCE, PurpleBuddyPresencePrivate))

/** @copydoc _PurpleBuddyPresencePrivate */
typedef struct _PurpleBuddyPresencePrivate  PurpleBuddyPresencePrivate;

/** Private data for a presence */
struct _PurplePresencePrivate
{
	gboolean idle;
	time_t idle_time;
	time_t login_time;

	GList *statuses;
	GHashTable *status_table;

	PurpleStatus *active_status;
};

/* Presence property enums */
enum
{
	PRES_PROP_0,
	PRES_PROP_IDLE,
	PRES_PROP_IDLE_TIME,
	PRES_PROP_LOGIN_TIME,
	PRES_PROP_STATUSES,
	PRES_PROP_ACTIVE_STATUS,
	PRES_PROP_LAST
};

/** Private data for an account presence */
struct _PurpleAccountPresencePrivate
{
	PurpleAccount *account;
};

/* Account presence property enums */
enum
{
	ACPRES_PROP_0,
	ACPRES_PROP_ACCOUNT,
	ACPRES_PROP_LAST
};

/** Private data for a buddy presence */
struct _PurpleBuddyPresencePrivate
{
	PurpleBuddy *buddy;
};

/* Buddy presence property enums */
enum
{
	BUDPRES_PROP_0,
	BUDPRES_PROP_BUDDY,
	BUDPRES_PROP_LAST
};

static GObjectClass         *parent_class;
static PurplePresenceClass  *presence_class;

int *_purple_get_primitive_scores(void);

/**************************************************************************
* PurplePresence API
**************************************************************************/

void
purple_presence_set_status_active(PurplePresence *presence, const char *status_id,
		gboolean active)
{
	PurpleStatus *status;

	g_return_if_fail(presence  != NULL);
	g_return_if_fail(status_id != NULL);

	status = purple_presence_get_status(presence, status_id);

	g_return_if_fail(status != NULL);
	/* TODO: Should we do the following? */
	/* g_return_if_fail(active == status->active); */

	if (purple_status_is_exclusive(status))
	{
		if (!active)
		{
			purple_debug_warning("status",
					"Attempted to set a non-independent status "
					"(%s) inactive. Only independent statuses "
					"can be specifically marked inactive.",
					status_id);
			return;
		}
	}

	purple_status_set_active(status, active);
}

void
purple_presence_switch_status(PurplePresence *presence, const char *status_id)
{
	purple_presence_set_status_active(presence, status_id, TRUE);
}

void
purple_presence_set_idle(PurplePresence *presence, gboolean idle, time_t idle_time)
{
	gboolean old_idle;
	PurplePresencePrivate *priv = PURPLE_PRESENCE_GET_PRIVATE(presence);
	PurplePresenceClass *klass = PURPLE_PRESENCE_GET_CLASS(presence);

	g_return_if_fail(priv != NULL);

	if (priv->idle == idle && priv->idle_time == idle_time)
		return;

	old_idle        = priv->idle;
	priv->idle      = idle;
	priv->idle_time = (idle ? idle_time : 0);

	if (klass->update_idle)
		klass->update_idle(presence, old_idle);
}

void
purple_presence_set_login_time(PurplePresence *presence, time_t login_time)
{
	PurplePresencePrivate *priv = PURPLE_PRESENCE_GET_PRIVATE(presence);

	g_return_if_fail(priv != NULL);

	if (priv->login_time == login_time)
		return;

	priv->login_time = login_time;
}

GList *
purple_presence_get_statuses(const PurplePresence *presence)
{
	PurplePresencePrivate *priv = PURPLE_PRESENCE_GET_PRIVATE(presence);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->statuses;
}

PurpleStatus *
purple_presence_get_status(const PurplePresence *presence, const char *status_id)
{
	PurpleStatus *status;
	PurplePresencePrivate *priv = PURPLE_PRESENCE_GET_PRIVATE(presence);
	GList *l = NULL;

	g_return_val_if_fail(priv      != NULL, NULL);
	g_return_val_if_fail(status_id != NULL, NULL);

	/* What's the purpose of this hash table? */
	status = (PurpleStatus *)g_hash_table_lookup(priv->status_table,
						   status_id);

	if (status == NULL) {
		for (l = purple_presence_get_statuses(presence);
			 l != NULL && status == NULL; l = l->next)
		{
			PurpleStatus *temp_status = l->data;

			if (purple_strequal(status_id, purple_status_get_id(temp_status)))
				status = temp_status;
		}

		if (status != NULL)
			g_hash_table_insert(priv->status_table,
								g_strdup(purple_status_get_id(status)), status);
	}

	return status;
}

PurpleStatus *
purple_presence_get_active_status(const PurplePresence *presence)
{
	PurplePresencePrivate *priv = PURPLE_PRESENCE_GET_PRIVATE(presence);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->active_status;
}

gboolean
purple_presence_is_available(const PurplePresence *presence)
{
	PurpleStatus *status;

	g_return_val_if_fail(presence != NULL, FALSE);

	status = purple_presence_get_active_status(presence);

	return ((status != NULL && purple_status_is_available(status)) &&
			!purple_presence_is_idle(presence));
}

gboolean
purple_presence_is_online(const PurplePresence *presence)
{
	PurpleStatus *status;

	g_return_val_if_fail(presence != NULL, FALSE);

	if ((status = purple_presence_get_active_status(presence)) == NULL)
		return FALSE;

	return purple_status_is_online(status);
}

gboolean
purple_presence_is_status_active(const PurplePresence *presence,
		const char *status_id)
{
	PurpleStatus *status;

	g_return_val_if_fail(presence  != NULL, FALSE);
	g_return_val_if_fail(status_id != NULL, FALSE);

	status = purple_presence_get_status(presence, status_id);

	return (status != NULL && purple_status_is_active(status));
}

gboolean
purple_presence_is_status_primitive_active(const PurplePresence *presence,
		PurpleStatusPrimitive primitive)
{
	GList *l;

	g_return_val_if_fail(presence  != NULL,              FALSE);
	g_return_val_if_fail(primitive != PURPLE_STATUS_UNSET, FALSE);

	for (l = purple_presence_get_statuses(presence);
	     l != NULL; l = l->next)
	{
		PurpleStatus *temp_status = l->data;
		PurpleStatusType *type = purple_status_get_status_type(temp_status);

		if (purple_status_type_get_primitive(type) == primitive &&
		    purple_status_is_active(temp_status))
			return TRUE;
	}
	return FALSE;
}

gboolean
purple_presence_is_idle(const PurplePresence *presence)
{
	PurplePresencePrivate *priv = PURPLE_PRESENCE_GET_PRIVATE(presence);

	g_return_val_if_fail(priv != NULL, FALSE);

	return purple_presence_is_online(presence) && priv->idle;
}

time_t
purple_presence_get_idle_time(const PurplePresence *presence)
{
	PurplePresencePrivate *priv = PURPLE_PRESENCE_GET_PRIVATE(presence);

	g_return_val_if_fail(priv != NULL, 0);

	return priv->idle_time;
}

time_t
purple_presence_get_login_time(const PurplePresence *presence)
{
	PurplePresencePrivate *priv = PURPLE_PRESENCE_GET_PRIVATE(presence);

	g_return_val_if_fail(priv != NULL, 0);

	return purple_presence_is_online(presence) ? priv->login_time : 0;
}

/**************************************************************************
 * GObject code for PurplePresence
 **************************************************************************/

/* GObject Property names */
#define PRES_PROP_IDLE_S           "idle"
#define PRES_PROP_IDLE_TIME_S      "idle-time"
#define PRES_PROP_LOGIN_TIME_S     "login-time"
#define PRES_PROP_STATUSES_S       "statuses"
#define PRES_PROP_ACTIVE_STATUS_S  "active-status"

/* Set method for GObject properties */
static void
purple_presence_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *pspec)
{
	PurplePresence *presence = PURPLE_PRESENCE(obj);
	PurplePresencePrivate *priv = PURPLE_PRESENCE_GET_PRIVATE(presence);

	switch (param_id) {
		case PRES_PROP_IDLE:
			purple_presence_set_idle(presence, g_value_get_boolean(value), 0);
			break;
		case PRES_PROP_IDLE_TIME:
#if SIZEOF_TIME_T == 4
			purple_presence_set_idle(presence, TRUE, g_value_get_int(value));
#elif SIZEOF_TIME_T == 8
			purple_presence_set_idle(presence, TRUE, g_value_get_int64(value));
#else
#error Unknown size of time_t
#endif
			break;
		case PRES_PROP_LOGIN_TIME:
#if SIZEOF_TIME_T == 4
			purple_presence_set_login_time(presence, g_value_get_int(value));
#elif SIZEOF_TIME_T == 8
			purple_presence_set_login_time(presence, g_value_get_int64(value));
#else
#error Unknown size of time_t
#endif
			break;
		case PRES_PROP_ACTIVE_STATUS:
			priv->active_status = g_value_get_object(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Get method for GObject properties */
static void
purple_presence_get_property(GObject *obj, guint param_id, GValue *value,
		GParamSpec *pspec)
{
	PurplePresence *presence = PURPLE_PRESENCE(obj);

	switch (param_id) {
		case PRES_PROP_IDLE:
			g_value_set_boolean(value, purple_presence_is_idle(presence));
			break;
		case PRES_PROP_IDLE_TIME:
#if SIZEOF_TIME_T == 4
			g_value_set_int(value, purple_presence_get_idle_time(presence));
#elif SIZEOF_TIME_T == 8
			g_value_set_int64(value, purple_presence_get_idle_time(presence));
#else
#error Unknown size of time_t
#endif
			break;
		case PRES_PROP_LOGIN_TIME:
#if SIZEOF_TIME_T == 4
			g_value_set_int(value, purple_presence_get_login_time(presence));
#elif SIZEOF_TIME_T == 8
			g_value_set_int64(value, purple_presence_get_login_time(presence));
#else
#error Unknown size of time_t
#endif
			break;
		case PRES_PROP_STATUSES:
			g_value_set_pointer(value, purple_presence_get_statuses(presence));
			break;
		case PRES_PROP_ACTIVE_STATUS:
			g_value_set_object(value, purple_presence_get_active_status(presence));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* GObject initialization function */
static void
purple_presence_init(GTypeInstance *instance, gpointer klass)
{
	PURPLE_DBUS_REGISTER_POINTER(PURPLE_PRESENCE(instance), PurplePresence);

	PURPLE_PRESENCE_GET_PRIVATE(instance)->status_table =
				g_hash_table_new_full(g_str_hash, g_str_equal,
				g_free, NULL);
}

/* GObject dispose function */
static void
purple_presence_dispose(GObject *object)
{
	PURPLE_DBUS_UNREGISTER_POINTER(object);

	g_list_foreach(PURPLE_PRESENCE_GET_PRIVATE(object)->statuses,
			(GFunc)g_object_unref, NULL);

	parent_class->dispose(object);
}

/* GObject finalize function */
static void
purple_presence_finalize(GObject *object)
{
	PurplePresencePrivate *priv = PURPLE_PRESENCE_GET_PRIVATE(object);

	g_list_free(priv->statuses);
	g_hash_table_destroy(priv->status_table);

	parent_class->finalize(object);
}

/* Class initializer function */
static void purple_presence_class_init(PurplePresenceClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	obj_class->dispose = purple_presence_dispose;
	obj_class->finalize = purple_presence_finalize;

	/* Setup properties */
	obj_class->get_property = purple_presence_get_property;
	obj_class->set_property = purple_presence_set_property;

	g_object_class_install_property(obj_class, PRES_PROP_IDLE,
			g_param_spec_boolean(PRES_PROP_IDLE_S, _("Idle"),
				_("Whether the presence is in idle state."), FALSE,
				G_PARAM_READWRITE)
			);


	g_object_class_install_property(obj_class, PRES_PROP_IDLE_TIME,
#if SIZEOF_TIME_T == 4
			g_param_spec_int
#elif SIZEOF_TIME_T == 8
			g_param_spec_int64
#else
#error Unknown size of time_t
#endif
				(PRES_PROP_IDLE_TIME_S, _("Idle time"),
				_("The idle time of the presence"),
#if SIZEOF_TIME_T == 4
				G_MININT, G_MAXINT, 0,
#elif SIZEOF_TIME_T == 8
				G_MININT64, G_MAXINT64, 0,
#else
#error Unknown size of time_t
#endif
				G_PARAM_READWRITE)
			);

	g_object_class_install_property(obj_class, PRES_PROP_LOGIN_TIME,
#if SIZEOF_TIME_T == 4
			g_param_spec_int
#elif SIZEOF_TIME_T == 8
			g_param_spec_int64
#else
#error Unknown size of time_t
#endif
				(PRES_PROP_LOGIN_TIME_S, _("Login time"),
				_("The login time of the presence."),
#if SIZEOF_TIME_T == 4
				G_MININT, G_MAXINT, 0,
#elif SIZEOF_TIME_T == 8
				G_MININT64, G_MAXINT64, 0,
#else
#error Unknown size of time_t
#endif
				G_PARAM_READWRITE)
			);

	g_object_class_install_property(obj_class, PRES_PROP_STATUSES,
			g_param_spec_pointer(PRES_PROP_STATUSES_S, _("Statuses"),
				_("The list of statuses in the presence."),
				G_PARAM_READABLE)
			);

	g_object_class_install_property(obj_class, PRES_PROP_ACTIVE_STATUS,
			g_param_spec_object(PRES_PROP_ACTIVE_STATUS_S, _("Active status"),
				_("The active status for the presence."), PURPLE_TYPE_STATUS,
				G_PARAM_READWRITE)
			);

	g_type_class_add_private(klass, sizeof(PurplePresencePrivate));
}

GType
purple_presence_get_type(void)
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(PurplePresenceClass),
			NULL,
			NULL,
			(GClassInitFunc)purple_presence_class_init,
			NULL,
			NULL,
			sizeof(PurplePresence),
			0,
			(GInstanceInitFunc)purple_presence_init,
			NULL,
		};

		type = g_type_register_static(G_TYPE_OBJECT, "PurplePresence",
				&info, G_TYPE_FLAG_ABSTRACT);
	}

	return type;
}

/**************************************************************************
* PurpleAccountPresence API
**************************************************************************/
static void
purple_account_presence_update_idle(PurplePresence *presence, gboolean old_idle)
{
	PurpleAccount *account;
	PurpleConnection *gc = NULL;
	PurplePluginProtocolInfo *prpl_info = NULL;
	gboolean idle = purple_presence_is_idle(presence);
	time_t idle_time = purple_presence_get_idle_time(presence);
	time_t current_time = time(NULL);

	account = purple_account_presence_get_account(PURPLE_ACCOUNT_PRESENCE(presence));

	if (purple_prefs_get_bool("/purple/logging/log_system"))
	{
		PurpleLog *log = purple_account_get_log(account, FALSE);

		if (log != NULL)
		{
			char *msg, *tmp;

			if (idle)
				tmp = g_strdup_printf(_("+++ %s became idle"), purple_account_get_username(account));
			else
				tmp = g_strdup_printf(_("+++ %s became unidle"), purple_account_get_username(account));

			msg = g_markup_escape_text(tmp, -1);
			g_free(tmp);
			purple_log_write(log, PURPLE_MESSAGE_SYSTEM,
			                 purple_account_get_username(account),
			                 (idle ? idle_time : current_time), msg);
			g_free(msg);
		}
	}

	gc = purple_account_get_connection(account);

	if(PURPLE_CONNECTION_IS_CONNECTED(gc))
		prpl_info = purple_connection_get_protocol_info(gc);

	if (prpl_info && prpl_info->set_idle)
		prpl_info->set_idle(gc, (idle ? (current_time - idle_time) : 0));
}

PurpleAccount *
purple_account_presence_get_account(const PurpleAccountPresence *presence)
{
	PurpleAccountPresencePrivate *priv = PURPLE_ACCOUNT_PRESENCE_GET_PRIVATE(presence);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->account;
}

static int
purple_buddy_presence_compute_score(const PurpleBuddyPresence *buddy_presence)
{
	GList *l;
	int score = 0;
	PurplePresence *presence = PURPLE_PRESENCE(buddy_presence);
	PurpleBuddy *b = purple_buddy_presence_get_buddy(buddy_presence);
	int *primitive_scores = _purple_get_primitive_scores();
	int offline_score = purple_prefs_get_int("/purple/status/scores/offline_msg");
	int idle_score = purple_prefs_get_int("/purple/status/scores/idle");

	for (l = purple_presence_get_statuses(presence); l != NULL; l = l->next) {
		PurpleStatus *status = (PurpleStatus *)l->data;
		PurpleStatusType *type = purple_status_get_status_type(status);

		if (purple_status_is_active(status)) {
			score += primitive_scores[purple_status_type_get_primitive(type)];
			if (!purple_status_is_online(status)) {
				if (b && purple_account_supports_offline_message(purple_buddy_get_account(b), b))
					score += offline_score;
			}
		}
	}
	score += purple_account_get_int(purple_buddy_get_account(b), "score", 0);
	if (purple_presence_is_idle(presence))
		score += idle_score;
	return score;
}

gint
purple_buddy_presence_compare(const PurpleBuddyPresence *buddy_presence1,
		const PurpleBuddyPresence *buddy_presence2)
{
	PurplePresence *presence1 = PURPLE_PRESENCE(buddy_presence1);
	PurplePresence *presence2 = PURPLE_PRESENCE(buddy_presence2);
	time_t idle_time_1, idle_time_2;
	int score1 = 0, score2 = 0;
	int idle_time_score = purple_prefs_get_int("/purple/status/scores/idle_time");

	if (presence1 == presence2)
		return 0;
	else if (presence1 == NULL)
		return 1;
	else if (presence2 == NULL)
		return -1;

	if (purple_presence_is_online(presence1) &&
			!purple_presence_is_online(presence2))
		return -1;
	else if (purple_presence_is_online(presence2) &&
			!purple_presence_is_online(presence1))
		return 1;

	/* Compute the score of the first set of statuses. */
	score1 = purple_buddy_presence_compute_score(buddy_presence1);

	/* Compute the score of the second set of statuses. */
	score2 = purple_buddy_presence_compute_score(buddy_presence2);

	idle_time_1 = time(NULL) - purple_presence_get_idle_time(presence1);
	idle_time_2 = time(NULL) - purple_presence_get_idle_time(presence2);

	if (idle_time_1 > idle_time_2)
		score1 += idle_time_score;
	else if (idle_time_1 < idle_time_2)
		score2 += idle_time_score;

	if (score1 < score2)
		return 1;
	else if (score1 > score2)
		return -1;

	return 0;
}

/**************************************************************************
 * GObject code for PurpleAccountPresence
 **************************************************************************/

/* GObject Property names */
#define ACPRES_PROP_ACCOUNT_S  "account"

/* Set method for GObject properties */
static void
purple_account_presence_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *pspec)
{
	PurpleAccountPresence *account_presence = PURPLE_ACCOUNT_PRESENCE(obj);
	PurpleAccountPresencePrivate *priv =
			PURPLE_ACCOUNT_PRESENCE_GET_PRIVATE(account_presence);

	switch (param_id) {
		case ACPRES_PROP_ACCOUNT:
			priv->account = g_value_get_object(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Get method for GObject properties */
static void
purple_account_presence_get_property(GObject *obj, guint param_id, GValue *value,
		GParamSpec *pspec)
{
	PurpleAccountPresence *account_presence = PURPLE_ACCOUNT_PRESENCE(obj);

	switch (param_id) {
		case ACPRES_PROP_ACCOUNT:
			g_value_set_object(value,
					purple_account_presence_get_account(account_presence));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Called when done constructing */
static void
purple_account_presence_constructed(GObject *object)
{
	PurplePresence *presence = PURPLE_PRESENCE(object);
	PurpleAccountPresencePrivate *priv = PURPLE_ACCOUNT_PRESENCE_GET_PRIVATE(presence);

	G_OBJECT_CLASS(presence_class)->constructed(object);

	PURPLE_PRESENCE_GET_PRIVATE(presence)->statuses =
			purple_prpl_get_statuses(priv->account, presence);
}

/* Class initializer function */
static void purple_account_presence_class_init(PurpleAccountPresenceClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	PURPLE_PRESENCE_CLASS(klass)->update_idle = purple_account_presence_update_idle;

	presence_class = g_type_class_peek_parent(klass);

	obj_class->constructed = purple_account_presence_constructed;

	/* Setup properties */
	obj_class->get_property = purple_account_presence_get_property;
	obj_class->set_property = purple_account_presence_set_property;

	g_object_class_install_property(obj_class, ACPRES_PROP_ACCOUNT,
			g_param_spec_object(ACPRES_PROP_ACCOUNT_S, _("Account"),
				_("The account that this presence is of."), PURPLE_TYPE_ACCOUNT,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)
			);

	g_type_class_add_private(klass, sizeof(PurpleAccountPresencePrivate));
}

GType
purple_account_presence_get_type(void)
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(PurpleAccountPresenceClass),
			NULL,
			NULL,
			(GClassInitFunc)purple_account_presence_class_init,
			NULL,
			NULL,
			sizeof(PurpleAccountPresence),
			0,
			NULL,
			NULL,
		};

		type = g_type_register_static(PURPLE_TYPE_PRESENCE,
				"PurpleAccountPresence",
				&info, 0);
	}

	return type;
}

PurpleAccountPresence *
purple_account_presence_new(PurpleAccount *account)
{
	g_return_val_if_fail(account != NULL, NULL);

	return g_object_new(PURPLE_TYPE_ACCOUNT_PRESENCE,
			ACPRES_PROP_ACCOUNT_S, account,
			NULL);
}

/**************************************************************************
* PurpleBuddyPresence API
**************************************************************************/
static void
purple_buddy_presence_update_idle(PurplePresence *presence, gboolean old_idle)
{
	PurpleBuddy *buddy = purple_buddy_presence_get_buddy(PURPLE_BUDDY_PRESENCE(presence));
	time_t current_time = time(NULL);
	PurpleBlistUiOps *ops = purple_blist_get_ui_ops();
	PurpleAccount *account = purple_buddy_get_account(buddy);
	gboolean idle = purple_presence_is_idle(presence);

	if (!old_idle && idle)
	{
		if (purple_prefs_get_bool("/purple/logging/log_system"))
		{
			PurpleLog *log = purple_account_get_log(account, FALSE);

			if (log != NULL)
			{
				char *tmp, *tmp2;
				tmp = g_strdup_printf(_("%s became idle"),
				purple_buddy_get_alias(buddy));
				tmp2 = g_markup_escape_text(tmp, -1);
				g_free(tmp);

				purple_log_write(log, PURPLE_MESSAGE_SYSTEM,
				purple_buddy_get_alias(buddy), current_time, tmp2);
				g_free(tmp2);
			}
		}
	}
	else if (old_idle && !idle)
	{
		if (purple_prefs_get_bool("/purple/logging/log_system"))
		{
			PurpleLog *log = purple_account_get_log(account, FALSE);

			if (log != NULL)
			{
				char *tmp, *tmp2;
				tmp = g_strdup_printf(_("%s became unidle"),
				purple_buddy_get_alias(buddy));
				tmp2 = g_markup_escape_text(tmp, -1);
				g_free(tmp);

				purple_log_write(log, PURPLE_MESSAGE_SYSTEM,
				purple_buddy_get_alias(buddy), current_time, tmp2);
				g_free(tmp2);
			}
		}
	}

	if (old_idle != idle)
		purple_signal_emit(purple_blist_get_handle(), "buddy-idle-changed", buddy,
		                 old_idle, idle);

	purple_contact_invalidate_priority_buddy(purple_buddy_get_contact(buddy));

	/* Should this be done here? It'd perhaps make more sense to
	 * connect to buddy-[un]idle signals and update from there
	 */

	if (ops != NULL && ops->update != NULL)
		ops->update(purple_blist_get_buddy_list(), (PurpleBlistNode *)buddy);
}

PurpleBuddy *
purple_buddy_presence_get_buddy(const PurpleBuddyPresence *presence)
{
	PurpleBuddyPresencePrivate *priv = PURPLE_BUDDY_PRESENCE_GET_PRIVATE(presence);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->buddy;
}

/**************************************************************************
 * GObject code for PurpleBuddyPresence
 **************************************************************************/

/* GObject Property names */
#define BUDPRES_PROP_BUDDY_S  "buddy"

/* Set method for GObject properties */
static void
purple_buddy_presence_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *pspec)
{
	PurpleBuddyPresence *buddy_presence = PURPLE_BUDDY_PRESENCE(obj);
	PurpleBuddyPresencePrivate *priv =
			PURPLE_BUDDY_PRESENCE_GET_PRIVATE(buddy_presence);

	switch (param_id) {
		case BUDPRES_PROP_BUDDY:
			priv->buddy = g_value_get_object(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Get method for GObject properties */
static void
purple_buddy_presence_get_property(GObject *obj, guint param_id, GValue *value,
		GParamSpec *pspec)
{
	PurpleBuddyPresence *buddy_presence = PURPLE_BUDDY_PRESENCE(obj);

	switch (param_id) {
		case BUDPRES_PROP_BUDDY:
			g_value_set_object(value,
					purple_buddy_presence_get_buddy(buddy_presence));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Called when done constructing */
static void
purple_buddy_presence_constructed(GObject *object)
{
	PurplePresence *presence = PURPLE_PRESENCE(object);
	PurpleBuddyPresencePrivate *priv = PURPLE_BUDDY_PRESENCE_GET_PRIVATE(presence);
	PurpleAccount *account;

	G_OBJECT_CLASS(presence_class)->constructed(object);

	account = purple_buddy_get_account(priv->buddy);
	PURPLE_PRESENCE_GET_PRIVATE(presence)->statuses =
			purple_prpl_get_statuses(account, presence);
}

/* Class initializer function */
static void purple_buddy_presence_class_init(PurpleBuddyPresenceClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	PURPLE_PRESENCE_CLASS(klass)->update_idle = purple_buddy_presence_update_idle;

	presence_class = g_type_class_peek_parent(klass);

	obj_class->constructed = purple_buddy_presence_constructed;

	/* Setup properties */
	obj_class->get_property = purple_buddy_presence_get_property;
	obj_class->set_property = purple_buddy_presence_set_property;

	g_object_class_install_property(obj_class, BUDPRES_PROP_BUDDY,
			g_param_spec_object(BUDPRES_PROP_BUDDY_S, _("Buddy"),
				_("The buddy that this presence is of."), PURPLE_TYPE_BUDDY,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)
			);

	g_type_class_add_private(klass, sizeof(PurpleBuddyPresencePrivate));
}

GType
purple_buddy_presence_get_type(void)
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(PurpleBuddyPresenceClass),
			NULL,
			NULL,
			(GClassInitFunc)purple_buddy_presence_class_init,
			NULL,
			NULL,
			sizeof(PurpleBuddyPresence),
			0,
			NULL,
			NULL,
		};

		type = g_type_register_static(PURPLE_TYPE_PRESENCE,
				"PurpleBuddyPresence",
				&info, 0);
	}

	return type;
}

PurpleBuddyPresence *
purple_buddy_presence_new(PurpleBuddy *buddy)
{
	g_return_val_if_fail(buddy != NULL, NULL);

	return g_object_new(PURPLE_TYPE_BUDDY_PRESENCE,
			BUDPRES_PROP_BUDDY_S, buddy,
			NULL);
}
