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

static gboolean tcl_validate_account(GaimAccount *account, Tcl_Interp *interp);
static gboolean tcl_validate_conversation(GaimConversation *convo, Tcl_Interp *interp);
static gboolean tcl_validate_gc(GaimConnection *gc);

static gboolean tcl_validate_account(GaimAccount *account, Tcl_Interp *interp)
{
	GList *cur;
	for (cur = gaim_accounts_get_all(); cur != NULL; cur = g_list_next(cur)) {
		if (account == cur->data)
			return TRUE;
	}
	if (interp != NULL)
		Tcl_SetStringObj(Tcl_GetObjResult(interp), "invalid account", -1);
	return FALSE;
}

static gboolean tcl_validate_conversation(GaimConversation *convo, Tcl_Interp *interp)
{
	GList *cur;

	for (cur = gaim_get_conversations(); cur != NULL; cur = g_list_next(cur)) {
		if (convo == cur->data)
			return TRUE;
	}
	if (interp != NULL)
		Tcl_SetStringObj(Tcl_GetObjResult(interp), "invalid account", -1);
	return FALSE;
}

static gboolean tcl_validate_gc(GaimConnection *gc)
{
	GList *cur;
	for (cur = gaim_connections_get_all(); cur != NULL; cur = g_list_next(cur)) {
		if (gc == cur->data)
			return TRUE;
	}
	return FALSE;
}

