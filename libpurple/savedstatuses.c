/**
 * @file savedstatuses.c Saved Status API
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

#include "debug.h"
#include "idle.h"
#include "notify.h"
#include "savedstatuses.h"
#include "dbus-maybe.h"
#include "status.h"
#include "util.h"
#include "xmlnode.h"

/**
 * The maximum number of transient statuses to save.  This
 * is used during the shutdown process to clean out old
 * transient statuses.
 */
#define MAX_TRANSIENTS 5

/**
 * The default message to use when the user becomes auto-away.
 */
#define DEFAULT_AUTOAWAY_MESSAGE _("I'm not here right now")

/**
 * The information stores a snap-shot of the statuses of all
 * your accounts.  Basically these are your saved away messages.
 * There is an overall status and message that applies to
 * all your accounts, and then each individual account can
 * optionally have a different custom status and message.
 *
 * The changes to status.xml caused by the new status API
 * are fully backward compatible.  The new status API just
 * adds the optional sub-statuses to the XML file.
 */
struct _GaimSavedStatus
{
	char *title;
	GaimStatusPrimitive type;
	char *message;

	/** The timestamp when this saved status was created. This must be unique. */
	time_t creation_time;

	time_t lastused;

	unsigned int usage_count;

	GList *substatuses;      /**< A list of GaimSavedStatusSub's. */
};

/*
 * TODO: If a GaimStatusType is deleted, need to also delete any
 *       associated GaimSavedStatusSub's?
 */
struct _GaimSavedStatusSub
{
	GaimAccount *account;
	const GaimStatusType *type;
	char *message;
};

static GList      *saved_statuses = NULL;
static guint       save_timer = 0;
static gboolean    statuses_loaded = FALSE;

/*
 * This hash table keeps track of which timestamps we've
 * used so that we don't have two saved statuses with the
 * same 'creation_time' timestamp.  The 'created' timestamp
 * is used as a unique identifier.
 *
 * So the key in this hash table is the creation_time and
 * the value is a pointer to the GaimSavedStatus.
 */
static GHashTable *creation_times;

static void schedule_save(void);

/*********************************************************************
 * Private utility functions                                         *
 *********************************************************************/

static void
free_saved_status_sub(GaimSavedStatusSub *substatus)
{
	g_return_if_fail(substatus != NULL);

	g_free(substatus->message);
	GAIM_DBUS_UNREGISTER_POINTER(substatus);
	g_free(substatus);
}

static void
free_saved_status(GaimSavedStatus *status)
{
	g_return_if_fail(status != NULL);

	g_free(status->title);
	g_free(status->message);

	while (status->substatuses != NULL)
	{
		GaimSavedStatusSub *substatus = status->substatuses->data;
		status->substatuses = g_list_remove(status->substatuses, substatus);
		free_saved_status_sub(substatus);
	}

	GAIM_DBUS_UNREGISTER_POINTER(status);
	g_free(status);
}

/*
 * Set the timestamp for when this saved status was created, and
 * make sure it is unique.
 */
static void
set_creation_time(GaimSavedStatus *status, time_t creation_time)
{
	g_return_if_fail(status != NULL);

	/* Avoid using 0 because it's an invalid hash key */
	status->creation_time = creation_time != 0 ? creation_time : 1;

	while (g_hash_table_lookup(creation_times, &status->creation_time) != NULL)
		status->creation_time++;

	g_hash_table_insert(creation_times,
						&status->creation_time,
						status);
}

/**
 * A magic number is calcuated for each status, and then the
 * statuses are ordered by the magic number.  The magic number
 * is the date the status was last used offset by one day for
 * each time the status has been used (but only by 10 days at
 * the most).
 *
 * The goal is to have recently used statuses at the top of
 * the list, but to also keep frequently used statuses near
 * the top.
 */
static gint
saved_statuses_sort_func(gconstpointer a, gconstpointer b)
{
	const GaimSavedStatus *saved_status_a = a;
	const GaimSavedStatus *saved_status_b = b;
	time_t time_a = saved_status_a->lastused +
						(MIN(saved_status_a->usage_count, 10) * 86400);
	time_t time_b = saved_status_b->lastused +
						(MIN(saved_status_b->usage_count, 10) * 86400);
	if (time_a > time_b)
		return -1;
	if (time_a < time_b)
		return 1;
	return 0;
}

