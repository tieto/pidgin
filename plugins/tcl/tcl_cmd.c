/**
 * @file tcl_cmd.c Gaim Tcl cmd API
 *
 * gaim
 *
 * Copyright (C) 2006 Etan Reisner <deryni@gmail.com>
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

#include "tcl_gaim.h"

#include "internal.h"
#include "cmds.h"
#include "debug.h"

static GList *tcl_cmd_callbacks;

static GaimCmdRet tcl_cmd_callback(GaimConversation *conv, const gchar *cmd,
                                   gchar **args, gchar **errors,
                                   struct tcl_cmd_handler *handler);
static Tcl_Obj *new_cmd_cb_namespace(void);

void tcl_cmd_init()
{
    tcl_cmd_callbacks = NULL;
}

void tcl_cmd_handler_free(struct tcl_cmd_handler *handler)
{
	if (handler == NULL)
		return;

	Tcl_DecrRefCount(handler->namespace);
	g_free(handler);
}

void tcl_cmd_cleanup(Tcl_Interp *interp)
{
	GList *cur;
	struct tcl_cmd_handler *handler;

	for (cur = tcl_cmd_callbacks; cur != NULL; cur = g_list_next(cur)) {
		handler = cur->data;
		if (handler->interp == interp) {
			gaim_cmd_unregister(handler->id);
			tcl_cmd_handler_free(handler);
			cur->data = NULL;
		}
	}
	tcl_cmd_callbacks = g_list_remove_all(tcl_cmd_callbacks, NULL);
}

GaimCmdId tcl_cmd_register(struct tcl_cmd_handler *handler)
{
	int id;
	GString *proc;

	if ((id = gaim_cmd_register(Tcl_GetString(handler->cmd),
				    handler->args, handler->priority,
				    handler->flags, handler->prpl_id,
				    GAIM_CMD_FUNC(tcl_cmd_callback),
				    handler->helpstr, (void *)handler)) == 0)
		return 0;

	handler->namespace = new_cmd_cb_namespace ();
	Tcl_IncrRefCount(handler->namespace);
	proc = g_string_new("");
	g_string_append_printf(proc, "namespace eval %s { proc cb { conv cmd arglist } { %s } }",
	                       Tcl_GetString(handler->namespace),
	                       Tcl_GetString(handler->proc));
	if (Tcl_Eval(handler->interp, proc->str) != TCL_OK) {
		Tcl_DecrRefCount(handler->namespace);
		g_string_free(proc, TRUE);
		return 0;
	}
	g_string_free(proc, TRUE);

	tcl_cmd_callbacks = g_list_append(tcl_cmd_callbacks, (gpointer)handler);

	return id;
}

void tcl_cmd_unregister(GaimCmdId id, Tcl_Interp *interp)
{
	GList *cur;
	GString *cmd;
	gboolean found = FALSE;
	struct tcl_cmd_handler *handler;

	for (cur = tcl_cmd_callbacks; cur != NULL; cur = g_list_next(cur)) {
		handler = cur->data;
		if (handler->interp == interp && handler->id == id) {
			gaim_cmd_unregister(id);
			cmd = g_string_sized_new(64);
			g_string_printf(cmd, "namespace delete %s",
			                Tcl_GetString(handler->namespace));
			Tcl_EvalEx(interp, cmd->str, -1, TCL_EVAL_GLOBAL);
			tcl_cmd_handler_free(handler);
			g_string_free(cmd, TRUE);
			cur->data = NULL;
			found = TRUE;
			break;
		}
	}

	if (found)
		tcl_cmd_callbacks = g_list_remove_all(tcl_cmd_callbacks, NULL);
}

static GaimCmdRet tcl_cmd_callback(GaimConversation *conv, const gchar *cmd,
                                   gchar **args, gchar **errors,
                                   struct tcl_cmd_handler *handler)
{
	int retval, error, i;
	Tcl_Obj *command, *arg, *tclargs, *result;

	command = Tcl_NewListObj(0, NULL);
	Tcl_IncrRefCount(command);

	/* The callback */
	arg = Tcl_DuplicateObj(handler->namespace);
	Tcl_AppendStringsToObj(arg, "::cb", NULL);
	Tcl_ListObjAppendElement(handler->interp, command, arg);

	/* The conversation */
	arg = Tcl_NewIntObj((int)conv);
	Tcl_ListObjAppendElement(handler->interp, command, arg);

	/* The command */
	arg = Tcl_NewStringObj(cmd, -1);
	Tcl_ListObjAppendElement(handler->interp, command, arg);

	/* The args list */
	tclargs = Tcl_NewListObj(0, NULL);
	for (i = 0; i < handler->nargs; i++) {
		arg = Tcl_NewStringObj(args[i], -1);

		Tcl_ListObjAppendElement(handler->interp, tclargs, arg);
	}
	Tcl_ListObjAppendElement(handler->interp, command, tclargs);

	if ((error = Tcl_EvalObjEx(handler->interp, command,
	                           TCL_EVAL_GLOBAL)) != TCL_OK) {
		gchar *errorstr;

		errorstr = g_strdup_printf("error evaluating callback: %s\n",
		                           Tcl_GetString(Tcl_GetObjResult(handler->interp)));
		gaim_debug(GAIM_DEBUG_ERROR, "tcl", errorstr);
		*errors = errorstr;
		retval = GAIM_CMD_RET_FAILED;
	} else {
		result = Tcl_GetObjResult(handler->interp);
		if ((error = Tcl_GetIntFromObj(handler->interp, result,
		                               &retval)) != TCL_OK) {
			gchar *errorstr;

			errorstr = g_strdup_printf("Error retreiving procedure result: %s\n",
			                           Tcl_GetString(Tcl_GetObjResult(handler->interp)));
			gaim_debug(GAIM_DEBUG_ERROR, "tcl", errorstr);
			*errors = errorstr;
			retval = GAIM_CMD_RET_FAILED;
		}
	}

	return retval;
}

static Tcl_Obj *new_cmd_cb_namespace()
{
	char name[32];
	static int cbnum;

	g_snprintf(name, sizeof(name), "::gaim::_callback::cb_%d", cbnum++);
	return Tcl_NewStringObj(name, -1);
}
