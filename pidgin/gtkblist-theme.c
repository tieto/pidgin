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
 */

#include "internal.h"
#include "glibcompat.h"

#include "gtkblist-theme.h"

#define PIDGIN_BLIST_THEME_GET_PRIVATE(Gobject) \
	(G_TYPE_INSTANCE_GET_PRIVATE((Gobject), PIDGIN_TYPE_BLIST_THEME, PidginBlistThemePrivate))

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
	PidginThemeFont *expanded;

	GdkColor *collapsed_color;
	PidginThemeFont *collapsed;

	/* buddy */
	GdkColor *contact_color;

	PidginThemeFont *contact;

	PidginThemeFont *online;
	PidginThemeFont *away;
	PidginThemeFont *offline;
	PidginThemeFont *idle;
	PidginThemeFont *message;
	PidginThemeFont *message_nick_said;

	PidginThemeFont *status;

} PidginBlistThemePrivate;

struct _PidginThemeFont
{
	gchar *font;
	gchar color[10];
	GdkColor *gdkcolor;
};

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
	PROP_MESSAGE_NICK_SAID,
	PROP_STATUS,
	PROP_LAST
};

/******************************************************************************
 * Globals
 *****************************************************************************/

static GObjectClass *parent_class = NULL;
static GParamSpec *properties[PROP_LAST];

/******************************************************************************
 * Helpers
 *****************************************************************************/

PidginThemeFont *
pidgin_theme_font_new(const gchar *face, GdkColor *color)
{
	PidginThemeFont *font = g_new0(PidginThemeFont, 1);
	font->font = g_strdup(face);
	if (color)
		pidgin_theme_font_set_color(font, color);
	return font;
}

void
pidgin_theme_font_free(PidginThemeFont *pair)
{
	if (pair != NULL) {
		g_free(pair->font);
		if (pair->gdkcolor)
			gdk_color_free(pair->gdkcolor);
		g_free(pair);
	}
}

static PidginThemeFont *
copy_font_and_color(const PidginThemeFont *pair)
{
	PidginThemeFont *copy;

	if (pair == NULL)
		return NULL;

	copy = g_new0(PidginThemeFont, 1);
	copy->font  = g_strdup(pair->font);
	strncpy(copy->color, pair->color, sizeof(copy->color) - 1);
	if (pair->gdkcolor)
		copy->gdkcolor = pair->gdkcolor ? gdk_color_copy(pair->gdkcolor) : NULL;
	return copy;
}

void
pidgin_theme_font_set_font_face(PidginThemeFont *font, const gchar *face)
{
	g_return_if_fail(font);
	g_return_if_fail(face);

	g_free(font->font);
	font->font = g_strdup(face);
}

void
pidgin_theme_font_set_color(PidginThemeFont *font, const GdkColor *color)
{
	g_return_if_fail(font);

	if (font->gdkcolor)
		gdk_color_free(font->gdkcolor);

	font->gdkcolor = color ? gdk_color_copy(color) : NULL;
	if (color)
		g_snprintf(font->color, sizeof(font->color),
				"#%02x%02x%02x", color->red >> 8, color->green >> 8, color->blue >> 8);
	else
		font->color[0] = '\0';
}

const gchar *
pidgin_theme_font_get_font_face(PidginThemeFont *font)
{
	g_return_val_if_fail(font, NULL);
	return font->font;
}

const GdkColor *
pidgin_theme_font_get_color(PidginThemeFont *font)
{
	g_return_val_if_fail(font, NULL);
	return font->gdkcolor;
}

const gchar *
pidgin_theme_font_get_color_describe(PidginThemeFont *font)
{
	g_return_val_if_fail(font, NULL);
	return font->color[0] ? font->color : NULL;
}

/******************************************************************************
 * GObject Stuff
 *****************************************************************************/

