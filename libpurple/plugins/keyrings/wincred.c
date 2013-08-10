/**
 * @file wincred.c Passwords storage using Windows credentials
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

#include "debug.h"
#include "internal.h"
#include "keyring.h"
#include "plugins.h"
#include "version.h"

#include <wincred.h>

#define WINCRED_NAME        N_("Windows credentials")
#define WINCRED_SUMMARY     N_("Store passwords using Windows credentials")
#define WINCRED_DESCRIPTION N_("This plugin stores passwords using Windows " \
	"credentials.")
#define WINCRED_AUTHOR      "Tomek Wasilczyk (tomkiewicz@cpw.pidgin.im)"
#define WINCRED_ID          "keyring-wincred"
#define WINCRED_DOMAIN      (g_quark_from_static_string(WINCRED_ID))

#define WINCRED_MAX_TARGET_NAME 256

static PurpleKeyring *keyring_handler = NULL;

static gunichar2 *
wincred_get_target_name(PurpleAccount *account)
{
	gchar target_name_utf8[WINCRED_MAX_TARGET_NAME];
	gunichar2 *target_name_utf16;

	g_return_val_if_fail(account != NULL, NULL);

	g_snprintf(target_name_utf8, WINCRED_MAX_TARGET_NAME, "libpurple_%s_%s",
		purple_account_get_protocol_id(account),
		purple_account_get_username(account));

	target_name_utf16 = g_utf8_to_utf16(target_name_utf8, -1,
		NULL, NULL, NULL);

	if (target_name_utf16 == NULL) {
		purple_debug_fatal("keyring-wincred", "Couldn't convert target "
			"name\n");
	}

	return target_name_utf16;
}

static void
wincred_read(PurpleAccount *account, PurpleKeyringReadCallback cb,
	gpointer data)
{
	GError *error = NULL;
	gunichar2 *target_name = NULL;
	gchar *password;
	PCREDENTIALW credential;

	g_return_if_fail(account != NULL);

	target_name = wincred_get_target_name(account);
	g_return_if_fail(target_name != NULL);

	if (!CredReadW(target_name, CRED_TYPE_GENERIC, 0, &credential)) {
		DWORD error_code = GetLastError();

		if (error_code == ERROR_NOT_FOUND) {
			if (purple_debug_is_verbose()) {
				purple_debug_misc("keyring-wincred",
					"No password found for account %s\n",
					purple_account_get_username(account));
			}
			error = g_error_new(PURPLE_KEYRING_ERROR,
				PURPLE_KEYRING_ERROR_NOPASSWORD,
				_("Password not found."));
		} else if (error_code == ERROR_NO_SUCH_LOGON_SESSION) {
			purple_debug_error("keyring-wincred",
				"Cannot read password, no valid logon "
				"session\n");
			error = g_error_new(PURPLE_KEYRING_ERROR,
				PURPLE_KEYRING_ERROR_ACCESSDENIED,
				_("Cannot read password, no valid logon "
				"session."));
		} else {
			purple_debug_error("keyring-wincred",
				"Cannot read password, error %lx\n",
				error_code);
			error = g_error_new(PURPLE_KEYRING_ERROR,
				PURPLE_KEYRING_ERROR_BACKENDFAIL,
				_("Cannot read password (error %lx)."), error_code);
		}

		if (cb != NULL)
			cb(account, NULL, error, data);
		g_error_free(error);
		return;
	}

	password = g_utf16_to_utf8((gunichar2*)credential->CredentialBlob,
		credential->CredentialBlobSize / sizeof(gunichar2),
		NULL, NULL, NULL);

	memset(credential->CredentialBlob, 0, credential->CredentialBlobSize);
	CredFree(credential);

	if (password == NULL) {
		purple_debug_error("keyring-wincred",
			"Cannot convert password\n");
		error = g_error_new(PURPLE_KEYRING_ERROR,
			PURPLE_KEYRING_ERROR_BACKENDFAIL,
			_("Cannot read password (unicode error)."));
	} else {
		purple_debug_misc("keyring-wincred",
			_("Got password for account %s.\n"),
			purple_account_get_username(account));
	}

	if (cb != NULL)
		cb(account, password, error, data);
	if (error != NULL)
		g_error_free(error);

	purple_str_wipe(password);
}

static void
wincred_save(PurpleAccount *account, const gchar *password,
	PurpleKeyringSaveCallback cb, gpointer data)
{
	GError *error = NULL;
	gunichar2 *target_name = NULL;
	gunichar2 *username_utf16 = NULL;
	gunichar2 *password_utf16 = NULL;
	CREDENTIALW credential;

	g_return_if_fail(account != NULL);

	target_name = wincred_get_target_name(account);
	g_return_if_fail(target_name != NULL);

	if (password == NULL)
	{
		if (CredDeleteW(target_name, CRED_TYPE_GENERIC, 0)) {
			purple_debug_misc("keyring-wincred", "Password for "
				"account %s removed\n",
				purple_account_get_username(account));
		} else {
			DWORD error_code = GetLastError();

			if (error_code == ERROR_NOT_FOUND) {
				if (purple_debug_is_verbose()) {
					purple_debug_misc("keyring-wincred",
					"Password for account %s was already "
					"removed.\n",
					purple_account_get_username(account));
				}
			} else if (error_code == ERROR_NO_SUCH_LOGON_SESSION) {
				purple_debug_error("keyring-wincred",
					"Cannot remove password, no valid "
					"logon session\n");
				error = g_error_new(PURPLE_KEYRING_ERROR,
					PURPLE_KEYRING_ERROR_ACCESSDENIED,
					_("Cannot remove password, no valid "
					"logon session."));
			} else {
				purple_debug_error("keyring-wincred",
					"Cannot remove password, error %lx\n",
					error_code);
				error = g_error_new(PURPLE_KEYRING_ERROR,
					PURPLE_KEYRING_ERROR_BACKENDFAIL,
					_("Cannot remove password (error %lx)."),
					error_code);
			}
		}

		if (cb != NULL)
			cb(account, error, data);
		if (error != NULL)
			g_error_free(error);
		return;
	}

	username_utf16 = g_utf8_to_utf16(purple_account_get_username(account),
		-1, NULL, NULL, NULL);
	password_utf16 = g_utf8_to_utf16(password, -1, NULL, NULL, NULL);

	if (username_utf16 == NULL || password_utf16 == NULL) {
		g_free(username_utf16);
		purple_utf16_wipe(password_utf16);

		purple_debug_fatal("keyring-wincred", "Couldn't convert "
			"username or password\n");
		g_return_if_reached();
	}

	memset(&credential, 0, sizeof(CREDENTIALW));
	credential.Type = CRED_TYPE_GENERIC;
	credential.TargetName = target_name;
	credential.CredentialBlobSize = purple_utf16_size(password_utf16) - 2;
	credential.CredentialBlob = (LPBYTE)password_utf16;
	credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
	credential.UserName = username_utf16;

	if (!CredWriteW(&credential, 0)) {
		DWORD error_code = GetLastError();

		if (error_code == ERROR_NO_SUCH_LOGON_SESSION) {
			purple_debug_error("keyring-wincred",
				"Cannot store password, no valid logon "
				"session\n");
			error = g_error_new(PURPLE_KEYRING_ERROR,
				PURPLE_KEYRING_ERROR_ACCESSDENIED,
				_("Cannot remove password, no valid logon "
				"session."));
		} else {
			purple_debug_error("keyring-wincred",
				"Cannot store password, error %lx\n",
				error_code);
			error = g_error_new(PURPLE_KEYRING_ERROR,
				PURPLE_KEYRING_ERROR_BACKENDFAIL,
				_("Cannot store password (error %lx)."), error_code);
		}
	} else {
		purple_debug_misc("keyring-wincred",
			"Password updated for account %s.\n",
			purple_account_get_username(account));
	}

	g_free(target_name);
	g_free(username_utf16);
	purple_utf16_wipe(password_utf16);

	if (cb != NULL)
		cb(account, error, data);
	if (error != NULL)
		g_error_free(error);
}

static PurplePluginInfo *
plugin_query(GError **error)
{
	return purple_plugin_info_new(
		"id",           WINCRED_ID,
		"name",         WINCRED_NAME,
		"version",      DISPLAY_VERSION,
		"category",     N_("Keyring"),
		"summary",      WINCRED_SUMMARY,
		"description",  WINCRED_DESCRIPTION,
		"author",       WINCRED_AUTHOR,
		"website",      PURPLE_WEBSITE,
		"purple-abi",   PURPLE_ABI_VERSION,
		"flags",        GPLUGIN_PLUGIN_INFO_FLAGS_INTERNAL,
		NULL
	);
}

static gboolean
plugin_load(PurplePlugin *plugin, GError **error)
{
	keyring_handler = purple_keyring_new();

	purple_keyring_set_name(keyring_handler, _(WINCRED_NAME));
	purple_keyring_set_id(keyring_handler, WINCRED_ID);
	purple_keyring_set_read_password(keyring_handler, wincred_read);
	purple_keyring_set_save_password(keyring_handler, wincred_save);

	purple_keyring_register(keyring_handler);

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	if (purple_keyring_get_inuse() == keyring_handler) {
		g_set_error(error, WINCRED_DOMAIN, 0, "The keyring is currently "
			"in use.");
		purple_debug_warning("keyring-wincred",
			"keyring in use, cannot unload\n");
		return FALSE;
	}

	purple_keyring_unregister(keyring_handler);
	purple_keyring_free(keyring_handler);
	keyring_handler = NULL;

	return TRUE;
}

PURPLE_PLUGIN_INIT(gnome_keyring, plugin_query, plugin_load, plugin_unload);
