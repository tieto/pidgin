/* This file contains macros that wrap calls to the gaim dbus module.
   These macros call the appropriate functions if the build includes
   dbus support and do nothing otherwise.  See "dbus-server.h" for
   documentation.  */

#ifndef _GAIM_DBUS_MAYBE_H_
#define _GAIM_DBUS_MAYBE_H_

#ifdef HAVE_DBUS

#include "dbus-server.h"

/* this provides a type check */
#define GAIM_DBUS_REGISTER_POINTER(ptr, type) { \
    type *typed_ptr = ptr; \
    gaim_dbus_register_pointer(typed_ptr, GAIM_DBUS_TYPE(type));	\
}
#define GAIM_DBUS_UNREGISTER_POINTER(ptr) gaim_dbus_unregister_pointer(ptr)

#else  /* !HAVE_DBUS */

#define GAIM_DBUS_REGISTER_POINTER(ptr, type) 
#define GAIM_DBUS_UNREGISTER_POINTER(ptr)
#define DBUS_EXPORT

#endif	/* HAVE_DBUS */

#endif
