/**
 * @file savedstatus.c Saved Status API
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
#include "notify.h"
#include "savedstatuses.h"
#include "status.h"
#include "util.h"
#include "xmlnode.h"

/**
 * The information of a snap-shot of the statuses of all
 * your accounts.  Basically these are your saved away messages.
 * There is an overall status and message that applies to
 * all your accounts, and then each individual account can
 * optionally have a different custom status and message.
 *
 * The changes to status.xml caused by the new status API
 * are fully backward compatible.  The new status API just
 * adds the optional sub-statuses to the XML file.
 */
struct _GaimStatusSaved
{
	char *title;
	GaimStatusPrimitive type;
	char *message;

	GList *substatuses;      /**< A list of GaimStatusSavedSub's. */
};

/*
 * TODO: If an account is deleted, need to also delete any associated
 *       GaimStatusSavedSub's.
 * TODO: If a GaimStatusType is deleted, need to also delete any
 *       associated GaimStatusSavedSub's?
 */
struct _GaimStatusSavedSub
{
	GaimAccount *account;
	const GaimStatusType *type;
	char *message;
};

static GList *saved_statuses = NULL;
gboolean have_read_saved_statuses = FALSE;
static guint statuses_save_timer = 0;

/**************************************************************************
* Helper functions
**************************************************************************/

/**
 * Elements of this array correspond to the GaimStatusPrimitive
 * enumeration.
 */
static const char *primitive_names[] =
{
	"unset",
	"offline",
	"available",
	"unavailable",
	"hidden",
	"away",
	"extended_away"
};

static GaimStatusPrimitive
gaim_primitive_get_type(const char *name)
{
	int i;

	g_return_val_if_fail(name != NULL, GAIM_STATUS_UNSET);

	for (i = 0; i < GAIM_STATUS_NUM_PRIMITIVES; i++)
	{
		if (!strcmp(name, primitive_names[i]))
			return i;
	}

	return GAIM_STATUS_UNSET;
}

static void
free_statussavedsub(GaimStatusSavedSub *substatus)
{
	g_return_if_fail(substatus != NULL);

	g_free(substatus->message);
	g_free(substatus);
}

static void
free_statussaved(GaimStatusSaved *status)
{
	g_return_if_fail(status != NULL);

	g_free(status->title);
	g_free(status->message);

	while (status->substatuses != NULL)
	{
		GaimStatusSavedSub *substatus = status->substatuses->data;
		status->substatuses = g_list_remove(status->substatuses, substatus);
		free_statussavedsub(substatus);
	}

	g_free(status);
}


/**************************************************************************
* Saved status writting to disk
**************************************************************************/

static xmlnode *
substatus_to_xmlnode(GaimStatusSavedSub *substatus)
{
	xmlnode *node, *child;

	node = xmlnode_new("substatus");

	child = xmlnode_new("account");
	xmlnode_set_attrib(node, "protocol",
					   gaim_account_get_protocol_id(substatus->account));
	xmlnode_insert_data(child,
						gaim_account_get_username(substatus->account), -1);
	xmlnode_insert_child(node, child);

	child = xmlnode_new("state");
	xmlnode_insert_data(child, gaim_status_type_get_id(substatus->type), -1);
	xmlnode_insert_child(node, child);

	if (substatus->message != NULL)
	{
		child = xmlnode_new("message");
		xmlnode_insert_data(child, substatus->message, -1);
		xmlnode_insert_child(node, child);
	}

	return node;
}

