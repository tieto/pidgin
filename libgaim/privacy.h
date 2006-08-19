/**
 * @file privacy.h Privacy API
 * @ingroup core
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
#ifndef _GAIM_PRIVACY_H_
#define _GAIM_PRIVACY_H_

#include "account.h"

/**
 * Privacy data types.
 */
typedef enum _GaimPrivacyType
{
	GAIM_PRIVACY_ALLOW_ALL = 1,
	GAIM_PRIVACY_DENY_ALL,
	GAIM_PRIVACY_ALLOW_USERS,
	GAIM_PRIVACY_DENY_USERS,
	GAIM_PRIVACY_ALLOW_BUDDYLIST
} GaimPrivacyType;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Privacy core/UI operations.
 */
typedef struct
{
	void (*permit_added)(GaimAccount *account, const char *name);
	void (*permit_removed)(GaimAccount *account, const char *name);
	void (*deny_added)(GaimAccount *account, const char *name);
	void (*deny_removed)(GaimAccount *account, const char *name);

} GaimPrivacyUiOps;

/**
 * Adds a user to the account's permit list.
 *
 * @param account    The account.
 * @param name       The name of the user to add to the list.
 * @param local_only If TRUE, only the local list is updated, and not
 *                   the server.
 *
 * @return TRUE if the user was added successfully, or @c FALSE otherwise.
 */
gboolean gaim_privacy_permit_add(GaimAccount *account, const char *name,
								 gboolean local_only);

/**
 * Removes a user from the account's permit list.
 *
 * @param account    The account.
 * @param name       The name of the user to add to the list.
 * @param local_only If TRUE, only the local list is updated, and not
 *                   the server.
 *
 * @return TRUE if the user was removed successfully, or @c FALSE otherwise.
 */
gboolean gaim_privacy_permit_remove(GaimAccount *account, const char *name,
									gboolean local_only);

/**
 * Adds a user to the account's deny list.
 *
 * @param account    The account.
 * @param name       The name of the user to add to the list.
 * @param local_only If TRUE, only the local list is updated, and not
 *                   the server.
 *
 * @return TRUE if the user was added successfully, or @c FALSE otherwise.
 */
gboolean gaim_privacy_deny_add(GaimAccount *account, const char *name,
							   gboolean local_only);

/**
 * Removes a user from the account's deny list.
 *
 * @param account    The account.
 * @param name       The name of the user to add to the list.
 * @param local_only If TRUE, only the local list is updated, and not
 *                   the server.
 *
 * @return TRUE if the user was removed successfully, or @c FALSE otherwise.
 */
gboolean gaim_privacy_deny_remove(GaimAccount *account, const char *name,
								  gboolean local_only);


/**
 * Check the privacy-setting for a user.
 *
 * @param account	The account.
 * @param who		The name of the user.
 *
 * @return @c FALSE if the specified account's privacy settings block the user or @c TRUE otherwise. The meaning of "block" is protocol-dependent and generally relates to status and/or sending of messages.
 */
gboolean gaim_privacy_check(GaimAccount *account, const char *who);

/**
 * Sets the UI operations structure for the privacy subsystem.
 *
 * @param ops The UI operations structure.
 */
void gaim_privacy_set_ui_ops(GaimPrivacyUiOps *ops);

/**
 * Returns the UI operations structure for the privacy subsystem.
 *
 * @return The UI operations structure.
 */
GaimPrivacyUiOps *gaim_privacy_get_ui_ops(void);

/**
 * Initializes the privacy subsystem.
 */
void gaim_privacy_init(void);

#ifdef __cplusplus
}
#endif

#endif /* _GAIM_PRIVACY_H_ */
