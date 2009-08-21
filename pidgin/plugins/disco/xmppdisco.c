/*
 * Purple - XMPP Service Disco Browser
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301 USA
 *
 */

/* TODO list (a little bit of a brain dump):
	* Support more actions than "register" and "add" based on context.
		- Subscribe to pubsub nodes (just...because?)
		- Execute ad-hoc commands
		- Change 'Register' to 'Unregister' if we're registered?
		- Administer MUCs
	* Enumerate pubsub node contents.
		- PEP too? (useful development tool at times)
	* See if we can better handle the ad-hoc commands that ejabberd returns
	  when disco'ing a server as an administrator:
from disco#items:
	<item jid='darkrain42.org' node='announce' name='Announcements'/>
disco#info:
	<iq from='darkrain42.org' type='result'>
		<query xmlns='http://jabber.org/protocol/disco#info' node='announce'/>
	</iq>
	* For services that are a JID w/o a node, handle fetching ad-hoc commands?
*/

#include "internal.h"
#include "pidgin.h"

#include "debug.h"
#include "signals.h"
#include "version.h"

#include "gtkconv.h"
#include "gtkimhtml.h"
#include "gtkplugin.h"

#include "xmppdisco.h"
#include "gtkdisco.h"

/* Variables */
PurplePlugin *my_plugin = NULL;
static GHashTable *iq_callbacks = NULL;
static gboolean iq_listening = FALSE;

typedef void (*XmppIqCallback)(PurpleConnection *pc, const char *type,
                               const char *id, const char *from, xmlnode *iq,
                               gpointer data);

struct item_data {
	PidginDiscoList *list;
	XmppDiscoService *parent;
	char *name;
	char *node; /* disco#info replies don't always include the node */
};

struct xmpp_iq_cb_data
{
	/*
	 * Every IQ callback in this plugin uses the same structure for the
	 * callback data. It's a hack (it wouldn't scale), but it's used so that
	 * it's easy to clean up all the callbacks when the account disconnects
	 * (see remove_iq_callbacks_by_pc below).
	 */
	struct item_data *context;
	PurpleConnection *pc;
	XmppIqCallback cb;
};


static char*
generate_next_id()
{
	static guint32 index = 0;

	if (index == 0) {
		do {
			index = g_random_int();
		} while (index == 0);
	}

	return g_strdup_printf("purpledisco%x", index++);
}

static gboolean
remove_iq_callbacks_by_pc(gpointer key, gpointer value, gpointer user_data)
{
	struct xmpp_iq_cb_data *cb_data = value;

	if (cb_data && cb_data->pc == user_data) {
		struct item_data *item_data = cb_data->context;

		if (item_data) {
			pidgin_disco_list_unref(item_data->list);
			g_free(item_data->name);
			g_free(item_data->node);
			g_free(item_data);
		}

		return TRUE;
	} else
		return FALSE;
}

static gboolean
xmpp_iq_received(PurpleConnection *pc, const char *type, const char *id,
                 const char *from, xmlnode *iq)
{
	struct xmpp_iq_cb_data *cb_data;

	cb_data = g_hash_table_lookup(iq_callbacks, id);
	if (!cb_data)
		return FALSE;

	cb_data->cb(cb_data->pc, type, id, from, iq, cb_data->context);

	g_hash_table_remove(iq_callbacks, id);
	if (g_hash_table_size(iq_callbacks) == 0) {
		PurplePlugin *prpl = purple_connection_get_prpl(pc);
		iq_listening = FALSE;
		purple_signal_disconnect(prpl, "jabber-receiving-iq", my_plugin,
		                         PURPLE_CALLBACK(xmpp_iq_received));
	}

	/* Om nom nom nom */
	return TRUE;
}

static void
xmpp_iq_register_callback(PurpleConnection *pc, gchar *id, gpointer data,
                          XmppIqCallback cb)
{
	struct xmpp_iq_cb_data *cbdata = g_new0(struct xmpp_iq_cb_data, 1);

	cbdata->context = data;
	cbdata->cb = cb;
	cbdata->pc = pc;

	g_hash_table_insert(iq_callbacks, id, cbdata);

	if (!iq_listening) {
		PurplePlugin *prpl = purple_plugins_find_with_id(XMPP_PLUGIN_ID);
		iq_listening = TRUE;
		purple_signal_connect(prpl, "jabber-receiving-iq", my_plugin,
		                      PURPLE_CALLBACK(xmpp_iq_received), NULL);
	}
}

