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
 * SECTION:gtksession
 * @section_id: pidgin-gtksession
 * @short_description: <filename>gtksession.h</filename>
 * @title: X Windows Session Management
 */

#ifndef _PIDGINSESSION_H_
#define _PIDGINSESSION_H_

G_BEGIN_DECLS

/**************************************************************************/
/* X Windows session subsystem                                            */
/**************************************************************************/

/**
 * pidgin_session_init:
 * @argv0:       The first argument passed into the program.  This
 *                    will be the name of the executable, e.g. 'purple'
 * @previous_id: An optional session ID to use.  This can be NULL.
 * @config_dir:  The path to the configuration directory used by
 *                    this instance of this program, e.g. '/home/user/.purple'
 *
 * Register this instance of Pidgin with the user's current session
 * manager.
 */
void pidgin_session_init(gchar *argv0, gchar *previous_id, gchar *config_dir);

/**
 * pidgin_session_end:
 *
 * Unregister this instance of Pidgin with the user's current session
 * manager.
 */
void pidgin_session_end(void);

G_END_DECLS

#endif /* _PIDGINSESSION_H_ */
