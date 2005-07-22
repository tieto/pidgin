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

#define DBUS_API_SUBJECT_TO_CHANGE

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib-bindings.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <glib/gquark.h>
#include <glib.h>

#include "account.h"
#include "blist.h"
#include "conversation.h"
#include "dbus-gaim.h"
#include "dbus-server.h"
#include "dbus-useful.h"
#include "debug.h"
#include "core.h"
#include "value.h"



/**************************************************************************/
/** @name Lots of GObject stuff I don't really understand                 */
/**************************************************************************/

GType gaim_object_get_type(void);

struct _GaimObject {
    GObject parent;
    DBusGProxy *proxy;
};

typedef struct {
	GObjectClass parent;
} GaimObjectClass;


#define GAIM_DBUS_TYPE_OBJECT              (gaim_object_get_type ())
#define GAIM_DBUS_OBJECT(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GAIM_DBUS_TYPE_OBJECT, GaimObject))
#define GAIM_DBUS_OBJECT_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GAIM_DBUS_TYPE_OBJECT, GaimObjectClass))
#define GAIM_DBUS_IS_OBJECT(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GAIM_DBUS_TYPE_OBJECT))
#define GAIM_DBUS_IS_OBJECT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GAIM_DBUS_TYPE_OBJECT))
#define GAIM_DBUS_OBJECT_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GAIM_DBUS_TYPE_OBJECT, GaimObjectClass))

G_DEFINE_TYPE(GaimObject, gaim_object, G_TYPE_OBJECT)

GaimObject *gaim_dbus_object;
static GQuark gaim_object_error_quark;

#define NULLIFY(id) id = empty_to_null(id)

static const char* empty_to_null(const char *str) {
    if (str == NULL || str[0] == 0)
	return NULL;
    else
	return str;
}

static const char* null_to_empty(const char *s) {
	if (s)
		return s;
	else
		return "";
}



static void gaim_object_class_init(GaimObjectClass *klass) 
{ 
}

static void gaim_object_init(GaimObject *object) 
{
}

/**************************************************************************/
/** @name Gaim DBUS pointer registration mechanism                        */
/**************************************************************************/

GaimDBusPointerType dbus_type_parent[DBUS_POINTER_LASTTYPE];

static GHashTable *map_id_node; 
static GHashTable *map_id_type; 
static GHashTable *map_node_id; 


