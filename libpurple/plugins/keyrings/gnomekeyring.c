/**
 * @file gnomekeyring.c Gnome keyring password storage
 * @ingroup plugins
 *
 * @todo
 *   cleanup error handling and reporting
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#include "internal.h"
#include "account.h"
#include "debug.h"
#include "keyring.h"
#include "plugin.h"
#include "version.h"

#include <gnome-keyring.h>

#define GNOMEKEYRING_NAME        N_("GNOME-Keyring")
#define GNOMEKEYRING_DESCRIPTION N_("This plugin will store passwords in GNOME Keyring.")
#define GNOMEKEYRING_AUTHOR      "Scrouaf (scrouaf[at]soc.pidgin.im)"
#define GNOMEKEYRING_ID          "keyring-gnomekeyring"

#define ERR_GNOMEKEYRINGPLUGIN 	gkp_error_domain()

static PurpleKeyring *keyring_handler = NULL;

typedef struct _InfoStorage InfoStorage;

struct _InfoStorage
{
	PurpleAccount *account;
	gpointer cb;
	gpointer user_data;
};

static GQuark gkp_error_domain(void)
{
	return g_quark_from_static_string("Gnome-Keyring plugin");
}


/***********************************************/
/*     Keyring interface                       */
/***********************************************/
static void
gkp_read_continue(GnomeKeyringResult result,
                  const char *password,
                  gpointer data)
/* XXX : make sure list is freed on return */
{
	InfoStorage *storage = data;
	PurpleAccount *account = storage->account;
	PurpleKeyringReadCallback cb = storage->cb;
	GError *error;

	if (result != GNOME_KEYRING_RESULT_OK) {
		switch(result) {
			case GNOME_KEYRING_RESULT_NO_MATCH:
				error = g_error_new(ERR_GNOMEKEYRINGPLUGIN,
					PURPLE_KEYRING_ERROR_NOPASSWD,
					"No password found for account : %s",
					purple_account_get_username(account));
				if(cb != NULL)
					cb(account, NULL, error, storage->user_data);
				g_error_free(error);
				return;

			case GNOME_KEYRING_RESULT_NO_KEYRING_DAEMON:
			case GNOME_KEYRING_RESULT_IO_ERROR:
				error = g_error_new(ERR_GNOMEKEYRINGPLUGIN,
					PURPLE_KEYRING_ERROR_NOCHANNEL,
					"Failed to communicate with GNOME Keyring (account : %s).",
					purple_account_get_username(account));
				if(cb != NULL)
					cb(account, NULL, error, storage->user_data);
				g_error_free(error);
				return;

			default:
				error = g_error_new(ERR_GNOMEKEYRINGPLUGIN,
					PURPLE_KEYRING_ERROR_NOCHANNEL,
					"Unknown error (account : %s).",
					purple_account_get_username(account));
				if(cb != NULL)
					cb(account, NULL, error, storage->user_data);
				g_error_free(error);
				return;
		}

	} else {
		if (cb != NULL) {
			cb(account, password, NULL, storage->user_data);
		}
	}
}

static void
gkp_read(PurpleAccount *account, PurpleKeyringReadCallback cb, gpointer data)
{
	InfoStorage *storage = g_new0(InfoStorage, 1);

	storage->account = account;
	storage->cb = cb;
	storage->user_data = data;

	gnome_keyring_find_password(GNOME_KEYRING_NETWORK_PASSWORD,
	                            gkp_read_continue,
	                            storage,
	                            g_free,
	                            "user", purple_account_get_username(account),
	                            "protocol", purple_account_get_protocol_id(account),
	                            NULL);
}

