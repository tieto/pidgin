/**
 * @file mediamanager.h Media Manager API
 * @ingroup core
 */

/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
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

#ifndef _PURPLE_MEDIA_MANAGER_H_
#define _PURPLE_MEDIA_MANAGER_H_

#include <glib.h>
#include <glib-object.h>

/** @copydoc _PurpleMediaManager */
typedef struct _PurpleMediaManager PurpleMediaManager;
/** @copydoc _PurpleMediaManagerClass */
typedef struct _PurpleMediaManagerClass PurpleMediaManagerClass;

typedef struct _PurpleMediaManagerPrivate PurpleMediaManagerPrivate;

#include "account.h"
#include "media.h"

#define PURPLE_TYPE_MEDIA_MANAGER            (purple_media_manager_get_type())
#define PURPLE_MEDIA_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_MEDIA_MANAGER, PurpleMediaManager))
#define PURPLE_MEDIA_MANAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_MEDIA_MANAGER, PurpleMediaManagerClass))
#define PURPLE_IS_MEDIA_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_MEDIA_MANAGER))
#define PURPLE_IS_MEDIA_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_MEDIA_MANAGER))
#define PURPLE_MEDIA_MANAGER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_MEDIA_MANAGER, PurpleMediaManagerClass))

/** The media manager's data. */
struct _PurpleMediaManager
{
	GObject parent;                  /**< The parent of this manager. */

	/*< private >*/
	PurpleMediaManagerPrivate *priv; /**< Private data for the manager. */
};

/** The media manager class. */
struct _PurpleMediaManagerClass
{
	GObjectClass parent_class;       /**< The parent class. */

	/*< private >*/
	void (*purple_reserved1)(void);
	void (*purple_reserved2)(void);
	void (*purple_reserved3)(void);
	void (*purple_reserved4)(void);
};

G_BEGIN_DECLS

/**************************************************************************/
/** @name Media Manager API                                              */
/**************************************************************************/
/*@{*/

/**
 * Gets the media manager's GType.
 *
 * Returns: The media manager's GType.
 */
GType purple_media_manager_get_type(void);

/**
 * Gets the "global" media manager object. It's created if it doesn't already exist.
 *
 * Returns: The "global" instance of the media manager object.
 */
PurpleMediaManager *purple_media_manager_get(void);

/**
 * Creates a media session.
 *
 * @manager: The media manager to create the session under.
 * @account: The account to create the session on.
 * @conference_type: The conference type to feed into Farsight2.
 * @remote_user: The remote user to initiate the session with.
 * @initiator: TRUE if the local user is the initiator of this media call, FALSE otherwise.
 *
 * Returns: A newly created media session.
 */
PurpleMedia *purple_media_manager_create_media(PurpleMediaManager *manager,
						PurpleAccount *account,
						const char *conference_type,
						const char *remote_user,
						gboolean initiator);

/**
 * Gets all of the media sessions.
 *
 * @manager: The media manager to get all of the sessions from.
 *
 * Returns: A list of all the media sessions.
 */
GList *purple_media_manager_get_media(PurpleMediaManager *manager);

/**
 * Gets all of the media sessions for a given account.
 *
 * @manager: The media manager to get the sessions from.
 * @account: The account the sessions are on.
 *
 * Returns: A list of the media sessions on the given account.
 */
GList *purple_media_manager_get_media_by_account(
		PurpleMediaManager *manager, PurpleAccount *account);

/**
 * Removes a media session from the media manager.
 *
 * @manager: The media manager to remove the media session from.
 * @media: The media session to remove.
 */
void
purple_media_manager_remove_media(PurpleMediaManager *manager,
				  PurpleMedia *media);

/**
 * Signals that output windows should be created for the chosen stream.
 *
 * This shouldn't be called outside of mediamanager.c and media.c
 *
 * @manager: Manager the output windows are registered with.
 * @media: Media session the output windows are registered for.
 * @session_id: The session the output windows are registered with.
 * @participant: The participant the output windows are registered with.
 *
 * Returns: TRUE if it succeeded, FALSE if it failed.
 */
gboolean purple_media_manager_create_output_window(
		PurpleMediaManager *manager, PurpleMedia *media,
		const gchar *session_id, const gchar *participant);

/**
 * Registers a video output window to be created for a given stream.
 *
 * @manager: The manager to register the output window with.
 * @media: The media instance to find the stream in.
 * @session_id: The session the stream is associated with.
 * @participant: The participant the stream is associated with.
 * @window_id: The window ID to embed the video in.
 *
 * Returns: A unique ID to the registered output window, 0 if it failed.
 */
gulong purple_media_manager_set_output_window(PurpleMediaManager *manager,
		PurpleMedia *media, const gchar *session_id,
		const gchar *participant, gulong window_id);

/**
 * Remove a previously registerd output window.
 *
 * @manager: The manager the output window was registered with.
 * @output_window_id: The ID of the output window.
 *
 * Returns: TRUE if it found the output window and was successful, else FALSE.
 */
gboolean purple_media_manager_remove_output_window(
		PurpleMediaManager *manager, gulong output_window_id);

/**
 * Remove all output windows for a given conference/session/participant/stream.
 *
 * @manager: The manager the output windows were registered with.
 * @media: The media instance the output windows were registered for.
 * @session_id: The session the output windows were registered for.
 * @participant: The participant the output windows were registered for.
 */
void purple_media_manager_remove_output_windows(
		PurpleMediaManager *manager, PurpleMedia *media,
		const gchar *session_id, const gchar *participant);

/**
 * Sets which media caps the UI supports.
 *
 * @manager: The manager to set the caps on.
 * @caps: The caps to set.
 */
void purple_media_manager_set_ui_caps(PurpleMediaManager *manager,
		PurpleMediaCaps caps);

/**
 * Gets which media caps the UI supports.
 *
 * @manager: The manager to get caps from.
 *
 * Returns: caps The caps retrieved.
 */
PurpleMediaCaps purple_media_manager_get_ui_caps(PurpleMediaManager *manager);

/**
 * Sets which media backend type media objects will use.
 *
 * @manager: The manager to set the caps on.
 * @backend_type: The media backend type to use.
 */
void purple_media_manager_set_backend_type(PurpleMediaManager *manager,
		GType backend_type);

/**
 * Gets which media backend type media objects will use.
 *
 * @manager: The manager to get the media backend type from.
 *
 * Returns: The type of media backend type media objects will use.
 */
GType purple_media_manager_get_backend_type(PurpleMediaManager *manager);

/*}@*/

G_END_DECLS

#endif  /* _PURPLE_MEDIA_MANAGER_H_ */
