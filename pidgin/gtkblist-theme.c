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

#define PIDGIN_BLIST_THEME_GET_PRIVATE(Gobject) \
	((PidginBlistThemePrivate *) ((PIDGIN_BLIST_THEME(Gobject))->priv))

#define free_font_and_color(font_color_pair) \
	g_free(font_color_pair->font); \
	g_free(font_color_pair)
/******************************************************************************
 * Structs
 *****************************************************************************/
typedef struct {
	gchar *icon_theme;

	/* Buddy list */
	GdkColor *bgcolor;
	gdouble opacity;
	blist_layout *layout;
	
	/* groups */
	GdkColor *expanded_color;
	font_color_pair *expanded;

	GdkColor *collapsed_color;
	font_color_pair *collapsed;

	/* buddy */
	GdkColor *buddy_bgcolor1;
	GdkColor *buddy_bgcolor2;

	font_color_pair *online;
	font_color_pair *away;
	font_color_pair *offline;
	font_color_pair *message;
	font_color_pair *status;

} PidginBlistThemePrivate;

/******************************************************************************
 * Globals
 *****************************************************************************/

static GObjectClass *parent_class = NULL;

/******************************************************************************
 * Enums
 *****************************************************************************/
enum {
	PROP_ZERO = 0,
	PROP_ICON_THEME,
	PROP_BACKGROUND_COLOR,
	PROP_OPACITY,
	PROP_LAYOUT,
	PROP_EXPANDED_COLOR,
	PROP_EXPANDED_TEXT,
	PROP_COLLAPSED_COLOR,
	PROP_COLLAPSED_TEXT,
	PROP_BGCOLOR1,
	PROP_BGCOLOR2,
	PROP_ONLINE,
	PROP_AWAY,
	PROP_OFFLINE,
	PROP_MESSAGE,
	PROP_STATUS,
};
/******************************************************************************
 * GObject Stuff                                                              
 *****************************************************************************/

static void
pidgin_blist_theme_init(GTypeInstance *instance,
			gpointer klass)
{
	(PIDGIN_BLIST_THEME(instance))->priv = g_new0(PidginBlistThemePrivate, 1);
	
}

static void
pidgin_blist_theme_get_property(GObject *obj, guint param_id, GValue *value,
						 GParamSpec *psec)
{
	PidginBlistThemePrivate *priv = PIDGIN_BLIST_THEME_GET_PRIVATE(obj);

	switch(param_id) {
		case PROP_ICON_THEME:
			g_value_set_string(value, priv->icon_theme);
			break;
		case PROP_BACKGROUND_COLOR:
			g_value_set_pointer(value, priv->bgcolor);
			break;
		case PROP_OPACITY:
			g_value_set_double(value, priv->opacity);
			break;
		case PROP_LAYOUT:
			g_value_set_pointer(value, priv->layout);
			break;
		case PROP_EXPANDED_COLOR:
			g_value_set_pointer(value, priv->expanded_color);
			break;
		case PROP_EXPANDED_TEXT:
			g_value_set_pointer(value, priv->expanded);
			break;
		case PROP_COLLAPSED_COLOR:
			g_value_set_pointer(value, priv->collapsed_color);
			break;
		case PROP_COLLAPSED_TEXT:
			g_value_set_pointer(value, priv->collapsed);
			break;
		case PROP_BGCOLOR1:
			g_value_set_pointer(value, priv->buddy_bgcolor1);
			break;
		case PROP_BGCOLOR2:
			g_value_set_pointer(value, priv->buddy_bgcolor2);
			break;
		case PROP_ONLINE:
			g_value_set_pointer(value, priv->online);
			break;
		case PROP_AWAY:
			g_value_set_pointer(value, priv->away);
			break;
		case PROP_OFFLINE:
			g_value_set_pointer(value, priv->offline);
			break;
		case PROP_MESSAGE:
			g_value_set_pointer(value, priv->message);
			break;
		case PROP_STATUS:
			g_value_set_pointer(value, priv->status);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, psec);
			break;
	}
}

