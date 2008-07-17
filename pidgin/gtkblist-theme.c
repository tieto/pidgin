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

/******************************************************************************
 * Structs
 *****************************************************************************/
typedef struct {
	gchar *icon_theme;

	/* Buddy list */
	GdkColor *bgcolor;
	gdouble opacity;
	PidginBlistLayout *layout;
	
	/* groups */
	GdkColor *expanded_color;
	FontColorPair *expanded;

	GdkColor *collapsed_color;
	FontColorPair *collapsed;

	/* buddy */
	GdkColor *buddy_bgcolor1;
	GdkColor *buddy_bgcolor2;

	FontColorPair *online;
	FontColorPair *away;
	FontColorPair *offline;
	FontColorPair *message;
	FontColorPair *status;

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
 * Helpers
 *****************************************************************************/

static void
free_font_and_color(FontColorPair *pair) 
{
	if(pair != NULL){
		g_free(pair->font); 
		g_free(pair);
	}
}

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
	PidginBlistTheme *theme = PIDGIN_BLIST_THEME(obj);

	switch(param_id) {
		case PROP_ICON_THEME:
			g_value_set_string(value, pidgin_blist_theme_get_icon_theme(theme));
			break;
		case PROP_BACKGROUND_COLOR:
			g_value_set_pointer(value, pidgin_blist_theme_get_background_color(theme));
			break;
		case PROP_OPACITY:
			g_value_set_double(value, pidgin_blist_theme_get_opacity(theme));
			break;
		case PROP_LAYOUT:
			g_value_set_pointer(value, pidgin_blist_theme_get_layout(theme));
			break;
		case PROP_EXPANDED_COLOR:
			g_value_set_pointer(value, pidgin_blist_theme_get_expanded_background_color(theme));
			break;
		case PROP_EXPANDED_TEXT:
			g_value_set_pointer(value, pidgin_blist_theme_get_expanded_text_info(theme));
			break;
		case PROP_COLLAPSED_COLOR:
			g_value_set_pointer(value, pidgin_blist_theme_get_collapsed_background_color(theme));
			break;
		case PROP_COLLAPSED_TEXT:
			g_value_set_pointer(value, pidgin_blist_theme_get_collapsed_text_info(theme));
			break;
		case PROP_BGCOLOR1:
			g_value_set_pointer(value, pidgin_blist_theme_get_buddy_color_1(theme));
			break;
		case PROP_BGCOLOR2:
			g_value_set_pointer(value, pidgin_blist_theme_get_buddy_color_2(theme));
			break;
		case PROP_ONLINE:
			g_value_set_pointer(value, pidgin_blist_theme_get_online_text_info(theme));
			break;
		case PROP_AWAY:
			g_value_set_pointer(value, pidgin_blist_theme_get_away_text_info(theme));
			break;
		case PROP_OFFLINE:
			g_value_set_pointer(value, pidgin_blist_theme_get_offline_text_info(theme));
			break;
		case PROP_MESSAGE:
			g_value_set_pointer(value, pidgin_blist_theme_get_unread_message_text_info(theme));
			break;
		case PROP_STATUS:
			g_value_set_pointer(value, pidgin_blist_theme_get_status_text_info(theme));
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
	PidginBlistTheme *theme = PIDGIN_BLIST_THEME(obj);

	switch(param_id) {
		case PROP_ICON_THEME:
			pidgin_blist_theme_set_icon_theme(theme, g_value_get_string(value));
			break;
		case PROP_BACKGROUND_COLOR:
			pidgin_blist_theme_set_background_color(theme, g_value_get_pointer(value));
			break;
		case PROP_OPACITY:
			pidgin_blist_theme_set_opacity(theme, g_value_get_double(value));
			break;
		case PROP_LAYOUT:
			pidgin_blist_theme_set_layout(theme, g_value_get_pointer(value));
			break;
		case PROP_EXPANDED_COLOR:
			pidgin_blist_theme_set_expanded_background_color(theme, g_value_get_pointer(value));
			break;
		case PROP_EXPANDED_TEXT:
			pidgin_blist_theme_set_expanded_text_info(theme, g_value_get_pointer(value));
			break;
		case PROP_COLLAPSED_COLOR:
			pidgin_blist_theme_set_collapsed_background_color(theme, g_value_get_pointer(value));
			break;
		case PROP_COLLAPSED_TEXT:
			pidgin_blist_theme_set_collapsed_text_info(theme, g_value_get_pointer(value));
			break;
		case PROP_BGCOLOR1:
			pidgin_blist_theme_set_buddy_color_1(theme, g_value_get_pointer(value));
			break;
		case PROP_BGCOLOR2:
			pidgin_blist_theme_set_buddy_color_2(theme, g_value_get_pointer(value));
			break;
		case PROP_ONLINE:
			pidgin_blist_theme_set_online_text_info(theme, g_value_get_pointer(value));
			break;
		case PROP_AWAY:
			pidgin_blist_theme_set_away_text_info(theme, g_value_get_pointer(value));
			break;
		case PROP_OFFLINE:
			pidgin_blist_theme_set_offline_text_info(theme, g_value_get_pointer(value));
			break;
		case PROP_MESSAGE:
			pidgin_blist_theme_set_unread_message_text_info(theme, g_value_get_pointer(value));
			break;
		case PROP_STATUS:
			pidgin_blist_theme_set_status_text_info(theme, g_value_get_pointer(value));
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

/* get methods */
gchar *
pidgin_blist_theme_get_icon_theme(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	return priv->icon_theme;
}

GdkColor *
pidgin_blist_theme_get_background_color(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	return priv->bgcolor;
}

gdouble
pidgin_blist_theme_get_opacity(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), 1.0);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	return priv->opacity;
}

PidginBlistLayout *
pidgin_blist_theme_get_layout(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	return priv->layout;
}

GdkColor *
pidgin_blist_theme_get_expanded_background_color(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	return priv->expanded_color;
}

FontColorPair *
pidgin_blist_theme_get_expanded_text_info(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	return priv->expanded;
}

GdkColor *
pidgin_blist_theme_get_collapsed_background_color(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	return priv->collapsed_color;
}

FontColorPair *
pidgin_blist_theme_get_collapsed_text_info(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	return priv->collapsed;
}

GdkColor *
pidgin_blist_theme_get_buddy_color_1(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	return priv->buddy_bgcolor1;
}

GdkColor *
pidgin_blist_theme_get_buddy_color_2(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	return priv->buddy_bgcolor2;
}

FontColorPair *
pidgin_blist_theme_get_online_text_info(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	return priv->online;
}

FontColorPair *
pidgin_blist_theme_get_away_text_info(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	return priv->away;
}

FontColorPair *
pidgin_blist_theme_get_offline_text_info(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	return priv->offline;
}

FontColorPair *
pidgin_blist_theme_get_unread_message_text_info(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	return priv->message;
}

FontColorPair *
pidgin_blist_theme_get_status_text_info(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	return priv->status;
}

/* Set Methods */
void
pidgin_blist_theme_set_icon_theme(PidginBlistTheme *theme, const gchar *icon_theme)
{
	PidginBlistThemePrivate *priv;

	g_return_if_fail(PIDGIN_IS_BLIST_THEME(theme));

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	g_free(priv->icon_theme);
	priv->icon_theme = g_strdup(icon_theme);
}

void
pidgin_blist_theme_set_background_color(PidginBlistTheme *theme, GdkColor *color)
{
	PidginBlistThemePrivate *priv;

	g_return_if_fail(PIDGIN_IS_BLIST_THEME(theme));

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	g_free(priv->bgcolor);
	priv->bgcolor = color;
}

void
pidgin_blist_theme_set_opacity(PidginBlistTheme *theme, gdouble opacity)
{
	PidginBlistThemePrivate *priv;

	g_return_if_fail(PIDGIN_IS_BLIST_THEME(theme) || opacity < 0.0 || opacity > 1.0);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	priv->opacity = opacity;
}

void
pidgin_blist_theme_set_layout(PidginBlistTheme *theme, PidginBlistLayout *layout)
{
	PidginBlistThemePrivate *priv;

	g_return_if_fail(PIDGIN_IS_BLIST_THEME(theme));

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	g_free(priv->layout);
	priv->layout = layout;
}

void
pidgin_blist_theme_set_expanded_background_color(PidginBlistTheme *theme, GdkColor *color)
{
	PidginBlistThemePrivate *priv;

	g_return_if_fail(PIDGIN_IS_BLIST_THEME(theme));

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	g_free(priv->expanded_color);
	priv->expanded_color = color;
}

void
pidgin_blist_theme_set_expanded_text_info(PidginBlistTheme *theme, FontColorPair *pair)
{
	PidginBlistThemePrivate *priv;

	g_return_if_fail(PIDGIN_IS_BLIST_THEME(theme));

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	free_font_and_color(priv->expanded); 
	priv->expanded = pair;
}

void
pidgin_blist_theme_set_collapsed_background_color(PidginBlistTheme *theme, GdkColor *color)
{
	PidginBlistThemePrivate *priv;

	g_return_if_fail(PIDGIN_IS_BLIST_THEME(theme));

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	g_free(priv->collapsed_color);
	priv->collapsed_color = color;
}

void
pidgin_blist_theme_set_collapsed_text_info(PidginBlistTheme *theme, FontColorPair *pair)
{
	PidginBlistThemePrivate *priv;

	g_return_if_fail(PIDGIN_IS_BLIST_THEME(theme));

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	free_font_and_color(priv->collapsed); 
	priv->collapsed = pair;
}

void
pidgin_blist_theme_set_buddy_color_1(PidginBlistTheme *theme, GdkColor *color)
{
	PidginBlistThemePrivate *priv;

	g_return_if_fail(PIDGIN_IS_BLIST_THEME(theme));

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	g_free(priv->buddy_bgcolor1);
	priv->buddy_bgcolor1 = color;
}

void
pidgin_blist_theme_set_buddy_color_2(PidginBlistTheme *theme, GdkColor *color)
{
	PidginBlistThemePrivate *priv;

	g_return_if_fail(PIDGIN_IS_BLIST_THEME(theme));

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	g_free(priv->buddy_bgcolor2);
	priv->buddy_bgcolor2 = color;
}

void
pidgin_blist_theme_set_online_text_info(PidginBlistTheme *theme, FontColorPair *pair)
{
	PidginBlistThemePrivate *priv;

	g_return_if_fail(PIDGIN_IS_BLIST_THEME(theme));

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	free_font_and_color(priv->online); 
	priv->online = pair;
}

void
pidgin_blist_theme_set_away_text_info(PidginBlistTheme *theme, FontColorPair *pair)
{
	PidginBlistThemePrivate *priv;

	g_return_if_fail(PIDGIN_IS_BLIST_THEME(theme));

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	free_font_and_color(priv->away); 
	priv->away = pair;
}

void
pidgin_blist_theme_set_offline_text_info(PidginBlistTheme *theme, FontColorPair *pair)
{
	PidginBlistThemePrivate *priv;

	g_return_if_fail(PIDGIN_IS_BLIST_THEME(theme));

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	free_font_and_color(priv->offline); 
	priv->offline = pair;
}

void
pidgin_blist_theme_set_unread_message_text_info(PidginBlistTheme *theme, FontColorPair *pair)
{
	PidginBlistThemePrivate *priv;

	g_return_if_fail(PIDGIN_IS_BLIST_THEME(theme));

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	free_font_and_color(priv->message); 
	priv->message = pair;
}

void
pidgin_blist_theme_set_status_text_info(PidginBlistTheme *theme, FontColorPair *pair)
{
	PidginBlistThemePrivate *priv;

	g_return_if_fail(PIDGIN_IS_BLIST_THEME(theme));

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	free_font_and_color(priv->status); 
	priv->status = pair;
}
