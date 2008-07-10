/*
 * Buddy List Themes for Pidgin
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

#include "gtkblist-theme.h"

#define PIDGIN_BUDDY_LIST_THEME_GET_PRIVATE(Gobject) \
	((PidginBuddyListThemePrivate *) ((PIDGIN_BUDDY_LIST_THEME(Gobject))->priv))


/******************************************************************************
 * Structs
 *****************************************************************************/
typedef struct {
	gchar *icon_theme;

	/* Buddy list */
	gchar *blist_color;
	gdouble opacity;
	
	/* groups */
	gchar *expanded_bgcolor;
	gchar *expanded_tcolor;
	gchar *expanded_font;

	gchar *minimized_bgcolor;
	gchar *minimized_tcolor;
	gchar *minimized_font;

	/* buddy */
	gchar *buddy_bgcolor1;
	gchar *buddy_bgcolor2;
	
	gint icon;
	gint text;
	gint status_icon;
	gboolean status; 

	gchar *online_font;
	gchar *online_color;
	
	gchar *away_font;
	gchar *away_color;

	gchar *offline_font;
	gchar *offline_color;

	gchar *message_font;
	gchar *message_color;

	gchar *status_font;
	gchar *status_color;

} PidginBuddyListThemePrivate;

/******************************************************************************
 * Globals
 *****************************************************************************/

static GObjectClass *parent_class = NULL;

/******************************************************************************
 * Enums
 *****************************************************************************/

/******************************************************************************
 * GObject Stuff                                                              
 *****************************************************************************/

static void
pidgin_buddy_list_theme_init(GTypeInstance *instance,
			gpointer klass)
{
	PidginBuddyListThemePrivate *priv;

	(PIDGIN_BUDDY_LIST_THEME(instance))->priv = g_new0(PidginBuddyListThemePrivate, 1);

	priv = PIDGIN_BUDDY_LIST_THEME_GET_PRIVATE(instance);

}

static void 
pidgin_buddy_list_theme_finalize (GObject *obj)
{
	PidginBuddyListThemePrivate *priv;

	priv = PIDGIN_BUDDY_LIST_THEME_GET_PRIVATE(obj);

	parent_class->finalize (obj);
}

static void
pidgin_buddy_list_theme_class_init (PidginBuddyListThemeClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent (klass);

        obj_class->finalize = pidgin_buddy_list_theme_finalize;
}

GType 
pidgin_buddy_list_theme_get_type (void)
{
  static GType type = 0;
  if (type == 0) {
    static const GTypeInfo info = {
      sizeof (PidginBuddyListThemeClass),
      NULL,   /* base_init */
      NULL,   /* base_finalize */
      (GClassInitFunc)pidgin_buddy_list_theme_class_init,   /* class_init */
      NULL,   /* class_finalize */
      NULL,   /* class_data */
      sizeof (PidginBuddyListTheme),
      0,      /* n_preallocs */
      pidgin_buddy_list_theme_init,    /* instance_init */
      NULL,   /* value table */
    };
    type = g_type_register_static (PURPLE_TYPE_THEME,
                                   "PidginBuddyListTheme",
                                   &info, 0);
  }
  return type;
}


/*****************************************************************************
 * Public API functions                                                      
 *****************************************************************************/

