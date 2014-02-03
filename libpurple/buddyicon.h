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
/**
 * SECTION:buddyicon
 * @section_id: libpurple-buddyicon
 * @title: buddyicon.h
 * @short_description: Buddy Icon API
 */

#ifndef _PURPLE_BUDDYICON_H_
#define _PURPLE_BUDDYICON_H_

#define PURPLE_TYPE_BUDDY_ICON (purple_buddy_icon_get_type())

/**
 * PurpleBuddyIcon:
 *
 * An opaque structure representing a buddy icon for a particular user on a
 * particular #PurpleAccount.  Instances are reference-counted; use
 * purple_buddy_icon_ref() and purple_buddy_icon_unref() to take and release
 * references.
 */
typedef struct _PurpleBuddyIcon PurpleBuddyIcon;

#include "account.h"
#include "buddylist.h"
#include "imgstore.h"
#include "prpl.h"
#include "util.h"

G_BEGIN_DECLS

/**************************************************************************/
/** @name Buddy Icon API                                                  */
/**************************************************************************/
/*@{*/

/**
 * purple_buddy_icon_get_type:
 *
 * Returns the GType for the PurpleBuddyIcon boxed structure.
 */
GType purple_buddy_icon_get_type(void);

/**
 * purple_buddy_icon_new:
 * @account:   The account the user is on.
 * @username:  The username the icon belongs to.
 * @icon_data: The buddy icon data.
 * @icon_len:  The buddy icon length.
 * @checksum:  A protocol checksum from the protocol or %NULL.
 *
 * Creates a new buddy icon structure and populates it.
 *
 * If an icon for this account+username already exists, you'll get a reference
 * to that structure, which will have been updated with the data supplied.
 *
 * Returns: The buddy icon structure, with a reference for the caller.
 */
PurpleBuddyIcon *purple_buddy_icon_new(PurpleAccount *account, const char *username,
                                       void *icon_data, size_t icon_len,
                                       const char *checksum);

/**
 * purple_buddy_icon_ref:
 * @icon: The buddy icon.
 *
 * Increments the reference count on a buddy icon.
 *
 * Returns: @icon.
 */
PurpleBuddyIcon *purple_buddy_icon_ref(PurpleBuddyIcon *icon);

/**
 * purple_buddy_icon_unref:
 * @icon: The buddy icon.
 *
 * Decrements the reference count on a buddy icon.
 *
 * If the reference count reaches 0, the icon will be destroyed.
 */
void purple_buddy_icon_unref(PurpleBuddyIcon *icon);

/**
 * purple_buddy_icon_update:
 * @icon: The buddy icon.
 *
 * Updates every instance of this icon.
 */
void purple_buddy_icon_update(PurpleBuddyIcon *icon);

/**
 * purple_buddy_icon_set_data:
 * @icon: The buddy icon.
 * @data: The buddy icon data, which the buddy icon code
 *             takes ownership of and will free.
 * @len:  The length of the data in @a data.
 * @checksum:  A protocol checksum from the protocol or %NULL.
 *
 * Sets the buddy icon's data.
 */
void
purple_buddy_icon_set_data(PurpleBuddyIcon *icon, guchar *data,
                           size_t len, const char *checksum);

/**
 * purple_buddy_icon_get_account:
 * @icon: The buddy icon.
 *
 * Returns the buddy icon's account.
 *
 * Returns: The account.
 */
PurpleAccount *purple_buddy_icon_get_account(const PurpleBuddyIcon *icon);

/**
 * purple_buddy_icon_get_username:
 * @icon: The buddy icon.
 *
 * Returns the buddy icon's username.
 *
 * Returns: The username.
 */
const char *purple_buddy_icon_get_username(const PurpleBuddyIcon *icon);

/**
 * purple_buddy_icon_get_checksum:
 * @icon: The buddy icon.
 *
 * Returns the buddy icon's checksum.
 *
 * This function is really only for protocol use.
 *
 * Returns: The checksum.
 */
const char *purple_buddy_icon_get_checksum(const PurpleBuddyIcon *icon);

/**
 * purple_buddy_icon_get_data:
 * @icon: The buddy icon.
 * @len:  If not %NULL, the length of the icon data returned will be
 *             set in the location pointed to by this.
 *
 * Returns the buddy icon's data.
 *
 * Returns: A pointer to the icon data.
 */
