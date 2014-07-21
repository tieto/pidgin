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
/**
 * SECTION:mediamanager
 * @section_id: libpurple-mediamanager
 * @short_description: <filename>mediamanager.h</filename>
 * @title: Media Manager Object
 */

#include <glib.h>
#include <glib-object.h>

typedef struct _PurpleMediaManager PurpleMediaManager;
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

/**
 * PurpleMediaManager:
 *
 * The media manager's data.
 */
struct _PurpleMediaManager
{
	GObject parent;

	/*< private >*/
	PurpleMediaManagerPrivate *priv;
};

/**
 * PurpleMediaManagerClass:
 *
 * The media manager class.
 */
struct _PurpleMediaManagerClass
{
	GObjectClass parent_class;

	/*< private >*/
	void (*purple_reserved1)(void);
	void (*purple_reserved2)(void);
	void (*purple_reserved3)(void);
	void (*purple_reserved4)(void);
};

/**
 * PurpleMediaAppDataCallbacks:
 * @readable: Called when the stream has received data and is readable.
 * @writable: Called when the stream has become writable or has stopped being
 * writable.
 *
 * A set of callbacks that can be installed on an Application data session with
 * purple_media_manager_set_application_data_callbacks()
 *
 * Once installed the @readable callback will get called as long as data is
 * available to read, so the data must be read completely.
 * The @writable callback will only be called when the writable state of the
 * stream changes. The @writable argument defines whether the stream has
 * become writable or stopped being writable.
 *
 */
typedef struct {
	void      (*readable)    (PurpleMediaManager *manager, PurpleMedia *media,
		const gchar *session_id, const gchar *participant, gpointer user_data);
	void      (*writable)    (PurpleMediaManager *manager, PurpleMedia *media,
		const gchar *session_id, const gchar *participant, gboolean writable,
		gpointer user_data);
} PurpleMediaAppDataCallbacks;

G_BEGIN_DECLS

/**************************************************************************/
/* Media Manager API                                                      */
/**************************************************************************/

/**
 * purple_media_manager_get_type:
 *
 * Gets the media manager's GType.
 *
 * Returns: The media manager's GType.
 */
GType purple_media_manager_get_type(void);

/**
 * purple_media_manager_get:
 *
 * Gets the "global" media manager object. It's created if it doesn't already exist.
 *
 * Returns: The "global" instance of the media manager object.
 */
PurpleMediaManager *purple_media_manager_get(void);

/**
 * purple_media_manager_create_media:
 * @manager: The media manager to create the session under.
 * @account: The account to create the session on.
 * @conference_type: The conference type to feed into Farsight2.
 * @remote_user: The remote user to initiate the session with.
 * @initiator: TRUE if the local user is the initiator of this media call, FALSE otherwise.
 *
 * Creates a media session.
 *
 * Returns: A newly created media session.
 */
PurpleMedia *purple_media_manager_create_media(PurpleMediaManager *manager,
						PurpleAccount *account,
						const char *conference_type,
						const char *remote_user,
						gboolean initiator);

/**
 * purple_media_manager_get_media:
 * @manager: The media manager to get all of the sessions from.
 *
 * Gets all of the media sessions.
 *
 * Returns: A list of all the media sessions.
 */
GList *purple_media_manager_get_media(PurpleMediaManager *manager);

/**
 * purple_media_manager_get_media_by_account:
 * @manager: The media manager to get the sessions from.
 * @account: The account the sessions are on.
 *
 * Gets all of the media sessions for a given account.
 *
 * Returns: A list of the media sessions on the given account.
 */
GList *purple_media_manager_get_media_by_account(
		PurpleMediaManager *manager, PurpleAccount *account);

/**
 * purple_media_manager_remove_media:
 * @manager: The media manager to remove the media session from.
 * @media: The media session to remove.
 *
 * Removes a media session from the media manager.
 */
