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
 **/

#include "gntinternal.h"
#include "gntprogressbar.h"
#include "gntutils.h"

#include <string.h>

typedef struct _GntProgressBarPrivate
{
	gdouble fraction;
	gboolean show_value;
	GntProgressBarOrientation orientation;
} GntProgressBarPrivate;

struct _GntProgressBar
{
	GntWidget parent;
#if !GLIB_CHECK_VERSION(2,4,0)
	GntProgressBarPrivate priv;
#endif
};

#if GLIB_CHECK_VERSION(2,4,0)
#define GNT_PROGRESS_BAR_GET_PRIVATE(o)   (G_TYPE_INSTANCE_GET_PRIVATE ((o), GNT_TYPE_PROGRESS_BAR, GntProgressBarPrivate))
#else
#define GNT_PROGRESS_BAR_GET_PRIVATE(o)   &(GNT_PROGRESS_BAR(o)->priv)
#endif

static GntWidgetClass *parent_class = NULL;


static void
gnt_progress_bar_draw (GntWidget *widget)
{
	GntProgressBarPrivate *priv = GNT_PROGRESS_BAR_GET_PRIVATE (GNT_PROGRESS_BAR (widget));
	gchar progress[8];
	gint start, end, i, pos;
	int color;

	g_snprintf (progress, sizeof (progress), "%.1f%%", priv->fraction * 100);
	color = gnt_color_pair(GNT_COLOR_NORMAL);

	switch (priv->orientation) {
		case GNT_PROGRESS_LEFT_TO_RIGHT:
		case GNT_PROGRESS_RIGHT_TO_LEFT:
			start = (priv->orientation == GNT_PROGRESS_LEFT_TO_RIGHT ? 0 : (1.0 - priv->fraction) * widget->priv.width);
			end = (priv->orientation == GNT_PROGRESS_LEFT_TO_RIGHT ? widget->priv.width * priv->fraction : widget->priv.width);

			/* background */
			for (i = 0; i < widget->priv.height; i++)
				mvwhline (widget->window, i, 0, ' ' | color, widget->priv.width);

			/* foreground */
			for (i = 0; i < widget->priv.height; i++)
				mvwhline (widget->window, i, start, ACS_CKBOARD | color | A_REVERSE, end);

			/* text */
			if (priv->show_value) {
				pos = widget->priv.width / 2 - strlen (progress) / 2;
				for (i = 0; i < progress[i]; i++, pos++) {
					wattrset (widget->window, color | ((pos < start || pos > end) ? A_NORMAL : A_REVERSE));
					mvwprintw (widget->window, widget->priv.height / 2, pos, "%c", progress[i]);
				}
				wattrset (widget->window, color);
			}

			break;
		case GNT_PROGRESS_TOP_TO_BOTTOM:
		case GNT_PROGRESS_BOTTOM_TO_TOP:
			start = (priv->orientation == GNT_PROGRESS_TOP_TO_BOTTOM ? 0 : (1.0 - priv->fraction) * widget->priv.height);
			end = (priv->orientation == GNT_PROGRESS_TOP_TO_BOTTOM ? widget->priv.height * priv->fraction : widget->priv.height);

			/* background */
			for (i = 0; i < widget->priv.width; i++)
				mvwvline (widget->window, 0, i, ' ' | color, widget->priv.height);

			/* foreground */
			for (i = 0; i < widget->priv.width; i++)
				mvwvline (widget->window, start, i, ACS_CKBOARD | color | A_REVERSE, end);

			/* text */
			if (priv->show_value) {
				pos = widget->priv.height / 2 - strlen (progress) / 2;
				for (i = 0; i < progress[i]; i++, pos++) {
					wattrset (widget->window, color | ((pos < start || pos > end) ? A_NORMAL : A_REVERSE));
					mvwprintw (widget->window, pos, widget->priv.width / 2, "%c\n", progress[i]);
				}
				wattrset (widget->window, color);
			}

			break;
		default:
			g_assert_not_reached ();
	}
}

static void
gnt_progress_bar_size_request (GntWidget *widget)
{
	gnt_widget_set_size (widget, widget->priv.minw, widget->priv.minh);
}

static void
gnt_progress_bar_class_init (gpointer klass, gpointer class_data)
{
	GObjectClass *g_class = G_OBJECT_CLASS (klass);

	parent_class = GNT_WIDGET_CLASS (klass);

#if GLIB_CHECK_VERSION(2,4,0)
	g_type_class_add_private (g_class, sizeof (GntProgressBarPrivate));
#endif

	parent_class->draw = gnt_progress_bar_draw;
	parent_class->size_request = gnt_progress_bar_size_request;
}

