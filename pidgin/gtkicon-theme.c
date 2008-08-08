/*
 * Icon Themes for Pidgin
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
 *
 */

#include "gtkicon-theme.h"
#include "pidginstock.h"

#include <gtk/gtk.h>

#define PIDGIN_ICON_THEME_GET_PRIVATE(Gobject) \
	((PidginIconThemePrivate *) ((PIDGIN_ICON_THEME(Gobject))->priv))


/******************************************************************************
 * Structs
 *****************************************************************************/
typedef struct {
	/* used to store filenames of diffrent icons */
	GHashTable *icon_files;
} PidginIconThemePrivate;

/******************************************************************************
 * Globals
 *****************************************************************************/

static GObjectClass *parent_class = NULL;

static const GtkStockItem stock_icons[] =
{
    { PIDGIN_STOCK_TRAY_AVAILABLE,	"",     0, 0, NULL },
    { PIDGIN_STOCK_TRAY_INVISIBLE,	"",     0, 0, NULL },
    { PIDGIN_STOCK_TRAY_AWAY,		"",     0, 0, NULL },
    { PIDGIN_STOCK_TRAY_BUSY,		"",     0, 0, NULL },
    { PIDGIN_STOCK_TRAY_XA,		"",     0, 0, NULL },
    { PIDGIN_STOCK_TRAY_OFFLINE,	"",     0, 0, NULL },
    { PIDGIN_STOCK_TRAY_CONNECT,	"",     0, 0, NULL },
    { PIDGIN_STOCK_TRAY_PENDING,	"",     0, 0, NULL },
    { PIDGIN_STOCK_TRAY_EMAIL,		"",     0, 0, NULL },

    { PIDGIN_STOCK_STATUS_AVAILABLE,    "",     0, 0, NULL },
    { PIDGIN_STOCK_STATUS_AVAILABLE_I,	"",     0, 0, NULL },
    { PIDGIN_STOCK_STATUS_AWAY,		"",     0, 0, NULL },
    { PIDGIN_STOCK_STATUS_AWAY_I,	"",     0, 0, NULL },
    { PIDGIN_STOCK_STATUS_BUSY,		"",     0, 0, NULL },
    { PIDGIN_STOCK_STATUS_BUSY_I,	"",     0, 0, NULL },
    { PIDGIN_STOCK_STATUS_CHAT,		"",     0, 0, NULL },
    { PIDGIN_STOCK_STATUS_INVISIBLE,	"",     0, 0, NULL },
    { PIDGIN_STOCK_STATUS_XA,		"",     0, 0, NULL },
    { PIDGIN_STOCK_STATUS_XA_I,		"",     0, 0, NULL },
    { PIDGIN_STOCK_STATUS_LOGIN,	"",     0, 0, NULL },
    { PIDGIN_STOCK_STATUS_LOGOUT,	"",     0, 0, NULL },
    { PIDGIN_STOCK_STATUS_OFFLINE,	"",     0, 0, NULL },
    { PIDGIN_STOCK_STATUS_OFFLINE_I,	"",     0, 0, NULL },
    { PIDGIN_STOCK_STATUS_PERSON,	"",     0, 0, NULL },
    { PIDGIN_STOCK_STATUS_MESSAGE,	"",     0, 0, NULL }
};



/******************************************************************************
 * Enums
 *****************************************************************************/

/******************************************************************************
 * GObject Stuff                                                              
 *****************************************************************************/

static void
pidgin_icon_theme_init(GTypeInstance *instance,
			gpointer klass)
{
	PidginIconThemePrivate *priv;

	(PIDGIN_ICON_THEME(instance))->priv = g_new0(PidginIconThemePrivate, 1);

	priv = PIDGIN_ICON_THEME_GET_PRIVATE(instance);

	priv->icon_files = g_hash_table_new_full (g_str_hash,
						   g_str_equal,
						   g_free,
						   g_free);
}

static void 
pidgin_icon_theme_finalize (GObject *obj)
{
	PidginIconThemePrivate *priv;

	priv = PIDGIN_ICON_THEME_GET_PRIVATE(obj);
	
	g_hash_table_destroy(priv->icon_files);

	parent_class->finalize (obj);
}

static void
pidgin_icon_theme_class_init (PidginIconThemeClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent (klass);

        obj_class->finalize = pidgin_icon_theme_finalize;
}

GType 
pidgin_icon_theme_get_type (void)
{
  static GType type = 0;
  if (type == 0) {
    static const GTypeInfo info = {
      sizeof (PidginIconThemeClass),
      NULL,   /* base_init */
      NULL,   /* base_finalize */
      (GClassInitFunc)pidgin_icon_theme_class_init,   /* class_init */
      NULL,   /* class_finalize */
      NULL,   /* class_data */
      sizeof (PidginIconTheme),
      0,      /* n_preallocs */
      pidgin_icon_theme_init,    /* instance_init */
      NULL,   /* value table */
    };
    type = g_type_register_static (PURPLE_TYPE_THEME,
                                   "PidginIconTheme",
                                   &info, 0 /*G_TYPE_FLAG_ABSTRACT*/);
  }
  return type;
}


/*****************************************************************************
 * Public API functions                                                      
 *****************************************************************************/

const gchar *
pidgin_icon_theme_get_file(PidginIconTheme *theme,
			    const gchar *id)
{
	PidginIconThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_ICON_THEME(theme), NULL);

	priv = PIDGIN_ICON_THEME_GET_PRIVATE(theme);
	
	return g_hash_table_lookup(priv->icon_files, id);
}

void 
pidgin_icon_theme_set_file(PidginIconTheme *theme,
			    const gchar *id, 
			    const gchar *filename)
{
	PidginIconThemePrivate *priv;
	g_return_if_fail(PIDGIN_IS_ICON_THEME(theme));
	
	priv = PIDGIN_ICON_THEME_GET_PRIVATE(theme);

	if (filename != NULL)g_hash_table_replace(priv->icon_files,
                 	             g_strdup(id),
                        	     g_strdup(filename));
	else g_hash_table_remove(priv->icon_files, id);
}
