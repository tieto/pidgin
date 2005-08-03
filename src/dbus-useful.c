#include <string.h>
#include <glib.h>

#include "conversation.h"
#include "util.h"



GaimAccount *
gaim_accounts_find_ext(const char *name, const char *protocol_id, 
		       gboolean (*account_test)(const GaimAccount *account))
{
    GaimAccount *result = NULL;
    GList *l;
    char *who;
    
    if (name)
	who = g_strdup(gaim_normalize(NULL, name));
    else 
	who = NULL;
    
    for (l = gaim_accounts_get_all(); l != NULL; l = l->next) {
	GaimAccount *account = (GaimAccount *)l->data;
	
	if (who && strcmp(gaim_normalize(NULL, gaim_account_get_username(account)), who))
	    continue;

	if (protocol_id && strcmp(account->protocol_id, protocol_id))
	    continue;

	if (account_test && !account_test(account)) 
	    continue;

	result = account;
	break;
    }

    g_free(who);

    return result;
}

GaimAccount *gaim_accounts_find_any(const char *name, const char *protocol) 
{
    return gaim_accounts_find_ext(name, protocol, NULL);
}

GaimAccount *gaim_accounts_find_connected(const char *name, const char *protocol) 
{
    return gaim_accounts_find_ext(name, protocol, gaim_account_is_connected);
}


/* DBusMessage *gaim_account_set_status_DBUS(DBusMessage *message_DBUS, DBusError *error_DBUS)  */
/* { */
/*     DBusMessage *reply; */
/*     DBusMessageIter iter; */

/*     dbus_int32_t account, active;     */
/*     char *status_id; */
    
/*     dbus_message_iter_init(message, &iter); */
/* const char *name; */
/* const char *protocol; */

/* dbus_message_get_args(message_DBUS, error_DBUS,  DBUS_TYPE_STRING, &name, DBUS_TYPE_STRING, &protocol, DBUS_TYPE_INVALID); */
/* CHECK_ERROR(error_DBUS); */
/* NULLIFY(name); */
/* NULLIFY(protocol); */
/* GAIM_DBUS_POINTER_TO_ID(RESULT, gaim_accounts_find_any(name, protocol), error_DBUS); */
/* reply_DBUS =  dbus_message_new_method_return (message_DBUS); */
/* dbus_message_append_args(reply_DBUS,  DBUS_TYPE_INT32, &RESULT,  DBUS_TYPE_INVALID); */
/* return reply_DBUS; */

/* } */
