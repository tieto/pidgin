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

#define GNOMEKEYRING_NAME        N_("GNOME Keyring")
#define GNOMEKEYRING_DESCRIPTION N_("This plugin will store passwords in GNOME Keyring.")
#define GNOMEKEYRING_AUTHOR      "Tomek Wasilczyk (tomkiewicz@cpw.pidgin.im)"
#define GNOMEKEYRING_ID          "keyring-gnomekeyring"

static PurpleKeyring *keyring_handler = NULL;

static void
keyring_gnome_read(PurpleAccount *account,PurpleKeyringReadCallback cb,
	gpointer data)
{
}

static void
keyring_gnome_save(PurpleAccount *account, const gchar *password,
	PurpleKeyringSaveCallback cb, gpointer data)
{
}

static void
keyring_gnome_close(GError **error)
{
}

static gboolean
keyring_gnome_load(PurplePlugin *plugin)
{
	if (!gnome_keyring_is_available()) {
		purple_debug_info("keyring-gnome", "GNOME Keyring service is "
			"disabled\n");
		return FALSE;
	}

	keyring_handler = purple_keyring_new();

	purple_keyring_set_name(keyring_handler, GNOMEKEYRING_NAME);
	purple_keyring_set_id(keyring_handler, GNOMEKEYRING_ID);
	purple_keyring_set_read_password(keyring_handler, keyring_gnome_read);
	purple_keyring_set_save_password(keyring_handler, keyring_gnome_save);
	purple_keyring_set_close_keyring(keyring_handler, keyring_gnome_close);

	purple_keyring_register(keyring_handler);

	return TRUE;
}

static gboolean
keyring_gnome_unload(PurplePlugin *plugin)
{
	if (purple_keyring_get_inuse() == keyring_handler) {
		purple_debug_warning("keyring-gnome",
			"keyring in use, cannot unload\n");
		return FALSE;
	}

	keyring_gnome_close(NULL);

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
	keyring_gnome_load,		/* load */
	keyring_gnome_unload,		/* unload */
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