gconstpointer purple_buddy_icon_get_data(const PurpleBuddyIcon *icon, size_t *len);

/**
 * purple_buddy_icon_get_extension:
 * @icon: The buddy icon.
 *
 * Returns an extension corresponding to the buddy icon's file type.
 *
 * Returns: The icon's extension, "icon" if unknown, or %NULL if
 *         the image data has disappeared.
 */
const char *purple_buddy_icon_get_extension(const PurpleBuddyIcon *icon);

/**
 * purple_buddy_icon_get_full_path:
 * @icon: The buddy icon
 *
 * Returns a full path to an icon.
 *
 * If the icon has data and the file exists in the cache, this will return
 * a full path to the cache file.
 *
 * In general, it is not appropriate to be poking in the icon cache
 * directly.  If you find yourself wanting to use this function, think
 * very long and hard about it, and then don't.
 *
 * Returns: A full path to the file, or %NULL under various conditions.
 */
char *purple_buddy_icon_get_full_path(PurpleBuddyIcon *icon);

/*@}*/

/**************************************************************************/
/** @name Buddy Icon Subsystem API                                        */
/**************************************************************************/
/*@{*/

/**
 * purple_buddy_icons_set_for_user:
 * @account:   The account the user is on.
 * @username:  The username of the user.
 * @icon_data: The buddy icon data, which the buddy icon code
 *                  takes ownership of and will free.
 * @icon_len:  The length of the icon data.
 * @checksum:  A protocol checksum from the protocol or %NULL.
 *
 * Sets a buddy icon for a user.
 */
void
purple_buddy_icons_set_for_user(PurpleAccount *account, const char *username,
                                void *icon_data, size_t icon_len,
                                const char *checksum);

/**
 * purple_buddy_icons_get_checksum_for_user:
 * @buddy: The buddy
 *
 * Returns the checksum for the buddy icon of a specified buddy.
 *
 * This avoids loading the icon image data from the cache if it's
 * not already loaded for some other reason.
 *
 * Returns: The checksum.
 */
const char *
purple_buddy_icons_get_checksum_for_user(PurpleBuddy *buddy);

/**
 * purple_buddy_icons_find:
 * @account:  The account the user is on.
 * @username: The username of the user.
 *
 * Returns the buddy icon information for a user.
 *
 * Returns: The icon (with a reference for the caller) if found, or %NULL if
 *         not found.
 */
PurpleBuddyIcon *
purple_buddy_icons_find(PurpleAccount *account, const char *username);

/**
 * purple_buddy_icons_find_account_icon:
 * @account: The account
 *
 * Returns the buddy icon image for an account.
 *
 * The caller owns a reference to the image in the store, and must dereference
 * the image with purple_imgstore_unref() for it to be freed.
 *
 * This function deals with loading the icon from the cache, if
 * needed, so it should be called in any case where you want the
 * appropriate icon.
 *
 * Returns: The account's buddy icon image.
 */
PurpleStoredImage *
purple_buddy_icons_find_account_icon(PurpleAccount *account);

/**
 * purple_buddy_icons_set_account_icon:
 * @account:   The account for which to set a custom icon.
 * @icon_data: The image data of the icon, which the
 *                  buddy icon code will free.
 * @icon_len:  The length of the data in @icon_data.
 *
 * Sets a buddy icon for an account.
 *
 * This function will deal with saving a record of the icon,
 * caching the data, etc.
 *
 * Returns: The icon that was set.  The caller does NOT own
 *         a reference to this, and must call purple_imgstore_ref()
 *         if it wants one.
 */
PurpleStoredImage *
purple_buddy_icons_set_account_icon(PurpleAccount *account,
                                    guchar *icon_data, size_t icon_len);

/**
 * purple_buddy_icons_get_account_icon_timestamp:
 * @account: The account
 *
 * Returns the timestamp of when the icon was set.
 *
 * This is intended for use in protocols that require a timestamp for
 * buddy icon update reasons.
 *
 * Returns: The time the icon was set, or 0 if an error occurred.
 */
time_t
purple_buddy_icons_get_account_icon_timestamp(PurpleAccount *account);

/**
 * purple_buddy_icons_node_has_custom_icon:
 * @node: The blist node.
 *
 * Returns a boolean indicating if a given blist node has a custom buddy icon.
 *
 * Returns: A boolean indicating if @node has a custom buddy icon.
 */