int tcl_cmd_account(ClientData unused, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
	Tcl_Obj *result = Tcl_GetObjResult(interp), *list, *elem;
	const char *cmds[] = { "alias", "connect", "connection", "disconnect", "find",
			 "handle", "isconnected", "list",
			 "protocol", "username", NULL };
	enum { CMD_ACCOUNT_ALIAS, CMD_ACCOUNT_CONNECT, CMD_ACCOUNT_CONNECTION,
	       CMD_ACCOUNT_DISCONNECT, CMD_ACCOUNT_FIND, CMD_ACCOUNT_HANDLE,
	       CMD_ACCOUNT_ISCONNECTED, CMD_ACCOUNT_LIST,
	       CMD_ACCOUNT_PROTOCOL, CMD_ACCOUNT_USERNAME } cmd;
	const char *listopts[] = { "-all", "-online", NULL };
	enum { CMD_ACCOUNTLIST_ALL, CMD_ACCOUNTLIST_ONLINE } listopt;
	const char *alias;
	GList *cur;
	GaimAccount *account;
	int error;

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
		error = Tcl_GetIntFromObj(interp, objv[2], (int *)&account);
		if (error || !tcl_validate_account(account, interp))
			return TCL_ERROR;
		alias = gaim_account_get_alias(account);
		Tcl_SetStringObj(result, alias ? (char *)alias : "", -1);
		break;
	case CMD_ACCOUNT_CONNECT:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "account");
			return TCL_ERROR;
		}
		error = Tcl_GetIntFromObj(interp, objv[2], (int *)&account);
		if (error || !tcl_validate_account(account, interp))
			return TCL_ERROR;
		if (!gaim_account_is_connected(account))
			gaim_account_connect(account);
		Tcl_SetIntObj(result, (int)gaim_account_get_connection(account));
		break;
	case CMD_ACCOUNT_CONNECTION:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "account");
			return TCL_ERROR;
		}
		error = Tcl_GetIntFromObj(interp, objv[2], (int *)&account);
		if (error || !tcl_validate_account(account, interp))
			return TCL_ERROR;
		Tcl_SetIntObj(result, (int)gaim_account_get_connection(account));
		break;
	case CMD_ACCOUNT_DISCONNECT:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "account");
			return TCL_ERROR;
		}
		error = Tcl_GetIntFromObj(interp, objv[2], (int *)&account);
		if (error || !tcl_validate_account(account, interp))
			return TCL_ERROR;
		gaim_account_disconnect(account);
		break;
	case CMD_ACCOUNT_FIND:
		if (objc != 4) {
			Tcl_WrongNumArgs(interp, 2, objv, "username protocol");
			return TCL_ERROR;
		}
		Tcl_SetIntObj(result, (int)gaim_accounts_find(Tcl_GetString(objv[2]),
									   Tcl_GetString(objv[3])));
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
		error = Tcl_GetIntFromObj(interp, objv[2], (int *)&account);
		if (error || !tcl_validate_account(account, interp))
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
			elem = Tcl_NewIntObj((int)account);
			Tcl_ListObjAppendElement(interp, list, elem);
		}
		Tcl_SetObjResult(interp, list);
		break;
	case CMD_ACCOUNT_PROTOCOL:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "account");
			return TCL_ERROR;
		}
		error = Tcl_GetIntFromObj(interp, objv[2], (int *)&account);
		if (error || !tcl_validate_account(account, interp))
			return TCL_ERROR;
		Tcl_SetStringObj(result, (char *)gaim_account_get_protocol_id(account), -1);
		break;
	case CMD_ACCOUNT_USERNAME:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "account");
			return TCL_ERROR;
		}
		error = Tcl_GetIntFromObj(interp, objv[2], (int *)&account);
		if (error || !tcl_validate_account(account, interp))
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
	if (Tcl_GetIntFromObj(interp, elems[2], (int *)&account) != TCL_OK)
		return NULL;
	if (!tcl_validate_account(account, interp))
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
			if ((error = Tcl_GetIntFromObj(interp, elems[2], (int *)&account)) != TCL_OK)
				return TCL_ERROR;
			if (!tcl_validate_account(account, interp))
				return TCL_ERROR;
			serv_get_info(gaim_account_get_connection(account), Tcl_GetString(elems[1]));
		} else {
			if ((error = Tcl_GetIntFromObj(interp, objv[2], (int *)&account)) != TCL_OK)
				return error;
			if (!tcl_validate_account(account, interp))
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
						Tcl_ListObjAppendElement(interp, tclbud, Tcl_NewIntObj((int)bud->account));
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
					Tcl_ListObjAppendElement(interp, tclbud, Tcl_NewIntObj((int)cnode->account));
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
		error = Tcl_GetIntFromObj(interp, objv[2], (int *)&gc);
		if (error || !tcl_validate_gc(gc)) {
			Tcl_SetStringObj(result, "invalid gc", -1);
			return TCL_ERROR;
		}
		Tcl_SetIntObj(result, (int)gaim_connection_get_account(gc));
		break;
	case CMD_CONN_DISPLAYNAME:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "gc");
			return TCL_ERROR;
		}
		error = Tcl_GetIntFromObj(interp, objv[2], (int *)&gc);
		if (error || !tcl_validate_gc(gc)) {
			Tcl_SetStringObj(result, "invalid gc", -1);
			return TCL_ERROR;
		}
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
			elem = Tcl_NewIntObj((int)cur->data);
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
	const char *cmds[] = { "find", "handle", "list", "new", "write", NULL };
	enum { CMD_CONV_FIND, CMD_CONV_HANDLE, CMD_CONV_LIST, CMD_CONV_NEW, CMD_CONV_WRITE } cmd;
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
		if ((error = Tcl_GetIntFromObj(interp, objv[2],
					       (int *)&account)) != TCL_OK)
			return error;
		if (!tcl_validate_account(account, interp))
			return TCL_ERROR;
		convo = gaim_find_conversation_with_account(GAIM_CONV_ANY,
							    Tcl_GetString(objv[3]),
							    account);
		Tcl_SetIntObj(result, (int)convo);
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
			elem = Tcl_NewIntObj((int)cur->data);
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
		type = GAIM_CONV_IM;
		while (argsused < objc) {
			opt = Tcl_GetString(objv[argsused]);
			if (*opt == '-') {
				if ((error = Tcl_GetIndexFromObj(interp, objv[argsused], newopts,
								 "option", 0, (int *)&newopt)) != TCL_OK)
					return error;
				argsused++;
				switch (newopt) {
				case CMD_CONV_NEW_CHAT:
					type = GAIM_CONV_CHAT;
					break;
				case CMD_CONV_NEW_IM:
					type = GAIM_CONV_IM;
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
		if ((error = Tcl_GetIntFromObj(interp, objv[argsused++], (int *)&account)) != TCL_OK)
			return error;
		if (!tcl_validate_account(account, interp))
			return TCL_ERROR;
		convo = gaim_conversation_new(type, account, Tcl_GetString(objv[argsused]));
		Tcl_SetIntObj(result, (int)convo);
		break;
	case CMD_CONV_WRITE:
		if (objc != 6) {
			Tcl_WrongNumArgs(interp, 2, objv, "conversation style from what");
			return TCL_ERROR;
		}
		if ((error = Tcl_GetIntFromObj(interp, objv[2], (int *)&convo)) != TCL_OK)
			return error;
		if ((error = Tcl_GetIndexFromObj(interp, objv[3], styles, "style", 0, (int *)&style)) != TCL_OK)
			return error;
		if (!tcl_validate_conversation(convo, interp))
			return TCL_ERROR;
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
		if (gaim_conversation_get_type(convo) == GAIM_CONV_CHAT)
			gaim_conv_chat_write(GAIM_CONV_CHAT(convo), from, what, flags, time(NULL));
		else
			gaim_conv_im_write(GAIM_CONV_IM(convo), from, what, flags, time(NULL));
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

int tcl_cmd_send_im(ClientData unused, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
	GaimConnection *gc;
	char *who, *text;
	int error;
	Tcl_Obj *result;

	if (objc != 4) {
		Tcl_WrongNumArgs(interp, 1, objv, "gc who text");
		return TCL_ERROR;
	}

	if ((error = Tcl_GetIntFromObj(interp, objv[1], (int *)&gc)) != TCL_OK)
		return error;
	if (!tcl_validate_gc(gc)) {
		result = Tcl_GetObjResult(interp);
		Tcl_SetStringObj(result, "invalid gc", -1);
		return TCL_ERROR;
	}

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
			Tcl_WrongNumArgs(interp, 2, objv, "signal");
			return TCL_ERROR;
		}
		if ((error = Tcl_GetIntFromObj(interp, objv[2], (int *)&instance)) != TCL_OK)
			return error;
		tcl_signal_disconnect(instance, Tcl_GetString(objv[3]), interp);
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