/**
 * Transient statuses are added and removed automatically by
 * Gaim.  If they're not used for a certain length of time then
 * they'll expire and be automatically removed.  This function
 * does the expiration.
 */
static void
remove_old_transient_statuses()
{
	GList *l, *next;
	GaimSavedStatus *saved_status, *current_status;
	int count;
	time_t creation_time;

	current_status = gaim_savedstatus_get_current();

	/*
	 * Iterate through the list of saved statuses.  Delete all
	 * transient statuses except for the first MAX_TRANSIENTS
	 * (remember, the saved statuses are already sorted by popularity).
	 */
	count = 0;
	for (l = saved_statuses; l != NULL; l = next)
	{
		next = l->next;
		saved_status = l->data;
		if (gaim_savedstatus_is_transient(saved_status))
		{
			if (count == MAX_TRANSIENTS)
			{
				if (saved_status != current_status)
				{
					saved_statuses = g_list_remove(saved_statuses, saved_status);
					creation_time = gaim_savedstatus_get_creation_time(saved_status);
					g_hash_table_remove(creation_times, &creation_time);
					free_saved_status(saved_status);
				}
			}
			else
				count++;
		}
	}

	if (count == MAX_TRANSIENTS)
		schedule_save();
}

/*********************************************************************
 * Writing to disk                                                   *
 *********************************************************************/

static xmlnode *
substatus_to_xmlnode(GaimSavedStatusSub *substatus)
{
	xmlnode *node, *child;

	node = xmlnode_new("substatus");

	child = xmlnode_new_child(node, "account");
	xmlnode_set_attrib(child, "protocol", gaim_account_get_protocol_id(substatus->account));
	xmlnode_insert_data(child, gaim_account_get_username(substatus->account), -1);

	child = xmlnode_new_child(node, "state");
	xmlnode_insert_data(child, gaim_status_type_get_id(substatus->type), -1);

	if (substatus->message != NULL)
	{
		child = xmlnode_new_child(node, "message");
		xmlnode_insert_data(child, substatus->message, -1);
	}

	return node;
}

static xmlnode *
status_to_xmlnode(GaimSavedStatus *status)
{
	xmlnode *node, *child;
	char buf[21];
	GList *cur;

	node = xmlnode_new("status");
	if (status->title != NULL)
	{
		xmlnode_set_attrib(node, "name", status->title);
	}
	else
	{
		/*
		 * Gaim 1.5.0 and earlier require a name to be set, so we
		 * do this little hack to maintain backward compatability
		 * in the status.xml file.  Eventually this should be removed
		 * and we should determine if a status is transient by
		 * whether the "name" attribute is set to something or if
		 * it does not exist at all.
		 */
		xmlnode_set_attrib(node, "name", "Auto-Cached");
		xmlnode_set_attrib(node, "transient", "true");
	}

	snprintf(buf, sizeof(buf), "%lu", status->creation_time);
	xmlnode_set_attrib(node, "created", buf);

	snprintf(buf, sizeof(buf), "%lu", status->lastused);
	xmlnode_set_attrib(node, "lastused", buf);

	snprintf(buf, sizeof(buf), "%u", status->usage_count);
	xmlnode_set_attrib(node, "usage_count", buf);

	child = xmlnode_new_child(node, "state");
	xmlnode_insert_data(child, gaim_primitive_get_id_from_type(status->type), -1);

	if (status->message != NULL)
	{
		child = xmlnode_new_child(node, "message");
		xmlnode_insert_data(child, status->message, -1);
	}

	for (cur = status->substatuses; cur != NULL; cur = cur->next)
	{
		child = substatus_to_xmlnode(cur->data);
		xmlnode_insert_child(node, child);
	}

	return node;
}

static xmlnode *
statuses_to_xmlnode(void)
{
	xmlnode *node, *child;
	GList *cur;

	node = xmlnode_new("statuses");
	xmlnode_set_attrib(node, "version", "1.0");

	for (cur = saved_statuses; cur != NULL; cur = cur->next)
	{
		child = status_to_xmlnode(cur->data);
		xmlnode_insert_child(node, child);
	}

	return node;
}

