/**
 * @file tcl_cmds.c Commands for the Gaim Tcl plugin bindings
 *
 * gaim
 *
 * Copyright (C) 2003 Ethan Blanton <eblanton@cs.purdue.edu>
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

#include <tcl.h>

#include "internal.h"
#include "conversation.h"
#include "connection.h"
#include "account.h"
#include "server.h"
#include "notify.h"
#include "blist.h"
#include "debug.h"
#include "prefs.h"
#include "core.h"

#include "tcl_gaim.h"

static GaimAccount *tcl_validate_account(Tcl_Obj *obj, Tcl_Interp *interp);
static GaimConversation *tcl_validate_conversation(Tcl_Obj *obj, Tcl_Interp *interp);
static GaimConnection *tcl_validate_gc(Tcl_Obj *obj, Tcl_Interp *interp);

static GaimAccount *tcl_validate_account(Tcl_Obj *obj, Tcl_Interp *interp)
{
	GaimAccount *account;
	GList *cur;

	account = gaim_tcl_ref_get(interp, obj, GaimTclRefAccount);

	if (account == NULL)
		return NULL;

	for (cur = gaim_accounts_get_all(); cur != NULL; cur = g_list_next(cur)) {
		if (account == cur->data)
			return account;
	}
	if (interp != NULL)
		Tcl_SetStringObj(Tcl_GetObjResult(interp), "invalid account", -1);
	return NULL;
}

static GaimConversation *tcl_validate_conversation(Tcl_Obj *obj, Tcl_Interp *interp)
{
	GaimConversation *convo;
	GList *cur;

	convo = gaim_tcl_ref_get(interp, obj, GaimTclRefConversation);

	if (convo == NULL)
		return NULL;

	for (cur = gaim_get_conversations(); cur != NULL; cur = g_list_next(cur)) {
		if (convo == cur->data)
			return convo;
	}
	if (interp != NULL)
		Tcl_SetStringObj(Tcl_GetObjResult(interp), "invalid conversation", -1);
	return NULL;
}

static GaimConnection *tcl_validate_gc(Tcl_Obj *obj, Tcl_Interp *interp)
{
	GaimConnection *gc;
	GList *cur;

	gc = gaim_tcl_ref_get(interp, obj, GaimTclRefConnection);

	if (gc == NULL)
		return NULL;

	for (cur = gaim_connections_get_all(); cur != NULL; cur = g_list_next(cur)) {
		if (gc == cur->data)
			return gc;
	}
	return NULL;
}

int tcl_cmd_account(ClientData unused, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
	Tcl_Obj *result = Tcl_GetObjResult(interp), *list, *elem;
	const char *cmds[] = { "alias", "connect", "connection", "disconnect",
	                       "enabled", "find", "handle", "isconnected",
	                       "list", "presence", "protocol", "status",
	                       "status_type", "status_types", "username",
	                       NULL };
	enum { CMD_ACCOUNT_ALIAS,
	       CMD_ACCOUNT_CONNECT, CMD_ACCOUNT_CONNECTION,
	       CMD_ACCOUNT_DISCONNECT, CMD_ACCOUNT_ENABLED, CMD_ACCOUNT_FIND,
	       CMD_ACCOUNT_HANDLE, CMD_ACCOUNT_ISCONNECTED, CMD_ACCOUNT_LIST,
	       CMD_ACCOUNT_PRESENCE, CMD_ACCOUNT_PROTOCOL, CMD_ACCOUNT_STATUS,
	       CMD_ACCOUNT_STATUS_TYPE, CMD_ACCOUNT_STATUS_TYPES,
	       CMD_ACCOUNT_USERNAME } cmd;
	const char *listopts[] = { "-all", "-online", NULL };
	enum { CMD_ACCOUNTLIST_ALL, CMD_ACCOUNTLIST_ONLINE } listopt;
	const char *alias;
	const GList *cur;
	GaimAccount *account;
	GaimStatus *status;
	GaimStatusType *status_type;
	GaimValue *value;
	char *attr_id;
	int error;
	int b, i;

	if (objc < 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "subcommand ?args?");
		return TCL_ERROR;
	}

	if ((error = Tcl_GetIndexFromObj(interp, objv[1], cmds, "subcommand", 0, (int *)&cmd)) != TCL_OK)
		return error;

	switch (cmd) {
	case CMD_ACCOUNT_ALIAS:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "account");
			return TCL_ERROR;
		}
		if ((account = tcl_validate_account(objv[2], interp)) == NULL)
			return TCL_ERROR;
		alias = gaim_account_get_alias(account);
		Tcl_SetStringObj(result, alias ? (char *)alias : "", -1);
		break;
	case CMD_ACCOUNT_CONNECT:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "account");
			return TCL_ERROR;
		}
		if ((account = tcl_validate_account(objv[2], interp)) == NULL)
			return TCL_ERROR;
		if (!gaim_account_is_connected(account))
			gaim_account_connect(account);
		Tcl_SetObjResult(interp,
		                 gaim_tcl_ref_new(GaimTclRefConnection,
		                                  gaim_account_get_connection(account)));
		break;
	case CMD_ACCOUNT_CONNECTION:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "account");
			return TCL_ERROR;
		}

		if ((account = tcl_validate_account(objv[2], interp)) == NULL)
			return TCL_ERROR;
		Tcl_SetObjResult(interp,
		                 gaim_tcl_ref_new(GaimTclRefConnection,
		                                  gaim_account_get_connection(account)));
		break;
	case CMD_ACCOUNT_DISCONNECT:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "account");
			return TCL_ERROR;
		}
		if ((account = tcl_validate_account(objv[2], interp)) == NULL)
			return TCL_ERROR;
		gaim_account_disconnect(account);
		break;
	case CMD_ACCOUNT_ENABLED:
		if (objc != 3 && objc != 4) {
			Tcl_WrongNumArgs(interp, 2, objv, "account ?enabled?");
			return TCL_ERROR;
		}
		if ((account = tcl_validate_account(objv[2], interp)) == NULL)
			return TCL_ERROR;
		if (objc == 3) {
			Tcl_SetBooleanObj(result,
					  gaim_account_get_enabled(account,
								   gaim_core_get_ui()));
		} else {
			if ((error = Tcl_GetBooleanFromObj(interp, objv[3], &b)) != TCL_OK)
				return TCL_ERROR;
			gaim_account_set_enabled(account, gaim_core_get_ui(), b);
		}
		break;
	case CMD_ACCOUNT_FIND:
		if (objc != 4) {
			Tcl_WrongNumArgs(interp, 2, objv, "username protocol");
			return TCL_ERROR;
		}
		account = gaim_accounts_find(Tcl_GetString(objv[2]),
		                             Tcl_GetString(objv[3]));
		Tcl_SetObjResult(interp,
		                 gaim_tcl_ref_new(GaimTclRefAccount, account));
		break;
	case CMD_ACCOUNT_HANDLE:
		if (objc != 2) {
			Tcl_WrongNumArgs(interp, 2, objv, "");
			return TCL_ERROR;
		}
		Tcl_SetIntObj(result, (int)gaim_accounts_get_handle());
		break;
	case CMD_ACCOUNT_ISCONNECTED:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "account");
			return TCL_ERROR;
		}
		if ((account = tcl_validate_account(objv[2], interp)) == NULL)
			return TCL_ERROR;
		Tcl_SetBooleanObj(result, gaim_account_is_connected(account));
		break;
	case CMD_ACCOUNT_LIST:
		listopt = CMD_ACCOUNTLIST_ALL;
		if (objc > 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "?option?");
			return TCL_ERROR;
		}
		if (objc == 3) {
			if ((error = Tcl_GetIndexFromObj(interp, objv[2], listopts, "option", 0, (int *)&listopt)) != TCL_OK)
				return error;
		}
		list = Tcl_NewListObj(0, NULL);
		for (cur = gaim_accounts_get_all(); cur != NULL; cur = g_list_next(cur)) {
			account = cur->data;
			if (listopt == CMD_ACCOUNTLIST_ONLINE && !gaim_account_is_connected(account))
				continue;
			elem = gaim_tcl_ref_new(GaimTclRefAccount, account);
			Tcl_ListObjAppendElement(interp, list, elem);
		}
		Tcl_SetObjResult(interp, list);
		break;
	case CMD_ACCOUNT_PRESENCE:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "account");
			return TCL_ERROR;
		}
		if ((account = tcl_validate_account(objv[2], interp)) == NULL)
			return TCL_ERROR;
		Tcl_SetObjResult(interp, gaim_tcl_ref_new(GaimTclRefPresence,
							  gaim_account_get_presence(account)));
		break;
	case CMD_ACCOUNT_PROTOCOL:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "account");
			return TCL_ERROR;
		}
		if ((account = tcl_validate_account(objv[2], interp)) == NULL)
			return TCL_ERROR;
		Tcl_SetStringObj(result, (char *)gaim_account_get_protocol_id(account), -1);
		break;
	case CMD_ACCOUNT_STATUS:
		if (objc < 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "account ?status_id name value ...?");
			return TCL_ERROR;
		}
		if ((account = tcl_validate_account(objv[2], interp)) == NULL)
			return TCL_ERROR;
		if (objc == 3) {
			Tcl_SetObjResult(interp,
					 gaim_tcl_ref_new(GaimTclRefStatus,
							  gaim_account_get_active_status(account)));
		} else {
			GList *l = NULL;
			if (objc % 2) {
				Tcl_SetStringObj(result, "name without value setting status", -1);
				return TCL_ERROR;
			}
			status = gaim_account_get_status(account, Tcl_GetString(objv[3]));
			if (status == NULL) {
				Tcl_SetStringObj(result, "invalid status for account", -1);
				return TCL_ERROR;
			}
			for (i = 4; i < objc; i += 2) {
				attr_id = Tcl_GetString(objv[i]);
				value = gaim_status_get_attr_value(status, attr_id);
				if (value == NULL) {
					Tcl_SetStringObj(result, "invalid attribute for account", -1);
					return TCL_ERROR;
				}
				switch (gaim_value_get_type(value)) {
				case GAIM_TYPE_BOOLEAN:
					error = Tcl_GetBooleanFromObj(interp, objv[i + 1], &b);
					if (error != TCL_OK)
						return error;
					l = g_list_append(l, attr_id);
					l = g_list_append(l, GINT_TO_POINTER(b));
					break;
				case GAIM_TYPE_INT:
					error = Tcl_GetIntFromObj(interp, objv[i + 1], &b);
					if (error != TCL_OK)
						return error;
					l = g_list_append(l, attr_id);
					l = g_list_append(l, GINT_TO_POINTER(b));
					break;
				case GAIM_TYPE_STRING:
					l = g_list_append(l, attr_id);
					l = g_list_append(l, Tcl_GetString(objv[i + 1]));
					break;
				default:
					Tcl_SetStringObj(result, "unknown GaimValue type", -1);
					return TCL_ERROR;
				}
			}
			gaim_account_set_status_list(account, Tcl_GetString(objv[3]), TRUE, l);
			g_list_free(l);
		}
		break;
	case CMD_ACCOUNT_STATUS_TYPE:
		if (objc != 4 && objc != 5) {
			Tcl_WrongNumArgs(interp, 2, objv, "account ?statustype? ?-primitive primitive?");
			return TCL_ERROR;
		}
		if ((account = tcl_validate_account(objv[2], interp)) == NULL)
			return TCL_ERROR;
		if (objc == 4) {
			status_type = gaim_account_get_status_type(account,
								   Tcl_GetString(objv[3]));
		} else {
			GaimStatusPrimitive primitive;
			if (strcmp(Tcl_GetString(objv[3]), "-primitive")) {
				Tcl_SetStringObj(result, "bad option \"", -1);
				Tcl_AppendObjToObj(result, objv[3]);
				Tcl_AppendToObj(result,
						"\": should be -primitive", -1);
				return TCL_ERROR;
			}
			primitive = gaim_primitive_get_type_from_id(Tcl_GetString(objv[4]));
			status_type = gaim_account_get_status_type_with_primitive(account,
										  primitive);
		}
		if (status_type == NULL) {
			Tcl_SetStringObj(result, "status type not found", -1);
			return TCL_ERROR;
		}
		Tcl_SetObjResult(interp,
				 gaim_tcl_ref_new(GaimTclRefStatusType,
						  status_type));
		break;
	case CMD_ACCOUNT_STATUS_TYPES:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "account");
			return TCL_ERROR;
		}
		if ((account = tcl_validate_account(objv[2], interp)) == NULL)
			return TCL_ERROR;
		list = Tcl_NewListObj(0, NULL);
		for (cur = gaim_account_get_status_types(account); cur != NULL;
		     cur = g_list_next(cur)) {
			Tcl_ListObjAppendElement(interp, list,
						 gaim_tcl_ref_new(GaimTclRefStatusType,
								  cur->data));
		}
		Tcl_SetObjResult(interp, list);
		break;
	case CMD_ACCOUNT_USERNAME:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "account");
			return TCL_ERROR;
		}
		if ((account = tcl_validate_account(objv[2], interp)) == NULL)
			return TCL_ERROR;
		Tcl_SetStringObj(result, (char *)gaim_account_get_username(account), -1);
		break;
	}

	return TCL_OK;
}

static GaimBlistNode *tcl_list_to_buddy(Tcl_Interp *interp, int count, Tcl_Obj **elems)
{
	GaimBlistNode *node = NULL;
	GaimAccount *account;
	char *name;
	char *type;

	if (count < 3) {
		Tcl_SetStringObj(Tcl_GetObjResult(interp), "list too short", -1);
		return NULL;
	}

	type = Tcl_GetString(elems[0]);
	name = Tcl_GetString(elems[1]);
	if ((account = tcl_validate_account(elems[2], interp)) == NULL)
		return NULL;

	if (!strcmp(type, "buddy")) {
		node = (GaimBlistNode *)gaim_find_buddy(account, name);
	} else if (!strcmp(type, "group")) {
		node = (GaimBlistNode *)gaim_blist_find_chat(account, name);
	}

	return node;
}

int tcl_cmd_buddy(ClientData unused, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
	Tcl_Obj *list, *tclgroup, *tclgrouplist, *tclcontact, *tclcontactlist, *tclbud, **elems, *result;
	const char *cmds[] = { "alias", "handle", "info", "list", NULL };
	enum { CMD_BUDDY_ALIAS, CMD_BUDDY_HANDLE, CMD_BUDDY_INFO, CMD_BUDDY_LIST } cmd;
	GaimBuddyList *blist;
	GaimBlistNode *node, *gnode, *bnode;
	GaimAccount *account;
	GaimBuddy *bud;
	GaimChat *cnode;
	int error, all = 0, count;

	if (objc < 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "subcommand ?args?");
		return TCL_ERROR;
	}
	if ((error = Tcl_GetIndexFromObj(interp, objv[1], cmds, "subcommand", 0, (int *)&cmd)) != TCL_OK)
		return error;

	result = Tcl_GetObjResult(interp);

	switch (cmd) {
	case CMD_BUDDY_ALIAS:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "buddy");
			return TCL_ERROR;
		}
		if ((error = Tcl_ListObjGetElements(interp, objv[2], &count, &elems)) != TCL_OK)
			return error;
		if ((node = tcl_list_to_buddy(interp, count, elems)) == NULL)
			return TCL_ERROR;
		if (node->type == GAIM_BLIST_CHAT_NODE)
			Tcl_SetStringObj(result, ((GaimChat *)node)->alias, -1);
		else if (node->type == GAIM_BLIST_BUDDY_NODE)
			Tcl_SetStringObj(result, (char *)gaim_buddy_get_alias((GaimBuddy *)node), -1);
		return TCL_OK;
		break;
	case CMD_BUDDY_HANDLE:
		if (objc != 2) {
			Tcl_WrongNumArgs(interp, 2, objv, "");
			return TCL_ERROR;
		}
		Tcl_SetIntObj(result, (int)gaim_blist_get_handle());
		break;
	case CMD_BUDDY_INFO:
		if (objc != 3 && objc != 4) {
			Tcl_WrongNumArgs(interp, 2, objv, "( buddy | account username )");
			return TCL_ERROR;
		}
		if (objc == 3) {
			if ((error = Tcl_ListObjGetElements(interp, objv[2], &count, &elems)) != TCL_OK)
				return error;
			if (count < 3) {
				Tcl_SetStringObj(result, "buddy too short", -1);
				return TCL_ERROR;
			}
			if (strcmp("buddy", Tcl_GetString(elems[0]))) {
				Tcl_SetStringObj(result, "invalid buddy", -1);
				return TCL_ERROR;
			}
			if ((account = tcl_validate_account(elems[2], interp)) == NULL)
				return TCL_ERROR;
			serv_get_info(gaim_account_get_connection(account), Tcl_GetString(elems[1]));
		} else {
			if ((account = tcl_validate_account(objv[2], interp)) == NULL)
				return TCL_ERROR;
			serv_get_info(gaim_account_get_connection(account), Tcl_GetString(objv[3]));
		}
		break;
	case CMD_BUDDY_LIST:
		if (objc == 3) {
			if (!strcmp("-all", Tcl_GetString(objv[2]))) {
				all = 1;
			} else {
				Tcl_SetStringObj(result, "", -1);
				Tcl_AppendStringsToObj(result, "unknown option: ", Tcl_GetString(objv[2]), NULL);
				return TCL_ERROR;
			}
		}
		list = Tcl_NewListObj(0, NULL);
		blist = gaim_get_blist();
		for (gnode = blist->root; gnode != NULL; gnode = gnode->next) {
			tclgroup = Tcl_NewListObj(0, NULL);
			Tcl_ListObjAppendElement(interp, tclgroup, Tcl_NewStringObj("group", -1));
			Tcl_ListObjAppendElement(interp, tclgroup,
						 Tcl_NewStringObj(((GaimGroup *)gnode)->name, -1));
			tclgrouplist = Tcl_NewListObj(0, NULL);
			for (node = gnode->child; node != NULL; node = node->next) {
				switch (node->type) {
				case GAIM_BLIST_CONTACT_NODE:
					tclcontact = Tcl_NewListObj(0, NULL);
					Tcl_IncrRefCount(tclcontact);
					Tcl_ListObjAppendElement(interp, tclcontact, Tcl_NewStringObj("contact", -1));
					tclcontactlist = Tcl_NewListObj(0, NULL);
					Tcl_IncrRefCount(tclcontactlist);
					count = 0;
					for (bnode = node->child; bnode != NULL; bnode = bnode ->next) {
						if (bnode->type != GAIM_BLIST_BUDDY_NODE)
							continue;
						bud = (GaimBuddy *)bnode;
						if (!all && !gaim_account_is_connected(bud->account))
							continue;
						count++;
						tclbud = Tcl_NewListObj(0, NULL);
						Tcl_ListObjAppendElement(interp, tclbud, Tcl_NewStringObj("buddy", -1));
						Tcl_ListObjAppendElement(interp, tclbud, Tcl_NewStringObj(bud->name, -1));
						Tcl_ListObjAppendElement(interp, tclbud, gaim_tcl_ref_new(GaimTclRefAccount, bud->account));
						Tcl_ListObjAppendElement(interp, tclcontactlist, tclbud);
					}
					if (count) {
						Tcl_ListObjAppendElement(interp, tclcontact, tclcontactlist);
						Tcl_ListObjAppendElement(interp, tclgrouplist, tclcontact);
					}
					Tcl_DecrRefCount(tclcontact);
					Tcl_DecrRefCount(tclcontactlist);
					break;
				case GAIM_BLIST_CHAT_NODE:
					cnode = (GaimChat *)node;
					if (!all && !gaim_account_is_connected(cnode->account))
						continue;
					tclbud = Tcl_NewListObj(0, NULL);
					Tcl_ListObjAppendElement(interp, tclbud, Tcl_NewStringObj("chat", -1));
					Tcl_ListObjAppendElement(interp, tclbud, Tcl_NewStringObj(cnode->alias, -1));
					Tcl_ListObjAppendElement(interp, tclbud, gaim_tcl_ref_new(GaimTclRefAccount, cnode->account));
					Tcl_ListObjAppendElement(interp, tclgrouplist, tclbud);
					break;
				default:
					gaim_debug(GAIM_DEBUG_WARNING, "tcl", "Unexpected buddy type %d", node->type);
					continue;
				}
			}
			Tcl_ListObjAppendElement(interp, tclgroup, tclgrouplist);
			Tcl_ListObjAppendElement(interp, list, tclgroup);
		}
		Tcl_SetObjResult(interp, list);
		break;
	}

	return TCL_OK;
}

int tcl_cmd_cmd(ClientData unused, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
	const char *cmds[] = { "register", "unregister", NULL };
	enum { CMD_CMD_REGISTER, CMD_CMD_UNREGISTER } cmd;
	struct tcl_cmd_handler *handler;
	Tcl_Obj *result = Tcl_GetObjResult(interp);
	GaimCmdId id;
	int error;

	if (objc < 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "subcommand ?args?");
		return TCL_ERROR;
	}

	if ((error = Tcl_GetIndexFromObj(interp, objv[1], cmds, "subcommand", 0, (int *)&cmd)) != TCL_OK)
		return error;

	switch (cmd) {
	case CMD_CMD_REGISTER:
		if (objc != 9) {
			Tcl_WrongNumArgs(interp, 2, objv, "cmd arglist priority flags prpl_id proc helpstr");
			return TCL_ERROR;
		}
		handler = g_new0(struct tcl_cmd_handler, 1);
		handler->cmd = objv[2];
		handler->args = Tcl_GetString(objv[3]);
		handler->nargs = strlen(handler->args);
		if ((error = Tcl_GetIntFromObj(interp, objv[4],
		                               &handler->priority)) != TCL_OK) {
			g_free(handler);
			return error;
		}
		if ((error = Tcl_GetIntFromObj(interp, objv[5],
		                               &handler->flags)) != TCL_OK) {
			g_free(handler);
			return error;
		}
		handler->prpl_id = Tcl_GetString(objv[6]);
		handler->proc = objv[7];
		handler->helpstr = Tcl_GetString(objv[8]);
		handler->interp = interp;
		if ((id = tcl_cmd_register(handler)) == 0) {
			tcl_cmd_handler_free(handler);
			Tcl_SetIntObj(result, 0);
		} else {
			handler->id = id;
			Tcl_SetIntObj(result, id);
		}
		break;
	case CMD_CMD_UNREGISTER:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "id");
			return TCL_ERROR;
		}
		if ((error = Tcl_GetIntFromObj(interp, objv[2],
		                               (int *)&id)) != TCL_OK)
			return error;
		tcl_cmd_unregister(id, interp);
		break;
	}

	return TCL_OK;
}

int tcl_cmd_connection(ClientData unused, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
	Tcl_Obj *result = Tcl_GetObjResult(interp), *list, *elem;
	const char *cmds[] = { "account", "displayname", "handle", "list", NULL };
	enum { CMD_CONN_ACCOUNT, CMD_CONN_DISPLAYNAME, CMD_CONN_HANDLE, CMD_CONN_LIST } cmd;
	int error;
	GList *cur;
	GaimConnection *gc;

	if (objc < 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "subcommand ?args?");
		return TCL_ERROR;
	}

	if ((error = Tcl_GetIndexFromObj(interp, objv[1], cmds, "subcommand", 0, (int *)&cmd)) != TCL_OK)
		return error;

	switch (cmd) {
	case CMD_CONN_ACCOUNT:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "gc");
			return TCL_ERROR;
		}
		if ((gc = tcl_validate_gc(objv[2], interp)) == NULL)
			return TCL_ERROR;
		Tcl_SetObjResult(interp,
		                 gaim_tcl_ref_new(GaimTclRefAccount,
		                                  gaim_connection_get_account(gc)));
		break;
	case CMD_CONN_DISPLAYNAME:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "gc");
			return TCL_ERROR;
		}
		if ((gc = tcl_validate_gc(objv[2], interp)) == NULL)
			return TCL_ERROR;
		Tcl_SetStringObj(result, (char *)gaim_connection_get_display_name(gc), -1);
		break;
	case CMD_CONN_HANDLE:
		if (objc != 2) {
			Tcl_WrongNumArgs(interp, 2, objv, "");
			return TCL_ERROR;
		}
		Tcl_SetIntObj(result, (int)gaim_connections_get_handle());
		break;
	case CMD_CONN_LIST:
		if (objc != 2) {
			Tcl_WrongNumArgs(interp, 2, objv, "");
			return TCL_ERROR;
		}
		list = Tcl_NewListObj(0, NULL);
		for (cur = gaim_connections_get_all(); cur != NULL; cur = g_list_next(cur)) {
			elem = gaim_tcl_ref_new(GaimTclRefConnection, cur->data);
			Tcl_ListObjAppendElement(interp, list, elem);
		}
		Tcl_SetObjResult(interp, list);
		break;
	}

	return TCL_OK;
}

int tcl_cmd_conversation(ClientData unused, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
	Tcl_Obj *list, *elem, *result = Tcl_GetObjResult(interp);
	const char *cmds[] = { "find", "handle", "list", "new", "write", "name", "title", "send", NULL };
	enum { CMD_CONV_FIND, CMD_CONV_HANDLE, CMD_CONV_LIST, CMD_CONV_NEW, CMD_CONV_WRITE , CMD_CONV_NAME, CMD_CONV_TITLE, CMD_CONV_SEND } cmd;
	const char *styles[] = { "send", "recv", "system", NULL };
	enum { CMD_CONV_WRITE_SEND, CMD_CONV_WRITE_RECV, CMD_CONV_WRITE_SYSTEM } style;
	const char *newopts[] = { "-chat", "-im" };
	enum { CMD_CONV_NEW_CHAT, CMD_CONV_NEW_IM } newopt;
	GaimConversation *convo;
	GaimAccount *account;
	GaimConversationType type;
	GList *cur;
	char *opt, *from, *what;
	int error, argsused, flags = 0;

	if (objc < 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "subcommand ?args?");
		return TCL_ERROR;
	}

	if ((error = Tcl_GetIndexFromObj(interp, objv[1], cmds, "subcommand", 0, (int *)&cmd)) != TCL_OK)
		return error;

	switch (cmd) {
	case CMD_CONV_FIND:
		if (objc != 4) {
			Tcl_WrongNumArgs(interp, 2, objv, "account name");
			return TCL_ERROR;
		}
		account = NULL;
		if ((account = tcl_validate_account(objv[2], interp)) == NULL)
			return TCL_ERROR;
		convo = gaim_find_conversation_with_account(GAIM_CONV_TYPE_ANY,
							    Tcl_GetString(objv[3]),
							    account);
		Tcl_SetObjResult(interp, gaim_tcl_ref_new(GaimTclRefConversation, convo));
		break;
	case CMD_CONV_HANDLE:
		if (objc != 2) {
			Tcl_WrongNumArgs(interp, 2, objv, "");
			return TCL_ERROR;
		}
		Tcl_SetIntObj(result, (int)gaim_conversations_get_handle());
		break;
	case CMD_CONV_LIST:
		list = Tcl_NewListObj(0, NULL);
		for (cur = gaim_get_conversations(); cur != NULL; cur = g_list_next(cur)) {
			elem = gaim_tcl_ref_new(GaimTclRefConversation, cur->data);
			Tcl_ListObjAppendElement(interp, list, elem);
		}
		Tcl_SetObjResult(interp, list);
		break;
	case CMD_CONV_NEW:
		if (objc < 4) {
			Tcl_WrongNumArgs(interp, 2, objv, "?options? account name");
			return TCL_ERROR;
		}
		argsused = 2;
		type = GAIM_CONV_TYPE_IM;
		while (argsused < objc) {
			opt = Tcl_GetString(objv[argsused]);
			if (*opt == '-') {
				if ((error = Tcl_GetIndexFromObj(interp, objv[argsused], newopts,
								 "option", 0, (int *)&newopt)) != TCL_OK)
					return error;
				argsused++;
				switch (newopt) {
				case CMD_CONV_NEW_CHAT:
					type = GAIM_CONV_TYPE_CHAT;
					break;
				case CMD_CONV_NEW_IM:
					type = GAIM_CONV_TYPE_IM;
					break;
				}
			} else {
				break;
			}
		}
		if (objc - argsused != 2) {
			Tcl_WrongNumArgs(interp, 2, objv, "?options? account name");
			return TCL_ERROR;
		}
		if ((account = tcl_validate_account(objv[argsused++], interp)) == NULL)
			return TCL_ERROR;
		convo = gaim_conversation_new(type, account, Tcl_GetString(objv[argsused]));
		Tcl_SetObjResult(interp, gaim_tcl_ref_new(GaimTclRefConversation, convo));
		break;
	case CMD_CONV_WRITE:
		if (objc != 6) {
			Tcl_WrongNumArgs(interp, 2, objv, "conversation style from what");
			return TCL_ERROR;
		}
		if ((convo = tcl_validate_conversation(objv[2], interp)) == NULL)
			return TCL_ERROR;
		if ((error = Tcl_GetIndexFromObj(interp, objv[3], styles, "style", 0, (int *)&style)) != TCL_OK)
			return error;
		from = Tcl_GetString(objv[4]);
		what = Tcl_GetString(objv[5]);
		
		switch (style) {
		case CMD_CONV_WRITE_SEND:
			flags = GAIM_MESSAGE_SEND;
			break;
		case CMD_CONV_WRITE_RECV:
			flags = GAIM_MESSAGE_RECV;
			break;
		case CMD_CONV_WRITE_SYSTEM:
			flags = GAIM_MESSAGE_SYSTEM;
			break;
		}
		if (gaim_conversation_get_type(convo) == GAIM_CONV_TYPE_CHAT)
			gaim_conv_chat_write(GAIM_CONV_CHAT(convo), from, what, flags, time(NULL));
		else
			gaim_conv_im_write(GAIM_CONV_IM(convo), from, what, flags, time(NULL));
		break;
	case CMD_CONV_NAME:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "conversation");
			return TCL_ERROR;
		}

		if ((convo = tcl_validate_conversation(objv[2], interp)) == NULL)
			return TCL_ERROR;
		Tcl_SetStringObj(result, (char *)gaim_conversation_get_name(convo), -1);
		break;
	case CMD_CONV_TITLE:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "conversation");
			return TCL_ERROR;
		}

		if ((convo = tcl_validate_conversation(objv[2], interp)) == NULL)
			return TCL_ERROR;
		Tcl_SetStringObj(result, (char *)gaim_conversation_get_title(convo), -1);
		break;
	case CMD_CONV_SEND:
		if (objc != 4) {
			Tcl_WrongNumArgs(interp, 2, objv, "conversation message");
			return TCL_ERROR;
		}
		if ((convo = tcl_validate_conversation(objv[2], interp)) == NULL)
			return TCL_ERROR;
		what = Tcl_GetString(objv[3]);
		if (gaim_conversation_get_type(convo) == GAIM_CONV_TYPE_CHAT)
			gaim_conv_chat_send(GAIM_CONV_CHAT(convo), what);
		else
			gaim_conv_im_send(GAIM_CONV_IM(convo), what);
		break;
	}

	return TCL_OK;
}

int tcl_cmd_core(ClientData unused, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
	Tcl_Obj *result = Tcl_GetObjResult(interp);
	const char *cmds[] = { "handle", "quit", NULL };
	enum { CMD_CORE_HANDLE, CMD_CORE_QUIT } cmd;
	int error;

	if (objc < 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "subcommand ?args?");
		return TCL_ERROR;
	}

	if ((error = Tcl_GetIndexFromObj(interp, objv[1], cmds, "subcommand", 0, (int *)&cmd)) != TCL_OK)
		return error;

	switch (cmd) {
	case CMD_CORE_HANDLE:
		if (objc != 2) {
			Tcl_WrongNumArgs(interp, 2, objv, "");
			return TCL_ERROR;
		}
		Tcl_SetIntObj(result, (int)gaim_get_core());
		break;
	case CMD_CORE_QUIT:
		if (objc != 2) {
			Tcl_WrongNumArgs(interp, 2, objv, "");
			return TCL_ERROR;
		}
		gaim_core_quit();
		break;
	}

	return TCL_OK;
}

int tcl_cmd_debug(ClientData unused, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
	char *category, *message;
	int lev;
	const char *levels[] = { "-misc", "-info", "-warning", "-error", NULL };
	GaimDebugLevel levelind[] = { GAIM_DEBUG_MISC, GAIM_DEBUG_INFO, GAIM_DEBUG_WARNING, GAIM_DEBUG_ERROR };
	int error;

	if (objc != 4) {
		Tcl_WrongNumArgs(interp, 1, objv, "level category message");
		return TCL_ERROR;
	}

	error = Tcl_GetIndexFromObj(interp, objv[1], levels, "debug level", 0, &lev);
	if (error != TCL_OK)
		return error;

	category = Tcl_GetString(objv[2]);
	message = Tcl_GetString(objv[3]);

	gaim_debug(levelind[lev], category, "%s\n", message);

	return TCL_OK;
}

int tcl_cmd_notify(ClientData unused, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
	int error, type;
	const char *opts[] = { "-error", "-warning", "-info", NULL };
	GaimNotifyMsgType optind[] = { GAIM_NOTIFY_MSG_ERROR, GAIM_NOTIFY_MSG_WARNING, GAIM_NOTIFY_MSG_INFO };
	char *title, *msg1, *msg2;

	if (objc < 4 || objc > 5) {
		Tcl_WrongNumArgs(interp, 1, objv, "?type? title primary secondary");
		return TCL_ERROR;
	}

	if (objc == 4) {
		type = 1;			/* Default to warning */
		title = Tcl_GetString(objv[1]);
		msg1 = Tcl_GetString(objv[2]);
		msg2 = Tcl_GetString(objv[3]);
	} else {
		error = Tcl_GetIndexFromObj(interp, objv[1], opts, "message type", 0, &type);
		if (error != TCL_OK)
			return error;
		title = Tcl_GetString(objv[2]);
		msg1 = Tcl_GetString(objv[3]);
		msg2 = Tcl_GetString(objv[4]);
	}

	gaim_notify_message(_tcl_plugin, optind[type], title, msg1, msg2, NULL, NULL);

	return TCL_OK;
}

