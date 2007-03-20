/**
 * @file buddyicon.h Buddy Icon API
 * @ingroup core
 *
 * purple
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef _PURPLE_BUDDYICON_H_
#define _PURPLE_BUDDYICON_H_

typedef struct _PurpleBuddyIcon PurpleBuddyIcon;

#include "account.h"
#include "blist.h"
#include "prpl.h"

struct _PurpleBuddyIcon
{
	PurpleAccount *account;  /**< The account the user is on.        */
	char *username;        /**< The username the icon belongs to.  */

	void  *data;           /**< The buddy icon data.               */
	size_t len;            /**< The length of the buddy icon data. */
	char *path;	       /**< The buddy icon's non-cached path.  */

	int ref_count;         /**< The buddy icon reference count.    */
};

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************/
/** @name Buddy Icon API                                                  */
/**************************************************************************/
/*@{*/

/**
 * Creates a new buddy icon structure.
 *
 * @param account   The account the user is on.
 * @param username  The username the icon belongs to.
 * @param icon_data The buddy icon data.
 * @param icon_len  The buddy icon length.
 *
 * @return The buddy icon structure.
 */
PurpleBuddyIcon *purple_buddy_icon_new(PurpleAccount *account, const char *username,
								void *icon_data, size_t icon_len);

/**
 * Destroys a buddy icon structure.
 *
 * If the buddy icon's reference count is greater than 1, this will
 * just decrease the reference count and return.
 *
 * @param icon The buddy icon structure to destroy.
 */
void purple_buddy_icon_destroy(PurpleBuddyIcon *icon);

/**
 * Increments the reference count on a buddy icon.
 *
 * @param icon The buddy icon.
 *
 * @return @a icon.
 */
PurpleBuddyIcon *purple_buddy_icon_ref(PurpleBuddyIcon *icon);

/**
 * Decrements the reference count on a buddy icon.
 *
 * If the reference count reaches 0, the icon will be destroyed.
 *
 * @param icon The buddy icon.
 *
 * @return @a icon, or @c NULL if the reference count reached 0.
 */
PurpleBuddyIcon *purple_buddy_icon_unref(PurpleBuddyIcon *icon);

/**
 * Updates every instance of this icon.
 *
 * @param icon The buddy icon.
 */
void purple_buddy_icon_update(PurpleBuddyIcon *icon);

/**
 * Caches a buddy icon associated with a specific buddy to disk.
 *
 * @param icon  The buddy icon.
 * @param buddy The buddy that this icon belongs to.
 */
void purple_buddy_icon_cache(PurpleBuddyIcon *icon, PurpleBuddy *buddy);

/**
 * Removes cached buddy icon for a specific buddy.
 *
 * @param buddy The buddy for which to remove the cached icon.
 */
void purple_buddy_icon_uncache(PurpleBuddy *buddy);

/**
 * Sets the buddy icon's account.
 *
 * @param icon    The buddy icon.
 * @param account The account.
 */
void purple_buddy_icon_set_account(PurpleBuddyIcon *icon, PurpleAccount *account);

/**
 * Sets the buddy icon's username.
 *
 * @param icon     The buddy icon.
 * @param username The username.
 */
void purple_buddy_icon_set_username(PurpleBuddyIcon *icon, const char *username);

/**
 * Sets the buddy icon's icon data.
 *
 * @param icon The buddy icon.
 * @param data The buddy icon data.
 * @param len  The length of the icon data.
 */
void purple_buddy_icon_set_data(PurpleBuddyIcon *icon, void *data, size_t len);

/**
 * Sets the buddy icon's path.
 *
 * @param icon The buddy icon.
 * @param path The buddy icon's non-cached path.
 */
void purple_buddy_icon_set_path(PurpleBuddyIcon *icon, const gchar *path);

/**
 * Returns the buddy icon's account.
 *
 * @param icon The buddy icon.
 *
 * @return The account.
 */
PurpleAccount *purple_buddy_icon_get_account(const PurpleBuddyIcon *icon);