static void
sync_statuses(void)
{
	xmlnode *node;
	char *data;

	if (!statuses_loaded)
	{
		gaim_debug_error("status", "Attempted to save statuses before they "
						 "were read!\n");
		return;
	}

	node = statuses_to_xmlnode();
	data = xmlnode_to_formatted_str(node, NULL);
	gaim_util_write_data_to_file("status.xml", data, -1);
	g_free(data);
	xmlnode_free(node);
}

static gboolean
save_cb(gpointer data)
{
	sync_statuses();
	save_timer = 0;
	return FALSE;
}

static void
schedule_save(void)
{
	if (save_timer == 0)
		save_timer = gaim_timeout_add(5000, save_cb, NULL);
}


/*********************************************************************
 * Reading from disk                                                 *
 *********************************************************************/

static GaimSavedStatusSub *
parse_substatus(xmlnode *substatus)
{
	GaimSavedStatusSub *ret;
	xmlnode *node;
	char *data;

	ret = g_new0(GaimSavedStatusSub, 1);

	/* Read the account */
	node = xmlnode_get_child(substatus, "account");
	if (node != NULL)
	{
		char *acct_name;
		const char *protocol;
		acct_name = xmlnode_get_data(node);
		protocol = xmlnode_get_attrib(node, "protocol");
		protocol = _gaim_oscar_convert(acct_name, protocol); /* XXX: Remove */
		if ((acct_name != NULL) && (protocol != NULL))
			ret->account = gaim_accounts_find(acct_name, protocol);
		g_free(acct_name);
	}

	if (ret->account == NULL)
	{
		g_free(ret);
		return NULL;
	}

	/* Read the state */
	node = xmlnode_get_child(substatus, "state");
	if ((node != NULL) && ((data = xmlnode_get_data(node)) != NULL))
	{
		ret->type = gaim_status_type_find_with_id(
							ret->account->status_types, data);
		g_free(data);
	}

	/* Read the message */
	node = xmlnode_get_child(substatus, "message");
	if ((node != NULL) && ((data = xmlnode_get_data(node)) != NULL))
	{
		ret->message = data;
	}

	GAIM_DBUS_REGISTER_POINTER(ret, GaimSavedStatusSub);
	return ret;
}

/**
 * Parse a saved status and add it to the saved_statuses linked list.
 *
 * Here's an example of the XML for a saved status:
 *   <status name="Girls">
 *       <state>away</state>
 *       <message>I like the way that they walk
 *   And it's chill to hear them talk
 *   And I can always make them smile
 *   From White Castle to the Nile</message>
 *       <substatus>
 *           <account protocol='prpl-oscar'>markdoliner</account>
 *           <state>available</state>
 *           <message>The ladies man is here to answer your queries.</message>
 *       </substatus>
 *       <substatus>
 *           <account protocol='prpl-oscar'>giantgraypanda</account>
 *           <state>away</state>
 *           <message>A.C. ain't in charge no more.</message>
 *       </substatus>
 *   </status>
 *
 * I know.  Moving, huh?
 */