static void
pidgin_blist_theme_set_property(GObject *obj, guint param_id, const GValue *value,
						 GParamSpec *psec)
{
	PidginBlistThemePrivate *priv = PIDGIN_BLIST_THEME_GET_PRIVATE(obj);

	switch(param_id) {
		case PROP_ICON_THEME:
			g_free(priv->icon_theme);
			priv->icon_theme = g_strdup(g_value_get_string(value));
			break;
		case PROP_BACKGROUND_COLOR:
			priv->bgcolor = g_value_get_pointer(value);
			break;
		case PROP_OPACITY:
			priv->opacity = g_value_get_double(value);
			break;
		case PROP_LAYOUT:
			g_free(priv->layout);
			priv->layout = g_value_get_pointer(value);
			break;
		case PROP_EXPANDED_COLOR:
			priv->expanded_color = g_value_get_pointer(value);
			break;
		case PROP_EXPANDED_TEXT:
			free_font_and_color(priv->expanded);
			priv->expanded = g_value_get_pointer(value);
			break;
		case PROP_COLLAPSED_COLOR:
			priv->collapsed_color = g_value_get_pointer(value);
			break;
		case PROP_COLLAPSED_TEXT:
			free_font_and_color(priv->collapsed);
			priv->collapsed = g_value_get_pointer(value);
			break;
		case PROP_BGCOLOR1:
			priv->buddy_bgcolor1 = g_value_get_pointer(value);
			break;
		case PROP_BGCOLOR2:
			priv->buddy_bgcolor2 = g_value_get_pointer(value);
			break;
		case PROP_ONLINE:
			free_font_and_color(priv->online);
			priv->online = g_value_get_pointer(value);
			break;
		case PROP_AWAY:
			free_font_and_color(priv->away);
			priv->away = g_value_get_pointer(value);
			break;
		case PROP_OFFLINE:
			free_font_and_color(priv->offline);
			priv->offline = g_value_get_pointer(value);
			break;
		case PROP_MESSAGE:
			free_font_and_color(priv->message);
			priv->message = g_value_get_pointer(value);
			break;
		case PROP_STATUS:
			free_font_and_color(priv->status);
			priv->status = g_value_get_pointer(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, psec);
			break;
	}
}
static void 
pidgin_blist_theme_finalize (GObject *obj)
{
	PidginBlistThemePrivate *priv;

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(obj);

	/* Buddy List */
	g_free(priv->icon_theme);
	g_free(priv->layout);
	
	/* Group */
	free_font_and_color(priv->expanded);
	free_font_and_color(priv->collapsed);

	/* Buddy */
	free_font_and_color(priv->online);
	free_font_and_color(priv->away);
	free_font_and_color(priv->offline);
	free_font_and_color(priv->message);
	free_font_and_color(priv->status);

	g_free(priv);

	parent_class->finalize (obj);
}

