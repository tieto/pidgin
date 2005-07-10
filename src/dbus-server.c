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
#include <glib.h>

#include "account.h"
#include "blist.h"
#include "conversation.h"
#include "dbus-gaim.h"
#include "dbus-server.h"
#include "debug.h"
#include "core.h"
#include "value.h"

static gint gaim_dbus_pointer_to_id(gpointer node);
static gpointer gaim_dbus_id_to_pointer(gint id, GaimDBusPointerType type);


/**************************************************************************/
/** @name Lots of GObject stuff I don't really understand                 */
/**************************************************************************/

GType gaim_object_get_type(void);

struct _GaimObject {
	GObject parent;

	int ping_signal_id;
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

static const char* null_to_empty(const char *s) {
	if (s)
		return s;
	else
		return "";
}



static void gaim_object_class_init(GaimObjectClass *klass) 
{ 
}



/**************************************************************************/
/** @name Signals                                                         */
/**************************************************************************/

/* used in #gaim_values_to_gvalues, undefined afterwards */
#define my_arg(type) (ptr != NULL ? * ((type *)ptr) : va_arg(data, type))

/**
   Converts from a list of data into an GValue array.

   @param gvalus      Array of empty gvalues to be filled.
   @param number      The number of data items.
   @param gaim_value  Array of #number pointers to GaimValues.  
                      The types of of these GaimValues determine the type
		      of data items.  The values do not matter.
   @param mainptr     A pointer to a single data item.  If this pointer is not #NULL,
                      then #number must be 1.
   @param data        A va_list containing data items.  If 		      

   Exactly one of #mainptr and #data must be not #NULL.  If #mainptr
   is not #NULL, then there is a single piece of data at the address
   pointed at by #mainptr.  If #data is not #NULL, then there are
   #number data items in the #va_list #data.
 */
static void gaim_values_to_gvalues(GValue *gvalue, int number,  
		       GaimValue **gaim_values, gpointer mainptr, va_list data) 
{
	int i;
	gpointer ptr;

	g_assert(mainptr == NULL || data == NULL);
	g_assert(mainptr != NULL || data != NULL);
	g_assert(number == 1 || data != NULL);

	for(i=0; i<number; i++, gvalue++) {
		ptr = mainptr;
		if (gaim_value_is_outgoing(gaim_values[i])) {
			ptr = my_arg(gpointer);
			g_assert(ptr);
		}

		switch(gaim_values[i]->type) {
		case  GAIM_TYPE_CHAR:  	
			g_value_init(gvalue, G_TYPE_CHAR);
			g_value_set_char(gvalue, (char) my_arg(int));
			break;
		case  GAIM_TYPE_INT:  
			g_value_init(gvalue, G_TYPE_INT);
			g_value_set_int(gvalue, my_arg(gint));
			break;
		case  GAIM_TYPE_UINT:  
			g_value_init(gvalue, G_TYPE_UINT);
			g_value_set_uint(gvalue, my_arg(guint));
			break;
		case  GAIM_TYPE_BOOLEAN:  
			g_value_init(gvalue, G_TYPE_BOOLEAN);
			g_value_set_boolean(gvalue, my_arg(gboolean));
			break;
		case GAIM_TYPE_STRING: 
			g_value_init(gvalue, G_TYPE_STRING);
			g_value_set_string(gvalue, null_to_empty(my_arg(char*)));
			break;
		case GAIM_TYPE_SUBTYPE: /* registered pointers only! */
			g_value_init(gvalue, G_TYPE_INT);
			g_value_set_int(gvalue, 
					gaim_dbus_pointer_to_id(my_arg(gpointer)));
			break;
		case GAIM_TYPE_POINTER:
 		case GAIM_TYPE_OBJECT: 
		case GAIM_TYPE_BOXED:
			my_arg(gpointer); /* cannot pass general pointers */
			g_value_init(gvalue, G_TYPE_INT);
			g_value_set_int(gvalue, 0);
			break;
		
		default:		/* no conversion implemented */
			g_assert_not_reached();
		}
	}

	if (data) 
		va_end(data);
}

#undef my_arg			/* my_arg was only used in gaim_values_to_gvalues  */



/**
   Converts from GaimTypes to GTypes.

   @param type   A GaimType to be converted.
   @result       The result of the conversion (GType).
*/
static GType gaim_type_to_g_type(GaimType type)
{
	switch(type) {
	case  GAIM_TYPE_CHAR: 
		return G_TYPE_CHAR;
	case  GAIM_TYPE_INT:  
		return G_TYPE_INT;
	case  GAIM_TYPE_UINT:  
		return G_TYPE_UINT;
	case  GAIM_TYPE_BOOLEAN:  
		return G_TYPE_BOOLEAN;
	case GAIM_TYPE_STRING: 
		return G_TYPE_STRING;
	case GAIM_TYPE_SUBTYPE:    /* registered pointers only! */
		return G_TYPE_INT;
	case GAIM_TYPE_POINTER:
	case GAIM_TYPE_BOXED:
 	case GAIM_TYPE_OBJECT: 
		return G_TYPE_INT; /* always 0 */
	default:		   /* no conversion implemented */
		g_assert_not_reached();
	}	
}


static const char *gaim_dbus_convert_signal_name(const char *gaim_name) 
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

/* Public signal-related functions */


void gaim_dbus_invalid_marshaller(GClosure *closure,
				  GValue *return_value,
				  guint n_param_values,
				  const GValue *param_values,
				  gpointer invocation_hint,
				  gpointer marshal_data)
{
	g_assert_not_reached();
}

int gaim_dbus_signal_register(GaimObject *object, const char *name, 
			      GSignalCMarshaller marshaller,
			      int num_values, ...) 
{
	va_list args;

	va_start(args, num_values);

	return g_signal_new_valist(name, G_OBJECT_TYPE(object),
				   G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
				   NULL, NULL, NULL, marshaller,
				   G_TYPE_NONE, num_values, args);
}

void gaim_dbus_signal_emit(GaimObject *object, int dbus_id, ...) {
	va_list args;
	
	va_start(args, dbus_id);
	
	gaim_dbus_signal_emit_valist(object, dbus_id, args);
}

void gaim_dbus_signal_emit_valist(GaimObject *object, int dbus_id, va_list args) {
	g_signal_emit_valist(object, dbus_id, 0, args);
}

int gaim_dbus_signal_register_gaim(GaimObject *object, const char *name, 
				   GSignalCMarshaller marshaller, 
				   int num_values, GaimValue **values)
{
	int i;
	int dbus_id;
	GType *types;
	
	types = g_new0(GType, num_values);

	for(i=0; i<num_values; i++) 
		types[i] = gaim_type_to_g_type(values[i]->type);
		
	dbus_id = 
		g_signal_newv(gaim_dbus_convert_signal_name(name),
			      G_OBJECT_TYPE(object),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
			      NULL, NULL, NULL, marshaller,
			      G_TYPE_NONE, num_values, types);

	g_free(types);

	return dbus_id;
}


void gaim_dbus_signal_emit_gaim(GaimObject *object, int dbus_id, int num_values, 
				GaimValue **values, va_list vargs)
{
	GValue *args;
	int i;

	g_return_if_fail(dbus_id);

	args = g_new0(GValue, num_values + 1);

	g_value_init(args + 0, G_OBJECT_TYPE(object));
	g_value_set_object(args + 0, object);

	gaim_values_to_gvalues(args + 1, num_values, values, NULL, vargs);
	
	g_signal_emitv(args, dbus_id, 0, NULL);

	for(i = 1; i <= num_values; i++)
		g_value_unset(args + i);
	
	g_free(args);		
}


/**************************************************************************/
/** @name Utility functions                                               */
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

typedef gboolean (*GaimNodeFilter)(GaimBlistNode *node, gpointer *user_data);

static gboolean 
filter_is_buddy(GaimBlistNode *node, gpointer *user_data) 
{
	return GAIM_BLIST_NODE_IS_BUDDY(node);
}

static gboolean 
filter_is_online_buddy(GaimBlistNode *node, 
		       gpointer *user_data) 
{
	return GAIM_BLIST_NODE_IS_BUDDY(node) && 
	  GAIM_BUDDY_IS_ONLINE((GaimBuddy *)node);
}


static GList* 
get_buddy_list (GList *created_list,		/**< can be NULL */
		GaimBlistNode *gaim_buddy_list, /**< can be NULL */
		GaimNodeFilter filter, 
		gpointer user_data) 
{
	GaimBlistNode *node;

	for(node = gaim_buddy_list; node; node = node->next) 
	{
		if ((*filter)(node, user_data)) 
			created_list = g_list_prepend(created_list, node);

		created_list = get_buddy_list(created_list, node->child,
					      filter, user_data);
	}
	
	return created_list;
}



/**************************************************************************/
/** @name Implementations of remote DBUS calls                            */
/**************************************************************************/

static gboolean 
gaim_object_ping(GaimObject *object, GError **error) 
{
 	gaim_dbus_signal_emit(object, object->ping_signal_id, "Ping Pong!"); 
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



static gboolean
gaim_object_get_buddy_list (GaimObject *obj, GArray **out_buddy_ids, 
			    GError **error) 
{
	GList *node;
	GList *buddy_list = get_buddy_list(NULL, gaim_get_blist()->root,
					   &filter_is_buddy, NULL);
	GArray *buddy_ids  = g_array_new(FALSE, TRUE, sizeof(gint32));;
  	
	buddy_list = g_list_reverse(buddy_list);

	for (node = buddy_list; node; node = node->next) {
		gint32 id = gaim_dbus_pointer_to_id(node->data);
		g_array_append_val(buddy_ids, id);
	}

	g_list_free(buddy_list);
	*out_buddy_ids = buddy_ids;

	return TRUE;
}





static gboolean
gaim_object_find_account(GaimObject *object, 
			 const char *account_name, const char *protocol_name,
			 gint *account_id, GError **error) 
{
	GaimAccount *account = gaim_accounts_find(account_name, protocol_name);
	
	error_unless_2(account, "Account '%s' with protocol '%s' not found", 
			    account_name, protocol_name);

	*account_id = gaim_dbus_pointer_to_id(account);
	return TRUE;
}

static gboolean
gaim_object_find_buddy(GaimObject *object, gint account_id, const char *buddy_name,
		       gint *buddy_id, GError **error)
{
	GaimAccount *account;
	GaimBuddy *buddy;

	account = gaim_dbus_id_to_pointer(account_id, DBUS_POINTER_ACCOUNT);
	error_unless_1(account, "Invalid account id: %i", account_id);	

	buddy = gaim_find_buddy(account, buddy_name);
	error_unless_1(account, "Buddy '%s' not found.", buddy_name);	

	*buddy_id = gaim_dbus_pointer_to_id(buddy);
	return TRUE;
}

static gboolean
gaim_object_start_im_conversation (GaimObject *object, gint buddy_id, GError **error)
{
	GaimBuddy *buddy = (GaimBuddy*) gaim_dbus_id_to_pointer(buddy_id, 
								DBUS_POINTER_BUDDY);
	
	error_unless_1(buddy, "Invalid buddy id: %i", buddy_id);

	gaim_conversation_new(GAIM_CONV_IM, buddy->account, buddy->name);

	return TRUE;
}



/**************************************************************************/
/** @name Gaim DBUS property handling functions                           */
/**************************************************************************/


typedef struct _GaimDBusProperty {
	char *name;
	gint offset;
	GaimType type;
} GaimDBusProperty;

/* change GAIM_TYPE into G_TYPE */
	  
static gboolean
gaim_dbus_get_property(GaimDBusProperty *list, int list_len, 
		       gpointer data, const char *name, 
		       GValue *value, GError **error)
{
	int i;

	for(i=0; i<list_len; i++) {
		if (!strcmp(list[i].name, name)) {
			gpointer ptr;
			GaimValue gaim_value, *gaim_value_ptr;

			ptr = G_STRUCT_MEMBER_P(data, list[i].offset);
			gaim_value.type = list[i].type;
			gaim_value.flags = 0;
			gaim_value_ptr = &gaim_value;
			
			gaim_values_to_gvalues(value, 1, &gaim_value_ptr, 
					       ptr, NULL);
			return TRUE;
		}
	}

	g_value_init(value, G_TYPE_INT);
	g_value_set_int(value, 0);
	error_unless_1(FALSE, "Invalid property '%s'", name);	
}


#define DECLARE_PROPERTY(maintype, name, type) {#name, G_STRUCT_OFFSET(maintype, name), type}

GaimDBusProperty buddy_properties [] = {
	DECLARE_PROPERTY(GaimBuddy, name, GAIM_TYPE_STRING),
	DECLARE_PROPERTY(GaimBuddy, alias, GAIM_TYPE_STRING),
	DECLARE_PROPERTY(GaimBuddy, server_alias, GAIM_TYPE_STRING),
	DECLARE_PROPERTY(GaimBuddy, account, GAIM_TYPE_SUBTYPE) 
};

GaimDBusProperty account_properties [] = {
	DECLARE_PROPERTY(GaimAccount, username, GAIM_TYPE_STRING),
	DECLARE_PROPERTY(GaimAccount, alias, GAIM_TYPE_STRING),
	DECLARE_PROPERTY(GaimAccount, user_info, GAIM_TYPE_STRING),
	DECLARE_PROPERTY(GaimAccount, protocol_id, GAIM_TYPE_STRING),
};

GaimDBusProperty contact_properties [] = {
	DECLARE_PROPERTY(GaimContact, alias, GAIM_TYPE_STRING),
	DECLARE_PROPERTY(GaimContact, totalsize, GAIM_TYPE_INT),
	DECLARE_PROPERTY(GaimContact, currentsize, GAIM_TYPE_INT),
	DECLARE_PROPERTY(GaimContact, online, GAIM_TYPE_INT),
	DECLARE_PROPERTY(GaimContact, priority, GAIM_TYPE_SUBTYPE),
	DECLARE_PROPERTY(GaimContact, priority_valid, GAIM_TYPE_BOOLEAN),
};

GaimDBusProperty group_properties [] = {
	DECLARE_PROPERTY(GaimGroup, name, GAIM_TYPE_STRING),
	DECLARE_PROPERTY(GaimGroup, totalsize, GAIM_TYPE_INT),
	DECLARE_PROPERTY(GaimGroup, currentsize, GAIM_TYPE_INT),
	DECLARE_PROPERTY(GaimGroup, online, GAIM_TYPE_INT),
};

GaimDBusProperty chat_properties [] = {
	DECLARE_PROPERTY(GaimChat, alias, GAIM_TYPE_STRING),
	DECLARE_PROPERTY(GaimChat, account, GAIM_TYPE_SUBTYPE),
};


#define DECLARE_PROPERTY_HANDLER(type, gaim_type)			\
	static gboolean							\
	gaim_object_get_##type##_property (GaimObject *object,		\
					   gint id, const char *property_name, \
					   GValue *value, GError **error) \
	{								\
		gpointer ptr = gaim_dbus_id_to_pointer(id, gaim_type); \
									\
		error_unless_1(ptr, "Invalid " #type " id: %i", id);	\
									\
		return gaim_dbus_get_property(type##_properties,	\
					      G_N_ELEMENTS(type##_properties), \
					      ptr, property_name, value, error); \
	}

DECLARE_PROPERTY_HANDLER(buddy, DBUS_POINTER_BUDDY)
DECLARE_PROPERTY_HANDLER(account, DBUS_POINTER_ACCOUNT)
DECLARE_PROPERTY_HANDLER(contact, DBUS_POINTER_CONTACT)
DECLARE_PROPERTY_HANDLER(group, DBUS_POINTER_GROUP)
DECLARE_PROPERTY_HANDLER(chat, DBUS_POINTER_CHAT)

#include "dbus-server-bindings.c"



/**************************************************************************/
/** @name Gaim DBUS pointer registration mechanism                        */
/**************************************************************************/

static GHashTable *map_id_node; 
static GHashTable *map_id_type; 
static GHashTable *map_node_id; 

void gaim_dbus_init_ids(void) {


	map_id_node = g_hash_table_new (g_direct_hash, g_direct_equal);
	map_id_type = g_hash_table_new (g_direct_hash, g_direct_equal);
	map_node_id = g_hash_table_new (g_direct_hash, g_direct_equal);
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
	g_assert(map_node_id);

	gint id = GPOINTER_TO_INT(g_hash_table_lookup(map_node_id, node));
	g_return_val_if_fail(id, 0);
	return id;
}
	
static gpointer gaim_dbus_id_to_pointer(gint id, GaimDBusPointerType type) {
	if (type != GPOINTER_TO_INT(g_hash_table_lookup(map_id_type, 
							GINT_TO_POINTER(id))))
		return NULL;
	return g_hash_table_lookup(map_id_node, GINT_TO_POINTER(id));
}
	


/**************************************************************************/
/** @name Gaim DBUS init function                                         */
/**************************************************************************/


gboolean gaim_dbus_connect(GaimObject *object) 
{
	DBusGConnection *connection;
	GError *error = NULL;
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
	



	dbus_g_object_type_install_info (GAIM_DBUS_TYPE_OBJECT,
					 &dbus_glib_gaim_object_object_info);

	dbus_g_connection_register_g_object (connection, DBUS_PATH_GAIM,
					     (GObject*) object);


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
	gaim_debug_misc ("dbus", "GLib test service entering main loop\n");

	return TRUE;
}


static void gaim_object_init(GaimObject *object) 
{

	object->ping_signal_id =
		gaim_dbus_signal_register(object, "PingSignal", 
					  g_cclosure_marshal_VOID__STRING,
					  1, G_TYPE_STRING);
}


gboolean gaim_dbus_init(void) 
{
	gaim_dbus_init_ids();
	gaim_object_error_quark =
		g_quark_from_static_string("org.gaim.GaimError");

	gaim_dbus_object = GAIM_DBUS_OBJECT(g_object_new (GAIM_DBUS_TYPE_OBJECT, NULL));

	return TRUE;
}