static GaimSavedStatus *
parse_status(xmlnode *status)
{
	GaimSavedStatus *ret;
	xmlnode *node;
	const char *attrib;
	char *data;
	int i;

	ret = g_new0(GaimSavedStatus, 1);

	attrib = xmlnode_get_attrib(status, "transient");
	if ((attrib == NULL) || (strcmp(attrib, "true")))
	{
		/* Read the title */
		attrib = xmlnode_get_attrib(status, "name");
		ret->title = g_strdup(attrib);
	}

	if (ret->title != NULL)
	{
		/* Ensure the title is unique */
		i = 2;
		while (gaim_savedstatus_find(ret->title) != NULL)
		{
			g_free(ret->title);
			ret->title = g_strdup_printf("%s %d", attrib, i);
			i++;
		}
	}

	/* Read the creation time */
	attrib = xmlnode_get_attrib(status, "created");
	set_creation_time(ret, (attrib != NULL ? atol(attrib) : 0));

	/* Read the last used time */
	attrib = xmlnode_get_attrib(status, "lastused");
	ret->lastused = (attrib != NULL ? atol(attrib) : 0);

	/* Read the usage count */
	attrib = xmlnode_get_attrib(status, "usage_count");
	ret->usage_count = (attrib != NULL ? atol(attrib) : 0);

	/* Read the primitive status type */
	node = xmlnode_get_child(status, "state");
	if ((node != NULL) && ((data = xmlnode_get_data(node)) != NULL))
	{
		ret->type = gaim_primitive_get_type_from_id(data);
		g_free(data);
	}

	/* Read the message */
	node = xmlnode_get_child(status, "message");
	if ((node != NULL) && ((data = xmlnode_get_data(node)) != NULL))
	{
		ret->message = data;
	}

	/* Read substatuses */
	for (node = xmlnode_get_child(status, "substatus"); node != NULL;
			node = xmlnode_get_next_twin(node))
	{
		GaimSavedStatusSub *new;
		new = parse_substatus(node);
		if (new != NULL)
			ret->substatuses = g_list_prepend(ret->substatuses, new);
	}

	GAIM_DBUS_REGISTER_POINTER(ret, GaimSavedStatus);
	return ret;
}

/**
 * Read the saved statuses from a file in the Gaim user dir.
 *
 * @return TRUE on success, FALSE on failure (if the file can not
 *         be opened, or if it contains invalid XML).
 */
static void
load_statuses(void)
{
	xmlnode *statuses, *status;

	statuses_loaded = TRUE;

	statuses = gaim_util_read_xml_from_file("status.xml", _("saved statuses"));

	if (statuses == NULL)
		return;

	for (status = xmlnode_get_child(statuses, "status"); status != NULL;
			status = xmlnode_get_next_twin(status))
	{
		GaimSavedStatus *new;
		new = parse_status(status);
		saved_statuses = g_list_prepend(saved_statuses, new);
	}
	saved_statuses = g_list_sort(saved_statuses, saved_statuses_sort_func);

	xmlnode_free(statuses);
}


/**************************************************************************
* Saved status API
**************************************************************************/
GaimSavedStatus *
gaim_savedstatus_new(const char *title, GaimStatusPrimitive type)
{
	GaimSavedStatus *status;

	/* Make sure we don't already have a saved status with this title. */
	if (title != NULL)
		g_return_val_if_fail(gaim_savedstatus_find(title) == NULL, NULL);

	status = g_new0(GaimSavedStatus, 1);
	GAIM_DBUS_REGISTER_POINTER(status, GaimSavedStatus);
	status->title = g_strdup(title);
	status->type = type;
	set_creation_time(status, time(NULL));

	saved_statuses = g_list_insert_sorted(saved_statuses, status, saved_statuses_sort_func);

	schedule_save();

	return status;
}

void
gaim_savedstatus_set_title(GaimSavedStatus *status, const char *title)
{
	g_return_if_fail(status != NULL);

	/* Make sure we don't already have a saved status with this title. */
	g_return_if_fail(gaim_savedstatus_find(title) == NULL);

	g_free(status->title);
	status->title = g_strdup(title);

	schedule_save();
}

void
gaim_savedstatus_set_type(GaimSavedStatus *status, GaimStatusPrimitive type)
{
	g_return_if_fail(status != NULL);

	status->type = type;

	schedule_save();
}

void
gaim_savedstatus_set_message(GaimSavedStatus *status, const char *message)
{
	g_return_if_fail(status != NULL);

	g_free(status->message);
	if ((message != NULL) && (*message == '\0'))
		status->message = NULL;
	else
		status->message = g_strdup(message);

	schedule_save();
}

void
gaim_savedstatus_set_substatus(GaimSavedStatus *saved_status,
							   const GaimAccount *account,
							   const GaimStatusType *type,
							   const char *message)
{
	GaimSavedStatusSub *substatus;

	g_return_if_fail(saved_status != NULL);
	g_return_if_fail(account      != NULL);
	g_return_if_fail(type         != NULL);

	/* Find an existing substatus or create a new one */
	substatus = gaim_savedstatus_get_substatus(saved_status, account);
	if (substatus == NULL)
	{
		substatus = g_new0(GaimSavedStatusSub, 1);
		GAIM_DBUS_REGISTER_POINTER(substatus, GaimSavedStatusSub);
		substatus->account = (GaimAccount *)account;
		saved_status->substatuses = g_list_prepend(saved_status->substatuses, substatus);
	}

	substatus->type = type;
	g_free(substatus->message);
	substatus->message = g_strdup(message);

	schedule_save();
}