static void
pidgin_blist_theme_class_init (PidginBlistThemeClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	GParamSpec *pspec;

	parent_class = g_type_class_peek_parent (klass);

	obj_class->get_property = pidgin_blist_theme_get_property;
	obj_class->set_property = pidgin_blist_theme_set_property;
	obj_class->finalize = pidgin_blist_theme_finalize;

	/* Icon Theme */
	pspec = g_param_spec_string("icon-theme", "Icon Theme",
				    "The icon theme to go with this buddy list theme",
				    NULL,
				    G_PARAM_READWRITE);
	g_object_class_install_property(obj_class, PROP_ICON_THEME, pspec);

	/* Buddy List */
	pspec = g_param_spec_pointer("background-color", "Background Color",
				    "The background color for the buddy list",
				    G_PARAM_READWRITE);
	g_object_class_install_property(obj_class, PROP_BACKGROUND_COLOR, pspec);

	pspec = g_param_spec_double("opacity","Opacity",
                	            "The transparency of the buddy list",
                	            0.0, 1.0,
                	            1.0,
                	            G_PARAM_READWRITE);
	g_object_class_install_property(obj_class, PROP_OPACITY, pspec);

	pspec = g_param_spec_pointer("layout", "Layout",
                                     "The layout of icons, name, and status of the blist",
                                     G_PARAM_READWRITE);

	g_object_class_install_property(obj_class, PROP_LAYOUT, pspec);

	/* Group */
	pspec = g_param_spec_pointer("expanded-color", "Expanded Background Color",
				    "The background color of an expanded group",
				    G_PARAM_READWRITE);
	g_object_class_install_property(obj_class, PROP_EXPANDED_COLOR, pspec);

	pspec = g_param_spec_pointer("expanded-text", "Expanded Text",
                                     "The text information for when a group is expanded",
                                     G_PARAM_READWRITE);
	g_object_class_install_property(obj_class, PROP_EXPANDED_TEXT, pspec);

	pspec = g_param_spec_pointer("collapsed-color", "Collapsed Background Color",
				    "The background color of a collapsed group",
				    G_PARAM_READWRITE);
	g_object_class_install_property(obj_class, PROP_EXPANDED_COLOR, pspec);

	pspec = g_param_spec_pointer("collapsed-text", "Collapsed Text",
                                     "The text information for when a group is collapsed",
                                     G_PARAM_READWRITE);
	g_object_class_install_property(obj_class, PROP_EXPANDED_TEXT, pspec);

	/* Buddy */
	pspec = g_param_spec_pointer("buddy-bgcolor1", "Buddy Background Color 1",
				    "The background color of a buddy",
				    G_PARAM_READWRITE);
	g_object_class_install_property(obj_class, PROP_BGCOLOR1, pspec);

	pspec = g_param_spec_pointer("buddy-bgcolor2", "Buddy Background Color 2",
				    "The background color of a buddy",
				    G_PARAM_READWRITE);
	g_object_class_install_property(obj_class, PROP_BGCOLOR2, pspec);

	pspec = g_param_spec_pointer("online", "On-line Text",
                                     "The text information for when a buddy is online",
                                     G_PARAM_READWRITE);
	g_object_class_install_property(obj_class, PROP_ONLINE, pspec);

	pspec = g_param_spec_pointer("away", "Away Text",
                                     "The text information for when a buddy is away",
                                     G_PARAM_READWRITE);
	g_object_class_install_property(obj_class, PROP_AWAY, pspec);

	pspec = g_param_spec_pointer("offline", "Off-line Text",
                                     "The text information for when a buddy is off-line",
                                     G_PARAM_READWRITE);
	g_object_class_install_property(obj_class, PROP_OFFLINE, pspec);

	pspec = g_param_spec_pointer("message", "Meggage Text",
                                     "The text information for when a buddy is has an unread message",
                                     G_PARAM_READWRITE);
	g_object_class_install_property(obj_class, PROP_MESSAGE, pspec);

	pspec = g_param_spec_pointer("status", "Status Text",
                                     "The text information for a buddy's status",
                                     G_PARAM_READWRITE);
	g_object_class_install_property(obj_class, PROP_STATUS, pspec);
}

GType 
pidgin_blist_theme_get_type (void)
{
  static GType type = 0;
  if (type == 0) {
    static const GTypeInfo info = {
      sizeof (PidginBlistThemeClass),
      NULL,   /* base_init */
      NULL,   /* base_finalize */
      (GClassInitFunc)pidgin_blist_theme_class_init,   /* class_init */
      NULL,   /* class_finalize */
      NULL,   /* class_data */
      sizeof (PidginBlistTheme),
      0,      /* n_preallocs */
      pidgin_blist_theme_init,    /* instance_init */
      NULL,   /* value table */
    };
    type = g_type_register_static (PURPLE_TYPE_THEME,
                                   "PidginBlistTheme",
                                   &info, 0);
  }
  return type;
}


/*****************************************************************************
 * Public API functions                                                      
 *****************************************************************************/

const gchar *
pidgin_blist_theme_get_icon_theme(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	return priv->icon_theme;
}

gdouble
pidgin_blist_theme_get_opacity(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), 1.0);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	return priv->opacity;
}

const blist_layout *
pidgin_blist_theme_get_layout(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	return priv->layout;
}

const GdkColor *
pidgin_blist_theme_get_expanded_background_color(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	return priv->expanded_color;
}

const font_color_pair *
pidgin_blist_theme_get_expanded_text_info(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	return priv->expanded;
}

const GdkColor *
pidgin_blist_theme_get_collapsed_background_color(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	return priv->collapsed_color;
}

const font_color_pair *
pidgin_blist_theme_get_collapsed_text_info(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	return priv->collapsed;
}

const GdkColor *
pidgin_blist_theme_get_buddy_color_1(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	return priv->buddy_bgcolor1;
}

const GdkColor *
pidgin_blist_theme_get_buddy_color_2(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	return priv->buddy_bgcolor2;
}

const font_color_pair *
pidgin_blist_theme_get_online_text_info(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	return priv->online;
}

const font_color_pair *
pidgin_blist_theme_get_away_text_info(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	return priv->away;
}

const font_color_pair *
pidgin_blist_theme_get_offline_text_info(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	return priv->offline;
}

const font_color_pair *
pidgin_blist_theme_get_unread_message_text_info(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	return priv->message;
}

const font_color_pair *
pidgin_blist_theme_get_status_text_info(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	return priv->status;
}

