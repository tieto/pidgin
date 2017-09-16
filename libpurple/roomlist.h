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

#ifndef _PURPLE_ROOMLIST_H_
#define _PURPLE_ROOMLIST_H_
/**
 * SECTION:roomlist
 * @section_id: libpurple-roomlist
 * @short_description: <filename>roomlist.h</filename>
 * @title: Room List API
 */

#define PURPLE_TYPE_ROOMLIST             (purple_roomlist_get_type())
#define PURPLE_ROOMLIST(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_ROOMLIST, PurpleRoomlist))
#define PURPLE_ROOMLIST_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_ROOMLIST, PurpleRoomlistClass))
#define PURPLE_IS_ROOMLIST(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_ROOMLIST))
#define PURPLE_IS_ROOMLIST_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_ROOMLIST))
#define PURPLE_ROOMLIST_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_ROOMLIST, PurpleRoomlistClass))

typedef struct _PurpleRoomlist PurpleRoomlist;
typedef struct _PurpleRoomlistClass PurpleRoomlistClass;

#define PURPLE_TYPE_ROOMLIST_ROOM        (purple_roomlist_room_get_type())

typedef struct _PurpleRoomlistRoom PurpleRoomlistRoom;

#define PURPLE_TYPE_ROOMLIST_FIELD       (purple_roomlist_field_get_type())

typedef struct _PurpleRoomlistField PurpleRoomlistField;

#define PURPLE_TYPE_ROOMLIST_UI_OPS      (purple_roomlist_ui_ops_get_type())

typedef struct _PurpleRoomlistUiOps PurpleRoomlistUiOps;

/**
 * PurpleRoomlistRoomType:
 * @PURPLE_ROOMLIST_ROOMTYPE_CATEGORY: It's a category, but not a room you can
 *                                     join.
 * @PURPLE_ROOMLIST_ROOMTYPE_ROOM:     It's a room, like the kind you can join.
 *
 * The types of rooms.
 *
 * These are ORable flags.
 */
typedef enum
{
	PURPLE_ROOMLIST_ROOMTYPE_CATEGORY = 0x01,
	PURPLE_ROOMLIST_ROOMTYPE_ROOM     = 0x02

} PurpleRoomlistRoomType;

/**
 * PurpleRoomlistFieldType:
 * @PURPLE_ROOMLIST_FIELD_STRING: We do a g_strdup on the passed value if it's
 *                                this type.
 *
 * The types of fields.
 */
typedef enum
{
	PURPLE_ROOMLIST_FIELD_BOOL,
	PURPLE_ROOMLIST_FIELD_INT,
	PURPLE_ROOMLIST_FIELD_STRING

} PurpleRoomlistFieldType;

#include "account.h"
#include <glib.h>

/**************************************************************************/
/** Data Structures                                                       */
/**************************************************************************/

/**
 * PurpleRoomlistUiOps:
 * @show_with_account: Force the ui to pop up a dialog and get the list.
 * @create:            A new list was created.
 * @set_fields:        Sets the columns.
 * @add_room:          Add a room to the list.
 * @in_progress:       Are we fetching stuff still?
 * @destroy:           We're destroying list.
 *
 * The room list ops to be filled out by the UI.
 */
struct _PurpleRoomlistUiOps {
	void (*show_with_account)(PurpleAccount *account);
	void (*create)(PurpleRoomlist *list);
	void (*set_fields)(PurpleRoomlist *list, GList *fields);
	void (*add_room)(PurpleRoomlist *list, PurpleRoomlistRoom *room);
	void (*in_progress)(PurpleRoomlist *list, gboolean flag);
	void (*destroy)(PurpleRoomlist *list);

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**
 * PurpleRoomlist:
 * @ui_data: The UI data associated with this room list. This is a convenience
 *           field provided to the UIs -- it is not used by the libpurple core.
 *
 * Represents a list of rooms for a given connection on a given protocol.
 */
struct _PurpleRoomlist {
	GObject gparent;

