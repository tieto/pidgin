/**
 * @file gtkpounce.h GTK+ buddy pounce API
 *
 * gaim
 *
 * Copyright (C) 2003, Christian Hammond <chipx86@gnupdate.org>
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
 *
 */
#ifndef _GAIM_GTK_POUNCE_H_
#define _GAIM_GTK_POUNCE_H_

#include "pounce.h"

/**
 * Types of actions that gaim's GTK+ interface supports.
 */
typedef enum
{
	GAIM_GTKPOUNCE_NONE       = 0x00, /**< No action.          */
	GAIM_GTKPOUNCE_OPEN_WIN   = 0x01, /**< Open an IM window.  */
	GAIM_GTKPOUNCE_POPUP      = 0x02, /**< Popup notification. */
	GAIM_GTKPOUNCE_SEND_MSG   = 0x04, /**< Send a message.     */
	GAIM_GTKPOUNCE_EXEC_CMD   = 0x08, /**< Execute a command.  */
	GAIM_GTKPOUNCE_PLAY_SOUND = 0x10  /**< Play a sound.       */

} GaimGtkPounceAction;

/**
 * GTK+ pounce-specific data.
 */
struct gaim_gtkpounce_data
{
	GaimGtkPounceAction actions;  /**< The action(s) for this pounce.      */

	char *message;          /**< The message to send, if
	                             GAIM_GTKPOUNCE_SEND_MSG is in actions.    */
	char *command;          /**< The command to execute, if
	                             GAIM_GTKPOUNCE_EXEC_CMD is in actions.    */
	char *sound;            /**< The sound file to play, if
	                             GAIM_GTKPOUNCE_PLAY_SOUND is in actions.  */

	gboolean save;          /**< If TRUE, the pounce should be saved after
	                             activation.                               */
};

#define GAIM_GTKPOUNCE(pounce) \
	((struct gaim_gtkpounce_data *)gaim_pounce_get_data(pounce))

/**
 * The pounce dialog.
 *
 * The structure is opaque, as nobody should be touching anything inside of
 * it.
 */
struct gaim_gtkpounce_dialog;

/**
 * Creates a GTK-specific pounce.
 *
 * @param pouncer The account that will pounce.
 * @param pouncee The buddy to pounce on.
 * @param events  The event(s) to pounce on.
 * @param actions The actions.
 * @param message The optional message to send.
 * @param command The optional command to execute.
 * @param sound   The optional sound to play.
 * @param save    Whether or not to save the data.
 *
 * @return The new buddy pounce.
 */
struct gaim_pounce *gaim_gtkpounce_new(struct gaim_account *pouncer,
									   const char *pouncee,
									   GaimPounceEvent events,
									   GaimGtkPounceAction actions,
									   const char *message,
									   const char *command,
									   const char *sound,
									   gboolean save);

/**
 * Displays a New Buddy Pounce or Edit Buddy Pounce dialog.
 *
 * @param buddy      The optional buddy to pounce on.
 * @param cur_pounce The current buddy pounce, if editting an existing one.
 */
void gaim_gtkpounce_dialog_show(struct buddy *buddy,
								struct gaim_pounce *cur_pounce);

/**
 * Displays all registered buddy pounces in a menu.
 *
 * @param menu The menu to add to.
 */
void gaim_gtkpounce_menu_build(GtkWidget *menu);

#endif /* _GAIM_GTK_POUNCE_H_ */
