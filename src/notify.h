/**
 * @file notify.h Notification API
 * @ingroup core
 *
 * gaim
 *
 * Copyright (C) 2003 Christian Hammond <chipx86@gnupdate.org>
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
#ifndef _GAIM_NOTIFY_H_
#define _GAIM_NOTIFY_H_

#include <stdlib.h>
#include <glib-object.h>
#include <glib.h>

/**
 * Notification types.
 */
typedef enum
{
	GAIM_NOTIFY_MESSAGE = 0, /**< Message notification.         */
	GAIM_NOTIFY_EMAIL,       /**< Single e-mail notification.   */
	GAIM_NOTIFY_EMAILS       /**< Multiple e-mail notification. */

} GaimNotifyType;

/**
 * Notification message types.
 */
typedef enum
{
	GAIM_NOTIFY_MSG_ERROR   = 0, /**< Error notification.       */
	GAIM_NOTIFY_MSG_WARNING,     /**< Warning notification.     */
	GAIM_NOTIFY_MSG_INFO         /**< Information notification. */

} GaimNotifyMsgType;

/**
 * Notification UI operations.
 */
typedef struct
{
	void *(*notify_message)(GaimNotifyMsgType type, const char *title,
							const char *primary, const char *secondary,
							GCallback cb, void *user_data);
	void *(*notify_email)(const char *subject, const char *from,
						  const char *to, const char *url,
						  GCallback cb, void *user_data);
	void *(*notify_emails)(size_t count, gboolean detailed,
						   const char **subjects, const char **froms,
						   const char **tos, const char **urls,
						   GCallback cb, void *user_data);

	void (*close_notify)(GaimNotifyType type, void *ui_handle);

} GaimNotifyUiOps;

/**************************************************************************/
/** @name Notification API                                                */
/**************************************************************************/
/*@{*/

/**
 * Displays a notification message to the user.
 *
 * @param handle    The plugin or connection handle.
 * @param type      The notification type.
 * @param title     The title of the message.
 * @param primary   The main point of the message.
 * @param secondary The secondary information.
 * @param cb        The callback to call when the user closes
 *                  the notification.
 * @param user_data The data to pass to the callback.
 *
 * @return A UI-specific handle.
 */
void *gaim_notify_message(void *handle, GaimNotifyType type,
						  const char *title, const char *primary,
						  const char *secondary, GCallback cb,
						  void *user_data);

/**
 * Displays a single e-mail notification to the user.
 *
 * @param handle    The plugin or connection handle.
 * @param subject   The subject of the e-mail.
 * @param from      The from address.
 * @param to        The destination address.
 * @param url       The URL where the message can be read.
 * @param cb        The callback to call when the user closes
 *                  the notification.
 * @param user_data The data to pass to the callback.
 *
 * @return A UI-specific handle.
 */
void *gaim_notify_email(void *handle, const char *subject,
						const char *from, const char *to,
						const char *url, GCallback cb,
						void *user_data);

/**
 * Displays a notification for multiple e-mails to the user.
 *
 * @param handle    The plugin or connection handle.
 * @param count     The number of e-mails.
 * @param detailed  @c TRUE if there is information for each e-mail in the
 *                  arrays.
 * @param subjects  The array of subjects.
 * @param froms     The array of from addresses.
 * @param tos       The array of destination addresses.
 * @param url       The URLs where the messages can be read.
 * @param cb        The callback to call when the user closes
 *                  the notification.
 * @param user_data The data to pass to the callback.
 *
 * @return A UI-specific handle.
 */
void *gaim_notify_emails(void *handle, size_t count, gboolean detailed,
						 const char **subjects, const char **froms,
						 const char **tos, const char **urls,
						 GCallback cb, void *user_data);

/**
 * Closes a notification.
 *
 * This should be used only by the UI operation functions and part of the
 * core.
 *
 * @param type      The notification type.
 * @param ui_handle The notification UI handle.
 */
void gaim_notify_close(GaimNotifyType type, void *ui_handle);

/**
 * Closes all notifications registered with the specified handle.
 *
 * @param handle The handle.
 */
void gaim_notify_close_with_handle(void *handle);

/**
 * A wrapper for gaim_notify_message that displays an information message.
 */
#define gaim_notify_info(handle, title, primary, secondary) \
	gaim_notify_message((handle), GAIM_NOTIFY_MSG_INFO, (title), \
						(primary), (secondary), NULL, NULL)

/**
 * A wrapper for gaim_notify_message that displays a warning message.
 */
#define gaim_notify_warning(handle, title, primary, secondary) \
	gaim_notify_message((handle), GAIM_NOTIFY_MSG_WARNING, (title), \
						(primary), (secondary), NULL, NULL)

/**
 * A wrapper for gaim_notify_message that displays an error message.
 */
#define gaim_notify_error(handle, title, primary, secondary) \
	gaim_notify_message((handle), GAIM_NOTIFY_MSG_ERROR, (title), \
						(primary), (secondary), NULL, NULL)

/*@}*/

/**************************************************************************/
/** @name UI Operations API                                               */
/**************************************************************************/
/*@{*/

/**
 * Sets the UI operations structure to be used when displaying a
 * notification.
 *
 * @param ops The UI operations structure.
 */
void gaim_set_notify_ui_ops(GaimNotifyUiOps *ops);

/**
 * Returns the UI operations structure to be used when displaying a
 * notification.
 *
 * @param ops The UI operations structure.
 */
GaimNotifyUiOps *gaim_get_notify_ui_ops(void);
/*@}*/

#endif /* _GAIM_NOTIFY_H_ */
