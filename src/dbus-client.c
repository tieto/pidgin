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

/* Do not use this program.  Use gaim-send instead. */

#define DBUS_API_SUBJECT_TO_CHANGE

#include <string.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-bindings.h>
#include <glib-object.h>

#include "dbus-gaim.h"
//#include "dbus-client-bindings.c"

int
main (int argc, char **argv)
{
	DBusGConnection *connection;
	DBusGProxy *driver;
	DBusGProxy *proxy;
	GError *error;
	guint32 result;
	int i;

    
	g_type_init ();
  
	/* Connecting to dbus */

	error = NULL;
	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

	if (connection == NULL) {
		g_assert (error != NULL);
		g_critical("Failed to open connection to dbuso %s.", 
			   error->message);
		g_error_free(error);
		return 1;
	} 

	g_message("Connected to dbus");

	/* Create a proxy object for the "bus driver" */
	
	driver = dbus_g_proxy_new_for_name(connection,
					   DBUS_SERVICE_DBUS,
					   DBUS_PATH_DBUS,
					   DBUS_INTERFACE_DBUS);


	/* Activate gaim */ 

	error = NULL;
	org_freedesktop_DBus_start_service_by_name(driver, DBUS_SERVICE_GAIM, 0, 
						   &result, &error);

	if (error) {
		g_critical("Failed to activate the gaim object: %s.", 
			   error->message);
		g_clear_error(&error);
		return 1;
	} 

	g_message("Gaim object activated");


	/* Create a proxy object for gaim */
	
	error = NULL;
	proxy = dbus_g_proxy_new_for_name_owner (connection,
						 DBUS_SERVICE_GAIM,
						 DBUS_PATH_GAIM,
						 DBUS_INTERFACE_GAIM,
						 &error);
  
	if (proxy == NULL) {
		g_assert(error);
		g_critical("Failed to create proxy for name owner: %s.\n", 
			   error->message);
		g_clear_error(&error);
		return 1;
	}

	g_message("Gaim proxy created");

	/* Processing commands */

	for(i=1; i<argc; i++) {
		gboolean success = TRUE;
		gchar *command = argv[i];
		
		error = NULL;
		g_print("Executing command: %s ...\n", command);
		
		if (!strcmp(command, "quit")) 
			success = org_gaim_GaimInterface_quit(proxy, &error);
		else if (!strcmp(command, "ping")) 
			success = org_gaim_GaimInterface_ping (proxy, &error);
		else if (!strcmp(command, "connect")) 
			success = org_gaim_GaimInterface_connect_all (proxy, &error);
		else
			g_print("Unknown command: %s.\n", command);
		
		if (!success) 
			g_print ("Command %s failed: %s.\n", command, 
				 error->message);

		g_clear_error(&error);
	}
	
	g_object_unref (G_OBJECT (proxy));
	g_object_unref (G_OBJECT (driver));
	g_print ("Successfully completed (%s).\n", argv[0]);
  
	return 0;
}
