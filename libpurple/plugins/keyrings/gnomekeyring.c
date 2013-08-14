/**
 * @file gnomekeyring.c Gnome keyring password storage
 * @ingroup plugins
 */

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
 * along with this program ; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

#include "internal.h"
#include "account.h"
#include "debug.h"
#include "keyring.h"
#include "plugin.h"
#include "version.h"

#include <gnome-keyring.h>
#include <gnome-keyring-memory.h>

#define GNOMEKEYRING_NAME        N_("GNOME Keyring")
#define GNOMEKEYRING_DESCRIPTION N_("This plugin will store passwords in " \
	"GNOME Keyring.")
#define GNOMEKEYRING_AUTHOR      "Tomek Wasilczyk (tomkiewicz@cpw.pidgin.im)"
#define GNOMEKEYRING_ID          "keyring-gnomekeyring"

static PurpleKeyring *keyring_handler = NULL;
static GList *request_queue = NULL;
static gpointer current_request = NULL;

typedef struct
{
	enum
	{
		GNOMEKEYRING_REQUEST_READ,
		GNOMEKEYRING_REQUEST_SAVE
	} type;
	PurpleAccount *account;
	gchar *password;
	union
	{
		PurpleKeyringReadCallback read;
		PurpleKeyringSaveCallback save;
	} cb;
	gpointer cb_data;
	gboolean handled;
} gnomekeyring_request;

static void gnomekeyring_cancel_queue(void);
static void gnomekeyring_process_queue(void);

static void gnomekeyring_request_free(gnomekeyring_request *req)
{
	if (req->password != NULL) {
		memset(req->password, 0, strlen(req->password));
		gnome_keyring_memory_free(req->password);
	}
	g_free(req);
}

static void
gnomekeyring_enqueue(gnomekeyring_request *req)
{
	request_queue = g_list_append(request_queue, req);
	gnomekeyring_process_queue();
}

static void
gnomekeyring_read_cb(GnomeKeyringResult result, const char *password,
	gpointer _req)
{
	gnomekeyring_request *req = _req;
	PurpleAccount *account;
	GError *error = NULL;

	g_return_if_fail(req != NULL);

	current_request = NULL;
	account = req->account;

	if (result == GNOME_KEYRING_RESULT_OK) {
		error = NULL;
	} else if (result == GNOME_KEYRING_RESULT_NO_MATCH) {
		error = g_error_new(PURPLE_KEYRING_ERROR,
			PURPLE_KEYRING_ERROR_NOPASSWORD,
			_("No password found for account."));
	} else if (result == GNOME_KEYRING_RESULT_DENIED ||
		result == GNOME_KEYRING_RESULT_CANCELLED) {
		error = g_error_new(PURPLE_KEYRING_ERROR,
			PURPLE_KEYRING_ERROR_ACCESSDENIED,
			_("Access denied."));
		gnomekeyring_cancel_queue();
	} else if (result == GNOME_KEYRING_RESULT_NO_KEYRING_DAEMON ||
		GNOME_KEYRING_RESULT_IO_ERROR) {
		error = g_error_new(PURPLE_KEYRING_ERROR,
			PURPLE_KEYRING_ERROR_BACKENDFAIL,
			_("Communication with GNOME Keyring failed."));
	} else {
		error = g_error_new(PURPLE_KEYRING_ERROR,
			PURPLE_KEYRING_ERROR_BACKENDFAIL,
			_("Unknown error (code: %d)."), result);
	}

	if (error == NULL && password == NULL) {
		error = g_error_new(PURPLE_KEYRING_ERROR,
			PURPLE_KEYRING_ERROR_BACKENDFAIL,
			_("Unknown error (password empty)."));
	}

	if (error == NULL) {
		purple_debug_misc("keyring-gnome",
			"Got password for account %s (%s).\n",
			purple_account_get_username(account),
			purple_account_get_protocol_id(account));
	} else if (result == GNOME_KEYRING_RESULT_NO_MATCH) {
		if (purple_debug_is_verbose()) {
			purple_debug_info("keyring-gnome",
				"Password for account %s (%s) isn't stored.\n",
				purple_account_get_username(account),
				purple_account_get_protocol_id(account));
		}
	} else {
		password = NULL;
		purple_debug_warning("keyring-gnome", "Failed to read "
			"password for account %s (%s), code: %d.\n",
			purple_account_get_username(account),
			purple_account_get_protocol_id(account),
			result);
	}

	if (req->cb.read != NULL)
		req->cb.read(account, password, error, req->cb_data);
	req->handled = TRUE;

	if (error)
		g_error_free(error);

	gnomekeyring_process_queue();
}