int tcl_cmd_plugins(ClientData unused, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
	Tcl_Obj *result = Tcl_GetObjResult(interp);
	const char *cmds[] = { "handle", NULL };
	enum { CMD_PLUGINS_HANDLE } cmd;
	int error;

	if (objc < 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "subcommand ?args?");
		return TCL_ERROR;
	}

	if ((error = Tcl_GetIndexFromObj(interp, objv[1], cmds, "subcommand", 0, (int *)&cmd)) != TCL_OK)
		return error;

	switch (cmd) {
	case CMD_PLUGINS_HANDLE:
		if (objc != 2) {
			Tcl_WrongNumArgs(interp, 2, objv, "");
			return TCL_ERROR;
		}
		Tcl_SetIntObj(result, (int)gaim_plugins_get_handle());
		break;
	}

	return TCL_OK;
}

int tcl_cmd_prefs(ClientData unused, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
	Tcl_Obj *result, *list, *elem, **elems;
	const char *cmds[] = { "get", "set", "type", NULL };
	enum { CMD_PREFS_GET, CMD_PREFS_SET, CMD_PREFS_TYPE } cmd;
	/* char *types[] = { "none", "boolean", "int", "string", "stringlist", NULL }; */
	/* enum { TCL_PREFS_NONE, TCL_PREFS_BOOL, TCL_PREFS_INT, TCL_PREFS_STRING, TCL_PREFS_STRINGLIST } type; */
	GaimPrefType preftype;
	GList *cur;
	int error, intval, nelem, i;

	if (objc < 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "subcommand ?args?");
		return TCL_ERROR;
	}

	if ((error = Tcl_GetIndexFromObj(interp, objv[1], cmds, "subcommand", 0, (int *)&cmd)) != TCL_OK)
		return error;

	result = Tcl_GetObjResult(interp);
	switch (cmd) {
	case CMD_PREFS_GET:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 1, objv, "path");
			return TCL_ERROR;
		}
		preftype = gaim_prefs_get_type(Tcl_GetString(objv[2]));
		switch (preftype) {
		case GAIM_PREF_NONE:
			Tcl_SetStringObj(result, "pref type none", -1);
			return TCL_ERROR;
			break;
		case GAIM_PREF_BOOLEAN:
			Tcl_SetBooleanObj(result, gaim_prefs_get_bool(Tcl_GetString(objv[2])));
			break;
		case GAIM_PREF_INT:
			Tcl_SetIntObj(result, gaim_prefs_get_int(Tcl_GetString(objv[2])));
			break;
		case GAIM_PREF_STRING:
			Tcl_SetStringObj(result, (char *)gaim_prefs_get_string(Tcl_GetString(objv[2])), -1);
			break;
		case GAIM_PREF_STRING_LIST:
			cur = gaim_prefs_get_string_list(Tcl_GetString(objv[2]));
			list = Tcl_NewListObj(0, NULL);
			while (cur != NULL) {
				elem = Tcl_NewStringObj((char *)cur->data, -1);
				Tcl_ListObjAppendElement(interp, list, elem);
				cur = g_list_next(cur);
			}
			Tcl_SetObjResult(interp, list);
			break;
		default:
			gaim_debug(GAIM_DEBUG_ERROR, "tcl", "tcl does not know about pref type %d\n", preftype);
			Tcl_SetStringObj(result, "unknown pref type", -1);
			return TCL_ERROR;
		}
		break;
	case CMD_PREFS_SET:
		if (objc != 4) {
			Tcl_WrongNumArgs(interp, 1, objv, "path value");
			return TCL_ERROR;
		}
		preftype = gaim_prefs_get_type(Tcl_GetString(objv[2]));
		switch (preftype) {
		case GAIM_PREF_NONE:
			Tcl_SetStringObj(result, "bad path or pref type none", -1);
			return TCL_ERROR;
			break;
		case GAIM_PREF_BOOLEAN:
			if ((error = Tcl_GetBooleanFromObj(interp, objv[3], &intval)) != TCL_OK)
				return error;
			gaim_prefs_set_bool(Tcl_GetString(objv[2]), intval);
			break;
		case GAIM_PREF_INT:
			if ((error = Tcl_GetIntFromObj(interp, objv[3], &intval)) != TCL_OK)
				return error;
			gaim_prefs_set_int(Tcl_GetString(objv[2]), intval);
			break;
		case GAIM_PREF_STRING:
			gaim_prefs_set_string(Tcl_GetString(objv[2]), Tcl_GetString(objv[3]));
			break;
		case GAIM_PREF_STRING_LIST:
			if ((error = Tcl_ListObjGetElements(interp, objv[3], &nelem, &elems)) != TCL_OK)
				return error;
			cur = NULL;
			for (i = 0; i < nelem; i++) {
				cur = g_list_append(cur, (gpointer)Tcl_GetString(elems[i]));
			}
			gaim_prefs_set_string_list(Tcl_GetString(objv[2]), cur);
			g_list_free(cur);
			break;
		default:
			gaim_debug(GAIM_DEBUG_ERROR, "tcl", "tcl does not know about pref type %d\n", preftype);
			return TCL_ERROR;
		}
		break;
	case CMD_PREFS_TYPE:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 1, objv, "path");
			return TCL_ERROR;
		}
		preftype = gaim_prefs_get_type(Tcl_GetString(objv[2]));
		switch (preftype) {
		case GAIM_PREF_NONE:
			Tcl_SetStringObj(result, "none", -1);
			break;
		case GAIM_PREF_BOOLEAN:
			Tcl_SetStringObj(result, "boolean", -1);
			break;
		case GAIM_PREF_INT:
			Tcl_SetStringObj(result, "int", -1);
			break;
		case GAIM_PREF_STRING:
			Tcl_SetStringObj(result, "string", -1);
			break;
		case GAIM_PREF_STRING_LIST:
			Tcl_SetStringObj(result, "stringlist", -1);
			break;
		default:
			gaim_debug(GAIM_DEBUG_ERROR, "tcl", "tcl does not know about pref type %d\n", preftype);
			Tcl_SetStringObj(result, "unknown", -1);
		}
		break;
	}

	return TCL_OK;
}

