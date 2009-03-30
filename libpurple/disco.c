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
	gpointer proto_data; /** Prpl private data. */
	guint ref; /**< The reference count. */
};

/**
 * Represents a list of services for a given connection on a given protocol.
 */
struct _PurpleDiscoService {
	PurpleDiscoList *list;
	gchar *name; /**< The name of the service. */
	gchar *description; /**< The name of the service. */

	PurpleDiscoServiceCategory category; /**< The category of service. */
	PurpleDiscoServiceType type; /**< The type of service. */
	PurpleDiscoServiceFlags flags;
};

static PurpleDiscoUiOps *ops = NULL;

PurpleDiscoList *purple_disco_list_new(PurpleAccount *account, void *ui_data)
{
	PurpleDiscoList *list;

	g_return_val_if_fail(account != NULL, NULL);

	list = g_new0(PurpleDiscoList, 1);
	list->account = account;
	list->ref = 1;
	list->ui_data = ui_data;

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
	g_free(r->name);
	g_free(r->description);
	g_free(r);
}

static void purple_disco_list_destroy(PurpleDiscoList *list)
{
	GList *l;

	purple_debug_misc("disco", "destroying list %p\n", list);

	if (ops && ops->destroy)
		ops->destroy(list);

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

void purple_disco_list_service_add(PurpleDiscoList *list, PurpleDiscoService *service, PurpleDiscoService *parent)
{
	g_return_if_fail(list != NULL);
	g_return_if_fail(service != NULL);

	list->services = g_list_append(list->services, service);
	service->list = list;

	if (ops && ops->add_service)
		ops->add_service(list, service, parent);
}

PurpleDiscoService *purple_disco_list_service_new(PurpleDiscoServiceCategory category, const gchar *name,
		PurpleDiscoServiceType type, const gchar *description, PurpleDiscoServiceFlags flags)
{
	PurpleDiscoService *s;

	g_return_val_if_fail(name != NULL, NULL);
	g_return_val_if_fail(category != PURPLE_DISCO_SERVICE_CAT_UNSET, NULL);

	s = g_new0(PurpleDiscoService, 1);
	s->category = category;
	s->name = g_strdup(name);
	s->type = type;
	s->description = g_strdup(description);
	s->flags = flags;

	return s;
}

void purple_disco_get_list(PurpleDiscoList *list)
{
	PurpleConnection *pc = NULL;
	PurplePlugin *prpl = NULL;
	PurplePluginProtocolInfo *prpl_info = NULL;

	g_return_if_fail(list != NULL);

	pc = purple_account_get_connection(list->account);

	g_return_if_fail(pc != NULL);

	prpl = purple_connection_get_prpl(pc);

	if (prpl != NULL)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);

	if (prpl_info && PURPLE_PROTOCOL_PLUGIN_HAS_FUNC(prpl_info, disco_get_list))
		prpl_info->disco_get_list(pc, list);
}

void purple_disco_cancel_get_list(PurpleDiscoList *list)
{
	PurplePlugin *prpl = NULL;
	PurplePluginProtocolInfo *prpl_info = NULL;
	PurpleConnection *gc;

	g_return_if_fail(list != NULL);

	gc = purple_account_get_connection(list->account);

	g_return_if_fail(gc != NULL);

	if (gc)
		prpl = purple_connection_get_prpl(gc);

	if (prpl)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);

	if (prpl_info && PURPLE_PROTOCOL_PLUGIN_HAS_FUNC(prpl_info, disco_cancel))
		prpl_info->disco_cancel(list);
}

void purple_disco_service_register(PurpleDiscoService *service)
{
	PurpleConnection *pc;
	PurplePlugin *prpl = NULL;
	PurplePluginProtocolInfo *prpl_info = NULL;

	g_return_if_fail(service != NULL);
	
	pc = purple_account_get_connection(service->list->account);

	g_return_if_fail(pc != NULL);

	prpl = purple_connection_get_prpl(pc);

	if (prpl != NULL)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);

	if (prpl_info && PURPLE_PROTOCOL_PLUGIN_HAS_FUNC(prpl_info, disco_service_register))
		prpl_info->disco_service_register(pc, service);
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

PurpleDiscoServiceCategory
purple_disco_service_get_category(PurpleDiscoService *service)
{
	g_return_val_if_fail(service != NULL, PURPLE_DISCO_SERVICE_CAT_UNSET);

	return service->category;
}

PurpleDiscoServiceType
purple_disco_service_get_type(PurpleDiscoService *service)
{
	g_return_val_if_fail(service != NULL, PURPLE_DISCO_SERVICE_TYPE_NONE);

	return service->type;
}

PurpleDiscoServiceFlags
purple_disco_service_get_flags(PurpleDiscoService *service)
{
	g_return_val_if_fail(service != NULL, PURPLE_DISCO_NONE);

	return service->flags;
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
                                         gpointer proto_data)
{
	g_return_if_fail(list != NULL);

	list->proto_data = proto_data;
}

gpointer purple_disco_list_get_protocol_data(PurpleDiscoList *list)
{
	g_return_val_if_fail(list != NULL, NULL);

	return list->proto_data;
}

void purple_disco_set_ui_ops(PurpleDiscoUiOps *ui_ops)
{
	ops = ui_ops;
}

PurpleDiscoUiOps *purple_disco_get_ui_ops(void)
{
	return ops;
}
