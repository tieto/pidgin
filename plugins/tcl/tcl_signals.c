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

void tcl_signal_init()
{
	tcl_callbacks = NULL;
}

void tcl_signal_handler_free(struct tcl_signal_handler *handler)
{
	if (handler == NULL)
		return;

	g_free(handler->signal);
        if (handler->argnames != NULL)
		g_free(handler->argnames);
	Tcl_DecrRefCount(handler->proc);
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
	gaim_signal_get_values(handler->instance, handler->signal, &handler->returntype,
			       &handler->nargs, &handler->argtypes);

	if (handler->nargs != handler->nnames)
		return FALSE;

	tcl_signal_disconnect(handler->interp, handler->signal, handler->interp);

	if (!gaim_signal_connect_vargs(handler->instance, handler->signal, (void *)handler->interp,
				       GAIM_CALLBACK(tcl_signal_callback), (void *)handler))
		return FALSE;

	tcl_callbacks = g_list_append(tcl_callbacks, (gpointer)handler);

	return TRUE;
}

void tcl_signal_disconnect(void *instance, const char *signal, Tcl_Interp *interp)
{
	GList *cur;
	struct tcl_signal_handler *handler;
	gboolean found = FALSE;

	for (cur = tcl_callbacks; cur != NULL; cur = g_list_next(cur)) {
		handler = cur->data;
		if (handler->interp == interp && handler->instance == instance 
		    && !strcmp(signal, handler->signal)) {
			gaim_signal_disconnect(instance, signal, handler->interp,
					       GAIM_CALLBACK(tcl_signal_callback));
			tcl_signal_handler_free(handler);
			cur->data = NULL;
			found = TRUE;
			break;
		}
	}
	if (found)
		tcl_callbacks = g_list_remove_all(tcl_callbacks, NULL);
}