static void
gnomekeyring_save_cb(GnomeKeyringResult result, gpointer _req)
{
	gnomekeyring_request *req = _req;
	PurpleAccount *account;
	GError *error = NULL;
	gboolean already_removed = FALSE;

	g_return_if_fail(req != NULL);

	current_request = NULL;
	account = req->account;

	if (result == GNOME_KEYRING_RESULT_OK) {
		error = NULL;
	} else if (result == GNOME_KEYRING_RESULT_NO_MATCH &&
		req->password == NULL) {
		error = NULL;
		already_removed = TRUE;
	} else if (result == GNOME_KEYRING_RESULT_DENIED ||
		result == GNOME_KEYRING_RESULT_CANCELLED) {
		error = g_error_new(PURPLE_KEYRING_ERROR,
			PURPLE_KEYRING_ERROR_ACCESSDENIED,
			_("Access denied."));
		gnomekeyring_cancel_queue();
	} else if (result == GNOME_KEYRING_RESULT_NO_KEYRING_DAEMON ||
		GNOME_KEYRING_RESULT_IO_ERROR) {
		error = g_error_new(PURPLE_KEYRING_ERROR,
			PURPLE_KEYRING_ERROR_BACKENDFAIL,
			_("Communication with GNOME Keyring failed."));
	} else {
		error = g_error_new(PURPLE_KEYRING_ERROR,
			PURPLE_KEYRING_ERROR_BACKENDFAIL,
			_("Unknown error (code: %d)."), result);
	}

	if (already_removed) {
		/* no operation */
	} else if (error == NULL) {
		purple_debug_misc("keyring-gnome",
			"Password %s for account %s (%s).\n",
			req->password ? "saved" : "removed",
			purple_account_get_username(account),
			purple_account_get_protocol_id(account));
	} else {
		purple_debug_warning("keyring-gnome", "Failed updating "
			"password for account %s (%s), code: %d.\n",
			purple_account_get_username(account),
			purple_account_get_protocol_id(account),
			result);
	}

	if (req->cb.save != NULL)
		req->cb.save(account, error, req->cb_data);
	req->handled = TRUE;

	if (error)
		g_error_free(error);

	gnomekeyring_process_queue();
}

static void
gnomekeyring_request_cancel(gpointer _req)
{
	gnomekeyring_request *req = _req;
	PurpleAccount *account;
	GError *error;

	g_return_if_fail(req != NULL);

	if (req->handled) {
		gnomekeyring_request_free(req);
		return;
	}

	purple_debug_warning("keyring-gnome",
		"operation cancelled (%d %s:%s)\n", req->type,
		purple_account_get_protocol_id(req->account),
		purple_account_get_username(req->account));

	account = req->account;
	error = g_error_new(PURPLE_KEYRING_ERROR,
		PURPLE_KEYRING_ERROR_CANCELLED,
		_("Operation cancelled."));
	if (req->type == GNOMEKEYRING_REQUEST_READ && req->cb.read)
		req->cb.read(account, NULL, error, req->cb_data);
	if (req->type == GNOMEKEYRING_REQUEST_SAVE && req->cb.save)
		req->cb.save(account, error, req->cb_data);
	g_error_free(error);

	gnomekeyring_request_free(req);
	gnomekeyring_process_queue();
}

static void
gnomekeyring_cancel_queue(void)
{
	GList *cancel_list = request_queue;

	if (request_queue == NULL)
		return;

	purple_debug_info("gnome-keyring", "cancelling all pending requests\n");
	request_queue = NULL;

	g_list_free_full(cancel_list, gnomekeyring_request_cancel);
}

static void
gnomekeyring_process_queue(void)
{
	gnomekeyring_request *req;
	PurpleAccount *account;
	GList *first;

	if (request_queue == NULL)
		return;

	if (current_request) {
		if (purple_debug_is_verbose())
			purple_debug_misc("keyring-gnome", "busy...\n");
		return;
	}

	first = g_list_first(request_queue);
	req = first->data;
	request_queue = g_list_delete_link(request_queue, first);
	account = req->account;

	if (purple_debug_is_verbose()) {
		purple_debug_misc("keyring-gnome",
			"%s password for account %s (%s)\n",
			req->type == GNOMEKEYRING_REQUEST_READ ? "reading" :
			(req->password == NULL ? "removing" : "updating"),
			purple_account_get_username(account),
			purple_account_get_protocol_id(account));
	}

	if (req->type == GNOMEKEYRING_REQUEST_READ) {
		current_request = gnome_keyring_find_password(
			GNOME_KEYRING_NETWORK_PASSWORD, gnomekeyring_read_cb,
			req, gnomekeyring_request_cancel,
			"user", purple_account_get_username(account),
			"protocol", purple_account_get_protocol_id(account),
			NULL);
	} else if (req->type == GNOMEKEYRING_REQUEST_SAVE &&
		req->password != NULL) {
		gchar *display_name = g_strdup_printf(
			_("Pidgin IM password for account %s"),
			purple_account_get_username(account));
		current_request = gnome_keyring_store_password(
			GNOME_KEYRING_NETWORK_PASSWORD, GNOME_KEYRING_DEFAULT,
			display_name, req->password, gnomekeyring_save_cb, req,
			gnomekeyring_request_cancel,
			"user", purple_account_get_username(account),
			"protocol", purple_account_get_protocol_id(account),
			NULL);
		g_free(display_name);
	} else if (req->type == GNOMEKEYRING_REQUEST_SAVE &&
		req->password == NULL) {
		current_request = gnome_keyring_delete_password(
			GNOME_KEYRING_NETWORK_PASSWORD, gnomekeyring_save_cb,
			req, gnomekeyring_request_cancel,
			"user", purple_account_get_username(account),
			"protocol", purple_account_get_protocol_id(account),
			NULL);
	} else {
		g_return_if_reached();
	}
}