int tcl_cmd_presence(ClientData unused, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
	const char *cmds[] = { "account", "active_status", "available",
			       "chat_user", "context", "conversation", "idle",
			       "login", "online", "status", "statuses", NULL };
	enum { CMD_PRESENCE_ACCOUNT, CMD_PRESENCE_ACTIVE_STATUS,
	       CMD_PRESENCE_AVAILABLE, CMD_PRESENCE_CHAT_USER,
	       CMD_PRESENCE_CONTEXT, CMD_PRESENCE_CONVERSATION,
	       CMD_PRESENCE_IDLE, CMD_PRESENCE_LOGIN, CMD_PRESENCE_ONLINE,
	       CMD_PRESENCE_STATUS, CMD_PRESENCE_STATUSES } cmd;
	Tcl_Obj *result = Tcl_GetObjResult(interp);
	Tcl_Obj *list, *elem;
	GaimPresence *presence;
	const GList *cur;
	int error, idle, idle_time, login_time;

	if (objc < 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "subcommand ?args?");
		return TCL_ERROR;
	}

	if ((error = Tcl_GetIndexFromObj(interp, objv[1], cmds, "subcommand", 0, (int *)&cmd)) != TCL_OK)
		return error;

	switch (cmd) {
	case CMD_PRESENCE_ACCOUNT:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "presence");
			return TCL_ERROR;
		}
		if ((presence = gaim_tcl_ref_get(interp, objv[2], GaimTclRefPresence)) == NULL)
			return TCL_ERROR;
		Tcl_SetObjResult(interp, gaim_tcl_ref_new(GaimTclRefAccount,
		                                          gaim_presence_get_account(presence)));
		break;
	case CMD_PRESENCE_ACTIVE_STATUS:
		if (objc != 3 && objc != 4 && objc != 5) {
			Tcl_WrongNumArgs(interp, 2, objv, "presence [?status_id? | ?-primitive primitive?]");
			return TCL_ERROR;
		}
		if ((presence = gaim_tcl_ref_get(interp, objv[2], GaimTclRefPresence)) == NULL)
			return TCL_ERROR;
		if (objc == 3) {
			Tcl_SetObjResult(interp,
					 gaim_tcl_ref_new(GaimTclRefStatus,
							  gaim_presence_get_active_status(presence)));
		} else if (objc == 4) {
			Tcl_SetBooleanObj(result,
					  gaim_presence_is_status_active(presence,
									 Tcl_GetString(objv[3])));
		} else {
			GaimStatusPrimitive primitive;
			if (strcmp(Tcl_GetString(objv[3]), "-primitive")) {
				Tcl_SetStringObj(result, "bad option \"", -1);
				Tcl_AppendObjToObj(result, objv[3]);
				Tcl_AppendToObj(result,
						"\": should be -primitive", -1);
				return TCL_ERROR;
			}
			primitive = gaim_primitive_get_type_from_id(Tcl_GetString(objv[4]));
			if (primitive == GAIM_STATUS_UNSET) {
				Tcl_SetStringObj(result, "invalid primitive ", -1);
				Tcl_AppendObjToObj(result, objv[4]);
				return TCL_ERROR;
			}
			Tcl_SetBooleanObj(result, gaim_presence_is_status_primitive_active(presence, primitive));
			break;
		}
		break;
	case CMD_PRESENCE_AVAILABLE:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "presence");
			return TCL_ERROR;
		}
		if ((presence = gaim_tcl_ref_get(interp, objv[2], GaimTclRefPresence)) == NULL)
			return TCL_ERROR;
		Tcl_SetBooleanObj(result, gaim_presence_is_available(presence));
		break;
	case CMD_PRESENCE_CHAT_USER:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "presence");
			return TCL_ERROR;
		}
		if ((presence = gaim_tcl_ref_get(interp, objv[2], GaimTclRefPresence)) == NULL)
			return TCL_ERROR;
		Tcl_SetStringObj(result, gaim_presence_get_chat_user(presence), -1);
		break;
	case CMD_PRESENCE_CONTEXT:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "presence");
			return TCL_ERROR;
		}
		if ((presence = gaim_tcl_ref_get(interp, objv[2], GaimTclRefPresence)) == NULL)
			return TCL_ERROR;
		switch (gaim_presence_get_context(presence)) {
		case GAIM_PRESENCE_CONTEXT_UNSET:
			Tcl_SetStringObj(result, "unset", -1);
			break;
		case GAIM_PRESENCE_CONTEXT_ACCOUNT:
			Tcl_SetStringObj(result, "account", -1);
			break;
		case GAIM_PRESENCE_CONTEXT_CONV:
			Tcl_SetStringObj(result, "conversation", -1);
			break;
		case GAIM_PRESENCE_CONTEXT_BUDDY:
			Tcl_SetStringObj(result, "buddy", -1);
			break;
		}
		break;
	case CMD_PRESENCE_CONVERSATION:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "presence");
			return TCL_ERROR;
		}
		if ((presence = gaim_tcl_ref_get(interp, objv[2], GaimTclRefPresence)) == NULL)
			return TCL_ERROR;
		Tcl_SetObjResult(interp, gaim_tcl_ref_new(GaimTclRefConversation,
		                                          gaim_presence_get_conversation(presence)));
		break;
	case CMD_PRESENCE_IDLE:
		if (objc < 3 || objc > 5) {
			Tcl_WrongNumArgs(interp, 2, objv, "presence ?idle? ?time?");
			return TCL_ERROR;
		}
		if ((presence = gaim_tcl_ref_get(interp, objv[2], GaimTclRefPresence)) == NULL)
			return TCL_ERROR;
		if (objc == 3) {
			if (gaim_presence_is_idle(presence)) {
				idle_time = gaim_presence_get_idle_time (presence);
				Tcl_SetIntObj(result, idle_time);
			} else {
				result = Tcl_NewListObj(0, NULL);
				Tcl_SetObjResult(interp, result);
			}
			break;
		}
		if ((error = Tcl_GetBooleanFromObj(interp, objv[3], &idle)) != TCL_OK)
			return TCL_ERROR;
		if (objc == 4) {
			gaim_presence_set_idle(presence, idle, time(NULL));
		} else if (objc == 5) {
			if ((error = Tcl_GetIntFromObj(interp,
		                                       objv[4],
		                                       &idle_time)) != TCL_OK)
				return TCL_ERROR;
			gaim_presence_set_idle(presence, idle, idle_time);
		}
		break;
	case CMD_PRESENCE_LOGIN:
		if (objc != 3 && objc != 4) {
			Tcl_WrongNumArgs(interp, 2, objv, "presence ?time?");
			return TCL_ERROR;
		}
		if ((presence = gaim_tcl_ref_get(interp, objv[2], GaimTclRefPresence)) == NULL)
			return TCL_ERROR;
		if (objc == 3) {
			Tcl_SetIntObj(result, gaim_presence_get_login_time(presence));
		} else {
			if ((error == Tcl_GetIntFromObj(interp,
			                                objv[3],
			                                &login_time)) != TCL_OK)
				return TCL_ERROR;
			gaim_presence_set_login_time(presence, login_time);
		}
		break;
	case CMD_PRESENCE_ONLINE:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "presence");
			return TCL_ERROR;
		}
		if ((presence = gaim_tcl_ref_get(interp, objv[2], GaimTclRefPresence)) == NULL)
			return TCL_ERROR;
		Tcl_SetBooleanObj(result, gaim_presence_is_online(presence));
		break;
	case CMD_PRESENCE_STATUS:
		if (objc != 4) {
			Tcl_WrongNumArgs(interp, 2, objv, "presence status_id");
			return TCL_ERROR;
		}
		if ((presence = gaim_tcl_ref_get(interp, objv[2], GaimTclRefPresence)) == NULL)
			return TCL_ERROR;
		Tcl_SetObjResult(interp,
		                 gaim_tcl_ref_new(GaimTclRefStatus,
		                                  gaim_presence_get_status(presence,
		                                                           Tcl_GetString(objv[3]))));
		break;
	case CMD_PRESENCE_STATUSES:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "presence");
			return TCL_ERROR;
		}
		if ((presence = gaim_tcl_ref_get(interp, objv[2], GaimTclRefPresence)) == NULL)
			return TCL_ERROR;
		list = Tcl_NewListObj(0, NULL);
		for (cur = gaim_presence_get_statuses(presence); cur != NULL;
		     cur = g_list_next(cur)) {
			elem = gaim_tcl_ref_new(GaimTclRefStatus, cur->data);
			Tcl_ListObjAppendElement(interp, list, elem);
		}
		Tcl_SetObjResult(interp, list);
		break;
	}

	return TCL_OK;
}