void
gaim_savedstatus_unset_substatus(GaimSavedStatus *saved_status,
								 const GaimAccount *account)
{
	GList *iter;
	GaimSavedStatusSub *substatus;

	g_return_if_fail(saved_status != NULL);
	g_return_if_fail(account      != NULL);

	for (iter = saved_status->substatuses; iter != NULL; iter = iter->next)
	{
		substatus = iter->data;
		if (substatus->account == account)
		{
			saved_status->substatuses = g_list_delete_link(saved_status->substatuses, iter);
			g_free(substatus->message);
			g_free(substatus);
			return;
		}
	}
}

/*
 * This gets called when an account is deleted.  We iterate through
 * all of our saved statuses and delete any substatuses that may
 * exist for this account.
 */
static void
gaim_savedstatus_unset_all_substatuses(const GaimAccount *account,
		gpointer user_data)
{
	GList *iter;
	GaimSavedStatus *status;

	g_return_if_fail(account != NULL);

	for (iter = saved_statuses; iter != NULL; iter = iter->next)
	{
		status = (GaimSavedStatus *)iter->data;
		gaim_savedstatus_unset_substatus(status, account);
	}
}

gboolean
gaim_savedstatus_delete(const char *title)
{
	GaimSavedStatus *status;
	time_t creation_time, current, idleaway;

	status = gaim_savedstatus_find(title);

	if (status == NULL)
		return FALSE;

	saved_statuses = g_list_remove(saved_statuses, status);
	creation_time = gaim_savedstatus_get_creation_time(status);
	g_hash_table_remove(creation_times, &creation_time);
	free_saved_status(status);

	schedule_save();

	/*
	 * If we just deleted our current status or our idleaway status,
	 * then set the appropriate pref back to 0.
	 */
	current = gaim_prefs_get_int("/core/savedstatus/default");
	if (current == creation_time)
		gaim_prefs_set_int("/core/savedstatus/default", 0);

	idleaway = gaim_prefs_get_int("/core/savedstatus/idleaway");
	if (idleaway == creation_time)
		gaim_prefs_set_int("/core/savedstatus/idleaway", 0);

	return TRUE;
}

const GList *
gaim_savedstatuses_get_all(void)
{
	return saved_statuses;
}

GList *
gaim_savedstatuses_get_popular(unsigned int how_many)
{
	GList *popular = NULL;
	GList *cur;
	int i;
	GaimSavedStatus *next;

	/* Copy 'how_many' elements to a new list */
	i = 0;
	cur = saved_statuses;
	while ((i < how_many) && (cur != NULL))
	{
		next = cur->data;
		if ((!gaim_savedstatus_is_transient(next)
			|| gaim_savedstatus_get_message(next) != NULL))
		{
			popular = g_list_prepend(popular, cur->data);
			i++;
		}
		cur = cur->next;
	}

	popular = g_list_reverse(popular);

	return popular;
}

GaimSavedStatus *
gaim_savedstatus_get_current(void)
{
	if (gaim_savedstatus_is_idleaway())
		return gaim_savedstatus_get_idleaway();
	else
		return gaim_savedstatus_get_default();
}

GaimSavedStatus *
gaim_savedstatus_get_default()
{
	int creation_time;
	GaimSavedStatus *saved_status = NULL;

	creation_time = gaim_prefs_get_int("/core/savedstatus/default");

	if (creation_time != 0)
		saved_status = g_hash_table_lookup(creation_times, &creation_time);

	if (saved_status == NULL)
	{
		/*
		 * We don't have a current saved status!  This is either a new
		 * Gaim user or someone upgrading from Gaim 1.5.0 or older, or
		 * possibly someone who deleted the status they were currently
		 * using?  In any case, add a default status.
		 */
		saved_status = gaim_savedstatus_new(NULL, GAIM_STATUS_AVAILABLE);
		gaim_prefs_set_int("/core/savedstatus/default",
						   gaim_savedstatus_get_creation_time(saved_status));
	}

	return saved_status;
}

