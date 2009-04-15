/**
 * @file disco.c Service Discovery API
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

#include "internal.h"
#include "debug.h"

#include "disco.h"

/**************************************************************************/
/* Main structures, members and constants                                 */
/**************************************************************************/

/**
 * Represents a list of services for a given connection on a given protocol.
 */
struct _PurpleDiscoList {
	PurpleAccount *account; /**< The account this list belongs to. */
	GList *services; /**< The list of services. */

	gboolean in_progress;

	gpointer ui_data; /**< UI private data. */
	gpointer proto_data; /**< Prpl private data. */
	guint ref; /**< The reference count. */

	struct {
		PurpleDiscoCancelFunc cancel_cb;
		PurpleDiscoCloseFunc close_cb; /**< Callback to free the prpl data */
		PurpleDiscoServiceCloseFunc service_close_cb;
		PurpleDiscoServiceExpandFunc expand_cb; /**< Expand a service (iteratively
		                                             look for things that are
		                                             "sub-elements" in the tree */
		PurpleDiscoRegisterFunc register_cb;
	} ops;
};

/**
 * Represents a list of services for a given connection on a given protocol.
 */
struct _PurpleDiscoService {
	PurpleDiscoList *list;
	gchar *name; /**< The name of the service. */
	gchar *description; /**< The name of the service. */

	gpointer proto_data;

	gchar *gateway_type; /**< The type of the gateway service. */
	PurpleDiscoServiceType type; /**< The type of service. */
	PurpleDiscoServiceFlags flags;
};

static PurpleDiscoUiOps *ops = NULL;

PurpleDiscoList *purple_disco_list_new(PurpleAccount *account)
{
	PurpleDiscoList *list;

	g_return_val_if_fail(account != NULL, NULL);

	list = g_new0(PurpleDiscoList, 1);
	list->account = account;
	list->ref = 1;

	if (ops && ops->create)
		ops->create(list);

	return list;
}

PurpleDiscoList *purple_disco_list_ref(PurpleDiscoList *list)
{
	g_return_val_if_fail(list != NULL, NULL);

	list->ref++;
	purple_debug_misc("disco", "reffing list, ref count now %d\n", list->ref);

	return list;
}

static void purple_disco_list_service_destroy(PurpleDiscoList *list, PurpleDiscoService *r)
{
	if (list->ops.service_close_cb)
		list->ops.service_close_cb(r);

	g_free(r->name);
	g_free(r->description);
	g_free(r->gateway_type);
	g_free(r);
}

static void purple_disco_list_destroy(PurpleDiscoList *list)
{
	GList *l;

	purple_debug_misc("disco", "destroying list %p\n", list);

	if (ops && ops->destroy)
		ops->destroy(list);

	if (list->ops.close_cb)
		list->ops.close_cb(list);

	for (l = list->services; l; l = l->next) {
		PurpleDiscoService *s = l->data;
		purple_disco_list_service_destroy(list, s);
	}
	g_list_free(list->services);

	g_free(list);
}

void purple_disco_list_unref(PurpleDiscoList *list)
{
	g_return_if_fail(list != NULL);
	g_return_if_fail(list->ref > 0);

	list->ref--;

	purple_debug_misc("disco", "unreffing list, ref count now %d\n", list->ref);
	if (list->ref == 0)
		purple_disco_list_destroy(list);
}

void purple_disco_list_service_add(PurpleDiscoList *list,
                                   PurpleDiscoService *service,
                                   PurpleDiscoService *parent)
{
	g_return_if_fail(list != NULL);
	g_return_if_fail(service != NULL);

	list->services = g_list_append(list->services, service);
	service->list = list;

	if (ops && ops->add_service)
		ops->add_service(list, service, parent);
}

PurpleDiscoService *purple_disco_list_service_new(PurpleDiscoServiceType type,
		const gchar *name, const gchar *description,
		PurpleDiscoServiceFlags flags, gpointer proto_data)
{
	PurpleDiscoService *s;

	g_return_val_if_fail(name != NULL, NULL);
	g_return_val_if_fail(type != PURPLE_DISCO_SERVICE_TYPE_UNSET, NULL);

	s = g_new0(PurpleDiscoService, 1);
	s->name = g_strdup(name);
	s->type = type;
	s->description = g_strdup(description);
	s->flags = flags;
	s->proto_data = proto_data;

	return s;
}

PurpleDiscoList *purple_disco_get_list(PurpleConnection *pc)
{
	PurplePlugin *prpl = NULL;
	PurplePluginProtocolInfo *prpl_info = NULL;

	g_return_val_if_fail(pc != NULL, NULL);

	prpl = purple_connection_get_prpl(pc);
	if (prpl != NULL)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);

	if (prpl_info && PURPLE_PROTOCOL_PLUGIN_HAS_FUNC(prpl_info, disco_get_list))
		return prpl_info->disco_get_list(pc);

	return NULL;
}

void purple_disco_cancel_get_list(PurpleDiscoList *list)
{
	PurpleConnection *pc;

	g_return_if_fail(list != NULL);

	pc = purple_account_get_connection(list->account);

	g_return_if_fail(pc != NULL);

	if (list->ops.cancel_cb)
		list->ops.cancel_cb(list);

	purple_disco_list_set_in_progress(list, FALSE);
}