gboolean
purple_buddy_icons_node_has_custom_icon(PurpleBlistNode *node);

/**
 * purple_buddy_icons_node_find_custom_icon:
 * @node: The node.
 *
 * Returns the custom buddy icon image for a blist node.
 *
 * The caller owns a reference to the image in the store, and must dereference
 * the image with purple_imgstore_unref() for it to be freed.
 *
 * This function deals with loading the icon from the cache, if
 * needed, so it should be called in any case where you want the
 * appropriate icon.
 *
 * Returns: The custom buddy icon.
 */
PurpleStoredImage *
purple_buddy_icons_node_find_custom_icon(PurpleBlistNode *node);

/**
 * purple_buddy_icons_node_set_custom_icon:
 * @node:      The blist node for which to set a custom icon.
 * @icon_data: The image data of the icon, which the buddy icon code will
 *                  free. Use NULL to unset the icon.
 * @icon_len:  The length of the data in @icon_data.
 *
 * Sets a custom buddy icon for a blist node.
 *
 * This function will deal with saving a record of the icon, caching the data,
 * etc.
 *
 * Returns: The icon that was set. The caller does NOT own a reference to this,
 *         and must call purple_imgstore_ref() if it wants one.
 */
PurpleStoredImage *
purple_buddy_icons_node_set_custom_icon(PurpleBlistNode *node,
                                        guchar *icon_data, size_t icon_len);

/**
 * purple_buddy_icons_node_set_custom_icon_from_file:
 * @node:      The blist node for which to set a custom icon.
 * @filename:  The path to the icon to set for the blist node. Use NULL
 *                  to unset the custom icon.
 *
 * Sets a custom buddy icon for a blist node.
 *
 * Convenience wrapper around purple_buddy_icons_node_set_custom_icon.
 * @see purple_buddy_icons_node_set_custom_icon()
 *
 * Returns: The icon that was set. The caller does NOT own a reference to this,
 *         and must call purple_imgstore_ref() if it wants one.
 */
PurpleStoredImage *
purple_buddy_icons_node_set_custom_icon_from_file(PurpleBlistNode *node,
                                                  const gchar *filename);

/**
 * purple_buddy_icons_set_caching:
 * @caching: TRUE if buddy icon caching should be enabled, or
 *                FALSE otherwise.
 *
 * Sets whether or not buddy icon caching is enabled.
 */
void purple_buddy_icons_set_caching(gboolean caching);

/**
 * purple_buddy_icons_is_caching:
 *
 * Returns whether or not buddy icon caching should be enabled.
 *
 * The default is TRUE, unless otherwise specified by
 * purple_buddy_icons_set_caching().
 *
 * Returns: TRUE if buddy icon caching is enabled, or FALSE otherwise.
 */
gboolean purple_buddy_icons_is_caching(void);

/**
 * purple_buddy_icons_set_cache_dir:
 * @cache_dir: The directory to store buddy icon cache files to.
 *
 * Sets the directory used to store buddy icon cache files.
 */
void purple_buddy_icons_set_cache_dir(const char *cache_dir);

/**
 * purple_buddy_icons_get_cache_dir:
 *
 * Returns the directory used to store buddy icon cache files.
 *
 * The default directory is PURPLEDIR/icons, unless otherwise specified
 * by purple_buddy_icons_set_cache_dir().
 *
 * Returns: The directory to store buddy icon cache files to.
 */
const char *purple_buddy_icons_get_cache_dir(void);

/**
 * purple_buddy_icons_get_handle:
 *
 * Returns the buddy icon subsystem handle.
 *
 * Returns: The subsystem handle.
 */
void *purple_buddy_icons_get_handle(void);

/**
 * purple_buddy_icons_init:
 *
 * Initializes the buddy icon subsystem.
 */
void purple_buddy_icons_init(void);

/**
 * purple_buddy_icons_uninit:
 *
 * Uninitializes the buddy icon subsystem.
 */
void purple_buddy_icons_uninit(void);

/*@}*/

/**************************************************************************/
/** @name Buddy Icon Helper API                                           */
/**************************************************************************/
/*@{*/

/**
 * purple_buddy_icon_get_scale_size:
 *
 * Gets display size for a buddy icon
 */
void purple_buddy_icon_get_scale_size(PurpleBuddyIconSpec *spec, int *width, int *height);

/*@}*/

G_END_DECLS

#endif /* _PURPLE_BUDDYICON_H_ */
