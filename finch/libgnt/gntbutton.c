/**
 * GNT - The GLib Ncurses Toolkit
 *
 * GNT is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This library is free software; you can redistribute it and/or modify
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

#include <stdlib.h>
#include <string.h>

#include "gntbutton.h"
#include "gntstyle.h"
#include "gntutils.h"

enum
{
	SIGS = 1,
};

static GntWidgetClass *parent_class = NULL;
static gboolean small_button = FALSE;

static void
gnt_button_draw(GntWidget *widget)
{
	GntButton *button = GNT_BUTTON(widget);
	GntColorType type;
	gboolean focus;

	if ((focus = gnt_widget_has_focus(widget)))
		type = GNT_COLOR_HIGHLIGHT;
	else
		type = GNT_COLOR_NORMAL;

	wbkgdset(widget->window, '\0' | gnt_color_pair(type));
	mvwaddstr(widget->window, (small_button) ? 0 : 1, 2, button->priv->text);
	if (small_button) {
		type = GNT_COLOR_HIGHLIGHT;
		mvwchgat(widget->window, 0, 0, widget->priv.width, focus ? A_BOLD : A_REVERSE, type, NULL);
	}

	GNTDEBUG;
}

static void
gnt_button_size_request(GntWidget *widget)
{
	GntButton *button = GNT_BUTTON(widget);
	gnt_util_get_text_bound(button->priv->text,
			&widget->priv.width, &widget->priv.height);
	widget->priv.width += 4;
	if (!GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_NO_BORDER))
		widget->priv.height += 2;
}

static void
gnt_button_map(GntWidget *widget)
{
	if (widget->priv.width == 0 || widget->priv.height == 0)
		gnt_widget_size_request(widget);
	GNTDEBUG;
}

static gboolean
gnt_button_clicked(GntWidget *widget, GntMouseEvent event, int x, int y)
{
	if (event == GNT_LEFT_MOUSE_DOWN) {
		gnt_widget_activate(widget);
		return TRUE;
	}
	return FALSE;
}

static void
gnt_button_destroy(GntWidget *widget)
{
	GntButton *button = GNT_BUTTON(widget);
	g_free(button->priv->text);
	g_free(button->priv);
}

static gboolean
button_activate(GntBindable *bind, GList *null)
{
	gnt_widget_activate(GNT_WIDGET(bind));
	return TRUE;
}

static void
gnt_button_class_init(GntWidgetClass *klass)
{
	char *style;
	GntBindableClass *bindable = GNT_BINDABLE_CLASS(klass);

	parent_class = GNT_WIDGET_CLASS(klass);
	parent_class->draw = gnt_button_draw;
	parent_class->map = gnt_button_map;
	parent_class->size_request = gnt_button_size_request;
	parent_class->clicked = gnt_button_clicked;
	parent_class->destroy = gnt_button_destroy;

	style = gnt_style_get_from_name(NULL, "small-button");
	small_button = gnt_style_parse_bool(style);
	g_free(style);

	gnt_bindable_class_register_action(bindable, "activate", button_activate,
				GNT_KEY_ENTER, NULL);
	gnt_style_read_actions(G_OBJECT_CLASS_TYPE(klass), GNT_BINDABLE_CLASS(klass));
}

static void
gnt_button_init(GTypeInstance *instance, gpointer class)
{
	GntWidget *widget = GNT_WIDGET(instance);
	GntButton *button = GNT_BUTTON(instance);
	button->priv = g_new0(GntButtonPriv, 1);

	widget->priv.minw = 4;
	widget->priv.minh = small_button ? 1 : 3;
	if (small_button)
		GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_NO_BORDER | GNT_WIDGET_NO_SHADOW);
	GNT_WIDGET_UNSET_FLAGS(widget, GNT_WIDGET_GROW_X | GNT_WIDGET_GROW_Y);
	GNTDEBUG;
}

/******************************************************************************
 * GntButton API
 *****************************************************************************/
GType
gnt_button_get_gtype(void) {
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(GntButtonClass),
			NULL,					/* base_init		*/
			NULL,					/* base_finalize	*/
			(GClassInitFunc)gnt_button_class_init,
			NULL,					/* class_finalize	*/
			NULL,					/* class_data		*/
			sizeof(GntButton),
			0,						/* n_preallocs		*/
			gnt_button_init,			/* instance_init	*/
			NULL					/* value_table		*/
		};

		type = g_type_register_static(GNT_TYPE_WIDGET,
									  "GntButton",
									  &info, 0);
	}

	return type;
}

GntWidget *gnt_button_new(const char *text)
{
	GntWidget *widget = g_object_new(GNT_TYPE_BUTTON, NULL);
	GntButton *button = GNT_BUTTON(widget);

	button->priv->text = gnt_util_onscreen_fit_string(text, -1);
	gnt_widget_set_take_focus(widget, TRUE);

	return widget;
}