static void
xmpp_disco_info_do(PurpleConnection *pc, gpointer cbdata, const char *jid,
                   const char *node, XmppIqCallback cb)
{
	xmlnode *iq, *query;
	char *id = generate_next_id();

	iq = xmlnode_new("iq");
	xmlnode_set_attrib(iq, "type", "get");
	xmlnode_set_attrib(iq, "to", jid);
	xmlnode_set_attrib(iq, "id", id);

	query = xmlnode_new_child(iq, "query");
	xmlnode_set_namespace(query, NS_DISCO_INFO);
	if (node)
		xmlnode_set_attrib(query, "node", node);

	/* Steals id */
	xmpp_iq_register_callback(pc, id, cbdata, cb);

	purple_signal_emit(purple_connection_get_prpl(pc), "jabber-sending-xmlnode",
	                   pc, &iq);
	if (iq != NULL)
		xmlnode_free(iq);
}

static void
xmpp_disco_items_do(PurpleConnection *pc, gpointer cbdata, const char *jid,
                    const char *node, XmppIqCallback cb)
{
	xmlnode *iq, *query;
	char *id = generate_next_id();

	iq = xmlnode_new("iq");
	xmlnode_set_attrib(iq, "type", "get");
	xmlnode_set_attrib(iq, "to", jid);
	xmlnode_set_attrib(iq, "id", id);

	query = xmlnode_new_child(iq, "query");
	xmlnode_set_namespace(query, NS_DISCO_ITEMS);
	if (node)
		xmlnode_set_attrib(query, "node", node);

	/* Steals id */
	xmpp_iq_register_callback(pc, id, cbdata, cb);

	purple_signal_emit(purple_connection_get_prpl(pc), "jabber-sending-xmlnode",
	                   pc, &iq);
	if (iq != NULL)
		xmlnode_free(iq);
}

static XmppDiscoServiceType
disco_service_type_from_identity(xmlnode *identity)
{
	const char *category, *type;

	if (!identity)
		return XMPP_DISCO_SERVICE_TYPE_OTHER;

	category = xmlnode_get_attrib(identity, "category");
	type = xmlnode_get_attrib(identity, "type");

	if (!category)
		return XMPP_DISCO_SERVICE_TYPE_OTHER;

	if (g_str_equal(category, "conference"))
		return XMPP_DISCO_SERVICE_TYPE_CHAT;
	else if (g_str_equal(category, "directory"))
		return XMPP_DISCO_SERVICE_TYPE_DIRECTORY;
	else if (g_str_equal(category, "gateway"))
		return XMPP_DISCO_SERVICE_TYPE_GATEWAY;
	else if (g_str_equal(category, "pubsub")) {
		if (!type || g_str_equal(type, "collection"))
			return XMPP_DISCO_SERVICE_TYPE_PUBSUB_COLLECTION;
		else if (g_str_equal(type, "leaf"))
			return XMPP_DISCO_SERVICE_TYPE_PUBSUB_LEAF;
		else if (g_str_equal(type, "service"))
			return XMPP_DISCO_SERVICE_TYPE_OTHER;
		else {
			purple_debug_warning("xmppdisco", "Unknown pubsub type '%s'\n", type);
			return XMPP_DISCO_SERVICE_TYPE_OTHER;
		}
	}

	return XMPP_DISCO_SERVICE_TYPE_OTHER;
}

static const struct {
	const char *from;
	const char *to;
} disco_type_mappings[] = {
	{ "gadu-gadu", "gadu-gadu" }, /* the prpl is prpl-gg, but list_icon returns "gadu-gadu" */
	{ "sametime",  "meanwhile" },
	{ "myspaceim", "myspace" },
	{ "xmpp",      "jabber" }, /* prpl-jabber (mentioned in case the prpl is renamed so this line will match) */
	{ NULL,        NULL }
};