/**
 * Returns the buddy icon's username.
 *
 * @param icon The buddy icon.
 *
 * @return The username.
 */
const char *purple_buddy_icon_get_username(const PurpleBuddyIcon *icon);

/**
 * Returns the buddy icon's data.
 *
 * @param icon The buddy icon.
 * @param len  The returned icon length.
 *
 * @return The icon data.
 */
const guchar *purple_buddy_icon_get_data(const PurpleBuddyIcon *icon, size_t *len);

/**
 * Returns the buddy icon's path.
 *
 * @param icon The buddy icon.
 * 
 * @preturn The buddy icon's non-cached path.
 */
const gchar *purple_buddy_icon_get_path(PurpleBuddyIcon *icon);

/**
 * Returns an extension corresponding to the buddy icon's file type.
 *
 * @param icon The buddy icon.
 *
 * @return The icon's extension.
 */
const char *purple_buddy_icon_get_type(const PurpleBuddyIcon *icon);

/*@}*/

/**************************************************************************/
/** @name Buddy Icon Subsystem API                                        */
/**************************************************************************/
/*@{*/

/**
 * Sets a buddy icon for a user.
 *
 * @param account   The account the user is on.
 * @param username  The username of the user.
 * @param icon_data The icon data.
 * @param icon_len  The length of the icon data.
 *
 * @return The buddy icon set, or NULL if no icon was set.
 */
void purple_buddy_icons_set_for_user(PurpleAccount *account, const char *username,
									void *icon_data, size_t icon_len);

/**
 * Returns the buddy icon information for a user.
 *
 * @param account  The account the user is on.
 * @param username The username of the user.
 *
 * @return The icon data if found, or @c NULL if not found.
 */
PurpleBuddyIcon *purple_buddy_icons_find(PurpleAccount *account,
									 const char *username);

/**
 * Sets whether or not buddy icon caching is enabled.
 *
 * @param caching TRUE of buddy icon caching should be enabled, or
 *                FALSE otherwise.
 */
void purple_buddy_icons_set_caching(gboolean caching);

/**
 * Returns whether or not buddy icon caching should be enabled.
 *
 * The default is TRUE, unless otherwise specified by
 * purple_buddy_icons_set_caching().
 *
 * @return TRUE if buddy icon caching is enabled, or FALSE otherwise.
 */
gboolean purple_buddy_icons_is_caching(void);

/**
 * Sets the directory used to store buddy icon cache files.
 *
 * @param cache_dir The directory to store buddy icon cache files to.
 */
void purple_buddy_icons_set_cache_dir(const char *cache_dir);

/**
 * Returns the directory used to store buddy icon cache files.
 *
 * The default directory is PURPLEDIR/icons, unless otherwise specified
 * by purple_buddy_icons_set_cache_dir().
 *
 * @return The directory to store buddy icon cache files to.
 */
const char *purple_buddy_icons_get_cache_dir(void);

/**
 * Takes a buddy icon and returns a full path.
 *
 * If @a icon is a full path to an existing file, a copy of
 * @a icon is returned. Otherwise, a newly allocated string
 * consiting of purple_buddy_icons_get_cache_dir() + @a icon is
 * returned.
 *
 * @return The full path for an icon.
 */
char *purple_buddy_icons_get_full_path(const char *icon);

/**
 * Returns the buddy icon subsystem handle.
 *
 * @return The subsystem handle.
 */
void *purple_buddy_icons_get_handle(void);

/**
 * Initializes the buddy icon subsystem.
 */
void purple_buddy_icons_init(void);

/**
 * Uninitializes the buddy icon subsystem.
 */
void purple_buddy_icons_uninit(void);

/*@}*/

/**************************************************************************/
/** @name Buddy Icon Helper API                                           */
/**************************************************************************/
/*@{*/

/**
 * Gets display size for a buddy icon
 */
void purple_buddy_icon_get_scale_size(PurpleBuddyIconSpec *spec, int *width, int *height);

/*@}*/

#ifdef __cplusplus
}
#endif

#endif /* _PURPLE_BUDDYICON_H_ */