void
purple_media_manager_remove_media(PurpleMediaManager *manager,
				  PurpleMedia *media);

/**
 * purple_media_manager_create_private_media:
 * @manager: The media manager to create the session under.
 * @account: The account to create the session on.
 * @conference_type: The conference type to feed into Farsight2.
 * @remote_user: The remote user to initiate the session with.
 * @initiator: TRUE if the local user is the initiator of this media call, FALSE otherwise.
 *
 * Creates a private media session.
 * A private media session is a media session which is private to the caller. It is
 * meant to be used by plugins to create a media session that the front-end does not
 * get notified about. It is useful especially for sessions with a type of
 * PURPLE_MEDIA_APPLICATION which the front-end wouldn't know how to handle.
 *
 * Returns: A newly created media session.
 */
PurpleMedia *purple_media_manager_create_private_media(
                                                PurpleMediaManager *manager,
						PurpleAccount *account,
						const char *conference_type,
						const char *remote_user,
						gboolean initiator);

/**
 * purple_media_manager_get_private_media:
 * @manager: The media manager to get all of the sessions from.
 *
 * Gets all of the private media sessions.
 *
 * Returns: A list of all the private media sessions.
 */
GList *purple_media_manager_get_private_media(PurpleMediaManager *manager);

/**
 * purple_media_manager_get_private_media_by_account:
 * @manager: The media manager to get the sessions from.
 * @account: The account the sessions are on.
 *
 * Gets all of the private media sessions for a given account.
 *
 * Returns: A list of the private media sessions on the given account.
 */
GList *purple_media_manager_get_private_media_by_account(
		PurpleMediaManager *manager, PurpleAccount *account);

/**
 * purple_media_manager_create_output_window:
 * @manager: Manager the output windows are registered with.
 * @media: Media session the output windows are registered for.
 * @session_id: The session the output windows are registered with.
 * @participant: The participant the output windows are registered with.
 *
 * Signals that output windows should be created for the chosen stream.
 *
 * This shouldn't be called outside of mediamanager.c and media.c
 *
 * Returns: TRUE if it succeeded, FALSE if it failed.
 */
gboolean purple_media_manager_create_output_window(
		PurpleMediaManager *manager, PurpleMedia *media,
		const gchar *session_id, const gchar *participant);

/**
 * purple_media_manager_set_output_window:
 * @manager: The manager to register the output window with.
 * @media: The media instance to find the stream in.
 * @session_id: The session the stream is associated with.
 * @participant: The participant the stream is associated with.
 * @window_id: The window ID to embed the video in.
 *
 * Registers a video output window to be created for a given stream.
 *
 * Returns: A unique ID to the registered output window, 0 if it failed.
 */
gulong purple_media_manager_set_output_window(PurpleMediaManager *manager,
		PurpleMedia *media, const gchar *session_id,
		const gchar *participant, gulong window_id);

/**
 * purple_media_manager_remove_output_window:
 * @manager: The manager the output window was registered with.
 * @output_window_id: The ID of the output window.
 *
 * Remove a previously registerd output window.
 *
 * Returns: TRUE if it found the output window and was successful, else FALSE.
 */
gboolean purple_media_manager_remove_output_window(
		PurpleMediaManager *manager, gulong output_window_id);

/**
 * purple_media_manager_remove_output_windows:
 * @manager: The manager the output windows were registered with.
 * @media: The media instance the output windows were registered for.
 * @session_id: The session the output windows were registered for.
 * @participant: The participant the output windows were registered for.
 *
 * Remove all output windows for a given conference/session/participant/stream.
 */
void purple_media_manager_remove_output_windows(
		PurpleMediaManager *manager, PurpleMedia *media,
		const gchar *session_id, const gchar *participant);

/**
 * purple_media_manager_set_ui_caps:
 * @manager: The manager to set the caps on.
 * @caps: The caps to set.
 *
 * Sets which media caps the UI supports.
 */
