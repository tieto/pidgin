/*
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
/**
 * SECTION:gntprogressbar
 * @section_id: libgnt-gntprogressbar
 * @short_description: <filename>gntprogressbar.h</filename>
 * @title: Progress Bar
 */

#ifndef GNT_PROGRESS_BAR_H
#define GNT_PROGRESS_BAR_H

#include "gnt.h"
#include "gntwidget.h"

#define GNT_TYPE_PROGRESS_BAR          (gnt_progress_bar_get_gtype ())
#define GNT_PROGRESS_BAR(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), GNT_TYPE_PROGRESS_BAR, GntProgressBar))
#define GNT_PROGRESS_BAR_CLASS(k)      (G_TYPE_CHECK_CLASS_CAST ((k), GNT_TYPE_PROGRESS_BAR, GntProgressBarClass))
#define GNT_IS_PROGRESS_BAR(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), GNT_TYPE_PROGRESS_BAR))
#define GNT_IS_PROGRESS_BAR_CLASS(k)   (G_TYPE_CHECK_CLASS_TYPE ((k), GNT_TYPE_PROGRESS_BAR))
#define GNT_PROGRESS_BAR_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), GNT_TYPE_PROGRESS_BAR, GntProgressBarClass))

typedef enum
{
   GNT_PROGRESS_LEFT_TO_RIGHT,
   GNT_PROGRESS_RIGHT_TO_LEFT,
   GNT_PROGRESS_BOTTOM_TO_TOP,
   GNT_PROGRESS_TOP_TO_BOTTOM,
} GntProgressBarOrientation;

typedef struct _GntProgressBar GntProgressBar;

typedef struct _GntProgressBarClass
{
   GntWidgetClass parent;

	/*< private >*/
   void (*gnt_reserved1)(void);
   void (*gnt_reserved2)(void);
   void (*gnt_reserved3)(void);
   void (*gnt_reserved4)(void);
} GntProgressBarClass;

G_BEGIN_DECLS

/**
 * gnt_progress_bar_get_gtype:
 *
 * Get the GType for GntProgressBar
 * Returns: The GType for GntProrgressBar
 */
GType
gnt_progress_bar_get_gtype (void);

/**
 * gnt_progress_bar_new:
 *
 * Create a new GntProgressBar
 * Returns: The new GntProgressBar
 */
GntWidget *
gnt_progress_bar_new (void);

/**
 * gnt_progress_bar_set_fraction:
 * @pbar: The GntProgressBar
 * @fraction: The value between 0 and 1 to display
 *
 * Set the progress for a progress bar
 */
void
gnt_progress_bar_set_fraction (GntProgressBar *pbar, gdouble fraction);

/**
 * gnt_progress_bar_set_orientation:
 * @pbar: The GntProgressBar
 * @orientation: The orientation to use
 *
 * Set the orientation for a progress bar
 */
void
gnt_progress_bar_set_orientation (GntProgressBar *pbar, GntProgressBarOrientation orientation);

/**
 * gnt_progress_bar_set_show_progress:
 * @pbar: The GntProgressBar
 * @show: A boolean indicating if the value is shown
 *
 * Controls whether the progress value is shown
 */
void
gnt_progress_bar_set_show_progress (GntProgressBar *pbar, gboolean show);

/**
 * gnt_progress_bar_get_fraction:
 * @pbar: The GntProgressBar
 *
 * Get the progress that is displayed
 *
 * Returns: The progress displayed as a value between 0 and 1
 */
gdouble
gnt_progress_bar_get_fraction (GntProgressBar *pbar);

/**
 * gnt_progress_bar_get_orientation:
 * @pbar: The GntProgressBar
 *
 * Get the orientation for the progress bar
 *
 * Returns: The current orientation of the progress bar
 */
GntProgressBarOrientation
gnt_progress_bar_get_orientation (GntProgressBar *pbar);

/**
 * gnt_progress_bar_get_show_progress:
 * @pbar: The GntProgressBar
 *
 * Get a boolean describing if the progress value is shown
 *
 * Returns: %TRUE if the progress value is shown, %FALSE otherwise.
 */
gboolean
gnt_progress_bar_get_show_progress (GntProgressBar *pbar);

G_END_DECLS

#endif /* GNT_PROGRESS_BAR_H */
