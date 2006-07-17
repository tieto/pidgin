/**
 * @file dbus-server.h Gaim DBUS Server
 * @ingroup core
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _GAIM_DBUS_SERVER_H_
#define _GAIM_DBUS_SERVER_H_

#include "value.h"


G_BEGIN_DECLS

/** 
   Types of pointers are identified by the ADDRESS of a GaimDbusType
   object.  This way, plugins can easily access types defined in gaim
   proper as well as introduce their own types that will not conflict
   with those introduced by other plugins.

   The structure GaimDbusType has only one element (GaimDBusType::parent), a
   contains a pointer to the parent type, or @c NULL if the type has no
   parent.  Parent means the same as the base class in object oriented
   programming.  
*/

typedef struct _GaimDBusType GaimDBusType;

struct _GaimDBusType {
    GaimDBusType *parent;
};

/* By convention, the GaimDBusType variable representing each structure
   GaimSomeStructure has the name GAIM_DBUS_TYPE_GaimSomeStructure.
   The following macros facilitate defining such variables

   #GAIM_DBUS_DECLARE_TYPE declares an extern variable representing a
   given type, for use in header files.

   #GAIM_DBUS_DEFINE_TYPE defines a variable representing a given
   type, use in .c files.  It defines a new type without a parent; for
   types with a parent use #GAIM_DBUS_DEFINE_INHERITING_TYPE.
  */

#define GAIM_DBUS_TYPE(type) (&GAIM_DBUS_TYPE_##type)


#define GAIM_DBUS_DECLARE_TYPE(type) \
    extern GaimDBusType GAIM_DBUS_TYPE_##type;

#define GAIM_DBUS_DEFINE_TYPE(type) \
    GaimDBusType GAIM_DBUS_TYPE_##type = { NULL };

#define GAIM_DBUS_DEFINE_INHERITING_TYPE(type, parent) \
    GaimDBusType GAIM_DBUS_TYPE_##type = { GAIM_DBUS_TYPE(parent) };


/**
   Initializes gaim dbus pointer registration engine.

   Remote dbus applications need a way of addressing objects exposed
   by gaim to the outside world.  In gaim itself, these objects (such
   as GaimBuddy and company) are identified by pointers.  The gaim
   dbus pointer registration engine converts pointers to handles and
   back.

   In order for an object to participate in the scheme, it must
   register itself and its type with the engine.  This registration
   allocates an integer id which can be resolved to the pointer and
   back.  

   Handles are not persistent.  They are reissued every time gaim is
   started.  This is not good; external applications that use gaim
   should work even whether gaim was restarted in the middle of the
   interaction.

   Pointer registration is only a temporary solution.  When GaimBuddy
   and similar structures have been converted into gobjects, this
   registration will be done automatically by objects themselves.
   
   By the way, this kind of object-handle translation should be so
   common that there must be a library (maybe even glib) that
   implements it.  I feel a bit like reinventing the wheel here.
*/
void gaim_dbus_init_ids(void);

/** 
    Registers a typed pointer.

    @param node   The pointer to register.
    @param type   Type of that pointer.
 */
void gaim_dbus_register_pointer(gpointer node, GaimDBusType *type);

/** 
    Unregisters a pointer previously registered with
    gaim_dbus_register_pointer.

    @param node   The pointer to register.
 */
void gaim_dbus_unregister_pointer(gpointer node);



/**
    Emits a dbus signal.

    @param name        The name of the signal ("bla-bla-blaa")
    @param num_values  The number of parameters.
    @param values      Array of pointers to #GaimValue objects representing
                       the types of the parameters.
    @param vargs       A va_list containing the actual parameters.
  */
void gaim_dbus_signal_emit_gaim(const char *name, int num_values, 
				GaimValue **values, va_list vargs);

/**
 * Returns whether Gaim's D-BUS subsystem is up and running.  If it's
 * NOT running then gaim_dbus_dispatch_init() failed for some reason,
 * and a message should have been gaim_debug_error()'ed.
 *
 * This function should be called by any DBUS plugin before trying
 * to use the DBUS API.  See plugins/dbus-example.c for usage.
 *
 * @return If the D-BUS subsystem started with no problems then this
 *         will return NULL and everything will be hunky dory.  If
 *         there was an error initializing the D-BUS subsystem then
 *         this will return an error message explaining why.
 */
const char *gaim_dbus_get_init_error(void);

/**
 * Returns the dbus subsystem handle.
 *
 * @return The dbus subsystem handle.
 */
void *gaim_dbus_get_handle(void);

/**
 * Starts Gaim's D-BUS server.  It is responsible for handling DBUS
 * requests from other applications.
 */
void gaim_dbus_init(void);

/**
 * Uninitializes Gaim's D-BUS server.
 */
void gaim_dbus_uninit(void);

/**

 Macro #DBUS_EXPORT expands to nothing.  It is used to indicate to the
 dbus-analize-functions.py script that the given function should be
 available to other applications through DBUS.  If
 dbus-analize-functions.py is run without the "--export-only" option,
 this prefix is ignored.

 */

#define DBUS_EXPORT

/* 
   Here we include the list of #GAIM_DBUS_DECLARE_TYPE statements for
   all structs defined in gaim.  This file has been generated by the
   #dbus-analize-types.py script.
*/

#include "dbus-types.h"

G_END_DECLS

#endif	/* _GAIM_DBUS_SERVER_H_ */
