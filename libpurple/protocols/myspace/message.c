/** MySpaceIM protocol messages
 *
 * \author Jeff Connelly
 *
 * Copyright (C) 2007, Jeff Connelly <jeff2@homing.pidgin.im>
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

#include "myspace.h"
#include "message.h"

static void msim_msg_free_element(gpointer data, gpointer user_data);
static void msim_msg_debug_string_element(gpointer data, gpointer user_data);
static gchar *msim_msg_pack_using(MsimMessage *msg, GFunc gf, gchar *sep, gchar *begin, gchar *end);
static gchar *msim_msg_element_as_string(MsimMessageElement *elem);

MsimMessage *msim_msg_new(void)
{
	/* Just an empty list. */
	return NULL;
}

/** Free an individual message element. */
static void msim_msg_free_element(gpointer data, gpointer user_data)
{
	MsimMessageElement *elem;

	elem = (MsimMessageElement *)data;

	switch (elem->type)
	{
		case MSIM_TYPE_BOOLEAN:			/* Fall through  */
		case MSIM_TYPE_INTEGER:
			/* Integer value stored in gpointer - no need to free(). */
			break;

		case MSIM_TYPE_STRING:
			/* Always free strings - caller should have g_strdup()'d if
			 * string was static or temporary and not to be freed. */
			g_free(elem->data);
			break;

		case MSIM_TYPE_BINARY:
			/* Free the GString itself and the binary data. */
			g_string_free((GString *)elem->data, TRUE);
			break;

		case MSIM_TYPE_DICTIONARY:
			/* TODO: free dictionary */
			break;
			
		case MSIM_TYPE_LIST:
			/* TODO: free list */
			break;

		default:
			purple_debug_info("msim", "msim_msg_free_element: not freeing unknown type %d\n",
					elem->type);
			break;
	}

	g_free(elem);
}

/** Free a complete message. */
void msim_msg_free(MsimMessage *msg)
{
	if (!msg)
	{
		/* already free as can be */
		return;
	}

	g_list_foreach(msg, msim_msg_free_element, NULL);
	g_list_free(msg);
}

/** Send an existing MsimMessage. */
gboolean msim_msg_send(MsimSession *session, MsimMessage *msg)
{
	gchar *raw;
	gboolean success;
	
	raw = msim_msg_pack(msg);
	success = msim_send_raw(session, raw);
	g_free(raw);
	
	return success;
}

/**
 *
 * Send a message to the server, whose contents is specified using 
 * variable arguments.
 *
 * @param session
 * @param ... A sequence of gchar* key/type/value triplets, terminated with NULL. 
 *
 * This function exists for coding convenience: it allows a message to be created
 * and sent in one line of code. Internally it calls msim_msg_send(). 
 *
 * IMPORTANT: See msim_msg_append() documentation for details on element types.
 *
 */
gboolean msim_send(MsimSession *session, ...)
{
	va_list argp;
	gchar *key, *value;
	MsimMessageType type;
	gboolean success;
	MsimMessage *msg;
	GString *gs;
    
	msg = msim_msg_new();

	/* Read key, type, value triplets until NULL. */
	va_start(argp, session);
	do
	{
		key = va_arg(argp, gchar *);
		if (!key)
		{
			break;
		}

		type = va_arg(argp, int);

		/* Interpret variadic arguments. */
		switch (type)
		{
			case MSIM_TYPE_INTEGER: 
				msg = msim_msg_append(msg, key, type, GUINT_TO_POINTER(va_arg(argp, int)));
				break;
				
			case MSIM_TYPE_STRING:
				value = va_arg(argp, char *);

				g_return_val_if_fail(value != NULL, FALSE);

				msg = msim_msg_append(msg, key, type, value);
				break;

			case MSIM_TYPE_BINARY:
				gs = va_arg(argp, GString *);

				g_return_val_if_fail(gs != NULL, FALSE);

				/* msim_msg_free() will free this GString the caller created. */
				msg = msim_msg_append(msg, key, type, gs);
				break;

			default:
				purple_debug_info("msim", "msim_send: unknown type %d\n", type);
				break;
		}
	} while(key);

	/* Actually send the message. */
	success = msim_msg_send(session, msg);

	/* Cleanup. */
	va_end(argp);	
	msim_msg_free(msg);

	return success;
}