int tcl_cmd_send_im(ClientData unused, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
	GaimConnection *gc;
	char *who, *text;

	if (objc != 4) {
		Tcl_WrongNumArgs(interp, 1, objv, "gc who text");
		return TCL_ERROR;
	}

	if ((gc = tcl_validate_gc(objv[1], interp)) == NULL)
		return TCL_ERROR;

	who = Tcl_GetString(objv[2]);
	text = Tcl_GetString(objv[3]);

	serv_send_im(gc, who, text, 0);

	return TCL_OK;
}

int tcl_cmd_signal(ClientData unused, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
	const char *cmds[] = { "connect", "disconnect", NULL };
	enum { CMD_SIGNAL_CONNECT, CMD_SIGNAL_DISCONNECT } cmd;
	struct tcl_signal_handler *handler;
	Tcl_Obj *result = Tcl_GetObjResult(interp);
	void *instance;
	int error;

	if (objc < 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "subcommand ?args?");
		return TCL_ERROR;
	}

	if ((error = Tcl_GetIndexFromObj(interp, objv[1], cmds, "subcommand", 0, (int *)&cmd)) != TCL_OK)
		return error;

	switch (cmd) {
	case CMD_SIGNAL_CONNECT:
		if (objc != 6) {
			Tcl_WrongNumArgs(interp, 2, objv, "instance signal args proc");
			return TCL_ERROR;
		}
		handler = g_new0(struct tcl_signal_handler, 1);
		if ((error = Tcl_GetIntFromObj(interp, objv[2], (int *)&handler->instance)) != TCL_OK) {
			g_free(handler);
			return error;
		}
		handler->signal = objv[3];
		Tcl_IncrRefCount(handler->signal);
		handler->args = objv[4];
		handler->proc = objv[5];
		handler->interp = interp;
		if (!tcl_signal_connect(handler)) {
			tcl_signal_handler_free(handler);
			Tcl_SetIntObj(result, 1);
		} else {
			Tcl_SetIntObj(result, 0);
		}
		break;
	case CMD_SIGNAL_DISCONNECT:
		if (objc != 4) {
			Tcl_WrongNumArgs(interp, 2, objv, "instance signal");
			return TCL_ERROR;
		}
		if ((error = Tcl_GetIntFromObj(interp, objv[2], (int *)&instance)) != TCL_OK)
			return error;
		tcl_signal_disconnect(instance, Tcl_GetString(objv[3]), interp);
		break;
	}

	return TCL_OK;
}