static xmlnode *
status_to_xmlnode(GaimStatusSaved *status)
{
	xmlnode *node, *child;
	GList *cur;

	node = xmlnode_new("status");
	xmlnode_set_attrib(node, "name", status->title);

	child = xmlnode_new("state");
	xmlnode_insert_data(child, primitive_names[status->type], -1);
	xmlnode_insert_child(node, child);

	child = xmlnode_new("message");
	xmlnode_insert_data(child, status->message, -1);
	xmlnode_insert_child(node, child);

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
	xmlnode_set_attrib(node, "version", "1");

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
	xmlnode *statuses;
	char *data;

	if (!have_read_saved_statuses) {
		gaim_debug_error("status", "Attempted to save statuses before they "
						 "were read!\n");
		return;
	}

	statuses = statuses_to_xmlnode();
	data = xmlnode_to_formatted_str(statuses, NULL);
	gaim_util_write_data_to_file("status.xml", data, -1);
	g_free(data);
	xmlnode_free(statuses);
}

static gboolean
save_callback(gpointer data)
{
	sync_statuses();
	statuses_save_timer = 0;
	return FALSE;
}

static void
schedule_save(void)
{
	if (statuses_save_timer != 0)
		gaim_timeout_remove(statuses_save_timer);
	statuses_save_timer = gaim_timeout_add(1000, save_callback, NULL);
}