/** Append a new element to a message. 
 *
 * @param name Textual name of element (static string, neither copied nor freed).
 * @param type An MSIM_TYPE_* code.
 * @param data Pointer to data, see below.
 *
 * @return The new message - must be assigned to as with GList*. For example:
 *
 * 		msg = msim_msg_append(msg, ...)
 *
 * The data parameter depends on the type given:
 *
 * * MSIM_TYPE_INTEGER: Use GUINT_TO_POINTER(x).
 *
 * * MSIM_TYPE_BINARY: Same as integer, non-zero is TRUE and zero is FALSE.
 *
 * * MSIM_TYPE_STRING: gchar *. The data WILL BE FREED - use g_strdup() if needed.
 *
 * * MSIM_TYPE_BINARY: g_string_new_len(data, length). The data AND GString will be freed.
 *
 * * MSIM_TYPE_DICTIONARY: TODO
 *
 * * MSIM_TYPE_LIST: TODO
 *
 * */
MsimMessage *msim_msg_append(MsimMessage *msg, gchar *name, MsimMessageType type, gpointer data)
{
	MsimMessageElement *elem;

	elem = g_new0(MsimMessageElement, 1);

	elem->name = name;
	elem->type = type;
	elem->data = data;

	return g_list_append(msg, elem);
}

/** Pack a string using the given GFunc and seperator.
 * Used by msim_msg_debug_string() and msim_msg_pack().
 */
static gchar *msim_msg_pack_using(MsimMessage *msg, GFunc gf, gchar *sep, gchar *begin, gchar *end)
{
	gchar **strings;
	gchar **strings_tmp;
	gchar *joined;
	gchar *final;
	int i;

	g_return_val_if_fail(msg != NULL, NULL);

	/* Add one for NULL terminator for g_strjoinv(). */
	strings = (gchar **)g_new0(gchar *, g_list_length(msg) + 1);

	strings_tmp = strings;
	g_list_foreach(msg, gf, &strings_tmp);

	joined = g_strjoinv(sep, strings);
	final = g_strconcat(begin, joined, end, NULL);
	g_free(joined);

	/* Clean up. */
	for (i = 0; i < g_list_length(msg); ++i)
	{
		g_free(strings[i]);
	}

	g_free(strings);

	return final;
}
/** Store a human-readable string describing the element.
 *
 * @param data Pointer to an MsimMessageElement.
 * @param user_data 
 */
static void msim_msg_debug_string_element(gpointer data, gpointer user_data)
{
	MsimMessageElement *elem;
	gchar *string;
	GString *gs;
	gchar *binary;
	gchar ***items;	 	/* wow, a pointer to a pointer to a pointer */

	elem = (MsimMessageElement *)data;
	items = user_data;

	switch (elem->type)
	{
		case MSIM_TYPE_INTEGER:
			string = g_strdup_printf("%s(integer): %d", elem->name, GPOINTER_TO_UINT(elem->data));
			break;

		case MSIM_TYPE_STRING:
			string = g_strdup_printf("%s(string): %s", elem->name, (gchar *)elem->data);
			break;

		case MSIM_TYPE_BINARY:
			gs = (GString *)elem->data;
			binary = purple_base64_encode((guchar*)gs->str, gs->len);
			string = g_strdup_printf("%s(binary, %d bytes): %s", elem->name, (int)gs->len, binary);
			g_free(binary);
			break;

		case MSIM_TYPE_BOOLEAN:
			string = g_strdup_printf("%s(boolean): %s", elem->name,
					GPOINTER_TO_UINT(elem->data) ? "TRUE" : "FALSE");
			break;

		case MSIM_TYPE_DICTIONARY:
			/* TODO: provide human-readable output of dictionary. */
			string = g_strdup_printf("%s(dict): TODO", elem->name);
			break;
			
		case MSIM_TYPE_LIST:
			/* TODO: provide human-readable output of list. */
			string = g_strdup_printf("%s(list): TODO", elem->name);
			break;

		default:
			string = g_strdup_printf("%s(unknown type %d)", elem->name, elem->type);
			break;
	}

	**items = string;
	++(*items);
}

/** Return a human-readable string of the message.
 *
 * @return A string. Caller must g_free().
 */
gchar *msim_msg_debug_string(MsimMessage *msg)
{
	if (!msg)
	{
		return g_strdup("<MsimMessage: empty>");
	}

	return msim_msg_pack_using(msg, msim_msg_debug_string_element, "\n", "<MsimMessage: \n", ">");
}