static void
gkp_save_continue(GnomeKeyringResult result, gpointer data)
{
	InfoStorage *storage;
	PurpleKeyringSaveCallback cb;
	GError *error;
	PurpleAccount *account;

	storage = data;
	g_return_if_fail(storage != NULL);

	account = storage->account;
	cb = storage->cb;

	if (result != GNOME_KEYRING_RESULT_OK) {
		switch(result) {
			case GNOME_KEYRING_RESULT_NO_MATCH:
				purple_debug_info("keyring-gnome",
					"Could not update password for %s (%s) : not found.\n",
					purple_account_get_username(account),
					purple_account_get_protocol_id(account));
				error = g_error_new(ERR_GNOMEKEYRINGPLUGIN,
					PURPLE_KEYRING_ERROR_NOPASSWD,
					"Could not update password for %s : not found",
					purple_account_get_username(account));
				if(cb != NULL)
					cb(account, error, storage->user_data);
				g_error_free(error);
				return;

			case GNOME_KEYRING_RESULT_NO_KEYRING_DAEMON:
			case GNOME_KEYRING_RESULT_IO_ERROR:
				purple_debug_info("keyring-gnome",
					"Failed to communicate with GNOME Keyring (account : %s (%s)).\n",
					purple_account_get_username(account),
					purple_account_get_protocol_id(account));
				error = g_error_new(ERR_GNOMEKEYRINGPLUGIN,
					PURPLE_KEYRING_ERROR_NOCHANNEL,
					"Failed to communicate with GNOME Keyring (account : %s).",
					purple_account_get_username(account));
				if(cb != NULL)
					cb(account, error, storage->user_data);
				g_error_free(error);
				return;

			default:
				purple_debug_info("keyring-gnome",
					"Unknown error (account : %s (%s)).\n",
					purple_account_get_username(account),
					purple_account_get_protocol_id(account));
				error = g_error_new(ERR_GNOMEKEYRINGPLUGIN,
					PURPLE_KEYRING_ERROR_NOCHANNEL,
					"Unknown error (account : %s).",
					purple_account_get_username(account));
				if(cb != NULL)
					cb(account, error, storage->user_data);
				g_error_free(error);
				return;
		}

	} else {
		purple_debug_info("keyring-gnome", "Password for %s updated.\n",
			purple_account_get_username(account));

		if (cb != NULL)
			cb(account, NULL, storage->user_data);
	}
}

static void
gkp_save(PurpleAccount *account,
         const gchar *password,
         PurpleKeyringSaveCallback cb,
         gpointer data)
{
	InfoStorage *storage = g_new0(InfoStorage, 1);

	storage->account = account;
	storage->cb = cb;
	storage->user_data = data;

	if (password != NULL && *password != '\0') {
		char *name;

		purple_debug_info("keyring-gnome",
			"Updating password for account %s (%s).\n",
			purple_account_get_username(account),
			purple_account_get_protocol_id(account));

		name = g_strdup_printf("purple-%s",
		                       purple_account_get_username(account));
		gnome_keyring_store_password(GNOME_KEYRING_NETWORK_PASSWORD,
		                             NULL,  /*default keyring */
		                             name,
		                             password,
		                             gkp_save_continue,
		                             storage,
		                             g_free,    /* function to free storage */
		                             "user", purple_account_get_username(account),
		                             "protocol", purple_account_get_protocol_id(account),
		                             NULL);
		g_free(name);

	} else {	/* password == NULL, delete password. */
		purple_debug_info("keyring-gnome",
			"Forgetting password for account %s (%s).\n",
			purple_account_get_username(account),
			purple_account_get_protocol_id(account));

		gnome_keyring_delete_password(GNOME_KEYRING_NETWORK_PASSWORD,
		                              gkp_save_continue,
		                              storage, g_free,
		                              "user", purple_account_get_username(account),
		                              "protocol", purple_account_get_protocol_id(account),
		                              NULL);
	}
}

static void
gkp_close(GError **error)
{
}

static gboolean
gkp_import_password(PurpleAccount *account,
                    const char *mode,
                    const char *data,
                    GError **error)
{
	purple_debug_info("keyring-gnome", "Importing password.\n");
	return TRUE;
}