static const gchar *
disco_type_from_string(const gchar *str)
{
	int i = 0;

	g_return_val_if_fail(str != NULL, "");

	for ( ; disco_type_mappings[i].from; ++i) {
		if (!strcasecmp(str, disco_type_mappings[i].from))
			return disco_type_mappings[i].to;
	}

	/* fallback to the string itself */
	return str;
}

static void
got_info_cb(PurpleConnection *pc, const char *type, const char *id,
            const char *from, xmlnode *iq, gpointer data)
{
	struct item_data *item_data = data;
	PidginDiscoList *list = item_data->list;
	xmlnode *query;

	--list->fetch_count;

	if (!list->in_progress)
		goto out;

	if (g_str_equal(type, "result") &&
			(query = xmlnode_get_child(iq, "query"))) {
		xmlnode *identity = xmlnode_get_child(query, "identity");
		XmppDiscoService *service;
		xmlnode *feature;

		service = g_new0(XmppDiscoService, 1);
		service->list = item_data->list;
		purple_debug_info("xmppdisco", "parent for %s is %p\n", from, item_data->parent);
		service->parent = item_data->parent;
		service->flags = 0;
		service->type = disco_service_type_from_identity(identity);

		if (item_data->node) {
			if (item_data->name) {
				service->name = item_data->name;
				item_data->name = NULL;
			} else
				service->name = g_strdup(item_data->node);

			service->node = item_data->node;
			item_data->node = NULL;

			if (service->type == XMPP_DISCO_SERVICE_TYPE_PUBSUB_COLLECTION)
				service->flags |= XMPP_DISCO_BROWSE;
		} else
			service->name = g_strdup(from);

		if (!service->node)
			/* Only support adding JIDs, not JID+node combos */
			service->flags |= XMPP_DISCO_ADD;

		if (item_data->name) {
			service->description = item_data->name;
			item_data->name = NULL;
		} else if (identity)
			service->description = g_strdup(xmlnode_get_attrib(identity, "name"));

		/* TODO: Overlap with service->name a bit */
		service->jid = g_strdup(from);

		for (feature = xmlnode_get_child(query, "feature"); feature;
				feature = xmlnode_get_next_twin(feature)) {
			const char *var;
			if (!(var = xmlnode_get_attrib(feature, "var")))
				continue;

			if (g_str_equal(var, NS_REGISTER))
				service->flags |= XMPP_DISCO_REGISTER;
			else if (g_str_equal(var, NS_DISCO_ITEMS))
				service->flags |= XMPP_DISCO_BROWSE;
			else if (g_str_equal(var, NS_MUC)) {
				service->flags |= XMPP_DISCO_BROWSE;
				service->type = XMPP_DISCO_SERVICE_TYPE_CHAT;
			}
		}

		if (service->type == XMPP_DISCO_SERVICE_TYPE_GATEWAY)
			service->gateway_type = g_strdup(disco_type_from_string(
					xmlnode_get_attrib(identity, "type")));

		pidgin_disco_add_service(list, service, service->parent);
	}

out:
	if (list->fetch_count == 0)
		pidgin_disco_list_set_in_progress(list, FALSE);

	g_free(item_data->name);
	g_free(item_data->node);
	g_free(item_data);
	pidgin_disco_list_unref(list);
}

