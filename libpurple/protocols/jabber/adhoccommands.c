/*
 * purple - Jabber Protocol Plugin
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */

#include "internal.h"

#include "adhoccommands.h"
#include <string.h>
#include "internal.h"
#include "xdata.h"
#include "iq.h"
#include "request.h"

static void do_adhoc_ignoreme(JabberStream *js, ...) {
	/* we don't have to do anything */
}

typedef struct _JabberAdHocActionInfo {
	char *sessionid;
	char *who;
	char *node;
	GList *actionslist;
} JabberAdHocActionInfo;

static void
jabber_adhoc_got_buddy_list(JabberStream *js, const char *from, PurpleXmlNode *query)
{
	JabberID *jid;
	JabberBuddy *jb;
	JabberBuddyResource *jbr = NULL;
	PurpleXmlNode *item;

	if ((jid = jabber_id_new(from))) {
		if (jid->resource && (jb = jabber_buddy_find(js, from, TRUE)))
			jbr = jabber_buddy_find_resource(jb, jid->resource);
		jabber_id_free(jid);
	}

	if(!jbr)
		return;

	if(jbr->commands) {
		/* since the list we just received is complete, wipe the old one */
		while(jbr->commands) {
			JabberAdHocCommands *cmd = jbr->commands->data;
			g_free(cmd->jid);
			g_free(cmd->node);
			g_free(cmd->name);
			g_free(cmd);
			jbr->commands = g_list_delete_link(jbr->commands, jbr->commands);
		}
	}

	for(item = query->child; item; item = item->next) {
		JabberAdHocCommands *cmd;
		if(item->type != PURPLE_XMLNODE_TYPE_TAG)
			continue;
		if(strcmp(item->name, "item"))
			continue;
		cmd = g_new0(JabberAdHocCommands, 1);

		cmd->jid = g_strdup(purple_xmlnode_get_attrib(item,"jid"));
		cmd->node = g_strdup(purple_xmlnode_get_attrib(item,"node"));
		cmd->name = g_strdup(purple_xmlnode_get_attrib(item,"name"));

		jbr->commands = g_list_append(jbr->commands,cmd);
	}
}

void
jabber_adhoc_disco_result_cb(JabberStream *js, const char *from,
                             JabberIqType type, const char *id,
                             PurpleXmlNode *packet, gpointer data)
{
	PurpleXmlNode *query;
	const char *node;

	if (type == JABBER_IQ_ERROR)
		return;

	query = purple_xmlnode_get_child_with_namespace(packet, "query", NS_DISCO_ITEMS);
	if (!query)
		return;
	node = purple_xmlnode_get_attrib(query, "node");
	if (!purple_strequal(node, "http://jabber.org/protocol/commands"))
		return;

	jabber_adhoc_got_buddy_list(js, from, query);
}

static void jabber_adhoc_parse(JabberStream *js, const char *from,
                               JabberIqType type, const char *id,
                               PurpleXmlNode *packet, gpointer data);

static void do_adhoc_action_cb(JabberStream *js, PurpleXmlNode *result, const char *actionhandle, gpointer user_data) {
	PurpleXmlNode *command;
	GList *action;
	JabberAdHocActionInfo *actionInfo = user_data;
	JabberIq *iq = jabber_iq_new(js, JABBER_IQ_SET);
	jabber_iq_set_callback(iq, jabber_adhoc_parse, NULL);

	purple_xmlnode_set_attrib(iq->node, "to", actionInfo->who);
	command = purple_xmlnode_new_child(iq->node,"command");
	purple_xmlnode_set_namespace(command,"http://jabber.org/protocol/commands");
	purple_xmlnode_set_attrib(command,"sessionid",actionInfo->sessionid);
	purple_xmlnode_set_attrib(command,"node",actionInfo->node);

	/* cancel is handled differently on ad-hoc commands than regular forms */
	if (purple_strequal(purple_xmlnode_get_namespace(result), "jabber:x:data") &&
			purple_strequal(purple_xmlnode_get_attrib(result, "type"), "cancel")) {
		purple_xmlnode_set_attrib(command,"action","cancel");
	} else {
		if(actionhandle)
			purple_xmlnode_set_attrib(command,"action",actionhandle);
		purple_xmlnode_insert_child(command,result);
	}

	for(action = actionInfo->actionslist; action; action = g_list_next(action)) {
		char *handle = action->data;
		g_free(handle);
	}
	g_list_free(actionInfo->actionslist);
	g_free(actionInfo->sessionid);
	g_free(actionInfo->who);
	g_free(actionInfo->node);

	jabber_iq_send(iq);
}

