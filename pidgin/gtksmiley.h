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
/**
 * SECTION:gtksmiley
 * @section_id: pidgin-gtksmiley
 * @short_description: <filename>gtksmiley.h</filename>
 * @title: Custom Smiley API
 */

#ifndef PIDGIN_GTKSMILEY_H
#define PIDGIN_GTKSMILEY_H

#include "smiley.h"

typedef struct _PidginSmiley PidginSmiley;

G_BEGIN_DECLS

/**
 * pidgin_smiley_add_to_list:
 * @smiley:	The smiley to be added.
 *
 * Add a PurpleSmiley to the GtkWebViewSmiley's list to be able to use it
 * in pidgin
 */
void pidgin_smiley_add_to_list(PurpleSmiley *smiley);

/**
 * pidgin_smiley_del_from_list:
 * @smiley:	The smiley to be deleted.
 *
 * Delete a PurpleSmiley from the GtkWebViewSmiley's list
 */
void pidgin_smiley_del_from_list(PurpleSmiley *smiley);

/**
 * pidgin_smileys_init:
 *
 * Load the GtkWebViewSmiley list
 */
void pidgin_smileys_init(void);

/**
 * pidgin_smileys_uninit:
 *
 * Uninit the GtkWebViewSmiley list
 */
void pidgin_smileys_uninit(void);

/**
 * pidgin_smileys_get_all:
 *
 * Returns a GSList with the GtkWebViewSmiley of each custom smiley
 *
 * Returns: (transfer none): A GtkWebViewSmiley list
 */
GSList *pidgin_smileys_get_all(void);

/******************************************************************************
 * Smiley Manager
 *****************************************************************************/
/**
 * pidgin_smiley_manager_show:
 *
 * Displays the Smiley Manager Window
 */
void pidgin_smiley_manager_show(void);

/**
 * pidgin_smiley_edit:
 * @widget: The parent widget to be linked or %NULL
 * @smiley: The PurpleSmiley to be edited, or %NULL for a new smiley
 *
 * Shows an editor for a smiley.
 *
 * @see pidgin_smiley_editor_set_shortcut
 * @see pidgin_smiley_editor_set_image
 *
 * Returns: The smiley add dialog
 */
PidginSmiley *pidgin_smiley_edit(GtkWidget *widget, PurpleSmiley *smiley);

/**
 * pidgin_smiley_editor_set_shortcut:
 * @editor: A smiley editor dialog (created by pidgin_smiley_edit)
 * @shortcut: The shortcut to set
 *
 * Set the shortcut in a smiley add dialog
 */
void pidgin_smiley_editor_set_shortcut(PidginSmiley *editor, const gchar *shortcut);

/**
 * pidgin_smiley_editor_set_image:
 * @editor: A smiley editor dialog
 * @image: A GdkPixbuf image
 *
 * Set the image in a smiley add dialog
 */
void pidgin_smiley_editor_set_image(PidginSmiley *editor, GdkPixbuf *image);

/**
 * pidgin_smiley_editor_set_data:
 * @editor: A smiley editor dialog
 * @data: A pointer to smiley's data
 * @datasize: The size of smiley's data
 *
 * Sets the image data in a smiley add dialog
 */
void pidgin_smiley_editor_set_data(PidginSmiley *editor, gpointer data, gsize datasize);

G_END_DECLS

#endif /* PIDGIN_GTKSMILEY_H */
