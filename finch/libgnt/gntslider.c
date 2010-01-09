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

#include "gntinternal.h"
#include "gntcolors.h"
#include "gntkeys.h"
#include "gntslider.h"
#include "gntstyle.h"

enum
{
	SIG_VALUE_CHANGED,
	SIGS,
};

static guint signals[SIGS] = { 0 };

static GntWidgetClass *parent_class = NULL;

/* returns TRUE if the value was changed */
static gboolean
sanitize_value(GntSlider *slider)
{
	if (slider->current < slider->min)
		slider->current = slider->min;
	else if (slider->current > slider->max)
		slider->current = slider->max;
	else
		return FALSE;
	return TRUE;
}

static void
redraw_slider(GntSlider *slider)
{
	GntWidget *widget = GNT_WIDGET(slider);
	if (GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_MAPPED))
		gnt_widget_draw(widget);
}

static void
slider_value_changed(GntSlider *slider)
{
	g_signal_emit(slider, signals[SIG_VALUE_CHANGED], 0, slider->current);
}

static void
gnt_slider_draw(GntWidget *widget)
{
	GntSlider *slider = GNT_SLIDER(widget);
	int attr = 0;
	int position, size = 0;

	if (slider->vertical)
		size = widget->priv.height;
	else
		size = widget->priv.width;

	if (gnt_widget_has_focus(widget))
		attr |= GNT_COLOR_HIGHLIGHT;
	else
		attr |= GNT_COLOR_HIGHLIGHT_D;

	if (slider->max != slider->min)
		position = ((size - 1) * (slider->current - slider->min)) / (slider->max - slider->min);
	else
		position = 0;
	if (slider->vertical) {
		mvwvline(widget->window, size-position, 0, ACS_VLINE | gnt_color_pair(GNT_COLOR_NORMAL) | A_BOLD,
				position);
		mvwvline(widget->window, 0, 0, ACS_VLINE | gnt_color_pair(GNT_COLOR_NORMAL),
				size-position);
	} else {
		mvwhline(widget->window, 0, 0, ACS_HLINE | gnt_color_pair(GNT_COLOR_NORMAL) | A_BOLD,
				position);
		mvwhline(widget->window, 0, position, ACS_HLINE | gnt_color_pair(GNT_COLOR_NORMAL),
				size - position);
	}

	mvwaddch(widget->window,
			slider->vertical ? (size - position - 1) : 0,
			slider->vertical ? 0 : position,
			ACS_CKBOARD | gnt_color_pair(attr));
}

static void
gnt_slider_size_request(GntWidget *widget)
{
	if (GNT_SLIDER(widget)->vertical) {
		widget->priv.width = 1;
		widget->priv.height = 5;
	} else {
		widget->priv.width = 5;
		widget->priv.height = 1;
	}
}

static void
gnt_slider_map(GntWidget *widget)
{
	if (widget->priv.width == 0 || widget->priv.height == 0)
		gnt_widget_size_request(widget);
	GNTDEBUG;
}

static gboolean
step_back(GntBindable *bindable, GList *null)
{
	GntSlider *slider = GNT_SLIDER(bindable);
	gnt_slider_advance_step(slider, -1);
	return TRUE;
}

static gboolean
small_step_back(GntBindable *bindable, GList *null)
{
	GntSlider *slider = GNT_SLIDER(bindable);
	gnt_slider_set_value(slider, slider->current - slider->smallstep);
	return TRUE;
}

static gboolean
large_step_back(GntBindable *bindable, GList *null)
{
	GntSlider *slider = GNT_SLIDER(bindable);
	gnt_slider_set_value(slider, slider->current - slider->largestep);
	return TRUE;
}

static gboolean
step_forward(GntBindable *bindable, GList *list)
{
	GntSlider *slider = GNT_SLIDER(bindable);
	gnt_slider_advance_step(slider, 1);
	return TRUE;
}

static gboolean
small_step_forward(GntBindable *bindable, GList *null)
{
	GntSlider *slider = GNT_SLIDER(bindable);
	gnt_slider_set_value(slider, slider->current + slider->smallstep);
	return TRUE;
}

static gboolean
large_step_forward(GntBindable *bindable, GList *null)
{
	GntSlider *slider = GNT_SLIDER(bindable);
	gnt_slider_set_value(slider, slider->current + slider->largestep);
	return TRUE;
}

static gboolean
move_min_value(GntBindable *bindable, GList *null)
{
	GntSlider *slider = GNT_SLIDER(bindable);
	gnt_slider_set_value(slider, slider->min);
	return TRUE;
}

static gboolean
move_max_value(GntBindable *bindable, GList *null)
{
	GntSlider *slider = GNT_SLIDER(bindable);
	gnt_slider_set_value(slider, slider->max);
	return TRUE;
}

