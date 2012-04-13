/**
 * @file pidgintooltip.h Pidgin Tooltip API
 * @ingroup pidgin
 */

/* pidgin
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
#ifndef _PIDGIN_TOOLTIP_H_
#define _PIDGIN_TOOLTIP_H_

#include <gtk/gtk.h>

/**
 * @param tipwindow  The window for the tooltip.
 * @param path       The GtkTreePath representing the row under the cursor.
 * @param userdata   The userdata set during pidgin_tooltip_setup_for_treeview.
 * @param w          The value of this should be set to the desired width of the tooltip window.
 * @param h          The value of this should be set to the desired height of the tooltip window.
 *
 * @return  @c TRUE if the tooltip was created correctly, @c FALSE otherwise.
 */
typedef gboolean (*PidginTooltipCreateForTree)(GtkWidget *tipwindow,
			GtkTreePath *path, gpointer userdata, int *w, int *h);

/**
 * @param tipwindow  The window for the tooltip.
 * @param userdata   The userdata set during pidgin_tooltip_show.
 * @param w          The value of this should be set to the desired width of the tooltip window.
 * @param h          The value of this should be set to the desired height of the tooltip window.
 *
 * @return  @c TRUE if the tooltip was created correctly, @c FALSE otherwise.
 */
typedef gboolean (*PidginTooltipCreate)(GtkWidget *tipwindow,
			gpointer userdata, int *w, int *h);

/**
 * @param  tipwindow   The window for the tooltip.
 * @param  userdata    The userdata set during pidgin_tooltip_setup_for_treeview or pidgin_tooltip_show.
 *
 * @return  @c TRUE if the tooltip was painted correctly, @c FALSE otherwise.
 */
typedef gboolean (*PidginTooltipPaint)(GtkWidget *tipwindow, gpointer userdata);

G_BEGIN_DECLS

/**
 * Setup tooltip drawing functions for a treeview.
 *
 * @param tree         The treeview
 * @param userdata     The userdata to send to the callback functions
 * @param create_cb    Callback function to create the tooltip for a GtkTreePath
 * @param paint_cb     Callback function to paint the tooltip
 *
 * @return   @c TRUE if the tooltip callbacks were setup correctly.
 */
gboolean pidgin_tooltip_setup_for_treeview(GtkWidget *tree, gpointer userdata,
		PidginTooltipCreateForTree create_cb, PidginTooltipPaint paint_cb);

/**
 * Setup tooltip drawing functions for any widget.
 *
 * @param widget       The widget
 * @param userdata     The userdata to send to the callback functions
 * @param create_cb    Callback function to create the tooltip for the widget
 * @param paint_cb     Callback function to paint the tooltip
 *
 * @return   @c TRUE if the tooltip callbacks were setup correctly.
 */
gboolean pidgin_tooltip_setup_for_widget(GtkWidget *widget, gpointer userdata,
		PidginTooltipCreate create_cb, PidginTooltipPaint paint_cb);

/**
 * Destroy the tooltip.
 */
void pidgin_tooltip_destroy(void);

/**
 * Create and show a tooltip.
 *
 * @param widget      The widget the tooltip is for
 * @param userdata    The userdata to send to the callback functions
 * @param create_cb    Callback function to create the tooltip from the GtkTreePath
 * @param paint_cb     Callback function to paint the tooltip
 */
void pidgin_tooltip_show(GtkWidget *widget, gpointer userdata,
		PidginTooltipCreate create_cb, PidginTooltipPaint paint_cb);

G_END_DECLS

#endif