static void *tcl_signal_callback(va_list args, struct tcl_signal_handler *handler)
{
	struct var {
		void *val;
		char *str;
	} *vars;
	GString *val, *name;
	GaimBlistNode *node;
	int error, i;
	void *retval = NULL;
	Tcl_Obj *result;

	vars = g_new0(struct var, handler->nargs);
	val = g_string_sized_new(32);
	name = g_string_sized_new(32);

	for (i = 0; i < handler->nargs; i++) {
		g_string_printf(name, "::gaim::_callback::%s", handler->argnames[i]);

		switch(gaim_value_get_type(handler->argtypes[i])) {
		default: /* Yes, at the top */
		case GAIM_TYPE_UNKNOWN:	/* What?  I guess just pass the word ... */
			/* treat this as a pointer, but complain first */
			gaim_debug(GAIM_DEBUG_ERROR, "tcl", "unknown GaimValue type %d\n",
				   gaim_value_get_type(handler->argtypes[i]));
		case GAIM_TYPE_POINTER:
		case GAIM_TYPE_OBJECT:
		case GAIM_TYPE_BOXED:
			/* These are all "pointer" types to us */
			if (gaim_value_is_outgoing(handler->argtypes[i])) {
				vars[i].val = va_arg(args, void **);
				Tcl_LinkVar(handler->interp, name->str, vars[i].val, TCL_LINK_INT);
			} else {
				vars[i].val = va_arg(args, void *);
				Tcl_LinkVar(handler->interp, name->str, (char *)&vars[i].val,
					    TCL_LINK_INT|TCL_LINK_READ_ONLY);
			}
			break;
		case GAIM_TYPE_BOOLEAN:
			if (gaim_value_is_outgoing(handler->argtypes[i])) {
				vars[i].val = va_arg(args, gboolean *);
				Tcl_LinkVar(handler->interp, name->str, vars[i].val, TCL_LINK_BOOLEAN);
			} else {
				vars[i].val = (void *)va_arg(args, gboolean);
				Tcl_LinkVar(handler->interp, name->str, (char *)&vars[i].val,
					    TCL_LINK_BOOLEAN|TCL_LINK_READ_ONLY);
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
			/* These next two are totally bogus */
		case GAIM_TYPE_INT64:
		case GAIM_TYPE_UINT64:
			/* I should really cast these individually to
			 * preserve as much information as possible ...
			 * but heh */
			if (gaim_value_is_outgoing(handler->argtypes[i])) {
				vars[i].val = (void *)va_arg(args, int *);
				Tcl_LinkVar(handler->interp, name->str, vars[i].val, TCL_LINK_INT);
			} else {
				vars[i].val = (void *)va_arg(args, int);
				Tcl_LinkVar(handler->interp, name->str, (char *)&vars[i].val,
					    TCL_LINK_INT|TCL_LINK_READ_ONLY);
			}
			break;
		case GAIM_TYPE_STRING:
			if (gaim_value_is_outgoing(handler->argtypes[i])) {
				vars[i].val = (void *)va_arg(args, char **);
				if (vars[i].val != NULL && *(char **)vars[i].val != NULL) {
					vars[i].str = (char *)ckalloc(strlen(*(char **)vars[i].val) + 1);
					strcpy(vars[i].str, *(char **)vars[i].val);
				} else {
					vars[i].str = (char *)ckalloc(1);
					*vars[i].str = '\0';
				}
				Tcl_LinkVar(handler->interp, name->str, (char *)&vars[i].str, TCL_LINK_STRING);
			} else {
				vars[i].val = (void *)va_arg(args, char *);
				Tcl_LinkVar(handler->interp, name->str, (char *)&vars[i].val,
					    TCL_LINK_STRING|TCL_LINK_READ_ONLY);
			}
			break;
		case GAIM_TYPE_SUBTYPE:
			switch (gaim_value_get_subtype(handler->argtypes[i])) {
			default:
			case GAIM_SUBTYPE_UNKNOWN:
				gaim_debug(GAIM_DEBUG_ERROR, "tcl", "subtype unknown\n");
			case GAIM_SUBTYPE_ACCOUNT:
			case GAIM_SUBTYPE_CONNECTION:
			case GAIM_SUBTYPE_CONVERSATION:
			case GAIM_SUBTYPE_CONV_WINDOW:
			case GAIM_SUBTYPE_PLUGIN:
				/* pointers again */
				if (gaim_value_is_outgoing(handler->argtypes[i])) {
					vars[i].val = va_arg(args, void **);
					Tcl_LinkVar(handler->interp, name->str, vars[i].val, TCL_LINK_INT);
				} else {
					vars[i].val = va_arg(args, void *);
					Tcl_LinkVar(handler->interp, name->str, (char *)&vars[i].val,
						    TCL_LINK_INT|TCL_LINK_READ_ONLY);
				}
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
					g_string_printf(val, "group {%s}", ((GaimGroup *)node)->name);
					break;
				case GAIM_BLIST_CONTACT_NODE:
					/* g_string_printf(val, "contact {%s}", Contact Name? ); */
					break;
				case GAIM_BLIST_BUDDY_NODE:
					g_string_printf(val, "buddy {%s} %lu", ((GaimBuddy *)node)->name,
							(unsigned long)((GaimBuddy *)node)->account);
					break;
				case GAIM_BLIST_CHAT_NODE:
					g_string_printf(val, "chat {%s} %lu", ((GaimChat *)node)->alias,
							(unsigned long)((GaimChat *)node)->account);
					break;
				case GAIM_BLIST_OTHER_NODE:
					g_string_printf(val, "other");
					break;
				}
				vars[i].str = g_strdup(val->str);
				Tcl_LinkVar(handler->interp, name->str, (char *)&vars[i].str,
					    TCL_LINK_STRING|TCL_LINK_READ_ONLY);
				break;
			}
		}
	}

	/* Call the friggin' procedure already */
	if ((error = Tcl_EvalObjEx(handler->interp, handler->proc, TCL_EVAL_GLOBAL)) != TCL_OK) {
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
		g_string_printf(name, "::gaim::_callback::%s", handler->argnames[i]);
		Tcl_UnlinkVar(handler->interp, name->str);
		/* We basically only have to deal with strings and buddies
		 * on the way out */
		switch (gaim_value_get_type(handler->argtypes[i])) {
		case GAIM_TYPE_STRING:
			if (gaim_value_is_outgoing(handler->argtypes[i])) {
				if (vars[i].val != NULL && *(char **)vars[i].val != NULL) {
					g_free(*(char **)vars[i].val);
					*(char **)vars[i].val = g_strdup(vars[i].str);
				}
				ckfree(vars[i].str);
			}
			break;
		case GAIM_TYPE_SUBTYPE:
			switch(gaim_value_get_subtype(handler->argtypes[i])) {
			case GAIM_SUBTYPE_BLIST:
			case GAIM_SUBTYPE_BLIST_BUDDY:
			case GAIM_SUBTYPE_BLIST_GROUP:
			case GAIM_SUBTYPE_BLIST_CHAT:
				g_free(vars[i].str);
				break;
			default:
				/* nothing */
				;
			}
			break;
		default:
			/* nothing */
			;
		}
	}

	g_string_free(name, TRUE);
	g_string_free(val, TRUE);
	g_free(vars);

	return retval;
}
