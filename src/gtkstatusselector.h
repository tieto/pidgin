/**
 * @file gtkstatusselector.h Status selector widget
 * @ingroup gtkui
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef _GAIM_GTKSTATUSSELECTOR_H_
#define _GAIM_GTKSTATUSSELECTOR_H_

#include <gtk/gtkvbox.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GAIM_GTK_TYPE_STATUS_SELECTOR            (gaim_gtk_status_selector_get_type())
#define GAIM_GTK_STATUS_SELECTOR(obj)            (GTK_CHECK_CAST((obj), GAIM_GTK_TYPE_STATUS_SELECTOR, GaimGtkStatusSelector))
#define GAIM_GTK_STATUS_SELECTOR_CLASS(klass)    (GTK_CHECK_CLASS_CAST((klass), GAIM_GTK_TYPE_STATUS_SELECTOR, GaimGtkStatusSelectorClass))
#define GAIM_GTK_IS_STATUS_SELECTOR(obj)         (GTK_CHECK_TYPE((obj), GAIM_GTK_TYPE_STATUS_SELECTOR))
#define GAIM_GTK_IS_STATUS_SELECTOR_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GAIM_GTK_TYPE_STATUS_SELECTOR))

typedef struct _GaimGtkStatusSelector        GaimGtkStatusSelector;
typedef struct _GaimGtkStatusSelectorClass   GaimGtkStatusSelectorClass;
typedef struct _GaimGtkStatusSelectorPrivate GaimGtkStatusSelectorPrivate;

struct _GaimGtkStatusSelector
{
	GtkVBox parent_object;

	GaimGtkStatusSelectorPrivate *priv;

	void (*_gtk_reserved1)(void);
	void (*_gtk_reserved2)(void);
	void (*_gtk_reserved3)(void);
	void (*_gtk_reserved4)(void);
};

struct _GaimGtkStatusSelectorClass
{
	GtkVBoxClass parent_class;

	void (*_gtk_reserved1)(void);
	void (*_gtk_reserved2)(void);
	void (*_gtk_reserved3)(void);
	void (*_gtk_reserved4)(void);
};


/**
 * Returns the status selector widget's GType.
 *
 * @return GaimGtkStatusSelector's GType.
 */
GType gaim_gtk_status_selector_get_type(void);

/**
 * Creates a new status selector widget.
 *
 * @return The new status selector widget.
 */
GtkWidget *gaim_gtk_status_selector_new(void);

#ifdef __cplusplus
}
#endif

#endif /* _GAIM_GTKSTATUSSELECTOR_H_ */