static void
jabber_adhoc_parse(JabberStream *js, const char *from,
                   JabberIqType type, const char *id,
                   PurpleXmlNode *packet, gpointer data)
{
	PurpleXmlNode *command = purple_xmlnode_get_child_with_namespace(packet, "command", "http://jabber.org/protocol/commands");
	const char *status = purple_xmlnode_get_attrib(command,"status");
	PurpleXmlNode *xdata = purple_xmlnode_get_child_with_namespace(command,"x","jabber:x:data");

	if (type == JABBER_IQ_ERROR) {
		char *msg = jabber_parse_error(js, packet, NULL);
		if(!msg)
			msg = g_strdup(_("Unknown Error"));

		purple_notify_error(NULL, _("Ad-Hoc Command Failed"),
			_("Ad-Hoc Command Failed"), msg,
			purple_request_cpar_from_connection(js->gc));
		g_free(msg);
		return;
	}

	if(!status)
		return;

	if(!strcmp(status,"completed")) {
		/* display result */
		PurpleXmlNode *note = purple_xmlnode_get_child(command,"note");

		if(note) {
			char *data = purple_xmlnode_get_data(note);
			purple_notify_info(NULL, from, data, NULL,
				purple_request_cpar_from_connection(js->gc));
			g_free(data);
		}

		if(xdata)
			jabber_x_data_request(js, xdata, (jabber_x_data_cb)do_adhoc_ignoreme, NULL);
		return;
	}
	if(!strcmp(status,"executing")) {
		/* this command needs more steps */
		PurpleXmlNode *actions, *action;
		int actionindex = 0;
		GList *actionslist = NULL;
		JabberAdHocActionInfo *actionInfo;
		if(!xdata)
			return; /* shouldn't happen */

		actions = purple_xmlnode_get_child(command,"actions");
		if(!actions) {
			JabberXDataAction *defaultaction = g_new0(JabberXDataAction, 1);
			defaultaction->name = g_strdup(_("execute"));
			defaultaction->handle = g_strdup("execute");
			actionslist = g_list_append(actionslist, defaultaction);
		} else {
			const char *defaultactionhandle = purple_xmlnode_get_attrib(actions, "execute");
			int index = 0;
			for(action = actions->child; action; action = action->next, ++index) {
				if(action->type == PURPLE_XMLNODE_TYPE_TAG) {
					JabberXDataAction *newaction = g_new0(JabberXDataAction, 1);
					newaction->name = g_strdup(_(action->name));
					newaction->handle = g_strdup(action->name);
					actionslist = g_list_append(actionslist, newaction);
					if(defaultactionhandle && !strcmp(defaultactionhandle, action->name))
						actionindex = index;
				}
			}
		}

		actionInfo = g_new0(JabberAdHocActionInfo, 1);
		actionInfo->sessionid = g_strdup(purple_xmlnode_get_attrib(command,"sessionid"));
		actionInfo->who = g_strdup(from);
		actionInfo->node = g_strdup(purple_xmlnode_get_attrib(command,"node"));
		actionInfo->actionslist = actionslist;

		jabber_x_data_request_with_actions(js,xdata,actionslist,actionindex,do_adhoc_action_cb,actionInfo);
	}
}

void jabber_adhoc_execute_action(PurpleBlistNode *node, gpointer data) {
	if (PURPLE_IS_BUDDY(node)) {
		JabberAdHocCommands *cmd = data;
		PurpleBuddy *buddy = (PurpleBuddy *) node;
		PurpleAccount *account = purple_buddy_get_account(buddy);
		PurpleConnection *gc = purple_account_get_connection(account);
		JabberStream *js = purple_connection_get_protocol_data(gc);

		jabber_adhoc_execute(js, cmd);
	}
}