/** Return a message element data as a new string, converting from other types (integer, etc.) if necessary.
 *
 * @return gchar * The data as a string, or NULL. Caller must g_free().
 */
static gchar *msim_msg_element_as_string(MsimMessageElement *elem)
{
	switch (elem->type)
	{
		case MSIM_TYPE_INTEGER:
			return g_strdup_printf("%d", GPOINTER_TO_UINT(elem->data));

		case MSIM_TYPE_STRING:
			/* Strings get escaped. msim_escape() creates a new string. */
			return msim_escape((gchar *)elem->data);

		case MSIM_TYPE_BINARY:
			{
				GString *gs;

				gs = (GString *)elem->data;
				/* Do not escape! */
				return purple_base64_encode((guchar *)gs->str, gs->len);
			}

		case MSIM_TYPE_BOOLEAN:
			/* These strings are not actually used by the wire protocol
			 * -- see msim_msg_pack_element. */
			return g_strdup(GPOINTER_TO_UINT(elem->data) ? "True" : "False");

		case MSIM_TYPE_DICTIONARY:
			/* TODO: pack using k=v\034k2=v2\034... */
			return NULL;
			
		case MSIM_TYPE_LIST:
			/* TODO: pack using a|b|c|d|... */
			return NULL;

		default:
			purple_debug_info("msim", "field %s, unknown type %d\n", elem->name, elem->type);
			return NULL;
	}
}

/** Pack an element into its protocol representation. 
 *
 * @param data Pointer to an MsimMessageElement.
 * @param user_data Pointer to a gchar ** array of string items.
 *
 * Called by msim_msg_pack(). Will pack the MsimMessageElement into
 * a part of the protocol string and append it to the array. Caller
 * is responsible for creating array to correct dimensions, and
 * freeing each string element of the array added by this function.
 */
static void msim_msg_pack_element(gpointer data, gpointer user_data)
{
	MsimMessageElement *elem;
	gchar *string, *data_string;
	gchar ***items;

	elem = (MsimMessageElement *)data;
	items = user_data;

	data_string = msim_msg_element_as_string(elem);

	switch (elem->type)
	{
		/* These types are represented by key name/value pairs. */
		case MSIM_TYPE_INTEGER:
		case MSIM_TYPE_STRING:
		case MSIM_TYPE_BINARY:
		case MSIM_TYPE_DICTIONARY:
		case MSIM_TYPE_LIST:
			string = g_strconcat("%s", "\\", data_string, NULL);
			break;

		/* Boolean is represented by absence or presence of name. */
		case MSIM_TYPE_BOOLEAN:
			if (GPOINTER_TO_UINT(elem->data))
			{
				/* True - leave in, with blank value. */
				string = g_strdup_printf("%s\\\\", elem->name);
			} else {
				/* False - leave out. */
				string = g_strdup("");
			}
			break;

		default:
			g_free(data_string);
			g_return_if_fail(FALSE);
			break;
	}

	g_free(data_string);

	**items = string;
	++(*items);
}


/** Return a packed string suitable for sending over the wire.
 *
 * @return A string. Caller must g_free().
 */
gchar *msim_msg_pack(MsimMessage *msg)
{
	g_return_val_if_fail(msg != NULL, NULL);

	return msim_msg_pack_using(msg, msim_msg_pack_element, "\\", "\\", "\\final\\");
}

/** 
 * Parse a raw protocol message string into a MsimMessage *.
 *
 * @param raw The raw message string to parse, will be g_free()'d.
 *
 * @return MsimMessage *. Caller should msim_msg_free() when done.
 */
MsimMessage *msim_parse(gchar *raw)
{
	MsimMessage *msg;
    gchar *token;
    gchar **tokens;
    gchar *key;
    gchar *value;
    int i;

    g_return_val_if_fail(raw != NULL, NULL);

    purple_debug_info("msim", "msim_parse: got <%s>\n", raw);

    key = NULL;

    /* All messages begin with a \. */
    if (raw[0] != '\\' || raw[1] == 0)
    {
        purple_debug_info("msim", "msim_parse: incomplete/bad string, "
                "missing initial backslash: <%s>\n", raw);
        /* XXX: Should we try to recover, and read to first backslash? */

        g_free(raw);
        return NULL;
    }

    msg = msim_msg_new();

    for (tokens = g_strsplit(raw + 1, "\\", 0), i = 0; 
            (token = tokens[i]);
            i++)
    {
#ifdef MSIM_DEBUG_PARSE
        purple_debug_info("msim", "tok=<%s>, i%2=%d\n", token, i % 2);
#endif
        if (i % 2)
        {
			/* Odd-numbered ordinal is a value. */
		
			/* Note: returns a new string. */	
            value = msim_unescape(token);

			/* Always append strings, since protocol has no incoming
			 * type information for each field. */
			msg = msim_msg_append(msg, g_strdup(key), MSIM_TYPE_STRING, value);
#ifdef MSIM_DEBUG_PARSE
			purple_debug_info("msim", "insert string: |%s|=|%s|\n", key, value);
#endif
        } else {
			/* Even numbered indexes are key names. */
            key = token;
        }
    }
    g_strfreev(tokens);

    /* Can free now since all data was copied to hash key/values */
    g_free(raw);

    return msg;
}