GaimSavedStatus *
gaim_savedstatus_get_idleaway()
{
	int creation_time;
	GaimSavedStatus *saved_status = NULL;

	creation_time = gaim_prefs_get_int("/core/savedstatus/idleaway");

	if (creation_time != 0)
		saved_status = g_hash_table_lookup(creation_times, &creation_time);

	if (saved_status == NULL)
	{
		/* We don't have a specified "idle" status!  Weird. */
		saved_status = gaim_savedstatus_find_transient_by_type_and_message(
				GAIM_STATUS_AWAY, DEFAULT_AUTOAWAY_MESSAGE);

		if (saved_status == NULL)
		{
			saved_status = gaim_savedstatus_new(NULL, GAIM_STATUS_AWAY);
			gaim_savedstatus_set_message(saved_status, DEFAULT_AUTOAWAY_MESSAGE);
			gaim_prefs_set_int("/core/savedstatus/idleaway",
							   gaim_savedstatus_get_creation_time(saved_status));
		}
	}

	return saved_status;
}

gboolean
gaim_savedstatus_is_idleaway()
{
	return gaim_prefs_get_bool("/core/savedstatus/isidleaway");
}

void
gaim_savedstatus_set_idleaway(gboolean idleaway)
{
	GList *accounts, *node;
	GaimSavedStatus *old, *saved_status;

	if (gaim_savedstatus_is_idleaway() == idleaway)
		/* Don't need to do anything */
		return;

	/* Changing our status makes us un-idle */
	if (!idleaway)
		gaim_idle_touch();

	old = gaim_savedstatus_get_current();
	gaim_prefs_set_bool("/core/savedstatus/isidleaway", idleaway);
	saved_status = idleaway ? gaim_savedstatus_get_idleaway()
			: gaim_savedstatus_get_default();

	if (idleaway && (gaim_savedstatus_get_type(old) != GAIM_STATUS_AVAILABLE))
		/* Our global status is already "away," so don't change anything */
		return;

	accounts = gaim_accounts_get_all_active();
	for (node = accounts; node != NULL; node = node->next)
	{
		GaimAccount *account;
		GaimPresence *presence;
		GaimStatus *status;

		account = node->data;
		presence = gaim_account_get_presence(account);
		status = gaim_presence_get_active_status(presence);

		if (!idleaway || gaim_status_is_available(status))
			gaim_savedstatus_activate_for_account(saved_status, account);
	}

	g_list_free(accounts);

	gaim_signal_emit(gaim_savedstatuses_get_handle(), "savedstatus-changed",
					 saved_status, old);
}

GaimSavedStatus *
gaim_savedstatus_get_startup()
{
	int creation_time;
	GaimSavedStatus *saved_status = NULL;

	creation_time = gaim_prefs_get_int("/core/savedstatus/startup");

	if (creation_time != 0)
		saved_status = g_hash_table_lookup(creation_times, &creation_time);

	if (saved_status == NULL)
	{
		/*
		 * We don't have a status to apply.
		 * This may be the first login, or the user wants to
		 * restore the "current" status.
		 */
		saved_status = gaim_savedstatus_get_current();
	}

	return saved_status;
}


GaimSavedStatus *
gaim_savedstatus_find(const char *title)
{
	GList *iter;
	GaimSavedStatus *status;

	g_return_val_if_fail(title != NULL, NULL);

	for (iter = saved_statuses; iter != NULL; iter = iter->next)
	{
		status = (GaimSavedStatus *)iter->data;
		if ((status->title != NULL) && !strcmp(status->title, title))
			return status;
	}

	return NULL;
}

GaimSavedStatus *
gaim_savedstatus_find_by_creation_time(time_t creation_time)
{
	GList *iter;
	GaimSavedStatus *status;

	for (iter = saved_statuses; iter != NULL; iter = iter->next)
	{
		status = (GaimSavedStatus *)iter->data;
		if (status->creation_time == creation_time)
			return status;
	}

	return NULL;
}

