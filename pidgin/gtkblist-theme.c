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
	/* Buddy list */
	gdouble opacity;
	GdkColor *bgcolor;
	PidginBlistLayout *layout;
	
	/* groups */
	GdkColor *expanded_color;
	FontColorPair *expanded;

	GdkColor *collapsed_color;
	FontColorPair *collapsed;

	/* buddy */
	GdkColor *contact_color;

	FontColorPair *contact;

	FontColorPair *online;
	FontColorPair *away;
	FontColorPair *offline;
	FontColorPair *idle;
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
	PROP_BACKGROUND_COLOR,
	PROP_OPACITY,
	PROP_LAYOUT,
	PROP_EXPANDED_COLOR,
	PROP_EXPANDED_TEXT,
	PROP_COLLAPSED_COLOR,
	PROP_COLLAPSED_TEXT,
	PROP_CONTACT_COLOR,
	PROP_CONTACT,
	PROP_ONLINE,
	PROP_AWAY,
	PROP_OFFLINE,
	PROP_IDLE,
	PROP_MESSAGE,
	PROP_STATUS,
};

/******************************************************************************
 * Helpers
 *****************************************************************************/

void
free_font_and_color(FontColorPair *pair) 
{
	if(pair != NULL){
		if (pair->font)
			g_free(pair->font);
		if (pair->color)
			g_free(pair->color);
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
		case PROP_CONTACT_COLOR:
			g_value_set_pointer(value, pidgin_blist_theme_get_contact_color(theme));
			break;
		case PROP_CONTACT:
			g_value_set_pointer(value, pidgin_blist_theme_get_contact_text_info(theme));
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
		case PROP_IDLE:
			g_value_set_pointer(value, pidgin_blist_theme_get_idle_text_info(theme));
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
		case PROP_CONTACT_COLOR:
			pidgin_blist_theme_set_contact_color(theme, g_value_get_pointer(value));
			break;
		case PROP_CONTACT:
			pidgin_blist_theme_set_contact_text_info(theme, g_value_get_pointer(value));
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
		case PROP_IDLE:
			pidgin_blist_theme_set_idle_text_info(theme, g_value_get_pointer(value));
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
	g_free(priv->layout);
	
	/* Group */
	free_font_and_color(priv->expanded);
	free_font_and_color(priv->collapsed);

	/* Buddy */
	free_font_and_color(priv->contact);
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

	/* Buddy List */
	pspec = g_param_spec_pointer("background-color", "Background Color",
				    "The background color for the buddy list",
				    G_PARAM_READWRITE);
	g_object_class_install_property(obj_class, PROP_BACKGROUND_COLOR, pspec);

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
	g_object_class_install_property(obj_class, PROP_COLLAPSED_COLOR, pspec);

	pspec = g_param_spec_pointer("collapsed-text", "Collapsed Text",
                                     "The text information for when a group is collapsed",
                                     G_PARAM_READWRITE);
	g_object_class_install_property(obj_class, PROP_COLLAPSED_TEXT, pspec);

	/* Buddy */
	pspec = g_param_spec_pointer("contact-color", "Contact/Chat Background Color",
				    "The background color of a contact or chat",
				    G_PARAM_READWRITE);
	g_object_class_install_property(obj_class, PROP_CONTACT_COLOR, pspec);

	pspec = g_param_spec_pointer("contact", "Contact Text",
                                     "The text information for when a contact is expanded",
                                     G_PARAM_READWRITE);
	g_object_class_install_property(obj_class, PROP_CONTACT, pspec);

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

	pspec = g_param_spec_pointer("idle", "Idle Text",
                                     "The text information for when a buddy is idle",
                                     G_PARAM_READWRITE);
	g_object_class_install_property(obj_class, PROP_IDLE, pspec);

	pspec = g_param_spec_pointer("message", "Message Text",
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
    static GTypeInfo info = {
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
pidgin_blist_theme_get_contact_color(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	return priv->contact_color;
}

FontColorPair *
pidgin_blist_theme_get_contact_text_info(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	return priv->contact;
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
pidgin_blist_theme_get_idle_text_info(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	return priv->idle;
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
pidgin_blist_theme_set_background_color(PidginBlistTheme *theme, GdkColor *color)
{
	PidginBlistThemePrivate *priv;

	g_return_if_fail(PIDGIN_IS_BLIST_THEME(theme));

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

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
pidgin_blist_theme_set_contact_color(PidginBlistTheme *theme, GdkColor *color)
{
	PidginBlistThemePrivate *priv;

	g_return_if_fail(PIDGIN_IS_BLIST_THEME(theme));

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	priv->contact_color = color;
}

void
pidgin_blist_theme_set_contact_text_info(PidginBlistTheme *theme, FontColorPair *pair)
{
	PidginBlistThemePrivate *priv;

	g_return_if_fail(PIDGIN_IS_BLIST_THEME(theme));

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	free_font_and_color(priv->contact); 
	priv->contact = pair;
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
pidgin_blist_theme_set_idle_text_info(PidginBlistTheme *theme, FontColorPair *pair)
{
	PidginBlistThemePrivate *priv;

	g_return_if_fail(PIDGIN_IS_BLIST_THEME(theme));

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(G_OBJECT(theme));

	free_font_and_color(priv->idle); 
	priv->idle = pair;
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
