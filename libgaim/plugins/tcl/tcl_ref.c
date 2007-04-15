/**
 * @file tcl_ref.c Gaim Tcl typed references API
 *
 * gaim
 *
 * Copyright (C) 2006 Ethan Blanton <eblanton@cs.purdue.edu>
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
#include <glib.h>

#include "tcl_gaim.h"
#include "stringref.h"

/* Instead of all that internal representation mumbo jumbo, use these
 * macros to access the internal representation of a GaimTclRef */
#define OBJ_REF_TYPE(obj) (obj->internalRep.twoPtrValue.ptr1)
#define OBJ_REF_VALUE(obj) (obj->internalRep.twoPtrValue.ptr2)

static Tcl_FreeInternalRepProc gaim_tcl_ref_free;
static Tcl_DupInternalRepProc gaim_tcl_ref_dup;
static Tcl_UpdateStringProc gaim_tcl_ref_update;
static Tcl_SetFromAnyProc gaim_tcl_ref_set;

static Tcl_ObjType gaim_tcl_ref = {
	"GaimTclRef",
	gaim_tcl_ref_free,
	gaim_tcl_ref_dup,
	gaim_tcl_ref_update,
	gaim_tcl_ref_set
};

void gaim_tcl_ref_init()
{
	Tcl_RegisterObjType(&gaim_tcl_ref);
}

void *gaim_tcl_ref_get(Tcl_Interp *interp, Tcl_Obj *obj, GaimStringref *type)
{
	if (obj->typePtr != &gaim_tcl_ref) {
		if (Tcl_ConvertToType(interp, obj, &gaim_tcl_ref) != TCL_OK)
			return NULL;
	}
	if (strcmp(gaim_stringref_value(OBJ_REF_TYPE(obj)),
		   gaim_stringref_value(type))) {
		if (interp) {
			Tcl_Obj *error = Tcl_NewStringObj("Bad Gaim reference type: expected ", -1);
			Tcl_AppendToObj(error, gaim_stringref_value(type), -1);
			Tcl_AppendToObj(error, " but got ", -1);
			Tcl_AppendToObj(error, gaim_stringref_value(OBJ_REF_TYPE(obj)), -1);
			Tcl_SetObjResult(interp, error);
		}
		return NULL;
	}
	return OBJ_REF_VALUE(obj);
}

Tcl_Obj *gaim_tcl_ref_new(GaimStringref *type, void *value)
{
	Tcl_Obj *obj = Tcl_NewObj();
	obj->typePtr = &gaim_tcl_ref;
	OBJ_REF_TYPE(obj) = gaim_stringref_ref(type);
	OBJ_REF_VALUE(obj) = value;
	Tcl_InvalidateStringRep(obj);
	return obj;
}

static void gaim_tcl_ref_free(Tcl_Obj *obj)
{
	gaim_stringref_unref(OBJ_REF_TYPE(obj));
}

static void gaim_tcl_ref_dup(Tcl_Obj *obj1, Tcl_Obj *obj2)
{
	OBJ_REF_TYPE(obj2) = gaim_stringref_ref(OBJ_REF_TYPE(obj1));
	OBJ_REF_VALUE(obj2) = OBJ_REF_VALUE(obj1);
}

static void gaim_tcl_ref_update(Tcl_Obj *obj)
{
	/* This is ugly on memory, but we pretty much have to either
	 * do this or guesstimate lengths or introduce a varargs
	 * function in here ... ugh. */
	char *bytes = g_strdup_printf("gaim-%s:%p",
				      gaim_stringref_value(OBJ_REF_TYPE(obj)),
				      OBJ_REF_VALUE(obj));

	obj->length = strlen(bytes);
	obj->bytes = ckalloc(obj->length + 1);
	strcpy(obj->bytes, bytes);
	g_free(bytes);
}

/* This isn't as memory-efficient as setting could be, because we
 * essentially have to synthesize the Stringref here, where we would
 * really rather dup it.  Oh, well. */
static int gaim_tcl_ref_set(Tcl_Interp *interp, Tcl_Obj *obj)
{
	char *bytes = Tcl_GetStringFromObj(obj, NULL);
	char *ptr;
	GaimStringref *type;
	void *value;

	if (strlen(bytes) < 7
	    || strncmp(bytes, "gaim-", 5)
	    || (ptr = strchr(bytes, ':')) == NULL
	    || (ptr - bytes) == 5)
		goto badobject;

	/* Bad Ethan */
	*ptr = '\0';
	type = gaim_stringref_new(bytes + 5);
	*ptr = ':';
	ptr++;

	if (sscanf(ptr, "%p", &value) == 0) {
		gaim_stringref_unref(type);
		goto badobject;
	}

	/* At this point we know we have a good object; free the old and
	 * install our internal representation. */
	if (obj->typePtr != NULL && obj->typePtr->freeIntRepProc != NULL)
		obj->typePtr->freeIntRepProc(obj);

	obj->typePtr = &gaim_tcl_ref;
	OBJ_REF_TYPE(obj) = type;
	OBJ_REF_VALUE(obj) = value;

	return TCL_OK;

badobject:
	if (interp) {
		Tcl_SetObjResult(interp,
				 Tcl_NewStringObj("invalid GaimTclRef representation", -1));
	}
	return TCL_ERROR;
}