static void
got_items_cb(PurpleConnection *pc, const char *type, const char *id,
             const char *from, xmlnode *iq, gpointer data)
{
	struct item_data *item_data = data;
	PidginDiscoList *list = item_data->list;
	xmlnode *query;
	gboolean has_items = FALSE;

	--list->fetch_count;

	if (!list->in_progress)
		goto out;

	if (g_str_equal(type, "result") &&
			(query = xmlnode_get_child(iq, "query"))) {
		xmlnode *item;

		for (item = xmlnode_get_child(query, "item"); item;
				item = xmlnode_get_next_twin(item)) {
			const char *jid = xmlnode_get_attrib(item, "jid");
			const char *name = xmlnode_get_attrib(item, "name");
			const char *node = xmlnode_get_attrib(item, "node");

			has_items = TRUE;

			if (item_data->parent->type == XMPP_DISCO_SERVICE_TYPE_CHAT) {
				/* This is a hacky first-order approximation. Any MUC
				 * component that has a >1 level hierarchy (a Yahoo MUC
				 * transport component probably does) will violate this.
				 *
				 * On the other hand, this is better than querying all the
				 * chats at conference.jabber.org to enumerate them.
				 */
				XmppDiscoService *service = g_new0(XmppDiscoService, 1);
				service->list = item_data->list;
				service->parent = item_data->parent;
				service->flags = XMPP_DISCO_ADD;
				service->type = XMPP_DISCO_SERVICE_TYPE_CHAT;

				service->name = g_strdup(name);
				service->jid = g_strdup(jid);
				service->node = g_strdup(node);
				pidgin_disco_add_service(list, service, item_data->parent);
			} else {
				struct item_data *item_data2 = g_new0(struct item_data, 1);

				item_data2->list = item_data->list;
				item_data2->parent = item_data->parent;
				item_data2->name = g_strdup(name);
				item_data2->node = g_strdup(node);

				++list->fetch_count;
				pidgin_disco_list_ref(list);
				xmpp_disco_info_do(pc, item_data2, jid, node, got_info_cb);
			}
		}
	}

	if (!has_items)
		pidgin_disco_add_service(list, NULL, item_data->parent);

out:
	if (list->fetch_count == 0)
		pidgin_disco_list_set_in_progress(list, FALSE);

	g_free(item_data);
	pidgin_disco_list_unref(list);
}

static void
server_items_cb(PurpleConnection *pc, const char *type, const char *id,
                const char *from, xmlnode *iq, gpointer data)
{
	struct item_data *cb_data = data;
	PidginDiscoList *list = cb_data->list;
	xmlnode *query;

	g_free(cb_data);
	--list->fetch_count;

	if (g_str_equal(type, "result") &&
			(query = xmlnode_get_child(iq, "query"))) {
		xmlnode *item;

		for (item = xmlnode_get_child(query, "item"); item;
				item = xmlnode_get_next_twin(item)) {
			const char *jid = xmlnode_get_attrib(item, "jid");
			const char *name = xmlnode_get_attrib(item, "name");
			const char *node = xmlnode_get_attrib(item, "node");
			struct item_data *item_data;

			if (!jid)
				continue;

			item_data = g_new0(struct item_data, 1);
			item_data->list = list;
			item_data->name = g_strdup(name);
			item_data->node = g_strdup(node);

			++list->fetch_count;
			pidgin_disco_list_ref(list);
			xmpp_disco_info_do(pc, item_data, jid, node, got_info_cb);
		}
	}

	if (list->fetch_count == 0)
		pidgin_disco_list_set_in_progress(list, FALSE);

	pidgin_disco_list_unref(list);
}

static void
server_info_cb(PurpleConnection *pc, const char *type, const char *id,
               const char *from, xmlnode *iq, gpointer data)
{
	struct item_data *cb_data = data;
	PidginDiscoList *list = cb_data->list;
	xmlnode *query;
	xmlnode *error;
	gboolean items = FALSE;

	--list->fetch_count;

	if (g_str_equal(type, "result") &&
			(query = xmlnode_get_child(iq, "query"))) {
		xmlnode *feature;

		for (feature = xmlnode_get_child(query, "feature"); feature;
				feature = xmlnode_get_next_twin(feature)) {
			const char *var = xmlnode_get_attrib(feature, "var");
			if (purple_strequal(var, NS_DISCO_ITEMS)) {
				items = TRUE;
				break;
			}
		}

		if (items) {
			xmpp_disco_items_do(pc, cb_data, from, NULL /* node */, server_items_cb);
			++list->fetch_count;
			pidgin_disco_list_ref(list);
		}
		else {
			pidgin_disco_list_set_in_progress(list, FALSE);
			g_free(cb_data);
		}
	}
	else {
		error = xmlnode_get_child(iq, "error");
		if (xmlnode_get_child(error, "remote-server-not-found")
		 || xmlnode_get_child(error, "jid-malformed")) {
			purple_notify_error(my_plugin, _("Error"),
			                    _("Server does not exist"),
 			                    NULL);
		}
		else {
			purple_notify_error(my_plugin, _("Error"),
			                    _("Server does not support service discovery"),
			                    NULL);
		}
		pidgin_disco_list_set_in_progress(list, FALSE);
		g_free(cb_data);
	}

	pidgin_disco_list_unref(list);
}

