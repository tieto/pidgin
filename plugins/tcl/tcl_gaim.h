/**
 * @file tcl_gaim.h Gaim Tcl definitions
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

#ifndef _GAIM_TCL_GAIM_H_
#define _GAIM_TCL_GAIM_H_

#include <tcl.h>

#include "internal.h"
#include "plugin.h"
#include "value.h"
#include "stringref.h"

struct tcl_signal_handler {
	Tcl_Obj *signal;
	Tcl_Interp *interp;

	void *instance;
	Tcl_Obj *namespace;
	/* These following two are temporary during setup */
	Tcl_Obj *args;
	Tcl_Obj *proc;

	GaimValue *returntype;
	int nargs;
	GaimValue **argtypes;
};

extern GaimPlugin *_tcl_plugin;

/* Capitalized this way because these are "types" */
extern GaimStringref *GaimTclRefAccount;
extern GaimStringref *GaimTclRefConversation;
extern GaimStringref *GaimTclRefStatus;
extern GaimStringref *GaimTclRefStatusAttr;
extern GaimStringref *GaimTclRefStatusType;

GaimPlugin *tcl_interp_get_plugin(Tcl_Interp *interp);

void tcl_signal_init(void);
void tcl_signal_handler_free(struct tcl_signal_handler *handler);
void tcl_signal_cleanup(Tcl_Interp *interp);
gboolean tcl_signal_connect(struct tcl_signal_handler *handler);
void tcl_signal_disconnect(void *instance, const char *signal, Tcl_Interp *interp);

void gaim_tcl_ref_init();
void *gaim_tcl_ref_get(Tcl_Interp *interp, Tcl_Obj *obj, GaimStringref *type);
Tcl_Obj *gaim_tcl_ref_new(GaimStringref *type, void *value);

Tcl_ObjCmdProc tcl_cmd_account;
Tcl_ObjCmdProc tcl_cmd_signal_connect;
Tcl_ObjCmdProc tcl_cmd_buddy;
Tcl_ObjCmdProc tcl_cmd_connection;
Tcl_ObjCmdProc tcl_cmd_conversation;
Tcl_ObjCmdProc tcl_cmd_core;
Tcl_ObjCmdProc tcl_cmd_debug;
Tcl_ObjCmdProc tcl_cmd_notify;
Tcl_ObjCmdProc tcl_cmd_prefs;
Tcl_ObjCmdProc tcl_cmd_send_im;
Tcl_ObjCmdProc tcl_cmd_signal;
Tcl_ObjCmdProc tcl_cmd_status;
Tcl_ObjCmdProc tcl_cmd_status_attr;
Tcl_ObjCmdProc tcl_cmd_status_type;
Tcl_ObjCmdProc tcl_cmd_unload;

#endif /* _GAIM_TCL_GAIM_H_ */