GaimSavedStatus *
gaim_savedstatus_find_transient_by_type_and_message(GaimStatusPrimitive type,
													const char *message)
{
	GList *iter;
	GaimSavedStatus *status;

	for (iter = saved_statuses; iter != NULL; iter = iter->next)
	{
		status = (GaimSavedStatus *)iter->data;
		if ((status->type == type) && gaim_savedstatus_is_transient(status) &&
			!gaim_savedstatus_has_substatuses(status) &&
			(((status->message == NULL) && (message == NULL)) ||
			((status->message != NULL) && (message != NULL) && !strcmp(status->message, message))))
		{
			return status;
		}
	}

	return NULL;
}

gboolean
gaim_savedstatus_is_transient(const GaimSavedStatus *saved_status)
{
	g_return_val_if_fail(saved_status != NULL, TRUE);

	return (saved_status->title == NULL);
}

const char *
gaim_savedstatus_get_title(const GaimSavedStatus *saved_status)
{
	const char *message;

	g_return_val_if_fail(saved_status != NULL, NULL);

	/* If we have a title then return it */
	if (saved_status->title != NULL)
		return saved_status->title;

	/* Otherwise, this is a transient status and we make up a title on the fly */
	message = gaim_savedstatus_get_message(saved_status);

	if ((message == NULL) || (*message == '\0'))
	{
		GaimStatusPrimitive primitive;
		primitive = gaim_savedstatus_get_type(saved_status);
		return gaim_primitive_get_name_from_type(primitive);
	}
	else
	{
		char *stripped;
		static char buf[64];
		stripped = gaim_markup_strip_html(message);
		gaim_util_chrreplace(stripped, '\n', ' ');
		strncpy(buf, stripped, sizeof(buf));
		buf[sizeof(buf) - 1] = '\0';
		if ((strlen(stripped) + 1) > sizeof(buf))
		{
			/* Truncate and ellipsize */
			char *tmp = g_utf8_find_prev_char(buf, &buf[sizeof(buf) - 4]);
			strcpy(tmp, "...");
		}
		g_free(stripped);
		return buf;
	}
}

GaimStatusPrimitive
gaim_savedstatus_get_type(const GaimSavedStatus *saved_status)
{
	g_return_val_if_fail(saved_status != NULL, GAIM_STATUS_OFFLINE);

	return saved_status->type;
}

const char *
gaim_savedstatus_get_message(const GaimSavedStatus *saved_status)
{
	g_return_val_if_fail(saved_status != NULL, NULL);

	return saved_status->message;
}

time_t
gaim_savedstatus_get_creation_time(const GaimSavedStatus *saved_status)
{
	g_return_val_if_fail(saved_status != NULL, 0);

	return saved_status->creation_time;
}

gboolean
gaim_savedstatus_has_substatuses(const GaimSavedStatus *saved_status)
{
	g_return_val_if_fail(saved_status != NULL, FALSE);

	return (saved_status->substatuses != NULL);
}

GaimSavedStatusSub *
gaim_savedstatus_get_substatus(const GaimSavedStatus *saved_status,
							   const GaimAccount *account)
{
	GList *iter;
	GaimSavedStatusSub *substatus;

	g_return_val_if_fail(saved_status != NULL, NULL);
	g_return_val_if_fail(account      != NULL, NULL);

	for (iter = saved_status->substatuses; iter != NULL; iter = iter->next)
	{
		substatus = iter->data;
		if (substatus->account == account)
			return substatus;
	}

	return NULL;
}

const GaimStatusType *
gaim_savedstatus_substatus_get_type(const GaimSavedStatusSub *substatus)
{
	g_return_val_if_fail(substatus != NULL, NULL);

	return substatus->type;
}

const char *
gaim_savedstatus_substatus_get_message(const GaimSavedStatusSub *substatus)
{
	g_return_val_if_fail(substatus != NULL, NULL);

	return substatus->message;
}