#define DECLARE_TYPE(type, parent) \
	dbus_type_parent[DBUS_POINTER_##type] = DBUS_POINTER_##parent

void gaim_dbus_init_ids(void) {
	int i;

	map_id_node = g_hash_table_new (g_direct_hash, g_direct_equal);
	map_id_type = g_hash_table_new (g_direct_hash, g_direct_equal);
	map_node_id = g_hash_table_new (g_direct_hash, g_direct_equal);

	for (i = 0; i < DBUS_POINTER_LASTTYPE; i++)
		dbus_type_parent[i] = DBUS_POINTER_NONE;

	/* some manual corrections */
	DECLARE_TYPE(GaimBuddy, GaimBlistNode);
	DECLARE_TYPE(GaimContact, GaimBlistNode);
	DECLARE_TYPE(GaimChat, GaimBlistNode);
	DECLARE_TYPE(GaimGroup, GaimBlistNode);
}

void gaim_dbus_register_pointer(gpointer node, GaimDBusPointerType type) 
{
	static gint last_id = 0;

	g_assert(map_node_id);
	g_assert(g_hash_table_lookup(map_node_id, node) == NULL);
	

	last_id++;
	g_hash_table_insert(map_node_id, node, GINT_TO_POINTER(last_id));
	g_hash_table_insert(map_id_node, GINT_TO_POINTER(last_id), node);
	g_hash_table_insert(map_id_type, GINT_TO_POINTER(last_id), 
			    GINT_TO_POINTER(type));
}


void gaim_dbus_unregister_pointer(gpointer node) {
	gpointer id = g_hash_table_lookup(map_node_id, node);
	
	g_hash_table_remove(map_node_id, node);
	g_hash_table_remove(map_id_node, GINT_TO_POINTER(id));
	g_hash_table_remove(map_id_node, GINT_TO_POINTER(id));
}

static gint gaim_dbus_pointer_to_id(gpointer node) {
	gint id = GPOINTER_TO_INT(g_hash_table_lookup(map_node_id, node));
	g_return_val_if_fail(id, 0);
	return id;
}
	
static gpointer gaim_dbus_id_to_pointer(gint id, GaimDBusPointerType type) {
	GaimDBusPointerType objtype = 
		GPOINTER_TO_INT(g_hash_table_lookup(map_id_type, 
						    GINT_TO_POINTER(id)));
							    
	while (objtype != type && objtype != DBUS_POINTER_NONE)
		objtype = dbus_type_parent[objtype];

	if (objtype == type)
		return g_hash_table_lookup(map_id_node, GINT_TO_POINTER(id));
	else
		return NULL;
}
	
/**************************************************************************/
/** @name Useful functions                                                */
/**************************************************************************/


#define error_unless_1(condition, str, parameter)		\
	if (!(condition)) {					\
		    g_set_error(error, gaim_object_error_quark,	\
				DBUS_ERROR_NOT_FOUND,		\
				str, parameter);		\
		    return FALSE;				\
	    }

#define error_unless_2(condition, str, a,b)			\
	if (!(condition)) {					\
		    g_set_error(error, gaim_object_error_quark,	\
				DBUS_ERROR_NOT_FOUND,		\
				str, a,b);			\
		    return FALSE;				\
	    }

#define GAIM_DBUS_ID_TO_POINTER(ptr, id, type)				\
	G_STMT_START {							\
		ptr = ((type*) gaim_dbus_id_to_pointer			\
		       (id, DBUS_POINTER_##type));			\
		error_unless_2(ptr != NULL || id == 0,			\
			       "%s object with ID = %i not found",	\
			       #type, id);				\
	} G_STMT_END
	       

#define GAIM_DBUS_POINTER_TO_ID(id, ptr)				\
	G_STMT_START {							\
		gpointer _ptr = ptr;					\
		id = gaim_dbus_pointer_to_id(_ptr);			\
		error_unless_1(ptr == NULL || id != 0,			\
			       "Result object not registered (%i)",	\
			       id);					\
	} G_STMT_END


/**************************************************************************/
/** @name Signals                                                         */
/**************************************************************************/



static  char *gaim_dbus_convert_signal_name(const char *gaim_name) 
{
	int gaim_index, g_index;
	char *g_name = g_new(char, strlen(gaim_name)+1);
	gboolean capitalize_next = TRUE;

	for(gaim_index = g_index = 0; gaim_name[gaim_index]; gaim_index++)
		if (gaim_name[gaim_index] != '-' && gaim_name[gaim_index] != '_') {
			if (capitalize_next)
				g_name[g_index++] = g_ascii_toupper(gaim_name[gaim_index]);
			else
				g_name[g_index++] = gaim_name[gaim_index];
			capitalize_next = FALSE;
		} else
			capitalize_next = TRUE;
	g_name[g_index] = 0;
	
	return g_name;
}

#define my_arg(type) (ptr != NULL ? * ((type *)ptr) : va_arg(data, type))

static void gaim_dbus_message_append_gaim_values(DBusMessageIter *iter,
						 int number,
						 GaimValue **gaim_values, 
						 va_list data) 
{
    int i;

    for(i=0; i<number; i++) {
	const char *str;
	int id;
	gint xint;
	guint xuint;
	gboolean xboolean;
	gpointer ptr = NULL;
	if (gaim_value_is_outgoing(gaim_values[i])) {
	    ptr = my_arg(gpointer);
	    g_assert(ptr);
	}
	
	switch(gaim_values[i]->type) {
	case  GAIM_TYPE_INT:  
	    g_print("appending int\n");
	    xint = my_arg(gint);
	    dbus_message_iter_append_basic(iter, DBUS_TYPE_INT32, &xint);
	    break;
	case  GAIM_TYPE_UINT:  
	    xuint = my_arg(guint);
	    g_print("appending uint\n");
	    dbus_message_iter_append_basic(iter, DBUS_TYPE_UINT32, &xuint);
	    break;
	case  GAIM_TYPE_BOOLEAN:  
	    g_print("appending boolean\n");
	    xboolean = my_arg(gboolean);
	    dbus_message_iter_append_basic(iter, DBUS_TYPE_BOOLEAN, &xboolean);
	    break;
	case GAIM_TYPE_STRING: 
	    g_print("appending string\n");
	    str = null_to_empty(my_arg(char*));
	    dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING, &str);
	    break;
	case GAIM_TYPE_SUBTYPE: /* registered pointers only! */
	case GAIM_TYPE_POINTER:
	case GAIM_TYPE_OBJECT: 
	case GAIM_TYPE_BOXED:		
	    g_print("appending obj\n");
	    id = gaim_dbus_pointer_to_id(my_arg(gpointer));
	    dbus_message_iter_append_basic(iter, DBUS_TYPE_INT32, &id);
	    break;
	default:		/* no conversion implemented */
	    g_assert_not_reached();
	}
    }
}

#undef my_arg

void gaim_dbus_signal_emit_gaim(GaimObject *object, char *name, int num_values, 
				GaimValue **values, va_list vargs)
{
	/* pass name */
    DBusMessage *signal;
    DBusMessageIter iter;
    char *newname;

    g_print("Emitting %s\n", name);    
    g_return_if_fail(object->proxy);
    
    newname =  gaim_dbus_convert_signal_name(name);
    signal = dbus_message_new_signal(DBUS_PATH_GAIM, DBUS_INTERFACE_GAIM, newname);
    dbus_message_iter_init_append(signal, &iter);

    gaim_dbus_message_append_gaim_values(&iter, num_values, values, vargs);

    dbus_g_proxy_send(object->proxy, signal, NULL);

    g_free(newname);
    dbus_message_unref(signal);
}


/**************************************************************/
/* DBus bindings ...                                          */
/**************************************************************/



GArray* gaim_dbusify_GList(GList *list, gboolean free_memory) {
	GArray *array;
	GList *elem;

	array = g_array_new (FALSE, TRUE, sizeof (guint32));
	for(elem = list; elem != NULL; elem = elem->next) {
		int objectid;

		objectid = gaim_dbus_pointer_to_id(elem->data);
		g_array_append_val(array, objectid);
	}

	if (free_memory)
		g_list_free(list);

	return array;
}

GArray* gaim_dbusify_GSList(GSList *list, gboolean free_memory) {
	GArray *array;
	GSList *elem;

	array = g_array_new (FALSE, TRUE, sizeof (guint32));
	for(elem = list; elem != NULL; elem = elem->next) {
		int objectid;

		objectid = gaim_dbus_pointer_to_id(elem->data);
		g_array_append_val(array, objectid);
	}

	if (free_memory)
		g_slist_free(list);
	return array;
}

#include "dbus-generated-code.c"

#include "dbus-server-bindings.c"





/**************************************************************************/
/** @name Gaim DBUS init function                                         */
/**************************************************************************/


/* fixme: why do we need two functions here instead of one?  */

gboolean gaim_dbus_connect(GaimObject *object) 
{
	DBusGConnection *connection;
	GError *error = NULL;
	DBusGProxy *driver_proxy;
	guint32 request_name_ret;

	gaim_debug_misc("dbus", "launching dbus server\n");
					
	dbus_g_object_type_install_info (GAIM_DBUS_TYPE_OBJECT,
					 &dbus_glib_gaim_object_object_info);	/* Connect to the bus */

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
	



	/* Obtain a proxy for the DBus object  */

	driver_proxy = dbus_g_proxy_new_for_name (connection,
						  DBUS_SERVICE_DBUS,
						  DBUS_PATH_DBUS,
						  DBUS_INTERFACE_DBUS);

	g_assert(driver_proxy);

	/* Test whether the registration was successful */

	org_freedesktop_DBus_request_name(driver_proxy, DBUS_SERVICE_GAIM,
					  DBUS_NAME_FLAG_PROHIBIT_REPLACEMENT, &request_name_ret, &error);

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



	dbus_g_connection_register_g_object (connection, DBUS_PATH_GAIM,
					     (GObject*) object);

	object->proxy = dbus_g_proxy_new_for_name (connection,
						  DBUS_SERVICE_GAIM,
						  DBUS_PATH_GAIM,
						  DBUS_INTERFACE_GAIM);

	gaim_debug_misc ("dbus", "GLib test service entering main loop\n");

	return TRUE;
}




gboolean gaim_dbus_init(void) 
{
    gaim_dbus_init_ids();
    gaim_object_error_quark =
	g_quark_from_static_string("org.gaim.GaimError");



	gaim_dbus_object = GAIM_DBUS_OBJECT(g_object_new (GAIM_DBUS_TYPE_OBJECT, NULL));

	gaim_dbus_object->proxy = NULL;
	return TRUE;
}


