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

/**
 * Represents a list of services for a given connection on a given protocol.
 */
struct _PurpleDiscoList {
	PurpleAccount *account; /**< The account this list belongs to. */
	GList *services; /**< The list of services. */
	gpointer *ui_data; /**< UI private data. */
	gboolean in_progress;
	gint fetch_count; /**< Uses in fetch processes */
	gpointer proto_data; /** Prpl private data. */
	guint ref; /**< The reference count. */
};

/**
 * The categories of services.
 */
typedef enum
{
	PURPLE_DISCO_SERVICE_CAT_UNSET,
	PURPLE_DISCO_SERVICE_CAT_GATEWAY,
	PURPLE_DISCO_SERVICE_CAT_DIRECTORY,
	PURPLE_DISCO_SERVICE_CAT_MUC,
	PURPLE_DISCO_SERVICE_CAT_OTHER
} PurpleDiscoServiceCategory;

/**
 * The types of services.
 */
typedef enum
{
	PURPLE_DISCO_SERVICE_TYPE_NONE,
	PURPLE_DISCO_SERVICE_TYPE_AIM,
	PURPLE_DISCO_SERVICE_TYPE_GG,
	PURPLE_DISCO_SERVICE_TYPE_GTALK,
	PURPLE_DISCO_SERVICE_TYPE_ICQ,
	PURPLE_DISCO_SERVICE_TYPE_IRC,
	PURPLE_DISCO_SERVICE_TYPE_MAIL,
	PURPLE_DISCO_SERVICE_TYPE_MSN
	PURPLE_DISCO_SERVICE_TYPE_USER,
	PURPLE_DISCO_SERVICE_TYPE_QQ,
	PURPLE_DISCO_SERVICE_TYPE_XMPP,
	PURPLE_DISCO_SERVICE_TYPE_YAHOO,
} PurpleDiscoServiceType;

/**
 * The flags of services.
 */
#define PURPLE_DISCO_FLAG_NONE			0
#define PURPLE_DISCO_FLAG_ADD			1 << 0
#define PURPLE_DISCO_FLAG_BROWSE		1 << 1
#define PURPLE_DISCO_FLAG_REGISTER		1 << 2

/**
 * Represents a list of services for a given connection on a given protocol.
 */
struct _PurpleDiscoService {
	PurpleDiscoList *list;
	PurpleDiscoServiceCategory category; /**< The category of service. */
	gchar *name; /**< The name of the service. */
	PurpleDiscoServiceType type; /**< The type of service. */
	guint flags;
	gchar *description; /**< The name of the service. */
};

struct _PurpleDiscoUiOps {
	void (*dialog_show_with_account)(PurpleAccount* account); /**< Force the ui to pop up a dialog */
	void (*create)(PurpleDiscoList *list); /**< Init ui resources */
	void (*destroy)(PurpleDiscoList *list); /**< Free ui resources */
	void (*add_service)(PurpleDiscoList *list, PurpleDiscoService *service, PurpleDiscoService *parent); /**< Add service to dialog */
	void (*in_progress)(PurpleDiscoList *list, gboolean in_progress); /**< Set progress to dialog */
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
PurpleDiscoList *purple_disco_list_new(PurpleAccount *account, void *ui_data);

/**
 * Increases the reference count on the service discovery list.
 *
 * @param list The object to ref.
 */
void purple_disco_list_ref(PurpleDiscoList *list);

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
 * @param gc The PurpleConnection to have get a list.
 *
 */
void purple_disco_get_list(PurpleConnection *gc, PurpleDiscoList *list);

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
PurpleDiscoService *purple_disco_list_service_new(PurpleDiscoServiceCategory category, const gchar *name,
		PurpleDiscoServiceType type, const gchar *description, int flags);

/**
 * Add service to list
 */
void purple_disco_list_service_add(PurpleDiscoList *list, PurpleDiscoService *service, PurpleDiscoService *parent);

/**
 * Set the "in progress" state of the Service Discovery.
 *
 * The UI is encouraged to somehow hint to the user
 * whether or not we're busy downloading a service list or not.
 *
 * @param list The service list.
 * @param in_progress We're downloading it, or we're not.
 */
void purple_disco_set_in_progress(PurpleDiscoList *list, gboolean in_progress);

/**
 * Gets the "in progress" state of the Service Discovery.
 *
 * The UI is encouraged to somehow hint to the user
 * whether or not we're busy downloading a service list or not.
 *
 * @param list The service list.
 * @return True if we're downloading it, or false if we're not.
 */
gboolean purple_disco_get_in_progress(PurpleDiscoList *list);


/**
 * Sets the UI operations structure to be used in all purple service discovery.
 *
 * @param ops The UI operations structure.
 */
void purple_disco_set_ui_ops(PurpleDiscoUiOps *ui_ops);

/**
 * Register service
 * @param gc Connection
 * @param service The service that will be registered
 */
int purple_disco_service_register(PurpleConnection *gc, PurpleDiscoService *service);

/**< Set/Get the account this list belongs to. */
void purple_disco_list_set_account(PurpleDiscoList *list, PurpleAccount *account);
PurpleAccount* purple_disco_list_get_account(PurpleDiscoList *list);

/**< The list of services. */
GList* spurple_disco_list_get_services(PurpleDiscoList *dl);

/**< Set/Get UI private data. */
void purple_disco_list_set_ui_data(PurpleDiscoList *list, gpointer ui_data);
gpointer purple_disco_list_get_ui_data(PurpleDiscoList *list);

/** Set/Get in progress flag */ 
void purple_disco_list_set_in_progress(PurpleDiscoList *list, gboolean in_progress);
gboolean purple_disco_list_get_in_progress(PurpleDiscoList *list);

/** Set/Get fetch counter */
void purple_disco_list_set_fetch_count(PurpleDiscoList *list, gint fetch_count);
gint purple_disco_list_get_fetch_count(PurpleDiscoList *list);

/** Set/Get prpl private data. */
void purple_disco_list_set_proto_data(PurpleDiscoList *list, gpointer proto_data);
gpointer purple_disco_list_get_proto_data(PurpleDiscoList *list);

#ifdef __cplusplus
}
#endif

#endif /* _PURPLE_DISCO_H_ */