int tcl_cmd_status(ClientData unused, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
	const char *cmds[] = { "attr", "type", NULL };
	enum { CMD_STATUS_ATTR, CMD_STATUS_TYPE } cmd;
	Tcl_Obj *result = Tcl_GetObjResult(interp);
	GaimStatus *status;
	GaimStatusType *status_type;
	GaimValue *value;
	const char *attr;
	int error, v;

	if (objc < 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "subcommand ?args?");
		return TCL_ERROR;
	}

	if ((error = Tcl_GetIndexFromObj(interp, objv[1], cmds, "subcommand", 0, (int *)&cmd)) != TCL_OK)
		return error;

	switch (cmd) {
	case CMD_STATUS_ATTR:
		if (objc != 4 && objc != 5) {
			Tcl_WrongNumArgs(interp, 2, objv, "status attr_id ?value?");
			return TCL_ERROR;
		}
		if ((status = gaim_tcl_ref_get(interp, objv[2], GaimTclRefStatus)) == NULL)
			return TCL_ERROR;
		attr = Tcl_GetString(objv[3]);
		value = gaim_status_get_attr_value(status, attr);
		if (value == NULL) {
			Tcl_SetStringObj(result, "no such attribute", -1);
			return TCL_ERROR;
		}
		switch (gaim_value_get_type(value)) {
		case GAIM_TYPE_BOOLEAN:
			if (objc == 4) {
				Tcl_SetBooleanObj(result, gaim_value_get_boolean(value));
			} else {
				if ((error = Tcl_GetBooleanFromObj(interp, objv[4], &v)) != TCL_OK)
					return error;
				gaim_status_set_attr_boolean(status, attr, v);
			}
			break;
		case GAIM_TYPE_INT:
			if (objc == 4) {
				Tcl_SetIntObj(result, gaim_value_get_int(value));
			} else {
				if ((error = Tcl_GetIntFromObj(interp, objv[4], &v)) != TCL_OK)
					return error;
				gaim_status_set_attr_int(status, attr, v );
			}
			break;
		case GAIM_TYPE_STRING:
			if (objc == 4)
				Tcl_SetStringObj(result, gaim_value_get_string(value), -1);
			else
				gaim_status_set_attr_string(status, attr, Tcl_GetString(objv[4]));
			break;
		default:
			Tcl_SetStringObj(result, "attribute has unknown type", -1);
			return TCL_ERROR;
		}
		break;
	case CMD_STATUS_TYPE:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "status");
			return TCL_ERROR;
		}
		if ((status = gaim_tcl_ref_get(interp, objv[2], GaimTclRefStatus)) == NULL)
			return TCL_ERROR;
		status_type = gaim_status_get_type(status);
		Tcl_SetObjResult(interp, gaim_tcl_ref_new(GaimTclRefStatusType,
		                                          status_type));
		break;
	}

	return TCL_OK;
}