static void
gnt_progress_bar_init (GTypeInstance *instance, gpointer g_class)
{
	GntWidget *widget = GNT_WIDGET (instance);
	GntProgressBarPrivate *priv = GNT_PROGRESS_BAR_GET_PRIVATE (GNT_PROGRESS_BAR (widget));

	gnt_widget_set_take_focus (widget, FALSE);
	GNT_WIDGET_SET_FLAGS (widget, GNT_WIDGET_NO_BORDER | GNT_WIDGET_NO_SHADOW | GNT_WIDGET_GROW_X);

	widget->priv.minw = 8;
	widget->priv.minh = 1;

	priv->show_value = TRUE;
}

GType
gnt_progress_bar_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (GntProgressBarClass),
			NULL,                         /* base_init */
			NULL,                         /* base_finalize */
			gnt_progress_bar_class_init,  /* class_init */
			NULL,                         /* class_finalize */
			NULL,                         /* class_data */
			sizeof (GntProgressBar),
			0,                            /* n_preallocs */
			gnt_progress_bar_init,        /* instance_init */
			NULL                          /* value_table */
		};

		type = g_type_register_static (GNT_TYPE_WIDGET, "GntProgressBar", &info, 0);
	}

	return type;
}

GntWidget *
gnt_progress_bar_new (void)
{
	GntWidget *widget = g_object_new (GNT_TYPE_PROGRESS_BAR, NULL);
	return widget;
}

void
gnt_progress_bar_set_fraction (GntProgressBar *pbar, gdouble fraction)
{
	GntProgressBarPrivate *priv = GNT_PROGRESS_BAR_GET_PRIVATE (pbar);

	if (fraction > 1.0)
		priv->fraction = 1.0;
	else if (fraction < 0.0)
		priv->fraction = 0.0;
	else
		priv->fraction = fraction;

	if ((GNT_WIDGET_FLAGS(pbar) & GNT_WIDGET_MAPPED))
		gnt_widget_draw(GNT_WIDGET(pbar));
}

void
gnt_progress_bar_set_orientation (GntProgressBar *pbar,
		GntProgressBarOrientation orientation)
{
	GntProgressBarPrivate *priv = GNT_PROGRESS_BAR_GET_PRIVATE (pbar);
	GntWidget *widget = GNT_WIDGET(pbar);

	priv->orientation = orientation;
	if (orientation == GNT_PROGRESS_LEFT_TO_RIGHT ||
			orientation == GNT_PROGRESS_RIGHT_TO_LEFT) {
		GNT_WIDGET_SET_FLAGS(pbar, GNT_WIDGET_GROW_X);
		GNT_WIDGET_UNSET_FLAGS(pbar, GNT_WIDGET_GROW_Y);
		widget->priv.minw = 8;
		widget->priv.minh = 1;
	} else {
		GNT_WIDGET_UNSET_FLAGS(pbar, GNT_WIDGET_GROW_X);
		GNT_WIDGET_SET_FLAGS(pbar, GNT_WIDGET_GROW_Y);
		widget->priv.minw = 1;
		widget->priv.minh = 8;
	}

	if ((GNT_WIDGET_FLAGS(pbar) & GNT_WIDGET_MAPPED))
		gnt_widget_draw(GNT_WIDGET(pbar));
}

void
gnt_progress_bar_set_show_progress (GntProgressBar *pbar, gboolean show)
{
	GntProgressBarPrivate *priv = GNT_PROGRESS_BAR_GET_PRIVATE (pbar);
	priv->show_value = show;
}

gdouble
gnt_progress_bar_get_fraction (GntProgressBar *pbar)
{
	GntProgressBarPrivate *priv = GNT_PROGRESS_BAR_GET_PRIVATE (pbar);
	return priv->fraction;
}

GntProgressBarOrientation
gnt_progress_bar_get_orientation (GntProgressBar *pbar)
{
	GntProgressBarPrivate *priv = GNT_PROGRESS_BAR_GET_PRIVATE (pbar);
	return priv->orientation;
}

gboolean
gnt_progress_bar_get_show_progress (GntProgressBar *pbar)
{
	GntProgressBarPrivate *priv = GNT_PROGRESS_BAR_GET_PRIVATE (pbar);
	return priv->show_value;
}

