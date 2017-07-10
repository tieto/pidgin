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

#ifndef PURPLE_CONTACT_H
#define PURPLE_CONTACT_H
/**
 * SECTION:contact
 * @section_id: libpurple-contact
 * @short_description: <filename>contact.h</filename>
 * @title: Contact Object
 */

#define PURPLE_TYPE_CONTACT             (purple_contact_get_type())
#define PURPLE_CONTACT(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_CONTACT, PurpleContact))
#define PURPLE_CONTACT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_CONTACT, PurpleContactClass))
#define PURPLE_IS_CONTACT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_CONTACT))
#define PURPLE_IS_CONTACT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_CONTACT))
#define PURPLE_CONTACT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_CONTACT, PurpleContactClass))

typedef struct _PurpleContact PurpleContact;
typedef struct _PurpleContactClass PurpleContactClass;

#include "countingnode.h"
#include "group.h"

/**
 * PurpleContact:
 *
 * A contact on the buddy list.
 *
 * A contact is a counting node, which means it keeps track of the counts of
 * the buddies under this contact.
 */
struct _PurpleContact {
	PurpleCountingNode counting;
};

struct _PurpleContactClass {
	PurpleCountingNodeClass counting_class;

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

G_BEGIN_DECLS

/**************************************************************************/
/* Contact API                                                            */
/**************************************************************************/

/**
 * purple_contact_get_type:
 *
 * Returns: The #GType for the #PurpleContact object.
 */
GType purple_contact_get_type(void);

/**
 * purple_contact_new:
 *
 * Creates a new contact
 *
 * Returns:       A new contact struct
 */
PurpleContact *purple_contact_new(void);

/**
 * purple_contact_get_group:
 * @contact:  The contact
 *
 * Gets the PurpleGroup from a PurpleContact
 *
 * Returns:         The group
 */
PurpleGroup *purple_contact_get_group(const PurpleContact *contact);

/**
 * purple_contact_get_priority_buddy:
 * @contact:  The contact
 *
 * Returns the highest priority buddy for a given contact.
 *
 * Returns: The highest priority buddy
 */
PurpleBuddy *purple_contact_get_priority_buddy(PurpleContact *contact);

/**
 * purple_contact_set_alias:
 * @contact:  The contact
 * @alias:    The alias
 *
 * Sets the alias for a contact.
 */
void purple_contact_set_alias(PurpleContact *contact, const char *alias);

/**
 * purple_contact_get_alias:
 * @contact:  The contact
 *
 * Gets the alias for a contact.
 *
 * Returns:  The alias, or NULL if it is not set.
 */
const char *purple_contact_get_alias(PurpleContact *contact);

/**
 * purple_contact_on_account:
 * @contact:  The contact to search through.
 * @account:  The account.
 *
 * Determines whether an account owns any buddies in a given contact
 *
 * Returns: TRUE if there are any buddies from account in the contact, or FALSE otherwise.
 */
gboolean purple_contact_on_account(PurpleContact *contact, PurpleAccount *account);

/**
 * purple_contact_invalidate_priority_buddy:
 * @contact:  The contact
 *
 * Invalidates the priority buddy so that the next call to
 * purple_contact_get_priority_buddy recomputes it.
 */
void purple_contact_invalidate_priority_buddy(PurpleContact *contact);

/**
 * purple_contact_merge:
 * @source:  The contact to merge
 * @node:    The place to merge to (a buddy or contact)
 *
 * Merges two contacts
 *
 * All of the buddies from source will be moved to target
 */
void purple_contact_merge(PurpleContact *source, PurpleBlistNode *node);


G_END_DECLS

#endif /* PURPLE_CONTACT_H */

