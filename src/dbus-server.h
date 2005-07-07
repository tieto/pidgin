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


G_BEGIN_DECLS

/* These are the categories of codes used by gaim dbus implementation
   for remote calls.  In practice, they don't matter
 */
typedef enum {
  DBUS_ERROR_NONE = 0,
  DBUS_ERROR_NOT_FOUND = 1
} DbusErrorCodes;

/* Types of pointers that can be registered with the gaim dbus pointer
   registration engine.  See below */
typedef enum {
  DBUS_POINTER_GROUP,
  DBUS_POINTER_CONTACT,
  DBUS_POINTER_BUDDY,
  DBUS_POINTER_CHAT,
  DBUS_POINTER_ACCOUNT
} GaimDBusPointerType;


/**
 * Starts the gaim DBUS server.  It is responsible for handling DBUS
 * requests from other applications.
 *
 * @return TRUE if successful, FALSE otherwise.
 */
gboolean dbus_server_init(void);

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

    @node   The pointer to register.
    @type   Type of that pointer.
 */
void gaim_dbus_register_pointer(gpointer node, GaimDBusPointerType type);

/** 
    Unregisters a pointer previously registered with
    gaim_dbus_register_pointer.

    @node   The pointer to register.
 */
void gaim_dbus_unregister_pointer(gpointer node);


G_END_DECLS

#endif	/* _GAIM_DBUS_SERVER_H_ */
