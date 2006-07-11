/**
 * @file tcl_signals.c Gaim Tcl signal API
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
#include <stdarg.h>

#include "tcl_gaim.h"

#include "internal.h"
#include "connection.h"
#include "conversation.h"
#include "signals.h"
#include "debug.h"
#include "value.h"
#include "core.h"

static GList *tcl_callbacks;

static void *tcl_signal_callback(va_list args, struct tcl_signal_handler *handler);
static Tcl_Obj *new_cb_namespace (void);

void tcl_signal_init()
{
	tcl_callbacks = NULL;
}

void tcl_signal_handler_free(struct tcl_signal_handler *handler)
{
	if (handler == NULL)
		return;

	Tcl_DecrRefCount(handler->signal);
	if (handler->namespace)
		Tcl_DecrRefCount(handler->namespace);
	g_free(handler);
}

void tcl_signal_cleanup(Tcl_Interp *interp)
{
	GList *cur;
	struct tcl_signal_handler *handler;

	for (cur = tcl_callbacks; cur != NULL; cur = g_list_next(cur)) {
		handler = cur->data;
		if (handler->interp == interp) {
			tcl_signal_handler_free(handler);
			cur->data = NULL;
		}
	}
	tcl_callbacks = g_list_remove_all(tcl_callbacks, NULL);
}

gboolean tcl_signal_connect(struct tcl_signal_handler *handler)
{
	GString *proc;

	gaim_signal_get_values(handler->instance,
			       Tcl_GetString(handler->signal),
			       &handler->returntype, &handler->nargs,
			       &handler->argtypes);

	tcl_signal_disconnect(handler->interp, Tcl_GetString(handler->signal),
			      handler->interp);

	if (!gaim_signal_connect_vargs(handler->instance,
				       Tcl_GetString(handler->signal),
				       (void *)handler->interp,
				       GAIM_CALLBACK(tcl_signal_callback),
				       (void *)handler))
		return FALSE;

	handler->namespace = new_cb_namespace ();
	Tcl_IncrRefCount(handler->namespace);
	proc = g_string_new("");
	g_string_append_printf(proc, "namespace eval %s { proc cb { %s } { %s } }",
			       Tcl_GetString(handler->namespace),
			       Tcl_GetString(handler->args),
	                       Tcl_GetString(handler->proc));
	if (Tcl_Eval(handler->interp, proc->str) != TCL_OK) {
		Tcl_DecrRefCount(handler->namespace);
		g_string_free(proc, TRUE);
		return FALSE;
	}
	g_string_free(proc, TRUE);

	tcl_callbacks = g_list_append(tcl_callbacks, (gpointer)handler);

	return TRUE;
}

void tcl_signal_disconnect(void *instance, const char *signal, Tcl_Interp *interp)
{
	GList *cur;
	struct tcl_signal_handler *handler;
	gboolean found = FALSE;
	GString *cmd;

	for (cur = tcl_callbacks; cur != NULL; cur = g_list_next(cur)) {
		handler = cur->data;
		if (handler->interp == interp && handler->instance == instance 
		    && !strcmp(signal, Tcl_GetString(handler->signal))) {
			gaim_signal_disconnect(instance, signal, handler->interp,
					       GAIM_CALLBACK(tcl_signal_callback));
			cmd = g_string_sized_new(64);
			g_string_printf(cmd, "namespace delete %s",
					Tcl_GetString(handler->namespace));
			Tcl_EvalEx(interp, cmd->str, -1, TCL_EVAL_GLOBAL);
			tcl_signal_handler_free(handler);
			g_string_free(cmd, TRUE);
			cur->data = NULL;
			found = TRUE;
			break;
		}
	}
	if (found)
		tcl_callbacks = g_list_remove_all(tcl_callbacks, NULL);
}

static GaimStringref *ref_type(GaimSubType type)
{
	switch (type) {
	case GAIM_SUBTYPE_ACCOUNT:
		return GaimTclRefAccount;
	case GAIM_SUBTYPE_CONNECTION:
		return GaimTclRefConnection;
	case GAIM_SUBTYPE_CONVERSATION:
		return GaimTclRefConversation;
	case GAIM_SUBTYPE_PLUGIN:
		return GaimTclRefPlugin;
	case GAIM_SUBTYPE_STATUS:
		return GaimTclRefStatus;
	case GAIM_SUBTYPE_XFER:
		return GaimTclRefXfer;
	default:
		return NULL;
	}
}

static void *tcl_signal_callback(va_list args, struct tcl_signal_handler *handler)
{
	GString *name, *val;
	GaimBlistNode *node;
	int error, i;
	void *retval = NULL;
	Tcl_Obj *cmd, *arg, *result;
	void **vals; /* Used for inout parameters */
	char ***strs;

	vals = g_new0(void *, handler->nargs);
	strs = g_new0(char **, handler->nargs);
	name = g_string_sized_new(32);
	val = g_string_sized_new(32);

	cmd = Tcl_NewListObj(0, NULL);
	Tcl_IncrRefCount(cmd);

	arg = Tcl_DuplicateObj(handler->namespace);
	Tcl_AppendStringsToObj(arg, "::cb", NULL);
	Tcl_ListObjAppendElement(handler->interp, cmd, arg);

	for (i = 0; i < handler->nargs; i++) {
		if (gaim_value_is_outgoing(handler->argtypes[i]))
			g_string_printf(name, "%s::arg%d",
					Tcl_GetString(handler->namespace), i);

		switch(gaim_value_get_type(handler->argtypes[i])) {
		case GAIM_TYPE_UNKNOWN:	/* What?  I guess just pass the word ... */
			/* treat this as a pointer, but complain first */
			gaim_debug(GAIM_DEBUG_ERROR, "tcl", "unknown GaimValue type %d\n",
				   gaim_value_get_type(handler->argtypes[i]));
		case GAIM_TYPE_POINTER:
		case GAIM_TYPE_OBJECT:
		case GAIM_TYPE_BOXED:
			/* These are all "pointer" types to us */
			if (gaim_value_is_outgoing(handler->argtypes[i]))
				gaim_debug_error("tcl", "pointer types do not currently support outgoing arguments\n");
			arg = gaim_tcl_ref_new(GaimTclRefPointer, va_arg(args, void *));
			break;
		case GAIM_TYPE_BOOLEAN:
			if (gaim_value_is_outgoing(handler->argtypes[i])) {
				vals[i] = va_arg(args, gboolean *);
				Tcl_LinkVar(handler->interp, name->str,
					    (char *)&vals[i], TCL_LINK_BOOLEAN);
				arg = Tcl_NewStringObj(name->str, -1);
			} else {
				arg = Tcl_NewBooleanObj(va_arg(args, gboolean));
			}
			break;
		case GAIM_TYPE_CHAR:
		case GAIM_TYPE_UCHAR:
		case GAIM_TYPE_SHORT:
		case GAIM_TYPE_USHORT:
		case GAIM_TYPE_INT:
		case GAIM_TYPE_UINT:
		case GAIM_TYPE_LONG:
		case GAIM_TYPE_ULONG:
		case GAIM_TYPE_ENUM:
			/* I should really cast these individually to
			 * preserve as much information as possible ...
			 * but heh */
			if (gaim_value_is_outgoing(handler->argtypes[i])) {
				vals[i] = va_arg(args, int *);
				Tcl_LinkVar(handler->interp, name->str,
					    vals[i], TCL_LINK_INT);
				arg = Tcl_NewStringObj(name->str, -1);
			} else {
				arg = Tcl_NewIntObj(va_arg(args, int));
			}
		case GAIM_TYPE_INT64:
		case GAIM_TYPE_UINT64:
			/* Tcl < 8.4 doesn't have wide ints, so we have ugly
			 * ifdefs in here */
			if (gaim_value_is_outgoing(handler->argtypes[i])) {
				vals[i] = (void *)va_arg(args, gint64 *);
				#if (TCL_MAJOR_VERSION >= 8 && TCL_MINOR_VERSION >= 4)
				Tcl_LinkVar(handler->interp, name->str,
					    vals[i], TCL_LINK_WIDE_INT);
				#else
				/* This is going to cause weirdness at best,
				 * but what do you want ... we're losing
				 * precision */
				Tcl_LinkVar(handler->interp, name->str,
					    vals[i], TCL_LINK_INT);
				#endif /* Tcl >= 8.4 */
				arg = Tcl_NewStringObj(name->str, -1);
			} else {
				#if (TCL_MAJOR_VERSION >= 8 && TCL_MINOR_VERSION >= 4)
				arg = Tcl_NewWideIntObj(va_arg(args, gint64));
				#else
				arg = Tcl_NewIntObj((int)va_arg(args, int));
				#endif /* Tcl >= 8.4 */
			}
			break;
		case GAIM_TYPE_STRING:
			if (gaim_value_is_outgoing(handler->argtypes[i])) {
				strs[i] = va_arg(args, char **);
				if (strs[i] == NULL || *strs[i] == NULL) {
					vals[i] = ckalloc(1);
					*(char *)vals[i] = '\0';
				} else {
					vals[i] = ckalloc(strlen(*strs[i]) + 1);
					strcpy(vals[i], *strs[i]);
				}
				Tcl_LinkVar(handler->interp, name->str,
					    (char *)&vals[i], TCL_LINK_STRING);
				arg = Tcl_NewStringObj(name->str, -1);
			} else {
				arg = Tcl_NewStringObj(va_arg(args, char *), -1);
			}
			break;
		case GAIM_TYPE_SUBTYPE:
			switch (gaim_value_get_subtype(handler->argtypes[i])) {
			case GAIM_SUBTYPE_UNKNOWN:
				gaim_debug(GAIM_DEBUG_ERROR, "tcl", "subtype unknown\n");
			case GAIM_SUBTYPE_ACCOUNT:
			case GAIM_SUBTYPE_CONNECTION:
			case GAIM_SUBTYPE_CONVERSATION:
			case GAIM_SUBTYPE_STATUS:
			case GAIM_SUBTYPE_PLUGIN:
			case GAIM_SUBTYPE_XFER:
				if (gaim_value_is_outgoing(handler->argtypes[i]))
					gaim_debug_error("tcl", "pointer subtypes do not currently support outgoing arguments\n");
				arg = gaim_tcl_ref_new(ref_type(gaim_value_get_subtype(handler->argtypes[i])), va_arg(args, void *));
				break;
			case GAIM_SUBTYPE_BLIST:
			case GAIM_SUBTYPE_BLIST_BUDDY:
			case GAIM_SUBTYPE_BLIST_GROUP:
			case GAIM_SUBTYPE_BLIST_CHAT:
				/* We're going to switch again for code-deduping */
				if (gaim_value_is_outgoing(handler->argtypes[i]))
					node = *va_arg(args, GaimBlistNode **);
				else
					node = va_arg(args, GaimBlistNode *);
				switch (node->type) {
				case GAIM_BLIST_GROUP_NODE:
					arg = Tcl_NewListObj(0, NULL);
					Tcl_ListObjAppendElement(handler->interp, arg,
								 Tcl_NewStringObj("group", -1));
					Tcl_ListObjAppendElement(handler->interp, arg,
								 Tcl_NewStringObj(((GaimGroup *)node)->name, -1));
					break;
				case GAIM_BLIST_CONTACT_NODE:
					/* g_string_printf(val, "contact {%s}", Contact Name? ); */
					arg = Tcl_NewStringObj("contact", -1);
					break;
				case GAIM_BLIST_BUDDY_NODE:
					arg = Tcl_NewListObj(0, NULL);
					Tcl_ListObjAppendElement(handler->interp, arg,
								 Tcl_NewStringObj("buddy", -1));
					Tcl_ListObjAppendElement(handler->interp, arg,
								 Tcl_NewStringObj(((GaimBuddy *)node)->name, -1));
					Tcl_ListObjAppendElement(handler->interp, arg,
								 gaim_tcl_ref_new(GaimTclRefAccount,
										  ((GaimBuddy *)node)->account));
					break;
				case GAIM_BLIST_CHAT_NODE:
					arg = Tcl_NewListObj(0, NULL);
					Tcl_ListObjAppendElement(handler->interp, arg,
								 Tcl_NewStringObj("chat", -1));
					Tcl_ListObjAppendElement(handler->interp, arg,
								 Tcl_NewStringObj(((GaimChat *)node)->alias, -1));
					Tcl_ListObjAppendElement(handler->interp, arg,
								 gaim_tcl_ref_new(GaimTclRefAccount,
										  ((GaimChat *)node)->account));
					break;
				case GAIM_BLIST_OTHER_NODE:
					arg = Tcl_NewStringObj("other", -1);
					break;
				}
				break;
			}
		}
		Tcl_ListObjAppendElement(handler->interp, cmd, arg);
	}

	/* Call the friggin' procedure already */
	if ((error = Tcl_EvalObjEx(handler->interp, cmd, TCL_EVAL_GLOBAL)) != TCL_OK) {
		gaim_debug(GAIM_DEBUG_ERROR, "tcl", "error evaluating callback: %s\n",
			   Tcl_GetString(Tcl_GetObjResult(handler->interp)));
	} else {
		result = Tcl_GetObjResult(handler->interp);
		/* handle return values -- strings and words only */
		if (handler->returntype) {
			if (gaim_value_get_type(handler->returntype) == GAIM_TYPE_STRING) {
				retval = (void *)g_strdup(Tcl_GetString(result));
			} else {
				if ((error = Tcl_GetIntFromObj(handler->interp, result, (int *)&retval)) != TCL_OK) {
					gaim_debug(GAIM_DEBUG_ERROR, "tcl", "Error retrieving procedure result: %s\n",
						   Tcl_GetString(Tcl_GetObjResult(handler->interp)));
					retval = NULL;
				}
			}
		}
	}

	/* And finally clean up */
	for (i = 0; i < handler->nargs; i++) {
		g_string_printf(name, "%s::arg%d",
				Tcl_GetString(handler->namespace), i);
		if (gaim_value_is_outgoing(handler->argtypes[i])
		    && gaim_value_get_type(handler->argtypes[i]) != GAIM_TYPE_SUBTYPE)
			Tcl_UnlinkVar(handler->interp, name->str);

		/* We basically only have to deal with strings on the
		 * way out */
		switch (gaim_value_get_type(handler->argtypes[i])) {
		case GAIM_TYPE_STRING:
			if (gaim_value_is_outgoing(handler->argtypes[i])) {
				if (vals[i] != NULL && *(char **)vals[i] != NULL) {
					g_free(*strs[i]);
					*strs[i] = g_strdup(vals[i]);
				}
				ckfree(vals[i]);
			}
			break;
		default:
			/* nothing */
			;
		}
	}

	g_string_free(name, TRUE);
	g_string_free(val, TRUE);
	g_free(vals);
	g_free(strs);

	return retval;
}

static Tcl_Obj *new_cb_namespace ()
{
	static int cbnum;
	char name[32];

	g_snprintf (name, sizeof(name), "::gaim::_callback::cb_%d", cbnum++);
	return Tcl_NewStringObj (name, -1);
}