void xmpp_disco_start(PidginDiscoList *list)
{
	struct item_data *cb_data;

	g_return_if_fail(list != NULL);

	++list->fetch_count;
	pidgin_disco_list_ref(list);

	cb_data = g_new0(struct item_data, 1);
	cb_data->list = list;

	xmpp_disco_info_do(list->pc, cb_data, list->server, NULL, server_info_cb);
}

void xmpp_disco_service_expand(XmppDiscoService *service)
{
	struct item_data *item_data;

	g_return_if_fail(service != NULL);

	if (service->expanded)
		return;

	item_data = g_new0(struct item_data, 1);
	item_data->list = service->list;
	item_data->parent = service;

	++service->list->fetch_count;
	pidgin_disco_list_ref(service->list);

	pidgin_disco_list_set_in_progress(service->list, TRUE);

	xmpp_disco_items_do(service->list->pc, item_data, service->jid, service->node,
	                    got_items_cb);
	service->expanded = TRUE;
}

void xmpp_disco_service_register(XmppDiscoService *service)
{
	xmlnode *iq, *query;
	char *id = generate_next_id();

	iq = xmlnode_new("iq");
	xmlnode_set_attrib(iq, "type", "get");
	xmlnode_set_attrib(iq, "to", service->jid);
	xmlnode_set_attrib(iq, "id", id);

	query = xmlnode_new_child(iq, "query");
	xmlnode_set_namespace(query, NS_REGISTER);

	purple_signal_emit(purple_connection_get_prpl(service->list->pc),
			"jabber-sending-xmlnode", service->list->pc, &iq);
	if (iq != NULL)
		xmlnode_free(iq);
	g_free(id);
}

static void
create_dialog(PurplePluginAction *action)
{
	pidgin_disco_dialog_new();
}

static GList *
actions(PurplePlugin *plugin, gpointer context)
{
	GList *l = NULL;
	PurplePluginAction *action = NULL;

	action = purple_plugin_action_new(_("XMPP Service Discovery"),
	                                  create_dialog);
	l = g_list_prepend(l, action);

	return l;
}

static void
signed_off_cb(PurpleConnection *pc, gpointer unused)
{
	/* Deal with any dialogs */
	pidgin_disco_signed_off_cb(pc);

	/* Remove all the IQ callbacks for this connection */
	g_hash_table_foreach_remove(iq_callbacks, remove_iq_callbacks_by_pc, pc);
}

static gboolean
plugin_load(PurplePlugin *plugin)
{
	PurplePlugin *xmpp_prpl;

	my_plugin = plugin;

	xmpp_prpl = purple_plugins_find_with_id(XMPP_PLUGIN_ID);
	if (NULL == xmpp_prpl)
		return FALSE;

	purple_signal_connect(purple_connections_get_handle(), "signing-off",
	                      plugin, PURPLE_CALLBACK(signed_off_cb), NULL);

	iq_callbacks = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
	g_hash_table_destroy(iq_callbacks);
	iq_callbacks = NULL;

	purple_signals_disconnect_by_handle(plugin);
	pidgin_disco_dialogs_destroy_all();

	return TRUE;
}

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,
	PIDGIN_PLUGIN_TYPE,
	0,
	NULL,
	PURPLE_PRIORITY_DEFAULT,
	"gtk-xmppdisco",
	N_("XMPP Service Discovery"),
	DISPLAY_VERSION,
	N_("Allows browsing and registering services."),
	N_("This plugin is useful for registering with legacy transports or other "
	   "XMPP services."),
	"Paul Aurich <paul@darkrain42.org>",
	PURPLE_WEBSITE,
	plugin_load,
	plugin_unload,
	NULL,               /**< destroy    */
	NULL,               /**< ui_info    */
	NULL,               /**< extra_info */
	NULL,               /**< prefs_info */
	actions,

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(PurplePlugin *plugin)
{
}

PURPLE_INIT_PLUGIN(xmppdisco, init_plugin, info)