static void
pidgin_blist_theme_get_property(GObject *obj, guint param_id, GValue *value,
		GParamSpec *psec)
{
	PidginBlistTheme *theme = PIDGIN_BLIST_THEME(obj);

	switch (param_id) {
		case PROP_BACKGROUND_COLOR:
			g_value_set_boxed(value, pidgin_blist_theme_get_background_color(theme));
			break;
		case PROP_OPACITY:
			g_value_set_double(value, pidgin_blist_theme_get_opacity(theme));
			break;
		case PROP_LAYOUT:
			g_value_set_pointer(value, pidgin_blist_theme_get_layout(theme));
			break;
		case PROP_EXPANDED_COLOR:
			g_value_set_boxed(value, pidgin_blist_theme_get_expanded_background_color(theme));
			break;
		case PROP_EXPANDED_TEXT:
			g_value_set_pointer(value, pidgin_blist_theme_get_expanded_text_info(theme));
			break;
		case PROP_COLLAPSED_COLOR:
			g_value_set_boxed(value, pidgin_blist_theme_get_collapsed_background_color(theme));
			break;
		case PROP_COLLAPSED_TEXT:
			g_value_set_pointer(value, pidgin_blist_theme_get_collapsed_text_info(theme));
			break;
		case PROP_CONTACT_COLOR:
			g_value_set_boxed(value, pidgin_blist_theme_get_contact_color(theme));
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
		case PROP_MESSAGE_NICK_SAID:
			g_value_set_pointer(value, pidgin_blist_theme_get_unread_message_nick_said_text_info(theme));
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

	switch (param_id) {
		case PROP_BACKGROUND_COLOR:
			pidgin_blist_theme_set_background_color(theme, g_value_get_boxed(value));
			break;
		case PROP_OPACITY:
			pidgin_blist_theme_set_opacity(theme, g_value_get_double(value));
			break;
		case PROP_LAYOUT:
			pidgin_blist_theme_set_layout(theme, g_value_get_pointer(value));
			break;
		case PROP_EXPANDED_COLOR:
			pidgin_blist_theme_set_expanded_background_color(theme, g_value_get_boxed(value));
			break;
		case PROP_EXPANDED_TEXT:
			pidgin_blist_theme_set_expanded_text_info(theme, g_value_get_pointer(value));
			break;
		case PROP_COLLAPSED_COLOR:
			pidgin_blist_theme_set_collapsed_background_color(theme, g_value_get_boxed(value));
			break;
		case PROP_COLLAPSED_TEXT:
			pidgin_blist_theme_set_collapsed_text_info(theme, g_value_get_pointer(value));
			break;
		case PROP_CONTACT_COLOR:
			pidgin_blist_theme_set_contact_color(theme, g_value_get_boxed(value));
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
		case PROP_MESSAGE_NICK_SAID:
			pidgin_blist_theme_set_unread_message_nick_said_text_info(theme, g_value_get_pointer(value));
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
pidgin_blist_theme_finalize(GObject *obj)
{
	PidginBlistThemePrivate *priv;

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(obj);

	/* Buddy List */
	if (priv->bgcolor)
		gdk_color_free(priv->bgcolor);
	g_free(priv->layout);

	/* Group */
	if (priv->expanded_color)
		gdk_color_free(priv->expanded_color);
	pidgin_theme_font_free(priv->expanded);
	if (priv->collapsed_color)
		gdk_color_free(priv->collapsed_color);
	pidgin_theme_font_free(priv->collapsed);

	/* Buddy */
	if (priv->contact_color)
		gdk_color_free(priv->contact_color);
	pidgin_theme_font_free(priv->contact);
	pidgin_theme_font_free(priv->online);
	pidgin_theme_font_free(priv->away);
	pidgin_theme_font_free(priv->offline);
	pidgin_theme_font_free(priv->idle);
	pidgin_theme_font_free(priv->message);
	pidgin_theme_font_free(priv->message_nick_said);
	pidgin_theme_font_free(priv->status);

	parent_class->finalize (obj);
}

static void
pidgin_blist_theme_init(PidginBlistTheme *theme)
{
	PIDGIN_BLIST_THEME_GET_PRIVATE(theme)->opacity = 1.0;
}

static void
pidgin_blist_theme_class_init(PidginBlistThemeClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent (klass);

	g_type_class_add_private(klass, sizeof(PidginBlistThemePrivate));

	obj_class->get_property = pidgin_blist_theme_get_property;
	obj_class->set_property = pidgin_blist_theme_set_property;
	obj_class->finalize = pidgin_blist_theme_finalize;

	/* Buddy List */
	properties[PROP_BACKGROUND_COLOR] = g_param_spec_boxed("background-color",
			"Background Color",
			"The background color for the buddy list",
			GDK_TYPE_COLOR, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	properties[PROP_OPACITY] = g_param_spec_double("opacity", "Opacity",
			"The opacity of the buddy list",
			0.0, 1.0, 1.0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	properties[PROP_LAYOUT] = g_param_spec_pointer("layout", "Layout",
			"The layout of icons, name, and status of the buddy list",
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	/* Group */
	properties[PROP_EXPANDED_COLOR] = g_param_spec_boxed("expanded-color",
			"Expanded Background Color",
			"The background color of an expanded group",
			GDK_TYPE_COLOR, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	properties[PROP_EXPANDED_TEXT] = g_param_spec_pointer("expanded-text",
			"Expanded Text",
			"The text information for when a group is expanded",
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	properties[PROP_COLLAPSED_COLOR] = g_param_spec_boxed("collapsed-color",
			"Collapsed Background Color",
			"The background color of a collapsed group",
			GDK_TYPE_COLOR, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	properties[PROP_COLLAPSED_TEXT] = g_param_spec_pointer("collapsed-text",
			"Collapsed Text",
			"The text information for when a group is collapsed",
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	/* Buddy */
	properties[PROP_CONTACT_COLOR] = g_param_spec_boxed("contact-color",
			"Contact/Chat Background Color",
			"The background color of a contact or chat",
			GDK_TYPE_COLOR, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	properties[PROP_CONTACT] = g_param_spec_pointer("contact",
			"Contact Text",
			"The text information for when a contact is expanded",
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	properties[PROP_ONLINE] = g_param_spec_pointer("online",
			"Online Text",
			"The text information for when a buddy is online",
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	properties[PROP_AWAY] = g_param_spec_pointer("away",
			"Away Text",
			"The text information for when a buddy is away",
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	properties[PROP_OFFLINE] = g_param_spec_pointer("offline",
			"Offline Text",
			"The text information for when a buddy is offline",
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	properties[PROP_IDLE] = g_param_spec_pointer("idle",
			"Idle Text",
			"The text information for when a buddy is idle",
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	properties[PROP_MESSAGE] = g_param_spec_pointer("message",
			"Message Text",
			"The text information for when a buddy has an unread message",
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	properties[PROP_MESSAGE_NICK_SAID] = g_param_spec_pointer("message-nick-said",
			"Message (Nick Said) Text",
			"The text information for when a chat has an unread message that mentions your nickname",
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	properties[PROP_STATUS] = g_param_spec_pointer("status",
			"Status Text",
			"The text information for a buddy's status",
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(obj_class, PROP_LAST, properties);
}

GType
pidgin_blist_theme_get_type (void)
{
	static GType type = 0;
	if (type == 0) {
		static GTypeInfo info = {
			sizeof(PidginBlistThemeClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc)pidgin_blist_theme_class_init, /* class_init */
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof(PidginBlistTheme),
			0, /* n_preallocs */
			(GInstanceInitFunc)pidgin_blist_theme_init, /* instance_init */
			NULL, /* value table */
		};
		type = g_type_register_static (PURPLE_TYPE_THEME,
				"PidginBlistTheme", &info, 0);
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

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(theme);

	return priv->bgcolor;
}

gdouble
pidgin_blist_theme_get_opacity(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), 1.0);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(theme);

	return priv->opacity;
}

PidginBlistLayout *
pidgin_blist_theme_get_layout(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(theme);

	return priv->layout;
}

GdkColor *
pidgin_blist_theme_get_expanded_background_color(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(theme);

	return priv->expanded_color;
}

PidginThemeFont *
pidgin_blist_theme_get_expanded_text_info(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(theme);

	return priv->expanded;
}

GdkColor *
pidgin_blist_theme_get_collapsed_background_color(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(theme);

	return priv->collapsed_color;
}

PidginThemeFont *
pidgin_blist_theme_get_collapsed_text_info(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(theme);

	return priv->collapsed;
}

GdkColor *
pidgin_blist_theme_get_contact_color(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(theme);

	return priv->contact_color;
}

PidginThemeFont *
pidgin_blist_theme_get_contact_text_info(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(theme);

	return priv->contact;
}

PidginThemeFont *
pidgin_blist_theme_get_online_text_info(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(theme);

	return priv->online;
}

PidginThemeFont *
pidgin_blist_theme_get_away_text_info(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(theme);

	return priv->away;
}

PidginThemeFont *
pidgin_blist_theme_get_offline_text_info(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(theme);

	return priv->offline;
}

PidginThemeFont *
pidgin_blist_theme_get_idle_text_info(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(theme);

	return priv->idle;
}

PidginThemeFont *
pidgin_blist_theme_get_unread_message_text_info(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(theme);

	return priv->message;
}

PidginThemeFont *
pidgin_blist_theme_get_unread_message_nick_said_text_info(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(theme);

	return priv->message_nick_said;
}

PidginThemeFont *
pidgin_blist_theme_get_status_text_info(PidginBlistTheme *theme)
{
	PidginBlistThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_BLIST_THEME(theme), NULL);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(theme);

	return priv->status;
}

/* Set Methods */
void
pidgin_blist_theme_set_background_color(PidginBlistTheme *theme, const GdkColor *color)
{
	PidginBlistThemePrivate *priv;

	g_return_if_fail(PIDGIN_IS_BLIST_THEME(theme));

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(theme);

	if (priv->bgcolor)
		gdk_color_free(priv->bgcolor);
	priv->bgcolor = color ? gdk_color_copy(color) : NULL;

	g_object_notify_by_pspec(G_OBJECT(theme), properties[PROP_BACKGROUND_COLOR]);
}

void
pidgin_blist_theme_set_opacity(PidginBlistTheme *theme, gdouble opacity)
{
	PidginBlistThemePrivate *priv;

	g_return_if_fail(PIDGIN_IS_BLIST_THEME(theme) || opacity < 0.0 || opacity > 1.0);

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(theme);

	priv->opacity = opacity;

	g_object_notify_by_pspec(G_OBJECT(theme), properties[PROP_OPACITY]);
}

void
pidgin_blist_theme_set_layout(PidginBlistTheme *theme, const PidginBlistLayout *layout)
{
	PidginBlistThemePrivate *priv;

	g_return_if_fail(PIDGIN_IS_BLIST_THEME(theme));

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(theme);

	g_free(priv->layout);
	priv->layout = g_memdup(layout, sizeof(PidginBlistLayout));

	g_object_notify_by_pspec(G_OBJECT(theme), properties[PROP_LAYOUT]);
}

void
pidgin_blist_theme_set_expanded_background_color(PidginBlistTheme *theme, const GdkColor *color)
{
	PidginBlistThemePrivate *priv;

	g_return_if_fail(PIDGIN_IS_BLIST_THEME(theme));

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(theme);

	if (priv->expanded_color)
		gdk_color_free(priv->expanded_color);
	priv->expanded_color = color ? gdk_color_copy(color) : NULL;

	g_object_notify_by_pspec(G_OBJECT(theme), properties[PROP_EXPANDED_COLOR]);
}

void
pidgin_blist_theme_set_expanded_text_info(PidginBlistTheme *theme, const PidginThemeFont *pair)
{
	PidginBlistThemePrivate *priv;

	g_return_if_fail(PIDGIN_IS_BLIST_THEME(theme));

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(theme);

	pidgin_theme_font_free(priv->expanded);
	priv->expanded = copy_font_and_color(pair);

	g_object_notify_by_pspec(G_OBJECT(theme), properties[PROP_EXPANDED_TEXT]);
}

void
pidgin_blist_theme_set_collapsed_background_color(PidginBlistTheme *theme, const GdkColor *color)
{
	PidginBlistThemePrivate *priv;

	g_return_if_fail(PIDGIN_IS_BLIST_THEME(theme));

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(theme);

	if (priv->collapsed_color)
		gdk_color_free(priv->collapsed_color);
	priv->collapsed_color = color ? gdk_color_copy(color) : NULL;

	g_object_notify_by_pspec(G_OBJECT(theme), properties[PROP_COLLAPSED_COLOR]);
}

void
pidgin_blist_theme_set_collapsed_text_info(PidginBlistTheme *theme, const PidginThemeFont *pair)
{
	PidginBlistThemePrivate *priv;

	g_return_if_fail(PIDGIN_IS_BLIST_THEME(theme));

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(theme);

	pidgin_theme_font_free(priv->collapsed);
	priv->collapsed = copy_font_and_color(pair);

	g_object_notify_by_pspec(G_OBJECT(theme), properties[PROP_COLLAPSED_TEXT]);
}

void
pidgin_blist_theme_set_contact_color(PidginBlistTheme *theme, const GdkColor *color)
{
	PidginBlistThemePrivate *priv;

	g_return_if_fail(PIDGIN_IS_BLIST_THEME(theme));

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(theme);

	if (priv->contact_color)
		gdk_color_free(priv->contact_color);
	priv->contact_color = color ? gdk_color_copy(color) : NULL;

	g_object_notify_by_pspec(G_OBJECT(theme), properties[PROP_CONTACT_COLOR]);
}

void
pidgin_blist_theme_set_contact_text_info(PidginBlistTheme *theme, const PidginThemeFont *pair)
{
	PidginBlistThemePrivate *priv;

	g_return_if_fail(PIDGIN_IS_BLIST_THEME(theme));

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(theme);

	pidgin_theme_font_free(priv->contact);
	priv->contact = copy_font_and_color(pair);

	g_object_notify_by_pspec(G_OBJECT(theme), properties[PROP_CONTACT]);
}

void
pidgin_blist_theme_set_online_text_info(PidginBlistTheme *theme, const PidginThemeFont *pair)
{
	PidginBlistThemePrivate *priv;

	g_return_if_fail(PIDGIN_IS_BLIST_THEME(theme));

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(theme);

	pidgin_theme_font_free(priv->online);
	priv->online = copy_font_and_color(pair);

	g_object_notify_by_pspec(G_OBJECT(theme), properties[PROP_ONLINE]);
}

void
pidgin_blist_theme_set_away_text_info(PidginBlistTheme *theme, const PidginThemeFont *pair)
{
	PidginBlistThemePrivate *priv;

	g_return_if_fail(PIDGIN_IS_BLIST_THEME(theme));

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(theme);

	pidgin_theme_font_free(priv->away);
	priv->away = copy_font_and_color(pair);

	g_object_notify_by_pspec(G_OBJECT(theme), properties[PROP_AWAY]);
}

void
pidgin_blist_theme_set_offline_text_info(PidginBlistTheme *theme, const PidginThemeFont *pair)
{
	PidginBlistThemePrivate *priv;

	g_return_if_fail(PIDGIN_IS_BLIST_THEME(theme));

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(theme);

	pidgin_theme_font_free(priv->offline);
	priv->offline = copy_font_and_color(pair);

	g_object_notify_by_pspec(G_OBJECT(theme), properties[PROP_OFFLINE]);
}

void
pidgin_blist_theme_set_idle_text_info(PidginBlistTheme *theme, const PidginThemeFont *pair)
{
	PidginBlistThemePrivate *priv;

	g_return_if_fail(PIDGIN_IS_BLIST_THEME(theme));

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(theme);

	pidgin_theme_font_free(priv->idle);
	priv->idle = copy_font_and_color(pair);

	g_object_notify_by_pspec(G_OBJECT(theme), properties[PROP_IDLE]);
}

void
pidgin_blist_theme_set_unread_message_text_info(PidginBlistTheme *theme, const PidginThemeFont *pair)
{
	PidginBlistThemePrivate *priv;

	g_return_if_fail(PIDGIN_IS_BLIST_THEME(theme));

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(theme);

	pidgin_theme_font_free(priv->message);
	priv->message = copy_font_and_color(pair);

	g_object_notify_by_pspec(G_OBJECT(theme), properties[PROP_MESSAGE]);
}

void
pidgin_blist_theme_set_unread_message_nick_said_text_info(PidginBlistTheme *theme, const PidginThemeFont *pair)
{
	PidginBlistThemePrivate *priv;

	g_return_if_fail(PIDGIN_IS_BLIST_THEME(theme));

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(theme);

	pidgin_theme_font_free(priv->message_nick_said);
	priv->message_nick_said = copy_font_and_color(pair);

	g_object_notify_by_pspec(G_OBJECT(theme), properties[PROP_MESSAGE_NICK_SAID]);
}

void
pidgin_blist_theme_set_status_text_info(PidginBlistTheme *theme, const PidginThemeFont *pair)
{
	PidginBlistThemePrivate *priv;

	g_return_if_fail(PIDGIN_IS_BLIST_THEME(theme));

	priv = PIDGIN_BLIST_THEME_GET_PRIVATE(theme);

	pidgin_theme_font_free(priv->status);
	priv->status = copy_font_and_color(pair);

	g_object_notify_by_pspec(G_OBJECT(theme), properties[PROP_STATUS]);
}
