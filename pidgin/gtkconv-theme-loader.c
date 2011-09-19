/*
 * PidginConvThemeLoader for Pidgin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
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

#include "gtkconv-theme-loader.h"
#include "gtkconv-theme.h"

#include "xmlnode.h"
#include "debug.h"

/*****************************************************************************
 * Conversation Theme Builder
 *****************************************************************************/

static GHashTable *
read_info_plist(xmlnode *plist)
{
	GHashTable *info;
	xmlnode *key, *value;
	gboolean fail = FALSE;

	info = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	for (key = xmlnode_get_child(plist, "dict/key");
	     key;
	     key = xmlnode_get_next_twin(key)) {
		char *keyname;
		GValue *val;

		;
		for (value = key->next; value && value->type != XMLNODE_TYPE_TAG; value = value->next)
			;
		if (!value) {
			fail = TRUE;
			break;
		}

		val = g_new0(GValue, 1);
		if (g_str_equal(value->name, "string")) {
			g_value_init(val, G_TYPE_STRING);
			g_value_take_string(val, xmlnode_get_data_unescaped(value));

		} else if (g_str_equal(value->name, "true")) {
			g_value_init(val, G_TYPE_BOOLEAN);
			g_value_set_boolean(val, TRUE);

		} else if (g_str_equal(value->name, "false")) {
			g_value_init(val, G_TYPE_BOOLEAN);
			g_value_set_boolean(val, FALSE);

		} else if (g_str_equal(value->name, "real")) {
			char *temp = xmlnode_get_data_unescaped(value);
			g_value_init(val, G_TYPE_FLOAT);
			g_value_set_float(val, atof(temp));
			g_free(temp);

		} else if (g_str_equal(value->name, "integer")) {
			char *temp = xmlnode_get_data_unescaped(value);
			g_value_init(val, G_TYPE_INT);
			g_value_set_int(val, atoi(temp));
			g_free(temp);

		} else {
			/* NOTE: We don't support array, data, date, or dict as values,
			   since they don't seem to be needed for styles. */
			g_free(val);
			fail = TRUE;
			break;
		}

		keyname = xmlnode_get_data_unescaped(key);
		g_hash_table_insert(info, keyname, val);
	}

	if (fail) {
		g_hash_table_destroy(info);
		info = NULL;
	}

	return info;
}

static PurpleTheme *
pidgin_conv_loader_build(const gchar *dir)
{
	PidginConvTheme *theme = NULL;
	char *contents;
	xmlnode *plist;
	GHashTable *info;
	GValue *val;
	int MessageViewVersion;
	const char *CFBundleName;
	const char *CFBundleIdentifier;
	GDir *variants;
	char *variant_dir;

	g_return_val_if_fail(dir != NULL, NULL);

	/* Load Info.plist for theme information */
	contents = g_build_filename(dir, "Contents", NULL);
	plist = xmlnode_from_file(contents, "Info.plist", "Info.plist", "gtkconv-theme-loader");
	g_free(contents);
	if (plist == NULL) {
		purple_debug_error("gtkconv-theme-loader",
		                   "Failed to load Contents/Info.plist in %s\n", dir);
		return NULL;
	}

	info = read_info_plist(plist);
	xmlnode_free(plist);
	if (info == NULL) {
		purple_debug_error("gtkconv-theme-loader",
		                   "Failed to load Contents/Info.plist in %s\n", dir);
		return NULL;
	}

	/* Check for required keys: CFBundleName */
	val = g_hash_table_lookup(info, "CFBundleName");
	if (!val || !G_VALUE_HOLDS_STRING(val)) {
		purple_debug_error("gtkconv-theme-loader",
		                   "%s/Contents/Info.plist missing required string key CFBundleName.\n",
		                   dir);
		g_hash_table_destroy(info);
		return NULL;
	}
	CFBundleName = g_value_get_string(val);

	/* Check for required keys: CFBundleIdentifier */
	val = g_hash_table_lookup(info, "CFBundleIdentifier");
	if (!val || !G_VALUE_HOLDS_STRING(val)) {
		purple_debug_error("gtkconv-theme-loader",
		                   "%s/Contents/Info.plist missing required string key CFBundleIdentifier.\n",
		                   dir);
		g_hash_table_destroy(info);
		return NULL;
	}
	CFBundleIdentifier = g_value_get_string(val);

	/* Check for required keys: MessageViewVersion */
	val = g_hash_table_lookup(info, "MessageViewVersion");
	if (!val || !G_VALUE_HOLDS_INT(val)) {
		purple_debug_error("gtkconv-theme-loader",
		                   "%s/Contents/Info.plist missing required integer key MessageViewVersion.\n",
		                   dir);
		g_hash_table_destroy(info);
		return NULL;
	}

	MessageViewVersion = g_value_get_int(val);
	if (MessageViewVersion < 3) {
		purple_debug_error("gtkconv-theme-loader",
		                   "%s is a legacy style (version %d) and will not be loaded.\n",
		                   CFBundleName, MessageViewVersion);
		g_hash_table_destroy(info);
		return NULL;
	}

	theme = g_object_new(PIDGIN_TYPE_CONV_THEME,
	                     "type", "conversation",
	                     "name", CFBundleName,
	                     "directory", dir,
	                     "info", info, NULL);

	/* Read list of variants */
	variant_dir = g_build_filename(dir, "Contents", "Resources", "Variants", NULL);
	variants = g_dir_open(variant_dir, 0, NULL);
	g_free(variant_dir);

	if (variants) {
		const char *file;
		char *name;

		while ((file = g_dir_read_name(variants)) != NULL) {
			const char *end = g_strrstr(file, ".css");
			char *name;

			if ((end == NULL) || (*(end + 4) != '\0'))
				continue;

			name = g_strndup(file, end - file);
			pidgin_conversation_theme_add_variant(theme, name);
		}

		g_dir_close(variants);
	}

	return PURPLE_THEME(theme);
}

/******************************************************************************
 * GObject Stuff
 *****************************************************************************/

static void
pidgin_conv_theme_loader_class_init(PidginConvThemeLoaderClass *klass)
{
	PurpleThemeLoaderClass *loader_klass = PURPLE_THEME_LOADER_CLASS(klass);

	loader_klass->purple_theme_loader_build = pidgin_conv_loader_build;
}


GType
pidgin_conversation_theme_loader_get_type(void)
{
	static GType type = 0;
	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(PidginConvThemeLoaderClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc)pidgin_conv_theme_loader_class_init, /* class_init */
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (PidginConvThemeLoader),
			0, /* n_preallocs */
			NULL, /* instance_init */
			NULL, /* value table */
		};
		type = g_type_register_static(PURPLE_TYPE_THEME_LOADER,
				"PidginConvThemeLoader", &info, 0);
	}
	return type;
}