void purple_disco_service_expand(PurpleDiscoService *service)
{
	PurpleDiscoList *list;
	PurpleConnection *pc;

	g_return_if_fail(service != NULL);
	g_return_if_fail((service->flags & PURPLE_DISCO_BROWSE));

	list = service->list;
	pc = purple_account_get_connection(list->account);
	
	g_return_if_fail(pc != NULL);

	purple_disco_list_set_in_progress(list, TRUE);

	if (list->ops.expand_cb)
		list->ops.expand_cb(list, service);
	else
		purple_debug_warning("disco", "Cannot expand %s for account %s, "
		                              "protocol did not provide expand op.\n",
		                     service->name,
		                     purple_account_get_username(list->account));
}

void purple_disco_service_register(PurpleDiscoService *service)
{
	PurpleDiscoList *list;
	PurpleConnection *pc;

	g_return_if_fail(service != NULL);

	list = service->list;
	pc = purple_account_get_connection(list->account);

	g_return_if_fail(pc != NULL);

	if (list->ops.register_cb)
		list->ops.register_cb(pc, service);
	else
		purple_debug_warning("disco", "Cannot register to %s for account %s, "
		                              "protocol did not provide register op.\n",
		                     service->name,
		                     purple_account_get_username(list->account));
}

const gchar *purple_disco_service_get_name(PurpleDiscoService *service)
{
	g_return_val_if_fail(service != NULL, NULL);

	return service->name;
}

const gchar *purple_disco_service_get_description(PurpleDiscoService *service)
{
	g_return_val_if_fail(service != NULL, NULL);

	return service->description;
}

gpointer
purple_disco_service_get_protocol_data(PurpleDiscoService *service)
{
	g_return_val_if_fail(service != NULL, NULL);

	return service->proto_data;
}

PurpleDiscoServiceType
purple_disco_service_get_type(PurpleDiscoService *service)
{
	g_return_val_if_fail(service != NULL, PURPLE_DISCO_SERVICE_TYPE_UNSET);

	return service->type;
}

PurpleDiscoServiceFlags
purple_disco_service_get_flags(PurpleDiscoService *service)
{
	g_return_val_if_fail(service != NULL, PURPLE_DISCO_NONE);

	return service->flags;
}

void purple_disco_service_set_gateway_type(PurpleDiscoService *service, const gchar *type)
{
	g_return_if_fail(service != NULL);

	g_free(service->gateway_type);
	service->gateway_type = g_strdup(type);
}

const gchar *purple_disco_service_get_gateway_type(PurpleDiscoService *service)
{
	g_return_val_if_fail(service != NULL, NULL);

	return service->gateway_type;
}

PurpleAccount* purple_disco_list_get_account(PurpleDiscoList *list)
{
	g_return_val_if_fail(list != NULL, NULL);

	return list->account;
}

GList* purple_disco_list_get_services(PurpleDiscoList *list)
{
	g_return_val_if_fail(list != NULL, NULL);

	return list->services;
}

void purple_disco_list_set_ui_data(PurpleDiscoList *list, gpointer ui_data)
{
	g_return_if_fail(list != NULL);

	list->ui_data = ui_data;
}

gpointer purple_disco_list_get_ui_data(PurpleDiscoList *list)
{
	g_return_val_if_fail(list != NULL, NULL);

	return list->ui_data;
}

void purple_disco_list_set_in_progress(PurpleDiscoList *list,
                                       gboolean in_progress)
{
	g_return_if_fail(list != NULL);

	list->in_progress = in_progress;

	if (ops && ops->in_progress)
		ops->in_progress(list, in_progress);
}

gboolean purple_disco_list_get_in_progress(PurpleDiscoList *list) 
{
	g_return_val_if_fail(list != NULL, FALSE);

	return list->in_progress;
}

void purple_disco_list_set_protocol_data(PurpleDiscoList *list,
                                         gpointer proto_data,
                                         PurpleDiscoCloseFunc cb)
{
	g_return_if_fail(list != NULL);

	list->proto_data   = proto_data;
	list->ops.close_cb = cb;
}

gpointer purple_disco_list_get_protocol_data(PurpleDiscoList *list)
{
	g_return_val_if_fail(list != NULL, NULL);

	return list->proto_data;
}

void purple_disco_list_set_service_close_func(PurpleDiscoList *list,
                                              PurpleDiscoServiceCloseFunc cb)
{
	g_return_if_fail(list != NULL);

	list->ops.service_close_cb = cb;
}

void purple_disco_list_set_cancel_func(PurpleDiscoList *list, PurpleDiscoCancelFunc cb)
{
	g_return_if_fail(list != NULL);

	list->ops.cancel_cb = cb;
}

void purple_disco_list_set_expand_func(PurpleDiscoList *list, PurpleDiscoServiceExpandFunc cb)
{
	g_return_if_fail(list != NULL);

	list->ops.expand_cb = cb;
}

void purple_disco_list_set_register_func(PurpleDiscoList *list, PurpleDiscoRegisterFunc cb)
{
	g_return_if_fail(list != NULL);

	list->ops.register_cb = cb;
}

void purple_disco_set_ui_ops(PurpleDiscoUiOps *ui_ops)
{
	ops = ui_ops;
}

PurpleDiscoUiOps *purple_disco_get_ui_ops(void)
{
	return ops;
}
