/**
 * @file privacy.h Privacy API
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
#ifndef _GAIM_PRIVACY_H_
#define _GAIM_PRIVACY_H_

#include "account.h"

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
 * @Param name       The name of the user to add to the list.
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
 * @Param name       The name of the user to add to the list.
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
 * @Param name       The name of the user to add to the list.
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
 * @Param name       The name of the user to add to the list.
 * @param local_only If TRUE, only the local list is updated, and not
 *                   the server.
 *
 * @return TRUE if the user was removed successfully, or @c FALSE otherwise.
 */
gboolean gaim_privacy_deny_remove(GaimAccount *account, const char *name,
								  gboolean local_only);

/**
 * Sets the UI operations structure for the privacy subsystem.
 *
 * @param ops The UI operations structure.
 */
void gaim_set_privacy_ui_ops(GaimPrivacyUiOps *ops);

/**
 * Returns the UI operations structure for the privacy subsystem.
 *
 * @return The UI operations structure.
 */
GaimPrivacyUiOps *gaim_get_privacy_ui_ops(void);

/**
 * Initializes the privacy subsystem.
 */
void gaim_privacy_init(void);

#ifdef __cplusplus
}
#endif

#endif /* _GAIM_PRIVACY_H_ */