void
gaim_savedstatus_activate(GaimSavedStatus *saved_status)
{
	GList *accounts, *node;
	GaimSavedStatus *old = gaim_savedstatus_get_current();

	g_return_if_fail(saved_status != NULL);

	/* Make sure our list of saved statuses remains sorted */
	saved_status->lastused = time(NULL);
	saved_status->usage_count++;
	saved_statuses = g_list_remove(saved_statuses, saved_status);
	saved_statuses = g_list_insert_sorted(saved_statuses, saved_status, saved_statuses_sort_func);

	accounts = gaim_accounts_get_all_active();
	for (node = accounts; node != NULL; node = node->next)
	{
		GaimAccount *account;

		account = node->data;

		gaim_savedstatus_activate_for_account(saved_status, account);
	}

	g_list_free(accounts);

	gaim_prefs_set_int("/core/savedstatus/default",
					   gaim_savedstatus_get_creation_time(saved_status));
	gaim_savedstatus_set_idleaway(FALSE);

	gaim_signal_emit(gaim_savedstatuses_get_handle(), "savedstatus-changed",
					 saved_status, old);
}

void
gaim_savedstatus_activate_for_account(const GaimSavedStatus *saved_status,
									  GaimAccount *account)
{
	const GaimStatusType *status_type;
	const GaimSavedStatusSub *substatus;
	const char *message = NULL;

	g_return_if_fail(saved_status != NULL);
	g_return_if_fail(account != NULL);

	substatus = gaim_savedstatus_get_substatus(saved_status, account);
	if (substatus != NULL)
	{
		status_type = substatus->type;
		message = substatus->message;
	}
	else
	{
		status_type = gaim_account_get_status_type_with_primitive(account, saved_status->type);
		if (status_type == NULL)
			return;
		message = saved_status->message;
	}

	if ((message != NULL) &&
		(gaim_status_type_get_attr(status_type, "message")))
	{
		gaim_account_set_status(account, gaim_status_type_get_id(status_type),
								TRUE, "message", message, NULL);
	}
	else
	{
		gaim_account_set_status(account, gaim_status_type_get_id(status_type),
								TRUE, NULL);
	}
}

void *
gaim_savedstatuses_get_handle(void)
{
	static int handle;

	return &handle;
}

void
gaim_savedstatuses_init(void)
{
	void *handle = gaim_savedstatuses_get_handle();

	creation_times = g_hash_table_new(g_int_hash, g_int_equal);

	/*
	 * Using 0 as the creation_time is a special case.
	 * If someone calls gaim_savedstatus_get_current() or
	 * gaim_savedstatus_get_idleaway() and either of those functions
	 * sees a creation_time of 0, then it will create a default
	 * saved status and return that to the user.
	 */
	gaim_prefs_add_none("/core/savedstatus");
	gaim_prefs_add_int("/core/savedstatus/default", 0);
	gaim_prefs_add_int("/core/savedstatus/startup", 0);
	gaim_prefs_add_bool("/core/savedstatus/startup_current_status", TRUE);
	gaim_prefs_add_int("/core/savedstatus/idleaway", 0);
	gaim_prefs_add_bool("/core/savedstatus/isidleaway", FALSE);

	load_statuses();

	gaim_signal_register(handle, "savedstatus-changed",
					 gaim_marshal_VOID__POINTER_POINTER, NULL, 2,
					 gaim_value_new(GAIM_TYPE_SUBTYPE,
									GAIM_SUBTYPE_SAVEDSTATUS),
					 gaim_value_new(GAIM_TYPE_SUBTYPE,
									GAIM_SUBTYPE_SAVEDSTATUS));

	gaim_signal_connect(gaim_accounts_get_handle(), "account-removed",
			handle,
			GAIM_CALLBACK(gaim_savedstatus_unset_all_substatuses),
			NULL);
}

void
gaim_savedstatuses_uninit(void)
{
	remove_old_transient_statuses();

	if (save_timer != 0)
	{
		gaim_timeout_remove(save_timer);
		save_timer = 0;
		sync_statuses();
	}

	while (saved_statuses != NULL) {
		GaimSavedStatus *saved_status = saved_statuses->data;
		saved_statuses = g_list_remove(saved_statuses, saved_status);
		free_saved_status(saved_status);
	}

	g_hash_table_destroy(creation_times);

	gaim_signals_unregister_by_instance(gaim_savedstatuses_get_handle());
}

