/**
 * @file disco.h Service Discovery API
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

#ifndef _PURPLE_DISCO_H_
#define _PURPLE_DISCO_H_


typedef struct _PurpleDiscoList PurpleDiscoList;
typedef struct _PurpleDiscoService PurpleDiscoService;
typedef struct _PurpleDiscoUiOps PurpleDiscoUiOps;

#include "account.h"
#include "connection.h"

/**
 * A prpl callback called to tell the prpl to cancel creating the list.
 * The prpl must implement this and must not add services to the
 * PurpleDiscoList after this is called.
 *
 * @param list  The disco list.
 */
typedef void (*PurpleDiscoCancelFunc)(PurpleDiscoList *list);

/**
 * A prpl callback called to tell the prpl to tell it to destroy
 * prpl-specific data relating to this PurpleDiscoList (which will be
 * destroyed imminently).
 *
 * @param list  The disco list.
 */
typedef void (*PurpleDiscoCloseFunc)(PurpleDiscoList *list);
typedef void (*PurpleDiscoServiceCloseFunc)(PurpleDiscoService *service);

typedef void (*PurpleDiscoServiceExpandFunc)(PurpleDiscoList *list,
                                             PurpleDiscoService *service);

/**
 * A prpl callback called to initiate registration with the specificed
 * service.
 *
 * @param pc       The connection.
 * @param service  The service to which to register.
 *
 */
typedef void (*PurpleDiscoRegisterFunc)(PurpleConnection *pc,
                                        PurpleDiscoService *service);

/**
 * The types of services.
 */
typedef enum
{
	PURPLE_DISCO_SERVICE_TYPE_UNSET,
	/**
	 * A registerable gateway to another protocol. An example would be
	 * XMPP legacy transports.
	 */
	PURPLE_DISCO_SERVICE_TYPE_GATEWAY,

	/**
	 * A directory (e.g. allows the user to search for other users).
	 */
	PURPLE_DISCO_SERVICE_TYPE_DIRECTORY,

	/**
	 * A chat (multi-user conversation).
	 */
	PURPLE_DISCO_SERVICE_TYPE_CHAT,

	/**
	 * Something else. Do we need more categories?
	 */
	PURPLE_DISCO_SERVICE_TYPE_OTHER
} PurpleDiscoServiceType;

/**
 * The flags of services.
 */
typedef enum
{
	PURPLE_DISCO_NONE          = 0x0000,
	PURPLE_DISCO_ADD           = 0x0001, /**< Supports an 'add' operation */
	PURPLE_DISCO_BROWSE        = 0x0002, /**< Supports browsing */
	PURPLE_DISCO_REGISTER      = 0x0004  /**< Supports a 'register' operation */
} PurpleDiscoServiceFlags;

struct _PurpleDiscoUiOps {
	/** Ask the UI to display a dialog for the specified account.
	 */
	void (*dialog_show_with_account)(PurpleAccount* account);
	void (*create)(PurpleDiscoList *list); /**< Sets UI-specific data on a disco list */
	void (*destroy)(PurpleDiscoList *list); /**< Free UI-specific data on the disco list */
	void (*add_service)(PurpleDiscoList *list, PurpleDiscoService *service, PurpleDiscoService *parent); /**< Add service to dialog */
	void (*in_progress)(PurpleDiscoList *list, gboolean in_progress); /**< Set progress to dialog */