/**************************************************************************
* Saved status reading from disk
**************************************************************************/
static GaimStatusSavedSub *
parse_substatus(xmlnode *substatus)
{
	GaimStatusSavedSub *ret;
	xmlnode *node;
	char *data = NULL;

	ret = g_new0(GaimStatusSavedSub, 1);

	/* Read the account */
	node = xmlnode_get_child(substatus, "account");
	if (node != NULL)
	{
		char *acct_name;
		const char *protocol;
		acct_name = xmlnode_get_data(node);
		protocol = xmlnode_get_attrib(node, "protocol");
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
	if (node != NULL)
		data = xmlnode_get_data(node);
	if (data != NULL) {
		ret->type = gaim_status_type_find_with_id(
									ret->account->status_types, data);
		g_free(data);
		data = NULL;
	}

	/* Read the message */
	node = xmlnode_get_child(substatus, "message");
	if (node != NULL)
		data = xmlnode_get_data(node);
	if (data != NULL)
		ret->message = data;

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
static GaimStatusSaved *
parse_status(xmlnode *status)
{
	GaimStatusSaved *ret;
	xmlnode *node;
	const char *attrib;
	char *data = NULL;
	int i;

	ret = g_new0(GaimStatusSaved, 1);

	/* Read the title */
	attrib = xmlnode_get_attrib(status, "name");
	if (attrib == NULL)
		attrib = "No Title";
	/* Ensure the title is unique */
	ret->title = g_strdup(attrib);
	i = 2;
	while (gaim_savedstatuses_find(ret->title) != NULL)
	{
		g_free(ret->title);
		ret->title = g_strdup_printf("%s %d", attrib, i);
		i++;
	}

	/* Read the primitive status type */
	node = xmlnode_get_child(status, "state");
	if (node != NULL)
		data = xmlnode_get_data(node);
	if (data != NULL) {
		ret->type = gaim_primitive_get_type(data);
		g_free(data);
		data = NULL;
	}

	/* Read the message */
	node = xmlnode_get_child(status, "message");
	if (node != NULL)
		data = xmlnode_get_data(node);
	if (data != NULL)
		ret->message = data;

	/* Read substatuses */
	for (node = xmlnode_get_child(status, "status"); node != NULL;
			node = xmlnode_get_next_twin(node))
	{
		GaimStatusSavedSub *new;
		new = parse_substatus(node);
		if (new != NULL)
			ret->substatuses = g_list_append(ret->substatuses, new);
	}

	return ret;
}

/**
 * Read the saved statuses from a file in the Gaim user dir.
 *
 * @return TRUE on success, FALSE on failure (if the file can not
 *         be opened, or if it contains invalid XML).
 */
static gboolean
read_statuses(const char *filename)
{
	GError *error;
	gchar *contents = NULL;
	gsize length;
	xmlnode *statuses, *status;

	gaim_debug_info("status", "Reading %s\n", filename);

	if (!g_file_get_contents(filename, &contents, &length, &error))
	{
		gaim_debug_error("status", "Error reading statuses: %s\n",
						 error->message);
		g_error_free(error);
		return FALSE;
	}

	statuses = xmlnode_from_str(contents, length);

	if (statuses == NULL)
	{
		FILE *backup;
		gchar *name;
		gaim_debug_error("status", "Error parsing statuses\n");
		name = g_strdup_printf("%s~", filename);
		if ((backup = fopen(name, "w")))
		{
			fwrite(contents, length, 1, backup);
			fclose(backup);
			chmod(name, S_IRUSR | S_IWUSR);
		}
		else
		{
			gaim_debug_error("status", "Unable to write backup %s\n", name);
		}
		g_free(name);
		g_free(contents);
		return FALSE;
	}

	g_free(contents);

	for (status = xmlnode_get_child(statuses, "status"); status != NULL;
			status = xmlnode_get_next_twin(status))
	{
		GaimStatusSaved *new;
		new = parse_status(status);
		saved_statuses = g_list_append(saved_statuses, new);
	}

	gaim_debug_info("status", "Finished reading statuses\n");

	xmlnode_free(statuses);

	return TRUE;
}

static void
load_statuses(void)
{
	const char *user_dir = gaim_user_dir();
	gchar *filename;
	gchar *msg;

	g_return_if_fail(user_dir != NULL);

	have_read_saved_statuses = TRUE;

	filename = g_build_filename(user_dir, "status.xml", NULL);

	if (g_file_test(filename, G_FILE_TEST_EXISTS))
	{
		if (!read_statuses(filename))
		{
			msg = g_strdup_printf(_("An error was encountered parsing the "
						"file containing your saved statuses (%s).  They "
						"have not been loaded, and the old file has been "
						"renamed to status.xml~."), filename);
			gaim_notify_error(NULL, NULL, _("Saved Statuses Error"), msg);
			g_free(msg);
		}
	}

	g_free(filename);
}


/**************************************************************************
* Saved status API
**************************************************************************/
GaimStatusSaved *
gaim_savedstatuses_new(const char *title, GaimStatusPrimitive type)
{
	GaimStatusSaved *status;

	status = g_new0(GaimStatusSaved, 1);
	status->title = g_strdup(title);
	status->type = type;

	saved_statuses = g_list_append(saved_statuses, status);

	schedule_save();

	return status;
}

gboolean
gaim_savedstatuses_delete(const char *title)
{
	GaimStatusSaved *status;

	status = gaim_savedstatuses_find(title);

	if (status == NULL)
		return FALSE;

	saved_statuses = g_list_remove(saved_statuses, status);
	free_statussaved(status);

	schedule_save();

	return TRUE;
}

const GList *
gaim_savedstatuses_get_all(void)
{
	return saved_statuses;
}

GaimStatusSaved *
gaim_savedstatuses_find(const char *title)
{
	GList *l;
	GaimStatusSaved *status;

	for (l = saved_statuses; l != NULL; l = g_list_next(l))
	{
		status = (GaimStatusSaved *)l->data;
		if (!strcmp(status->title, title))
			return status;
	}

	return NULL;
}

const char *
gaim_savedstatuses_get_title(const GaimStatusSaved *saved_status)
{
	return saved_status->title;
}

GaimStatusPrimitive
gaim_savedstatuses_get_type(const GaimStatusSaved *saved_status)
{
	return saved_status->type;
}

const char *
gaim_savedstatuses_get_message(const GaimStatusSaved *saved_status)
{
	return saved_status->message;
}

void
gaim_savedstatuses_init(void)
{
	load_statuses();
}

void
gaim_savedstatuses_uninit(void)
{
	if (statuses_save_timer != 0)
	{
		gaim_timeout_remove(statuses_save_timer);
		statuses_save_timer = 0;
		sync_statuses();
	}

	while (saved_statuses != NULL) {
		GaimStatusSaved *status = saved_statuses->data;
		saved_statuses = g_list_remove(saved_statuses, status);
		free_statussaved(status);
	}
}