	/*< public >*/
	gpointer ui_data;
};

/**
 * PurpleRoomlistClass:
 *
 * Base class for all #PurpleRoomlist's
 */
struct _PurpleRoomlistClass {
	GObjectClass parent_class;

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

G_BEGIN_DECLS

/**************************************************************************/
/* Room List API                                                          */
/**************************************************************************/

/**
 * purple_roomlist_get_type:
 *
 * Returns: The #GType for the Room List object.
 */
GType purple_roomlist_get_type(void);

/**
 * purple_roomlist_show_with_account:
 * @account: The account to get the list on.
 *
 * This is used to get the room list on an account, asking the UI
 * to pop up a dialog with the specified account already selected,
 * and pretend the user clicked the get list button.
 * While we're pretending, predend I didn't say anything about dialogs
 * or buttons, since this is the core.
 */
void purple_roomlist_show_with_account(PurpleAccount *account);

/**
 * purple_roomlist_new:
 * @account: The account that's listing rooms.
 *
 * Returns a newly created room list object.
 *
 * Returns: The new room list handle.
 */
PurpleRoomlist *purple_roomlist_new(PurpleAccount *account);

/**
 * purple_roomlist_get_account:
 * @list: The room list.
 *
 * Retrieve the PurpleAccount that was given when the room list was
 * created.
 *
 * Returns: The PurpleAccount tied to this room list.
 */
PurpleAccount *purple_roomlist_get_account(PurpleRoomlist *list);

/**
 * purple_roomlist_set_fields:
 * @list: The room list.
 * @fields: (element-type PurpleRoomlistField) (transfer full): UI's are
 *          encouraged to default to displaying these fields in the order given.
 *
 * Set the different field types and their names for this protocol.
 *
 * This must be called before purple_roomlist_room_add().
 */
void purple_roomlist_set_fields(PurpleRoomlist *list, GList *fields);

/**
 * purple_roomlist_set_in_progress:
 * @list: The room list.
 * @in_progress: We're downloading it, or we're not.
 *
 * Set the "in progress" state of the room list.
 *
 * The UI is encouraged to somehow hint to the user
 * whether or not we're busy downloading a room list or not.
 */
void purple_roomlist_set_in_progress(PurpleRoomlist *list, gboolean in_progress);

/**
 * purple_roomlist_get_in_progress:
 * @list: The room list.
 *
 * Gets the "in progress" state of the room list.
 *
 * The UI is encouraged to somehow hint to the user
 * whether or not we're busy downloading a room list or not.
 *
 * Returns: True if we're downloading it, or false if we're not.
 */
gboolean purple_roomlist_get_in_progress(PurpleRoomlist *list);

/**
 * purple_roomlist_room_add:
 * @list: The room list.
 * @room: The room to add to the list. The GList of fields must be in the same
               order as was given in purple_roomlist_set_fields().
 *
 * Adds a room to the list of them.
*/
void purple_roomlist_room_add(PurpleRoomlist *list, PurpleRoomlistRoom *room);

/**
 * purple_roomlist_get_list:
 * @gc: The PurpleConnection to have get a list.
 *
 * Returns a PurpleRoomlist structure from the protocol, and
 * instructs the protocol to start fetching the list.
 *
 * Returns: A PurpleRoomlist* or %NULL if the protocol
 *         doesn't support that.
 */
PurpleRoomlist *purple_roomlist_get_list(PurpleConnection *gc);

/**
 * purple_roomlist_cancel_get_list:
 * @list: The room list to cancel a get_list on.
 *
 * Tells the protocol to stop fetching the list.
 * If this is possible and done, the protocol will
 * call set_in_progress with %FALSE and possibly
 * unref the list if it took a reference.
 */
void purple_roomlist_cancel_get_list(PurpleRoomlist *list);

/**
 * purple_roomlist_expand_category:
 * @list:     The room list.
 * @category: The category that was expanded. The expression
 *                 (category->type & PURPLE_ROOMLIST_ROOMTYPE_CATEGORY)
 *                 must be true.
 *
 * Tells the protocol that a category was expanded.
 *
 * On some protocols, the rooms in the category
 * won't be fetched until this is called.
 */
void purple_roomlist_expand_category(PurpleRoomlist *list, PurpleRoomlistRoom *category);

/**
 * purple_roomlist_get_fields:
 * @roomlist: The roomlist, which must not be %NULL.
 *
 * Get the list of fields for a roomlist.
 *
 * Returns: (element-type PurpleRoomlistField) (transfer none): A list of fields
 */
GList *purple_roomlist_get_fields(PurpleRoomlist *roomlist);

/**
 * purple_roomlist_get_protocol_data:
 * @list: The roomlist, which must not be %NULL.
 *
 * Get the protocol data associated with this room list.
 *
 * Returns: The protocol data associated with this room list.  This is a
 *         convenience field provided to the protocol -- it is not
 *         used the libpurple core.
 */
gpointer purple_roomlist_get_protocol_data(PurpleRoomlist *list);

/**
 * purple_roomlist_set_protocol_data:
 * @list: The roomlist, which must not be %NULL.
 * @proto_data: A pointer to associate with this room list.
 *
 * Set the protocol data associated with this room list.
 */
void purple_roomlist_set_protocol_data(PurpleRoomlist *list, gpointer proto_data);

/**
 * purple_roomlist_get_ui_data:
 * @list: The roomlist, which must not be %NULL.
 *
 * Get the UI data associated with this room list.
 *
 * Returns: The UI data associated with this room list.  This is a
 *         convenience field provided to the UIs--it is not
 *         used by the libpurple core.
 */
gpointer purple_roomlist_get_ui_data(PurpleRoomlist *list);

/**
 * purple_roomlist_set_ui_data:
 * @list: The roomlist, which must not be %NULL.
 * @ui_data: A pointer to associate with this room list.
 *
 * Set the UI data associated with this room list.
 */
void purple_roomlist_set_ui_data(PurpleRoomlist *list, gpointer ui_data);

/**************************************************************************/
/* Room API                                                               */
/**************************************************************************/

/**
 * purple_roomlist_room_get_type:
 *
 * Returns: The #GType for the #PurpleRoomlistRoom boxed structure.
 */
GType purple_roomlist_room_get_type(void);

/**
 * purple_roomlist_room_new:
 * @type: The type of room.
 * @name: The name of the room.
 * @parent: The room's parent, if any.
 *
 * Creates a new room, to be added to the list.
 *
 * Returns: A new room.
 */
PurpleRoomlistRoom *purple_roomlist_room_new(PurpleRoomlistRoomType type, const gchar *name,
                                         PurpleRoomlistRoom *parent);

/**
 * purple_roomlist_room_add_field:
 * @list: The room list the room belongs to.
 * @room: The room.
 * @field: The field to append. Strings get g_strdup'd internally.
 *
 * Adds a field to a room.
 */
void purple_roomlist_room_add_field(PurpleRoomlist *list, PurpleRoomlistRoom *room, gconstpointer field);

/**
 * purple_roomlist_room_join:
 * @list: The room list the room belongs to.
 * @room: The room to join.
 *
 * Join a room, given a PurpleRoomlistRoom and it's associated PurpleRoomlist.
 */
void purple_roomlist_room_join(PurpleRoomlist *list, PurpleRoomlistRoom *room);

/**
 * purple_roomlist_room_get_room_type:
 * @room:  The room, which must not be %NULL.
 *
 * Get the type of a room.
 *
 * Returns: The type of the room.
 */
PurpleRoomlistRoomType purple_roomlist_room_get_room_type(PurpleRoomlistRoom *room);

/**
 * purple_roomlist_room_get_name:
 * @room:  The room, which must not be %NULL.
 *
 * Get the name of a room.
 *
 * Returns: The name of the room.
 */
const char * purple_roomlist_room_get_name(PurpleRoomlistRoom *room);

/**
 * purple_roomlist_room_get_parent:
 * @room:  The room, which must not be %NULL.
 *
 * Get the parent of a room.
 *
 * Returns: The parent of the room, which can be %NULL.
 */
PurpleRoomlistRoom * purple_roomlist_room_get_parent(PurpleRoomlistRoom *room);

/**
 * purple_roomlist_room_get_expanded_once:
 * @room:  The room, which must not be %NULL.
 *
 * Get the value of the expanded_once flag.
 *
 * Returns: The value of the expanded_once flag.
 */
gboolean purple_roomlist_room_get_expanded_once(PurpleRoomlistRoom *room);

/**
 * purple_roomlist_room_set_expanded_once:
 * @room: The room, which must not be %NULL.
 * @expanded_once: The new value of the expanded_once flag.
 *
 * Set the expanded_once flag.
 */
void purple_roomlist_room_set_expanded_once(PurpleRoomlistRoom *room, gboolean expanded_once);

/**
 * purple_roomlist_room_get_fields:
 * @room:  The room, which must not be %NULL.
 *
 * Get the list of fields for a room.
 *
 * Returns: (element-type PurpleRoomlistField) (transfer none): A list of fields
 */
GList * purple_roomlist_room_get_fields(PurpleRoomlistRoom *room);

/**************************************************************************/
/* Room Field API                                                         */
/**************************************************************************/

/**
 * purple_roomlist_field_get_type:
 *
 * Returns: The #GType for the #PurpleRoomlistField boxed structure.
 */
GType purple_roomlist_field_get_type(void);

/**
 * purple_roomlist_field_new:
 * @type:   The type of the field.
 * @label:  The i18n'ed, user displayable name.
 * @name:   The internal name of the field.
 * @hidden: Hide the field.
 *
 * Creates a new field.
 *
 * Returns: A new PurpleRoomlistField, ready to be added to a GList and passed to
 *         purple_roomlist_set_fields().
 */
PurpleRoomlistField *purple_roomlist_field_new(PurpleRoomlistFieldType type,
                                           const gchar *label, const gchar *name,
                                           gboolean hidden);

/**
 * purple_roomlist_field_get_field_type:
 * @field:  A PurpleRoomlistField, which must not be %NULL.
 *
 * Get the type of a field.
 *
 * Returns:  The type of the field.
 */
PurpleRoomlistFieldType purple_roomlist_field_get_field_type(PurpleRoomlistField *field);

/**
 * purple_roomlist_field_get_label:
 * @field:  A PurpleRoomlistField, which must not be %NULL.
 *
 * Get the label of a field.
 *
 * Returns:  The label of the field.
 */
const char * purple_roomlist_field_get_label(PurpleRoomlistField *field);

/**
 * purple_roomlist_field_get_hidden:
 * @field:  A PurpleRoomlistField, which must not be %NULL.
 *
 * Check whether a roomlist-field is hidden.
 *
 * Returns:  %TRUE if the field is hidden, %FALSE otherwise.
 */
gboolean purple_roomlist_field_get_hidden(PurpleRoomlistField *field);

/**************************************************************************/
/* UI Registration Functions                                              */
/**************************************************************************/

/**
 * purple_roomlist_ui_ops_get_type:
 *
 * Returns: The #GType for the #PurpleRoomlistUiOps boxed structure.
 */
GType purple_roomlist_ui_ops_get_type(void);

/**
 * purple_roomlist_set_ui_ops:
 * @ops: The UI operations structure.
 *
 * Sets the UI operations structure to be used in all purple room lists.
 */
void purple_roomlist_set_ui_ops(PurpleRoomlistUiOps *ops);

/**
 * purple_roomlist_get_ui_ops:
 *
 * Returns the purple window UI operations structure to be used in
 * new windows.
 *
 * Returns: A filled-out PurpleRoomlistUiOps structure.
 */
PurpleRoomlistUiOps *purple_roomlist_get_ui_ops(void);

G_END_DECLS

#endif /* _PURPLE_ROOMLIST_H_ */