	/* Padding */
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Returns a newly created service discovery object.
 *
 * It has an initial reference count of 1.
 *
 * @param account The account that's listing rooms.
 * @return The new service discovery list handle.
 */
PurpleDiscoList *purple_disco_list_new(PurpleAccount *account);

/**
 * Increases the reference count on the service discovery list.
 *
 * @param list The disco list to ref.
 */
PurpleDiscoList *purple_disco_list_ref(PurpleDiscoList *list);

/**
 * Decreases the reference count on the service discovery list.
 *
 * The room list will be destroyed when this reaches 0.
 *
 * @param list The room list object to unref and possibly
 *             destroy.
 */
void purple_disco_list_unref(PurpleDiscoList *list);

/**
 * Instructs the prpl to start fetching the list.
 *
 */
PurpleDiscoList *purple_disco_get_list(PurpleConnection *gc);

/**
 * Tells the prpl to stop fetching the list.
 * If this is possible and done, the prpl will
 * call set_in_progress with @c FALSE and possibly
 * unref the list if it took a reference.
 *
 * @param list The service list to cancel a get_list on.
 */
void purple_disco_cancel_get_list(PurpleDiscoList *list);

/**
 * Create new service object
 */
PurpleDiscoService *
purple_disco_list_service_new(PurpleDiscoServiceType type, const gchar *name,
                              const gchar *description,
                              PurpleDiscoServiceFlags flags,
                              gpointer proto_data);

/**
 * Add service to list
 */
void purple_disco_list_service_add(PurpleDiscoList *list, PurpleDiscoService *service, PurpleDiscoService *parent);

/**
 * Expand a (browsable) service. The UI should call this in order to
 * iteratively browse the children of this service. The service must
 * have the PURPLE_DISCO_BROWSE flag set.
 *
 * You probably don't want to call this if the service already has children.
 *
 * @param service  The browsable disco service.
 *
 * @since TODO
 */
void purple_disco_service_expand(PurpleDiscoService *service);

/**
 * Register service
 * @param service The service that will be registered
 */
void purple_disco_service_register(PurpleDiscoService *service);

/**
 * Returns a service's name.
 *
 * @param service The service.
 * @return The name.
 *
 * @since TODO
 */
const gchar *purple_disco_service_get_name(PurpleDiscoService *service);

/**
 * Return a service's description.
 *
 * @param service The service.
 * @return The description.
 *
 * @since TODO
 */
const gchar* purple_disco_service_get_description(PurpleDiscoService *service);

/**
 * Returns the service's protocol-specific data.
 *
 * This should only be called from the associated prpl.
 *
 * @param service The disco service.
 * @return        The protocol data.
 *
 * @since TODO
 */
gpointer purple_disco_service_get_protocol_data(PurpleDiscoService *service);

/**
 * Return a service's type.
 *
 * @param service The service.
 * @return The type.
 *
 * @since TODO
 */
PurpleDiscoServiceType purple_disco_service_get_type(PurpleDiscoService *service);

/**
 * Return a service's flags.
 *
 * @param service The service.
 * @return The flags.
 *
 * @since TODO
 */
PurpleDiscoServiceFlags purple_disco_service_get_flags(PurpleDiscoService *service);

/**
 * Set the gateway type for a gateway service. The gateway type is a string
 * that represents a canonical name of the protocol to which this service is
 * a gateway. For example, for an XMPP legacy transport to AIM, this would
 * be "aim".
 *
 * These strings should conform to the names of the libpurple prpls where
 * possible (so a UI can easily map types to icons) and, as a backup, the
 * XMPP registry list of gateways at
 * http://xmpp.org/registrar/disco-categories.html#gateway. 
 */
void purple_disco_service_set_gateway_type(PurpleDiscoService *service,
                                           const gchar *type);

/**
 * Get the gateway type for a gateway service.
 *
 * @param service The service.
 * @returns       The gateway type or NULL if none was set or service is not
 *                a gateway.
 *
 * @see purple_disco_service_set_gateway_type().
 */
const gchar *purple_disco_service_get_gateway_type(PurpleDiscoService *service);

/**
 * Get the account associated with a service list.
 *
 * @param list  The service list.
 * @return      The account
 *
 * @since TODO
 */
PurpleAccount* purple_disco_list_get_account(PurpleDiscoList *list);

/**
 * Get a list of the services associated with this service list.
 *
 * @param dl The serivce list.
 * @returns    A list of PurpleDiscoService items.
 *
 * @since TODO
 */
GList* purple_disco_list_get_services(PurpleDiscoList *dl);

/**
 * Set the service list's UI data.
 *
 * @param list  The service list.
 * @param data  The data.
 *
 * @see purple_disco_list_get_ui_data()
 * @since TODO
 */
void purple_disco_list_set_ui_data(PurpleDiscoList *list, gpointer data);

/**
 * Get the service list's UI data.
 *
 * @param list  The service list.
 * @return      The data.
 *
 * @see purple_disco_list_set_ui_data()
 * @since TODO
 */
gpointer purple_disco_list_get_ui_data(PurpleDiscoList *list);

/**
 * Set the "in progress" state of the Service Discovery.
 *
 * The UI is encouraged to somehow hint to the user
 * whether or not we're busy downloading a service list or not.
 *
 * @param list The service list.
 * @param in_progress We're downloading it, or we're not.
 *
 * @see purple_disco_list_get_in_progress()
 * @since TODO
 */
void purple_disco_list_set_in_progress(PurpleDiscoList *list, gboolean in_progress);

/**
 * Gets the "in progress" state of the Service Discovery.
 *
 * The UI is encouraged to somehow hint to the user
 * whether or not we're busy downloading a service list or not.
 *
 * @param list The service list.
 * @return True if we're downloading it, or false if we're not.
 *
 * @see purple_disco_list_set_in_progress()
 * @since TODO
 */
gboolean purple_disco_list_get_in_progress(PurpleDiscoList *list);

/**
 * Sets the disco list's protocol-specific data.
 *
 * This should only be called from the associated prpl.
 *
 * @param list     The disco list.
 * @param data     The protocol data.
 * @param close_cb The function to be called when destroying the disco list to
 *                 free the protocol data. May be NULL if the data does not need
 *                 to be freed.
 *
 * @see purple_disco_list_get_protocol_data()
 * @since TODO
 */
void purple_disco_list_set_protocol_data(PurpleDiscoList *list, gpointer data,
                                         PurpleDiscoCloseFunc cb);

/**
 * Returns the disco list's protocol-specific data.
 *
 * This should only be called from the associated prpl.
 *
 * @param list The disco list.
 * @return     The protocol data.
 *
 * @see purple_disco_list_set_protocol_data()
 * @since TODO
 */
gpointer purple_disco_list_get_protocol_data(PurpleDiscoList *list);

void purple_disco_list_set_service_close_func(PurpleDiscoList *list,
                                              PurpleDiscoServiceCloseFunc cb);
void purple_disco_list_set_cancel_func(PurpleDiscoList *list,
                                       PurpleDiscoCancelFunc cb);
void purple_disco_list_set_expand_func(PurpleDiscoList *list,
                                       PurpleDiscoServiceExpandFunc cb);
void purple_disco_list_set_register_func(PurpleDiscoList *list,
                                         PurpleDiscoRegisterFunc cb);

/**
 * Sets the UI operations structure to be used in all purple service discovery.
 *
 * @param ops The UI operations structure.
 *
 * @since TODO
 */
void purple_disco_set_ui_ops(PurpleDiscoUiOps *ui_ops);

/**
 * Returns the service discovery UI operations structure.
 *
 * @return A filled-out PurpleDiscoUiOps structure.
 *
 * @since TODO
 */
PurpleDiscoUiOps *purple_disco_get_ui_ops(void);

#ifdef __cplusplus
}
#endif

#endif /* _PURPLE_DISCO_H_ */