/**
 * Parse a \x1c-separated "dictionary" of key=value pairs into a hash table.
 *
 * @param body_str The text of the dictionary to parse. Often the
 *                  value for the 'body' field.
 *
 * @return Hash table of the keys and values. Must g_hash_table_destroy() when done.
 */
GHashTable *msim_parse_body(const gchar *body_str)
{
    GHashTable *table;
    gchar *item;
    gchar **items;
    gchar **elements;
    guint i;

    g_return_val_if_fail(body_str != NULL, NULL);

    table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
 
    for (items = g_strsplit(body_str, "\x1c", 0), i = 0; 
        (item = items[i]);
        i++)
    {
        gchar *key, *value;

        elements = g_strsplit(item, "=", 2);

        key = elements[0];
        if (!key)
        {
            purple_debug_info("msim", "msim_parse_body(%s): null key\n", 
					body_str);
            g_strfreev(elements);
            break;
        }

        value = elements[1];
        if (!value)
        {
            purple_debug_info("msim", "msim_parse_body(%s): null value\n", 
					body_str);
            g_strfreev(elements);
            break;
        }

#ifdef MSIM_DEBUG_PARSE
        purple_debug_info("msim", "-- %s: %s\n", key, value);
#endif

        /* XXX: This overwrites duplicates. */
        /* TODO: make the GHashTable values be GList's, and append to the list if 
         * there is already a value of the same key name. This is important for
         * the WebChallenge message. */
        g_hash_table_insert(table, g_strdup(key), g_strdup(value));
        
        g_strfreev(elements);
    }

    g_strfreev(items);

    return table;
}

/** Return the first MsimMessageElement * with given name in the MsimMessage *. 
 *
 * @param name Name to search for.
 *
 * @return MsimMessageElement * matching name, or NULL.
 *
 * Note: useful fields of MsimMessageElement are 'data' and 'type', which
 * you can access directly. But it is often more convenient to use
 * another msim_msg_get_* that converts the data to what type you want.
 */
MsimMessageElement *msim_msg_get(MsimMessage *msg, gchar *name)
{
	GList *i;

	/* Linear search for the given name. O(n) but n is small. */
	for (i = g_list_first(msg); i != NULL; i = g_list_next(msg))
	{
		MsimMessageElement *elem;

		elem = i->data;
		g_return_val_if_fail(elem != NULL, NULL);

		if (strcmp(elem->name, name) == 0)
			return elem;
	}
	return NULL;
}

/** Return the data of an element of a given name, as a string.
 *
 * @param name Name of element.
 *
 * @return gchar * The message string. Caller must g_free().
 */
gchar *msim_msg_get_string(MsimMessage *msg, gchar *name)
{
	MsimMessageElement *elem;

	elem = msim_msg_get(msg, name);
	if (!elem)
		return NULL;

	return msim_msg_element_as_string(elem);
}

/** Return the data of an element of a given name, as an integer.
 *
 * @param name Name of element.
 *
 * @return guint Numeric representation of data, or 0 if could not be converted.
 *
 * Useful to obtain an element's data if you know it should be an integer,
 * even if it is not stored as an MSIM_TYPE_INTEGER. MSIM_TYPE_STRING will
 * be converted handled correctly, for example.
 */
guint msim_msg_get_integer(MsimMessage *msg, gchar *name)
{
	MsimMessageElement *elem;

	elem = msim_msg_get(msg, name);

	switch (elem->type)
	{
		case MSIM_TYPE_INTEGER:
			return GPOINTER_TO_UINT(elem->data);

		case MSIM_TYPE_STRING:
			return (guint)atoi((gchar *)elem->data);

		default:
			return 0;
	}
}