static void
jabber_adhoc_got_server_list(JabberStream *js, const char *from, PurpleXmlNode *query)
{
	PurpleXmlNode *item;

	if(!query)
		return;

	/* clean current list (just in case there is one) */
	while(js->commands) {
		JabberAdHocCommands *cmd = js->commands->data;
		g_free(cmd->jid);
		g_free(cmd->node);
		g_free(cmd->name);
		g_free(cmd);
		js->commands = g_list_delete_link(js->commands, js->commands);
	}

	/* re-fill list */
	for(item = query->child; item; item = item->next) {
		JabberAdHocCommands *cmd;
		if(item->type != PURPLE_XMLNODE_TYPE_TAG)
			continue;
		if(strcmp(item->name, "item"))
			continue;
		cmd = g_new0(JabberAdHocCommands, 1);
		cmd->jid = g_strdup(purple_xmlnode_get_attrib(item,"jid"));
		cmd->node = g_strdup(purple_xmlnode_get_attrib(item,"node"));
		cmd->name = g_strdup(purple_xmlnode_get_attrib(item,"name"));

		js->commands = g_list_append(js->commands,cmd);
	}

	if (js->state == JABBER_STREAM_CONNECTED)
		purple_protocol_got_account_actions(purple_connection_get_account(js->gc));
}

static void
jabber_adhoc_server_got_list_cb(JabberStream *js, const char *from,
                                JabberIqType type, const char *id,
                                PurpleXmlNode *packet, gpointer data)
{
	PurpleXmlNode *query = purple_xmlnode_get_child_with_namespace(packet, "query",
			NS_DISCO_ITEMS);

	jabber_adhoc_got_server_list(js, from, query);

}

void jabber_adhoc_got_list(JabberStream *js, const char *from, PurpleXmlNode *query)
{
	if (purple_strequal(from, js->user->domain)) {
		jabber_adhoc_got_server_list(js, from, query);
	} else {
		jabber_adhoc_got_buddy_list(js, from, query);
	}
}

void jabber_adhoc_server_get_list(JabberStream *js) {
	JabberIq *iq = jabber_iq_new_query(js, JABBER_IQ_GET, NS_DISCO_ITEMS);
	PurpleXmlNode *query = purple_xmlnode_get_child_with_namespace(iq->node, "query",
			NS_DISCO_ITEMS);

	purple_xmlnode_set_attrib(iq->node,"to",js->user->domain);
	purple_xmlnode_set_attrib(query,"node","http://jabber.org/protocol/commands");

	jabber_iq_set_callback(iq,jabber_adhoc_server_got_list_cb,NULL);
	jabber_iq_send(iq);
}

void jabber_adhoc_execute(JabberStream *js, JabberAdHocCommands *cmd) {
	JabberIq *iq = jabber_iq_new(js, JABBER_IQ_SET);
	PurpleXmlNode *command = purple_xmlnode_new_child(iq->node,"command");
	purple_xmlnode_set_attrib(iq->node,"to",cmd->jid);
	purple_xmlnode_set_namespace(command,"http://jabber.org/protocol/commands");
	purple_xmlnode_set_attrib(command,"node",cmd->node);
	purple_xmlnode_set_attrib(command,"action","execute");

	jabber_iq_set_callback(iq,jabber_adhoc_parse,NULL);

	jabber_iq_send(iq);
}

static void jabber_adhoc_server_execute(PurpleProtocolAction *action) {
	JabberAdHocCommands *cmd = action->user_data;
	if(cmd) {
		PurpleConnection *gc = (PurpleConnection *) action->connection;
		JabberStream *js = purple_connection_get_protocol_data(gc);

		jabber_adhoc_execute(js, cmd);
	}
}

void jabber_adhoc_init_server_commands(JabberStream *js, GList **m) {
	GList *cmdlst;
	JabberBuddy *jb;

	/* also add commands for other clients connected to the same account on another resource */
	char *accountname = g_strdup_printf("%s@%s", js->user->node, js->user->domain);
	if((jb = jabber_buddy_find(js, accountname, TRUE))) {
		GList *iter;
		for(iter = jb->resources; iter; iter = g_list_next(iter)) {
			JabberBuddyResource *jbr = iter->data;
			GList *riter;
			for(riter = jbr->commands; riter; riter = g_list_next(riter)) {
				JabberAdHocCommands *cmd = riter->data;
				char *cmdname = g_strdup_printf("%s (%s)",cmd->name,jbr->name);
				PurpleProtocolAction *act = purple_protocol_action_new(cmdname, jabber_adhoc_server_execute);
				act->user_data = cmd;
				*m = g_list_append(*m, act);
				g_free(cmdname);
			}
		}
	}
	g_free(accountname);

	/* now add server commands */
	for(cmdlst = js->commands; cmdlst; cmdlst = g_list_next(cmdlst)) {
		JabberAdHocCommands *cmd = cmdlst->data;
		PurpleProtocolAction *act = purple_protocol_action_new(cmd->name, jabber_adhoc_server_execute);
		act->user_data = cmd;
		*m = g_list_append(*m, act);
	}
}