int tcl_cmd_status_attr(ClientData unused, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
	const char *cmds[] = { "id", "name", NULL };
	enum { CMD_STATUS_ATTR_ID, CMD_STATUS_ATTR_NAME } cmd;
	Tcl_Obj *result = Tcl_GetObjResult(interp);
	GaimStatusAttr *attr;
	int error;

	if (objc < 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "subcommand ?args?");
		return TCL_ERROR;
	}

	if ((error = Tcl_GetIndexFromObj(interp, objv[1], cmds, "subcommand", 0, (int *)&cmd)) != TCL_OK)
		return error;

	switch (cmd) {
	case CMD_STATUS_ATTR_ID:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "attr");
			return TCL_ERROR;
		}
		if ((attr = gaim_tcl_ref_get(interp, objv[2], GaimTclRefStatusAttr)) == NULL)
			return TCL_ERROR;
		Tcl_SetStringObj(result, gaim_status_attr_get_id(attr), -1);
		break;
	case CMD_STATUS_ATTR_NAME:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "attr");
			return TCL_ERROR;
		}
		if ((attr = gaim_tcl_ref_get(interp, objv[2], GaimTclRefStatusAttr)) == NULL)
			return TCL_ERROR;
		Tcl_SetStringObj(result, gaim_status_attr_get_name(attr), -1);
		break;
	}

	return TCL_OK;
}