static void
gnomekeyring_read(PurpleAccount *account, PurpleKeyringReadCallback cb,
	gpointer data)
{
	gnomekeyring_request *req;

	g_return_if_fail(account != NULL);

	req = g_new0(gnomekeyring_request, 1);
	req->type = GNOMEKEYRING_REQUEST_READ;
	req->account = account;
	req->cb.read = cb;
	req->cb_data = data;

	gnomekeyring_enqueue(req);
}

static void
gnomekeyring_save(PurpleAccount *account, const gchar *password,
	PurpleKeyringSaveCallback cb, gpointer data)
{
	gnomekeyring_request *req;

	g_return_if_fail(account != NULL);

	req = g_new0(gnomekeyring_request, 1);
	req->type = GNOMEKEYRING_REQUEST_SAVE;
	req->account = account;
	req->password = gnome_keyring_memory_strdup(password);
	req->cb.save = cb;
	req->cb_data = data;

	gnomekeyring_enqueue(req);
}

static void
gnomekeyring_cancel(void)
{
	gnomekeyring_cancel_queue();
	if (current_request) {
		gnome_keyring_cancel_request(current_request);
		while (g_main_iteration(FALSE));
	}
}

static void
gnomekeyring_close(void)
{
	gnomekeyring_cancel();
}

static gboolean
gnomekeyring_load(PurplePlugin *plugin)
{
	GModule *gkr_module;

	/* libgnome-keyring may crash, if was unloaded before glib main loop
	 * termination.
	 */
	gkr_module = g_module_open("libgnome-keyring", 0);
	if (gkr_module == NULL) {
		purple_debug_info("keyring-gnome", "GNOME Keyring module not "
			"found\n");
		return FALSE;
	}
	g_module_make_resident(gkr_module);

	if (!gnome_keyring_is_available()) {
		purple_debug_info("keyring-gnome", "GNOME Keyring service is "
			"disabled\n");
		return FALSE;
	}

	keyring_handler = purple_keyring_new();

	purple_keyring_set_name(keyring_handler, _(GNOMEKEYRING_NAME));
	purple_keyring_set_id(keyring_handler, GNOMEKEYRING_ID);
	purple_keyring_set_read_password(keyring_handler, gnomekeyring_read);
	purple_keyring_set_save_password(keyring_handler, gnomekeyring_save);
	purple_keyring_set_cancel_requests(keyring_handler,
		gnomekeyring_cancel);
	purple_keyring_set_close_keyring(keyring_handler, gnomekeyring_close);

	purple_keyring_register(keyring_handler);

	return TRUE;
}

static gboolean
gnomekeyring_unload(PurplePlugin *plugin)
{
	if (purple_keyring_get_inuse() == keyring_handler) {
		purple_debug_warning("keyring-gnome",
			"keyring in use, cannot unload\n");
		return FALSE;
	}

	gnomekeyring_close();

	purple_keyring_unregister(keyring_handler);
	purple_keyring_free(keyring_handler);
	keyring_handler = NULL;

	return TRUE;
}

PurplePluginInfo plugininfo =
{
	PURPLE_PLUGIN_MAGIC,		/* magic */
	PURPLE_MAJOR_VERSION,		/* major_version */
	PURPLE_MINOR_VERSION,		/* minor_version */
	PURPLE_PLUGIN_STANDARD,		/* type */
	NULL,				/* ui_requirement */
	PURPLE_PLUGIN_FLAG_INVISIBLE,	/* flags */
	NULL,				/* dependencies */
	PURPLE_PRIORITY_DEFAULT,	/* priority */
	GNOMEKEYRING_ID,		/* id */
	GNOMEKEYRING_NAME,		/* name */
	DISPLAY_VERSION,		/* version */
	"GNOME Keyring Plugin",		/* summary */
	GNOMEKEYRING_DESCRIPTION,	/* description */
	GNOMEKEYRING_AUTHOR,		/* author */
	PURPLE_WEBSITE,			/* homepage */
	gnomekeyring_load,		/* load */
	gnomekeyring_unload,		/* unload */
	NULL,				/* destroy */
	NULL,				/* ui_info */
	NULL,				/* extra_info */
	NULL,				/* prefs_info */
	NULL,				/* actions */
	NULL, NULL, NULL, NULL		/* padding */
};

static void
init_plugin(PurplePlugin *plugin)
{
}

PURPLE_INIT_PLUGIN(gnome_keyring, init_plugin, plugininfo)