static gboolean
gkp_export_password(PurpleAccount *account,
                    const char **mode,
                    char **data,
                    GError **error,
                    GDestroyNotify *destroy)
{
	purple_debug_info("keyring-gnome", "Exporting password.\n");
	*data = NULL;
	*mode = NULL;
	*destroy = NULL;

	return TRUE;
}

/* this was written just to test the pref change */
static void
gkp_change_master(PurpleKeyringChangeMasterCallback cb, gpointer data)
{
	purple_debug_info("keyring-gnome",
		"This keyring does not support master passwords.\n");

	purple_notify_info(NULL, _("GNOME Keyring plugin"),
	                   _("Failed to change master password."),
	                   _("This plugin does not really support master passwords, it just pretends to."));
	if (cb)
		cb(FALSE, NULL, data);
}

static gboolean
gkp_init(void)
{
	purple_debug_info("keyring-gnome", "Init.\n");

	if (gnome_keyring_is_available()) {
		keyring_handler = purple_keyring_new();

		purple_keyring_set_name(keyring_handler, GNOMEKEYRING_NAME);
		purple_keyring_set_id(keyring_handler, GNOMEKEYRING_ID);
		purple_keyring_set_read_password(keyring_handler, gkp_read);
		purple_keyring_set_save_password(keyring_handler, gkp_save);
		purple_keyring_set_close_keyring(keyring_handler, gkp_close);
		purple_keyring_set_change_master(keyring_handler, gkp_change_master);
		purple_keyring_set_import_password(keyring_handler, gkp_import_password);
		purple_keyring_set_export_password(keyring_handler, gkp_export_password);

		purple_keyring_register(keyring_handler);

		return TRUE;

	} else {
		purple_debug_info("keyring-gnome",
			"Failed to communicate with daemon, not loading.\n");
		return FALSE;
	}
}

static void
gkp_uninit(void)
{
	purple_debug_info("keyring-gnome", "Uninit.\n");
	gkp_close(NULL);
	purple_keyring_unregister(keyring_handler);
	purple_keyring_free(keyring_handler);
	keyring_handler = NULL;
}

/***********************************************/
/*     Plugin interface                        */
/***********************************************/

static gboolean
gkp_load(PurplePlugin *plugin)
{
	return gkp_init();
}

static gboolean
gkp_unload(PurplePlugin *plugin)
{
	if (purple_keyring_get_inuse() == keyring_handler)
		return FALSE;

	gkp_uninit();

	return TRUE;
}

PurplePluginInfo plugininfo =
{
	PURPLE_PLUGIN_MAGIC,		/* magic */
	PURPLE_MAJOR_VERSION,		/* major_version */
	PURPLE_MINOR_VERSION,		/* minor_version */
	PURPLE_PLUGIN_STANDARD,		/* type */
	NULL,						/* ui_requirement */
	PURPLE_PLUGIN_FLAG_INVISIBLE|PURPLE_PLUGIN_FLAG_AUTOLOAD,	/* flags */
	NULL,						/* dependencies */
	PURPLE_PRIORITY_DEFAULT,	/* priority */
	GNOMEKEYRING_ID,			/* id */
	GNOMEKEYRING_NAME,			/* name */
	DISPLAY_VERSION,			/* version */
	"GNOME Keyring Plugin",		/* summary */
	GNOMEKEYRING_DESCRIPTION,	/* description */
	GNOMEKEYRING_AUTHOR,		/* author */
	"N/A",						/* homepage */
	gkp_load,					/* load */
	gkp_unload,					/* unload */
	NULL,						/* destroy */
	NULL,						/* ui_info */
	NULL,						/* extra_info */
	NULL,						/* prefs_info */
	NULL,						/* actions */
	NULL,						/* padding... */
	NULL,
	NULL,
	NULL,
};

static void
init_plugin(PurplePlugin *plugin)
{
	purple_debug_info("keyring-gnome", "Init plugin called.\n");
}

PURPLE_INIT_PLUGIN(gnome_keyring, init_plugin, plugininfo)