int tcl_cmd_status_type(ClientData unused, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
	const char *cmds[] = { "attr", "attrs", "available", "exclusive", "id",
	                       "independent", "name", "primary_attr",
	                       "primitive", "saveable", "user_settable",
	                       NULL };
	enum { CMD_STATUS_TYPE_ATTR, CMD_STATUS_TYPE_ATTRS,
	       CMD_STATUS_TYPE_AVAILABLE, CMD_STATUS_TYPE_EXCLUSIVE,
	       CMD_STATUS_TYPE_ID, CMD_STATUS_TYPE_INDEPENDENT,
	       CMD_STATUS_TYPE_NAME, CMD_STATUS_TYPE_PRIMARY_ATTR,
	       CMD_STATUS_TYPE_PRIMITIVE, CMD_STATUS_TYPE_SAVEABLE,
	       CMD_STATUS_TYPE_USER_SETTABLE } cmd;
	Tcl_Obj *result = Tcl_GetObjResult(interp);
	GaimStatusType *status_type;
	Tcl_Obj *list, *elem;
	const GList *cur;
	int error;

	if (objc < 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "subcommand ?args?");
		return TCL_ERROR;
	}

	if ((error = Tcl_GetIndexFromObj(interp, objv[1], cmds, "subcommand", 0, (int *)&cmd)) != TCL_OK)
		return error;

	switch (cmd) {
	case CMD_STATUS_TYPE_AVAILABLE:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "statustype");
			return TCL_ERROR;
		}
		if ((status_type = gaim_tcl_ref_get(interp, objv[2], GaimTclRefStatusType)) == NULL)
			return TCL_ERROR;
		Tcl_SetBooleanObj(result, gaim_status_type_is_available(status_type));
		break;
	case CMD_STATUS_TYPE_ATTR:
		if (objc != 4) {
			Tcl_WrongNumArgs(interp, 2, objv, "statustype attr");
			return TCL_ERROR;
		}
		if ((status_type = gaim_tcl_ref_get(interp, objv[2], GaimTclRefStatusType)) == NULL)
			return TCL_ERROR;
		Tcl_SetObjResult(interp,
				 gaim_tcl_ref_new(GaimTclRefStatusAttr,
						  gaim_status_type_get_attr(status_type,
									    Tcl_GetStringFromObj(objv[3], NULL))));
		break;
	case CMD_STATUS_TYPE_ATTRS:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "statustype");
			return TCL_ERROR;
		}
		if ((status_type = gaim_tcl_ref_get(interp, objv[2], GaimTclRefStatusType)) == NULL)
			return TCL_ERROR;
		list = Tcl_NewListObj(0, NULL);
		for (cur = gaim_status_type_get_attrs(status_type);
		     cur != NULL; cur = g_list_next(cur)) {
			elem = gaim_tcl_ref_new(GaimTclRefStatusAttr, cur->data);
			Tcl_ListObjAppendElement(interp, list, elem);
		}
		Tcl_SetObjResult(interp, list);
		break;
	case CMD_STATUS_TYPE_EXCLUSIVE:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "statustype");
			return TCL_ERROR;
		}
		if ((status_type = gaim_tcl_ref_get(interp, objv[2], GaimTclRefStatusType)) == NULL)
			return TCL_ERROR;
		Tcl_SetBooleanObj(result, gaim_status_type_is_exclusive(status_type));
		break;
	case CMD_STATUS_TYPE_ID:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "statustype");
			return TCL_ERROR;
		}
		if ((status_type = gaim_tcl_ref_get(interp, objv[2], GaimTclRefStatusType)) == NULL)
			return TCL_ERROR;
		Tcl_SetStringObj(result, gaim_status_type_get_id(status_type), -1);
		break;
	case CMD_STATUS_TYPE_INDEPENDENT:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "statustype");
			return TCL_ERROR;
		}
		if ((status_type = gaim_tcl_ref_get(interp, objv[2], GaimTclRefStatusType)) == NULL)
			return TCL_ERROR;
		Tcl_SetBooleanObj(result, gaim_status_type_is_independent(status_type));
		break;
	case CMD_STATUS_TYPE_NAME:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "statustype");
			return TCL_ERROR;
		}
		if ((status_type = gaim_tcl_ref_get(interp, objv[2], GaimTclRefStatusType)) == NULL)
			return TCL_ERROR;
		Tcl_SetStringObj(result, gaim_status_type_get_name(status_type), -1);
		break;
	case CMD_STATUS_TYPE_PRIMITIVE:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "statustype");
			return TCL_ERROR;
		}
		if ((status_type = gaim_tcl_ref_get(interp, objv[2], GaimTclRefStatusType)) == NULL)
			return TCL_ERROR;
		Tcl_SetStringObj(result, gaim_primitive_get_id_from_type(gaim_status_type_get_primitive(status_type)), -1);
		break;
	case CMD_STATUS_TYPE_PRIMARY_ATTR:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "statustype");
			return TCL_ERROR;
		}
		if ((status_type = gaim_tcl_ref_get(interp, objv[2], GaimTclRefStatusType)) == NULL)
			return TCL_ERROR;
		Tcl_SetStringObj(result, gaim_status_type_get_primary_attr(status_type), -1);
		break;
	case CMD_STATUS_TYPE_SAVEABLE:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "statustype");
			return TCL_ERROR;
		}
		if ((status_type = gaim_tcl_ref_get(interp, objv[2], GaimTclRefStatusType)) == NULL)
			return TCL_ERROR;
		Tcl_SetBooleanObj(result, gaim_status_type_is_saveable(status_type));
		break;
	case CMD_STATUS_TYPE_USER_SETTABLE:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "statustype");
			return TCL_ERROR;
		}
		if ((status_type = gaim_tcl_ref_get(interp, objv[2], GaimTclRefStatusType)) == NULL)
			return TCL_ERROR;
		Tcl_SetBooleanObj(result, gaim_status_type_is_user_settable(status_type));
		break;
	}

	return TCL_OK;
}

static gboolean unload_self(gpointer data)
{
	GaimPlugin *plugin = data;
	gaim_plugin_unload(plugin);
	return FALSE;
}

int tcl_cmd_unload(ClientData unused, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
	GaimPlugin *plugin;
	if (objc != 1) {
		Tcl_WrongNumArgs(interp, 1, objv, "");
		return TCL_ERROR;
	}

	if ((plugin = tcl_interp_get_plugin(interp)) == NULL) {
		/* This isn't exactly OK, but heh.  What do you do? */
		return TCL_OK;
	}
	/* We can't unload immediately, but we can unload at the first 
	 * known safe opportunity. */
	g_idle_add(unload_self, (gpointer)plugin);

	return TCL_OK;
}