static void
gnt_slider_class_init(GntSliderClass *klass)
{
	GntBindableClass *bindable = GNT_BINDABLE_CLASS(klass);
	parent_class = GNT_WIDGET_CLASS(klass);
	parent_class->draw = gnt_slider_draw;
	parent_class->map = gnt_slider_map;
	parent_class->size_request = gnt_slider_size_request;

	klass->changed = NULL;

	signals[SIG_VALUE_CHANGED] =
		g_signal_new("changed",
		             G_TYPE_FROM_CLASS(klass),
		             G_SIGNAL_RUN_LAST,
		             G_STRUCT_OFFSET(GntSliderClass, changed),
		             NULL, NULL,
		             g_cclosure_marshal_VOID__INT,
		             G_TYPE_NONE, 1, G_TYPE_INT);

	gnt_bindable_class_register_action(bindable, "step-backward", step_back, GNT_KEY_LEFT, NULL);
	gnt_bindable_register_binding(bindable, "step-backward", GNT_KEY_DOWN, NULL);
	gnt_bindable_class_register_action(bindable, "step-forward", step_forward, GNT_KEY_RIGHT, NULL);
	gnt_bindable_register_binding(bindable, "step-forward", GNT_KEY_UP, NULL);
	gnt_bindable_class_register_action(bindable, "small-step-backward", small_step_back, GNT_KEY_CTRL_LEFT, NULL);
	gnt_bindable_register_binding(bindable, "small-step-backward", GNT_KEY_CTRL_DOWN, NULL);
	gnt_bindable_class_register_action(bindable, "small-step-forward", small_step_forward, GNT_KEY_CTRL_RIGHT, NULL);
	gnt_bindable_register_binding(bindable, "small-step-forward", GNT_KEY_CTRL_UP, NULL);
	gnt_bindable_class_register_action(bindable, "large-step-backward", large_step_back, GNT_KEY_PGDOWN, NULL);
	gnt_bindable_class_register_action(bindable, "large-step-forward", large_step_forward, GNT_KEY_PGUP, NULL);
	gnt_bindable_class_register_action(bindable, "min-value", move_min_value, GNT_KEY_HOME, NULL);
	gnt_bindable_class_register_action(bindable, "max-value", move_max_value, GNT_KEY_END, NULL);

	gnt_style_read_actions(G_OBJECT_CLASS_TYPE(klass), GNT_BINDABLE_CLASS(klass));
}

static void
gnt_slider_init(GTypeInstance *instance, gpointer class)
{
	GntWidget *widget = GNT_WIDGET(instance);
	GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_NO_SHADOW | GNT_WIDGET_NO_BORDER | GNT_WIDGET_CAN_TAKE_FOCUS);
	widget->priv.minw = 1;
	widget->priv.minh = 1;
	GNTDEBUG;
}

/******************************************************************************
 * GntSlider API
 *****************************************************************************/
GType
gnt_slider_get_gtype(void)
{
	static GType type = 0;

	if(type == 0)
	{
		static const GTypeInfo info = {
			sizeof(GntSliderClass),
			NULL,                   /* base_init        */
			NULL,                   /* base_finalize    */
			(GClassInitFunc)gnt_slider_class_init,
			NULL,                   /* class_finalize   */
			NULL,                   /* class_data       */
			sizeof(GntSlider),
			0,                      /* n_preallocs      */
			gnt_slider_init,        /* instance_init    */
			NULL                    /* value_table      */
		};

		type = g_type_register_static(GNT_TYPE_WIDGET,
		                              "GntSlider",
		                              &info, 0);
	}

	return type;
}

GntWidget *gnt_slider_new(gboolean vertical, int max, int min)
{
	GntWidget *widget = g_object_new(GNT_TYPE_SLIDER, NULL);
	GntSlider *slider = GNT_SLIDER(widget);

	slider->vertical = vertical;

	if (vertical) {
		GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_GROW_Y);
	} else {
		GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_GROW_X);
	}

	gnt_slider_set_range(slider, max, min);
	slider->step = 1;

	return widget;
}

void gnt_slider_set_value(GntSlider *slider, int value)
{
	int old;
	if (slider->current == value)
		return;
	old = slider->current;
	slider->current = value;
	sanitize_value(slider);
	if (old == slider->current)
		return;
	redraw_slider(slider);
	slider_value_changed(slider);
}

int gnt_slider_get_value(GntSlider *slider)
{
	return slider->current;
}

int gnt_slider_advance_step(GntSlider *slider, int steps)
{
	gnt_slider_set_value(slider, slider->current + steps * slider->step);
	return slider->current;
}

void gnt_slider_set_step(GntSlider *slider, int step)
{
	slider->step = step;
}

void gnt_slider_set_small_step(GntSlider *slider, int step)
{
	slider->smallstep = step;
}

void gnt_slider_set_large_step(GntSlider *slider, int step)
{
	slider->largestep = step;
}

void gnt_slider_set_range(GntSlider *slider, int max, int min)
{
	slider->max = MAX(max, min);
	slider->min = MIN(max, min);
	sanitize_value(slider);
}

static void
update_label(GntSlider *slider, int current_value, GntLabel *label)
{
	char value[256];
	g_snprintf(value, sizeof(value), "%d/%d", current_value, slider->max);
	gnt_label_set_text(label, value);
}

void gnt_slider_reflect_label(GntSlider *slider, GntLabel *label)
{
	g_signal_connect(G_OBJECT(slider), "changed", G_CALLBACK(update_label), label);
}