void purple_media_manager_set_ui_caps(PurpleMediaManager *manager,
		PurpleMediaCaps caps);

/**
 * purple_media_manager_get_ui_caps:
 * @manager: The manager to get caps from.
 *
 * Gets which media caps the UI supports.
 *
 * Returns: caps The caps retrieved.
 */
PurpleMediaCaps purple_media_manager_get_ui_caps(PurpleMediaManager *manager);

/**
 * purple_media_manager_set_backend_type:
 * @manager: The manager to set the caps on.
 * @backend_type: The media backend type to use.
 *
 * Sets which media backend type media objects will use.
 */
void purple_media_manager_set_backend_type(PurpleMediaManager *manager,
		GType backend_type);

/**
 * purple_media_manager_get_backend_type:
 * @manager: The manager to get the media backend type from.
 *
 * Gets which media backend type media objects will use.
 *
 * Returns: The type of media backend type media objects will use.
 */
GType purple_media_manager_get_backend_type(PurpleMediaManager *manager);

/**
 * purple_media_manager_set_application_data_callbacks:
 * @manager: The manager to register the callbacks with.
 * @media: The media instance to register the callbacks with.
 * @session_id: The session to register the callbacks with.
 * @participant: The participant to register the callbacks with.
 * @callbacks: The callbacks to be set on the session.
 * @user_data: a user_data argument for the callbacks.
 * @notify: a destroy notify function.
 *
 * Set callbacks on a session to be called when the stream becomes writable
 * or readable for media sessions of type #PURPLE_MEDIA_APPLICATION
 */
void purple_media_manager_set_application_data_callbacks(
	PurpleMediaManager *manager, PurpleMedia *media, const gchar *session_id,
	const gchar *participant, PurpleMediaAppDataCallbacks *callbacks,
	gpointer user_data, GDestroyNotify notify);

/**
 * purple_media_manager_send_application_data:
 * @manager: The manager to send data with.
 * @media: The media instance to which the session belongs.
 * @session_id: The session to send data to.
 * @participant: The participant to send data to.
 * @buffer: The buffer of data to send.
 * @size: The size of @buffer
 * @blocking: Whether to block until the data was send or not.
 *
 * Sends a buffer of data to a #PURPLE_MEDIA_APPLICATION session.
 * If @blocking is set, unless an error occured, the function will not return
 * until the data has been flushed into the network.
 * If the stream is not writable, the data will be queued. It is the
 * responsability of the user to stop sending data when the stream isn't
 * writable anymore. It is also the responsability of the user to only start
 * sending data after the stream has been configured correctly (encryption
 * parameters for example).
 *
 * Returns: Number of bytes sent or -1 in case of error.
 */
gint purple_media_manager_send_application_data (
	PurpleMediaManager *manager, PurpleMedia *media, const gchar *session_id,
	const gchar *participant, gpointer buffer, guint size, gboolean blocking);

/**
 * purple_media_manager_receive_application_data:
 * @manager: The manager to receive data with.
 * @media: The media instance to which the session belongs.
 * @session_id: The session to receive data from.
 * @participant: The participant to receive data from.
 * @buffer: The buffer to receive data into.
 * @max_size: The max_size of @buffer
 * @blocking: Whether to block until the buffer is entirely filled or return
 * with currently available data.
 *
 * Receive a buffer of data from a #PURPLE_MEDIA_APPLICATION session.
 * If @blocking is set, unless an error occured, the function will not return
 * until @max_size bytes are read.
 *
 * Returns: Number of bytes received or -1 in case of error.
 */
gint purple_media_manager_receive_application_data (
	PurpleMediaManager *manager, PurpleMedia *media, const gchar *session_id,
	const gchar *participant, gpointer buffer, guint max_size,
	gboolean blocking);

/*}@*/

G_END_DECLS

#endif  /* _PURPLE_MEDIA_MANAGER_H_ */
