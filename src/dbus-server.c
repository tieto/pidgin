/*
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

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-bindings.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <glib/gquark.h>

#include "account.h"
#include "dbus-gaim.h"
#include "dbus-server.h"
#include "debug.h"
#include "core.h"

/* GaimObject, basic definitions*/

typedef struct GaimObject GaimObject;
typedef struct GaimObjectClass GaimObjectClass;

GType gaim_object_get_type(void);

struct GaimObject {
	GObject parent;
};

struct GaimObjectClass {
	GObjectClass parent;
};

#define GAIM_TYPE_OBJECT              (gaim_object_get_type ())
#define GAIM_OBJECT(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GAIM_TYPE_OBJECT, GaimObject))
#define GAIM_OBJECT_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GAIM_TYPE_OBJECT, GaimObjectClass))
#define GAIM_IS_OBJECT(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GAIM_TYPE_OBJECT))
#define GAIM_IS_OBJECT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GAIM_TYPE_OBJECT))
#define GAIM_OBJECT_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GAIM_TYPE_OBJECT, GaimObjectClass))

G_DEFINE_TYPE(GaimObject, gaim_object, G_TYPE_OBJECT)


static void 
gaim_object_init(GaimObject *obj) 
{
}

static void 
gaim_object_class_init(GaimObjectClass *mobject_class) 
{ 
}

static GObject *gaim_object;


/* Implementations of remote DBUS calls  */


static gboolean 
gaim_object_ping(GaimObject *obj, GError **error) 
{
	return TRUE;
}

static gboolean 
gaim_object_quit(GaimObject *obj, GError **error) 
{
	g_timeout_add(0, gaim_core_quit_cb, NULL);
	return TRUE;
}

static gboolean
gaim_object_connect_all(GaimObject *obj, GError **error)
{
	GList *cur;

	for (cur = gaim_accounts_get_all(); cur != NULL; cur = cur->next) 
		gaim_account_connect((GaimAccount*) cur->data);

	return TRUE;
}

#include "dbus-server-bindings.c"


gboolean 
dbus_server_init(void) 
{
	DBusGConnection *connection;
	GError *error;
	DBusGProxy *driver_proxy;
	guint32 request_name_ret;

	gaim_debug_misc("dbus", "launching dbus server\n");

	/* Connect to the bus */

	error = NULL;
	connection = dbus_g_bus_get(DBUS_BUS_STARTER, &error);

	if (connection == NULL) {
		g_assert(error);
		gaim_debug_error("dbus", "Failed to open connection to bus: %s\n", 
				 error->message);
		g_error_free (error);
		return FALSE;
	}


	/* Instantiate the gaim dbus object and register it */
	
	gaim_object = g_object_new (GAIM_TYPE_OBJECT, NULL);

	dbus_g_object_type_install_info (GAIM_TYPE_OBJECT, 
					 &dbus_glib_gaim_object_object_info);

	dbus_g_connection_register_g_object (connection, DBUS_PATH_GAIM, 
					     gaim_object);


	/* Obtain a proxy for the DBus object  */

	driver_proxy = dbus_g_proxy_new_for_name (connection,
						  DBUS_SERVICE_DBUS,
						  DBUS_PATH_DBUS,
						  DBUS_INTERFACE_DBUS);


	/* Test whether the registration was successful */

	org_freedesktop_DBus_request_name(driver_proxy, DBUS_SERVICE_GAIM,
					  0, &request_name_ret, &error);

	if (error) {
		gaim_debug_error("dbus", "Failed to get name: %s\n", error->message);
		g_error_free (error);
		return FALSE;
	}

	if (request_name_ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
	{
		gaim_debug_error ("dbus", "Got result code %u from requesting name\n", 
				  request_name_ret);
		return FALSE;
	}

	gaim_debug_misc ("dbus", "GLib test service has name '%s'\n", 
			 DBUS_SERVICE_GAIM);
	gaim_debug_misc ("dbus", "GLib test service entering main loop\n");
	
	return TRUE;
}
