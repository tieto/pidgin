/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gaim
 *
 * Some code copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
 * libfaim code copyright 1998, 1999 Adam Fritzler <afritz@auk.cx>
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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
/*this must happen before sys/socket.h or freebsd won't compile*/

#ifndef _WIN32
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <unistd.h>
#else
#include "utsname.h"
#endif

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include "multi.h"
#include "prpl.h"
#include "gaim.h"
#ifdef MAX
#undef MAX
#endif
#ifdef MIN
#undef MIN
#endif
#include "jabber.h"
#include "proxy.h"

#ifdef _WIN32
#include "win32dep.h"
#endif

#include "pixmaps/protocols/jabber/available.xpm"
#include "pixmaps/protocols/jabber/available-away.xpm"
#include "pixmaps/protocols/jabber/available-chat.xpm"
#include "pixmaps/protocols/jabber/available-xa.xpm"
#include "pixmaps/protocols/jabber/available-dnd.xpm"
#include "pixmaps/protocols/jabber/available-error.xpm"

/* for win32 compatability */
G_MODULE_IMPORT GSList *connections;

/* The priv member of gjconn's is a gaim_connection for now. */
#define GJ_GC(x) ((struct gaim_connection *)(x)->priv)

#define IQID_AUTH "__AUTH__"

#define IQ_NONE -1
#define IQ_AUTH 0
#define IQ_ROSTER 1

#define UC_AWAY   (0x02 | UC_UNAVAILABLE)
#define UC_CHAT    0x04
#define UC_XA     (0x08 | UC_UNAVAILABLE)
#define UC_DND    (0x10 | UC_UNAVAILABLE)
#define UC_ERROR  (0x20 | UC_UNAVAILABLE)

#define DEFAULT_SERVER "jabber.org"
#define DEFAULT_GROUPCHAT "conference.jabber.org"
#define DEFAULT_PORT 5222

#define USEROPT_PORT 0

#define JABBER_TYPING_NOTIFY_INT 15	/* Delay (in seconds) between sending typing notifications */

/*
 * Note: "was_connected" may seem redundant, but it was needed and I
 * didn't want to touch the Jabber state stuff not specific to Gaim.
 */
typedef struct gjconn_struct {
	/* Core structure */
	pool p;			/* Memory allocation pool */
	int state;		/* Connection state flag */
	int was_connected;	/* We were once connected */
	int fd;			/* Connection file descriptor */
	jid user;		/* User info */
	char *pass;		/* User passwd */

	/* Stream stuff */
	int id;			/* id counter for jab_getid() function */
	char idbuf[9];		/* temporary storage for jab_getid() */
	char *sid;		/* stream id from server, for digest auth */
	XML_Parser parser;	/* Parser instance */
	xmlnode current;	/* Current node in parsing instance.. */

	/* Event callback ptrs */
	void (*on_state)(struct gjconn_struct *gjc, int state);
	void (*on_packet)(struct gjconn_struct *gjc, jpacket p);

	GHashTable *queries;	/* query tracker */

	void *priv;

} *gjconn, gjconn_struct;

typedef void (*gjconn_state_h)(gjconn gjc, int state);
typedef void (*gjconn_packet_h)(gjconn gjc, jpacket p);

static gjconn gjab_new(char *user, char *pass, void *priv);
static void gjab_delete(gjconn gjc);
static void gjab_state_handler(gjconn gjc, gjconn_state_h h);
static void gjab_packet_handler(gjconn gjc, gjconn_packet_h h);
static void gjab_start(gjconn gjc);
static void gjab_stop(gjconn gjc);
/*
static int gjab_getfd(gjconn gjc);
static jid gjab_getjid(gjconn gjc);
static char *gjab_getsid(gjconn gjc);
*/
static char *gjab_getid(gjconn gjc);
static void gjab_send(gjconn gjc, xmlnode x);
static void gjab_send_raw(gjconn gjc, const char *str);
static void gjab_recv(gjconn gjc);
static void gjab_auth(gjconn gjc);

/*
 * It is *this* to which we point the gaim_connection proto_data
 */
struct jabber_data {
	gjconn gjc;
	gboolean did_import;
	GSList *chats;
	time_t idle;
	gboolean die;
	GHashTable *buddies;
	GSList *file_transfers;
};

/*
 * Used in jabber_buddy_data.invisible, below
 */
#define JABBER_NOT_INVIS  0x00
#define JABBER_SERV_INVIS 0x01	/* Invisible set on server */
#define JABBER_BUD_INVIS  0x02	/* Invisible set on buddy */

/*
 * It is *this* to which we point the buddy proto_data
 */
struct jabber_buddy_data {
	GSList *resources;
	char *error_msg;
	unsigned invisible;	/* We've set presence type invisible for this buddy */
};

/*
 * per-resource info
 */
typedef struct jabber_resource_info {
	char *name;
	int priority;
	int state;
	char *away_msg;
	char *thread_id;
	gboolean has_composing;
} *jab_res_info;

/*
 * For our own jid handling
 *
 * We do our own so we can cleanly parse buddy names
 * (user@server/resource) and rid ourselves of the
 * struct when we're done with it.  The Jabber lib
 * structs last the life of the pool--we frequently
 * don't want that.
 *
 * We use the real jid structs so we can make use of
 * jid_safe(), jid_cmp() and some others.
 *
 *    BE CAREFUL using the Jabber lib routines.
 *    Many of them assume pool use and are not
 *    amenable to use with our own!
 *
 * We give them special names so we know, throughout
 * the code, that they're not alloc'd out of pool
 * memory and we can, and must, dispose of them when
 * we're done with 'em.
 */
#define gaim_jid_struct jid_struct
typedef struct gaim_jid_struct *gaim_jid;

/*
 * Jabber "chat group" info.  Pointers to these go in jabber_data
 * pending and existing chats lists.
 */
struct jabber_chat {
	gaim_jid gjid;
	struct gaim_connection *gc;
	struct conversation *b;
	int id;
	int state;
};

/*
 * Jabber chat states...
 *
 * Note: due to a bug in one version of the Jabber server, subscriptions
 * to chat groups aren't (always?) properly removed at the server.  The
 * result is clients receive Jabber "presence" notifications for JIDs
 * they no longer care about.  The problem with such vestigial notifies is
 * that we really have no way of telling if it's vestigial or if it's a
 * valid "buddy" presence notification.  So we keep jabber_chat structs
 * around after leaving a chat group and simply mark them "closed."  That
 * way we can test for such errant presence notifications.  I.e.: if we
 * get a presence notfication from a JID that matches a chat group JID,
 * we disregard it.
 */
#define JCS_PENDING 1	/* pending */
#define JCS_ACTIVE  2	/* active */
#define JCS_CLOSED  3	/* closed */


#define STATE_EVT(arg) if(gjc->on_state) { (gjc->on_state)(gjc, (arg) ); }

static void jabber_handlevcard(gjconn, xmlnode, char *);

static char *jabber_normalize(const char *s);

static char *create_valid_jid(const char *given, char *server, char *resource)
{
	char *valid;

	if (!strchr(given, '@'))
		valid = g_strdup_printf("%s@%s/%s", given, server, resource);
	else if (!strchr(strchr(given, '@'), '/'))
		valid = g_strdup_printf("%s/%s", given, resource);
	else
		valid = g_strdup(given);

	return valid;
}

/*
 * Dispose of a gaim_jid_struct
 */
static void gaim_jid_free(gaim_jid gjid)
{
	if(gjid) {
		if(gjid->resource)
			free(gjid->resource);
		if(gjid->user)
			free(gjid->user);
		if(gjid->server)
			free(gjid->server);
		if(gjid->full)
			free(gjid->full);
		free(gjid);
	}
}

/*
 * Create a new gjid struct
 *
 * Unlike jid_new(), also creates "full."
 *
 * Shamelessly copied, in part, from jid.c: jid_new()
 *
 * Caller is responsible for freeing the space allocated by this via
 * gaim_jid_free().
 *
 * JFIXME: Has a local declaration for jid.c:jid_safe().  I've put in a
 *         request to have that added to libjabber's lib.h file. (JSeymour)
 */
static gaim_jid gaim_jid_new(char *name)
{
	extern jid jid_safe(jid);	/* *retch* */

	gaim_jid gjid = NULL;

	if(name && strlen(name)) {
		char *server, *resource, *type, *str;
		int full_len = 0;

		/* user@server/resource */

		str = strdup(name);	/* we mangle a copy */

		gjid = calloc(1, sizeof(struct gaim_jid_struct));

		if((resource = strstr(str, "/")) != NULL) {
			*resource = '\0';
			++resource;
			if((full_len = strlen(resource)) > 0) {
				gjid->resource = strdup(resource);
				++full_len;	/* for later "/" addition */
			}
		} else {
			resource = str + strlen(str); /* point to end */
		}

		type = strstr(str, ":");
		if(type != NULL && type < resource) {
			*type = '\0';
			++type;
			str = type; /* ignore the type: prefix */
		}

		server = strstr(str, "@");

		/*
		 * if there's no @, it's just the server address
		 */
		if(server == NULL || server > resource) {
			gjid->server = strdup(str);
			full_len += strlen(str);
		} else {
			*server = '\0';
			++server;
			gjid->server = strdup(server);
			full_len += strlen(server) + 1;	/* account for later "@" */
			if(strlen(str) > 0) {
				gjid->user = strdup(str);
				full_len += strlen(str);
			}
		}

		free(str);

		if(!jid_safe(gjid)) {
			gaim_jid_free(gjid);
			gjid = NULL;
		} else {
			if(full_len) {
				char *s = gjid->full = malloc(++full_len);

				if(gjid->user) {
					strcpy(s, gjid->user);
					s += strlen(gjid->user);
				}
				if(gjid->server) {
					if(s > gjid->full)
						*(s++) = '@';
					strcpy(s, gjid->server);
					s += strlen(gjid->server);
				}
				if(gjid->resource) {
					*(s++) = '/';
					strcpy(s, gjid->resource);
				}
			}
		}
	}

	return gjid;
}

/*
 * Get a "username@server" from unadorned "username"
 *
 * If there's no "@server" part and "who" doesn't match the
 * gjconn server (which would indicate that "who" *is* the
 * server in case of server messages), the gjconn server is
 * appended.
 *
 * If incl_resource is TRUE (non-0), the returned string
 * includes the "/resource" part (if it exists), otherwise not.
 *
 * Allocates space for returned string.  Caller is
 * responsible for freeing it with g_free().
 *
 * If "gjid" is non-null, sets that as well.  Caller is
 * reponsible for freeing that via gaim_jid_free() when done
 * with it.
 */
static gchar *get_realwho(gjconn gjc, const char *who, int incl_resource, gaim_jid *gjid)
{
	gaim_jid my_gjid;
	gchar *my_who;
	gchar *realwho = NULL;

	if(!(who && who[0])) {
	    return NULL;
	}

	/*
	 * Bare username and "username" not the server itself?
	 */
	if(!strchr(who, '@') && strcasecmp(who, gjc->user->server)) {
		my_who = g_strdup_printf("%s@%s", who, gjc->user->server);
	} else {
		my_who = g_strdup(who);
	}

	if((my_gjid = gaim_jid_new(my_who)) != NULL) {
		/*
		 * If there's no "user" part, "who" was just the server or perhaps a transport (?)
		 */
		if(my_gjid->user) {
			/*
			 * Include "/resource" bit?
			 */
			if(incl_resource) {
				realwho = g_strdup(my_gjid->full);
			} else {
				realwho = g_strdup_printf("%s@%s", my_gjid->user, my_gjid->server);
			}
		} else {
			realwho = g_strdup(my_gjid->server);
		}
	}

	g_free(my_who);

	if(gjid) {
		*gjid = my_gjid;
	} else {
		gaim_jid_free(my_gjid);
	}

	return realwho;
}

static gjconn gjab_new(char *user, char *pass, void *priv)
{
	pool p;
	gjconn gjc;

	if (!user)
		return (NULL);

	p = pool_new();
	if (!p)
		return (NULL);
	gjc = pmalloc_x(p, sizeof(gjconn_struct), 0);
	if (!gjc) {
		pool_free(p);	/* no need for this anymore! */
		return (NULL);
	}
	gjc->p = p;

	if((gjc->user = jid_new(p, user)) == NULL) {
		pool_free(p);	/* no need for this anymore! */
		return (NULL);
	}

	gjc->pass = strdup(pass);

	gjc->state = JCONN_STATE_OFF;
	gjc->was_connected = 0;
	gjc->id = 1;
	gjc->fd = -1;

	gjc->priv = priv;

	return gjc;
}

static void gjab_delete(gjconn gjc)
{
	if (!gjc)
		return;

	gjab_stop(gjc);
	free(gjc->pass);
	pool_free(gjc->p);
}

static void gjab_state_handler(gjconn gjc, gjconn_state_h h)
{
	if (!gjc)
		return;

	gjc->on_state = h;
}

static void gjab_packet_handler(gjconn gjc, gjconn_packet_h h)
{
	if (!gjc)
		return;

	gjc->on_packet = h;
}

static void gjab_stop(gjconn gjc)
{
	if (!gjc || gjc->state == JCONN_STATE_OFF)
		return;

	gjab_send_raw(gjc, "</stream:stream>");
	gjc->state = JCONN_STATE_OFF;
	gjc->was_connected = 0;
	close(gjc->fd);
	gjc->fd = -1;
	XML_ParserFree(gjc->parser);
	gjc->parser = NULL;
}

/*
static int gjab_getfd(gjconn gjc)
{
	if (gjc)
		return gjc->fd;
	else
		return -1;
}

static jid gjab_getjid(gjconn gjc)
{
	if (gjc)
		return (gjc->user);
	else
		return NULL;
}

static char *gjab_getsid(gjconn gjc)
{
	if (gjc)
		return (gjc->sid);
	else
		return NULL;
}
*/

static char *gjab_getid(gjconn gjc)
{
	snprintf(gjc->idbuf, 8, "%d", gjc->id++);
	return &gjc->idbuf[0];
}

static void gjab_send(gjconn gjc, xmlnode x)
{
	if (gjc && gjc->state != JCONN_STATE_OFF) {
		char *buf = xmlnode2str(x);
		if (buf)
#ifndef _WIN32
			write(gjc->fd, buf, strlen(buf));
#else
			send(gjc->fd, buf, strlen(buf), 0);
#endif
		debug_printf("gjab_send: %s\n", buf);
	}
}

static void gjab_send_raw(gjconn gjc, const char *str)
{
	if (gjc && gjc->state != JCONN_STATE_OFF) {
		/*
		 * JFIXME: No error detection?!?!
		 */
#ifndef _WIN32
		if(write(gjc->fd, str, strlen(str)) < 0) {
#else
		if(send(gjc->fd, str, strlen(str), 0) < 0) {
#endif
			fprintf(stderr, "DBG: Problem sending.  Error: %d\n", errno);
			fflush(stderr);
		}
		debug_printf("gjab_send_raw: %s\n", str);
	}
}

static void gjab_reqroster(gjconn gjc)
{
	xmlnode x;

	x = jutil_iqnew(JPACKET__GET, NS_ROSTER);
	xmlnode_put_attrib(x, "id", gjab_getid(gjc));

	gjab_send(gjc, x);
	xmlnode_free(x);
}

static void gjab_reqauth(gjconn gjc)
{
	xmlnode x, y, z;
	char *user;

	if (!gjc)
		return;

	x = jutil_iqnew(JPACKET__GET, NS_AUTH);
	xmlnode_put_attrib(x, "id", IQID_AUTH);
	y = xmlnode_get_tag(x, "query");

	user = gjc->user->user;

	if (user) {
		z = xmlnode_insert_tag(y, "username");
		xmlnode_insert_cdata(z, user, -1);
	}

	gjab_send(gjc, x);
	xmlnode_free(x);
}

static void gjab_auth(gjconn gjc)
{
	xmlnode x, y, z;
	char *hash, *user;

	if (!gjc)
		return;

	x = jutil_iqnew(JPACKET__SET, NS_AUTH);
	xmlnode_put_attrib(x, "id", IQID_AUTH);
	y = xmlnode_get_tag(x, "query");

	user = gjc->user->user;

	if (user) {
		z = xmlnode_insert_tag(y, "username");
		xmlnode_insert_cdata(z, user, -1);
	}

	z = xmlnode_insert_tag(y, "resource");
	xmlnode_insert_cdata(z, gjc->user->resource, -1);

	if (gjc->sid) {
		debug_printf("digest authentication (sid %s)\n", gjc->sid);
		z = xmlnode_insert_tag(y, "digest");
		hash = pmalloc(x->p, strlen(gjc->sid) + strlen(gjc->pass) + 1);
		strcpy(hash, gjc->sid);
		strcat(hash, gjc->pass);
		hash = shahash(hash);
		xmlnode_insert_cdata(z, hash, 40);
	} else {
		z = xmlnode_insert_tag(y, "password");
		xmlnode_insert_cdata(z, gjc->pass, -1);
	}

	gjab_send(gjc, x);
	xmlnode_free(x);

	return;
}

static void gjab_recv(gjconn gjc)
{
	static char buf[4096];
	int len;

	if (!gjc || gjc->state == JCONN_STATE_OFF)
		return;
#ifndef _WIN32
	if ((len = read(gjc->fd, buf, sizeof(buf) - 1)) > 0) {
#else
	if ((len = recv(gjc->fd, buf, sizeof(buf) - 1, 0)) > 0) {
#endif
		struct jabber_data *jd = GJ_GC(gjc)->proto_data;
		buf[len] = '\0';
		debug_printf("input (len %d): %s\n", len, buf);
		XML_Parse(gjc->parser, buf, len, 0);
		if (jd->die)
			signoff(GJ_GC(gjc));
	} else if (len < 0 || errno != EAGAIN) {
		STATE_EVT(JCONN_STATE_OFF)
	}
}

static void startElement(void *userdata, const char *name, const char **attribs)
{
	xmlnode x;
	gjconn gjc = (gjconn) userdata;

	if (gjc->current) {
		/* Append the node to the current one */
		x = xmlnode_insert_tag(gjc->current, name);
		xmlnode_put_expat_attribs(x, attribs);

		gjc->current = x;
	} else {
		x = xmlnode_new_tag(name);
		xmlnode_put_expat_attribs(x, attribs);
		if (strcmp(name, "stream:stream") == 0) {
			/* special case: name == stream:stream */
			/* id attrib of stream is stored for digest auth */
			gjc->sid = g_strdup(xmlnode_get_attrib(x, "id"));
			/* STATE_EVT(JCONN_STATE_AUTH) */
			xmlnode_free(x);
		} else {
			gjc->current = x;
		}
	}
}

static void endElement(void *userdata, const char *name)
{
	gjconn gjc = (gjconn) userdata;
	xmlnode x;
	jpacket p;

	if (gjc->current == NULL) {
		/* we got </stream:stream> */
		STATE_EVT(JCONN_STATE_OFF)
		    return;
	}

	x = xmlnode_get_parent(gjc->current);

	if (!x) {
		/* it is time to fire the event */
		p = jpacket_new(gjc->current);

		if (gjc->on_packet)
			(gjc->on_packet) (gjc, p);
		else
			xmlnode_free(gjc->current);
	}

	gjc->current = x;
}

static void jabber_callback(gpointer data, gint source, GaimInputCondition condition)
{
	struct gaim_connection *gc = (struct gaim_connection *)data;
	struct jabber_data *jd = (struct jabber_data *)gc->proto_data;

	gjab_recv(jd->gjc);
}

static void charData(void *userdata, const char *s, int slen)
{
	gjconn gjc = (gjconn) userdata;

	if (gjc->current)
		xmlnode_insert_cdata(gjc->current, s, slen);
}

static void gjab_connected(gpointer data, gint source, GaimInputCondition cond)
{
	xmlnode x;
	char *t, *t2;
	struct gaim_connection *gc = data;
	struct jabber_data *jd;
	gjconn gjc;

	if (!g_slist_find(connections, gc)) {
		close(source);
		return;
	}

	jd = gc->proto_data;
	gjc = jd->gjc;

	if (gjc->fd != source)
		gjc->fd = source;

	if (source == -1) {
		STATE_EVT(JCONN_STATE_OFF)
		return;
	}

	gjc->state = JCONN_STATE_CONNECTED;
	STATE_EVT(JCONN_STATE_CONNECTED)

	/* start stream */
	x = jutil_header(NS_CLIENT, gjc->user->server);
	t = xmlnode2str(x);
	/* this is ugly, we can create the string here instead of jutil_header */
	/* what do you think about it? -madcat */
	t2 = strstr(t, "/>");
	*t2++ = '>';
	*t2 = '\0';
	gjab_send_raw(gjc, "<?xml version='1.0'?>");
	gjab_send_raw(gjc, t);
	xmlnode_free(x);

	gjc->state = JCONN_STATE_ON;
	STATE_EVT(JCONN_STATE_ON);

	gc = GJ_GC(gjc);
	gc->inpa = gaim_input_add(gjc->fd, GAIM_INPUT_READ, jabber_callback, gc);
}

static void gjab_start(gjconn gjc)
{
	struct aim_user *user;
	int port;

	if (!gjc || gjc->state != JCONN_STATE_OFF)
		return;

	user = GJ_GC(gjc)->user;
	port = user->proto_opt[USEROPT_PORT][0] ? atoi(user->proto_opt[USEROPT_PORT]) : DEFAULT_PORT;

	gjc->parser = XML_ParserCreate(NULL);
	XML_SetUserData(gjc->parser, (void *)gjc);
	XML_SetElementHandler(gjc->parser, startElement, endElement);
	XML_SetCharacterDataHandler(gjc->parser, charData);

	gjc->fd = proxy_connect(gjc->user->server, port, gjab_connected, GJ_GC(gjc));
	if (!user->gc || (gjc->fd < 0)) {
		STATE_EVT(JCONN_STATE_OFF)
		return;
	}
}

/*
 * Find chat by chat group name
 */
static struct conversation *find_chat(struct gaim_connection *gc, char *name)
{
	GSList *bcs = gc->buddy_chats;
	struct conversation *b = NULL;
	char *chat = g_strdup(normalize(name));

	while (bcs) {
		b = bcs->data;
		if (!strcasecmp(normalize(b->name), chat))
			break;
		b = NULL;
		bcs = bcs->next;
	}

	g_free(chat);
	return b;
}

/*
 * Find chat by "chat id"
 *
 * Returns: 0 on success and jabber_chat pointer set
 * or -EINVAL on error and jabber_chat pointer is
 * undefined.
 *
 * TBD: Slogging through the buddy_chats list seems
 * redundant since the chat i.d. is mirrored in the
 * jabber_chat struct list.  But that's the way it
 * was, so that's the way I'm leaving it--for now.
 */
static int jabber_find_chat_by_convo_id(struct gaim_connection *gc, int id, struct jabber_chat **jc)
{
	GSList *bcs = gc->buddy_chats;
	struct conversation *b = NULL;
	struct jabber_data *jd = gc->proto_data;

	*jc = NULL;

	while(bcs != NULL) {
		b = bcs->data;
		if (id == b->id)
			break;
		bcs = bcs->next;
	}

	if (bcs != NULL) {
		bcs = jd->chats;
		while (bcs != NULL) {
			*jc = bcs->data;
			if ((*jc)->state == JCS_ACTIVE && (*jc)->b == b)
				break;
			bcs = bcs->next;
		}
	}

	return(bcs == NULL? -EINVAL : 0);
}

/*
 * Find any chat
 */
static struct jabber_chat *find_any_chat(struct gaim_connection *gc, jid chat)
{
	GSList *jcs = ((struct jabber_data *)gc->proto_data)->chats;
	struct jabber_chat *jc = NULL;

	while (jcs) {
		jc = jcs->data;
		if (!jid_cmpx(chat, jc->gjid, JID_USER | JID_SERVER))
			break;
		jc = NULL;
		jcs = jcs->next;
	}

	return jc;
}


/*
 * Find existing/active Jabber chat
 */
static struct jabber_chat *find_existing_chat(struct gaim_connection *gc, jid chat)
{
	GSList *jcs = ((struct jabber_data *)gc->proto_data)->chats;
	struct jabber_chat *jc = NULL;

	while (jcs) {
		jc = jcs->data;
		if (jc->state == JCS_ACTIVE && !jid_cmpx(chat, jc->gjid, JID_USER | JID_SERVER))
			break;
		jc = NULL;
		jcs = jcs->next;
	}

	return jc;
}

/*
 * Find pending chat
 */
static struct jabber_chat *find_pending_chat(struct gaim_connection *gc, jid chat)
{
	GSList *jcs = ((struct jabber_data *)gc->proto_data)->chats;
	struct jabber_chat *jc = NULL;

	while (jcs) {
		jc = jcs->data;
		if (jc->state == JCS_PENDING && !jid_cmpx(chat, jc->gjid, JID_USER | JID_SERVER))
			break;
		jc = NULL;
		jcs = jcs->next;
	}

	return jc;
}

static gboolean find_chat_buddy(struct conversation *b, char *name)
{
	GList *m = b->in_room;

	while (m) {
		if (!strcmp(m->data, name))
			return TRUE;
		m = m->next;
	}

	return FALSE;
}

/*
 * Remove a buddy from the (gaim) buddylist (if he's on it)
 */
static void jabber_remove_gaim_buddy(struct gaim_connection *gc, const char *buddyname)
{
	struct buddy *b;

	if ((b = find_buddy(gc, buddyname)) != NULL) {
		struct group *group;

		group = find_group_by_buddy(gc, buddyname);
		debug_printf("removing buddy [1]: %s, from group: %s\n", buddyname, group->name);
		remove_buddy(gc, group, b);
		do_export(gc);
	}
}

static void jabber_change_passwd(struct gaim_connection *gc, const char *old, const char *new)
{
	gjconn gjc = ((struct jabber_data *)gc->proto_data)->gjc;

	if(strcmp(old, gjc->pass))
	{
		do_error_dialog(_("Unable to change password."),
			_("The current password you entered is incorrect.  Your password has "
			  "not been changed."), GAIM_ERROR);
	}
	else if(!strcmp(old, new))
	{
		do_error_dialog(_("Unable to change password"),
			_("The new password you entered is the same as your current password.  "
			  "Your password remains the same."), GAIM_ERROR);
	}
	else
	{
		xmlnode x, y, z;
		char *id;

		x = jutil_iqnew(JPACKET__SET, NS_REGISTER);
		xmlnode_put_attrib(x, "to", gjc->user->server);
		y = xmlnode_get_tag(x, "query");
		z = xmlnode_insert_tag(y, "username");
		xmlnode_insert_cdata(z, gjc->user->user, -1);
		z = xmlnode_insert_tag(y, "password");
		xmlnode_insert_cdata(z, new, -1);

		id = gjab_getid(gjc);
		xmlnode_put_attrib(x, "id", id);

		free(gjc->pass);
		gjc->pass = strdup(new);

		g_hash_table_insert(gjc->queries, g_strdup(id), g_strdup("change_password"));

		gjab_send(gjc, x);
		xmlnode_free(x);
	}
}

/*
 * Return pointer to jabber_buddy_data if buddy found.  Create if necessary.
 */
static struct jabber_buddy_data* jabber_find_buddy(struct gaim_connection *gc, char *buddy)
{
	struct jabber_data *jd = gc->proto_data;
	gpointer val;
	char *realwho;

	if((realwho = get_realwho(jd->gjc, buddy, FALSE, NULL)) == NULL)
		return NULL;

	val = g_hash_table_lookup(jd->buddies, realwho);
	if(val) {
		g_free(realwho);
		return (struct jabber_buddy_data *)val;

	} else {
		struct jabber_buddy_data *jbd = g_new0(struct jabber_buddy_data, 1);
		jbd->error_msg = NULL;
		jbd->resources = NULL;
		jbd->invisible = JABBER_NOT_INVIS;
		g_hash_table_insert(jd->buddies, g_strdup(realwho), jbd);
		g_free(realwho);
		return jbd;
	}
}

/*
 * find a resource by name, or if no name given, return the "default" resource
 * default being the highest priority one.
 */

static jab_res_info jabber_find_resource(struct gaim_connection *gc, char *who)
{
	GSList *resources;
	struct jabber_buddy_data *jbd = jabber_find_buddy(gc, who);
	jab_res_info jri = NULL;
	char *res = strstr(who, "/");

	if(res)
		res++;

	if(jbd)
	{
		resources = jbd->resources;
		while(resources)
		{
			if(!jri && !res) {
				jri = (jab_res_info) resources->data;
			} else if(!res) { /* we're looking for the default priority, so... */
				if(((jab_res_info) resources->data)->priority >= jri->priority)
					jri = (jab_res_info) resources->data;
			} else if(((jab_res_info)resources->data)->name) {
				if(!strcasecmp(((jab_res_info) resources->data)->name, res)) {
					jri = (jab_res_info) resources->data;
					break;
				}
			}
			resources = resources->next;
		}
	}

	return jri;
}

/*
 * if the resource doesn't exist, create it.  otherwise, just update the priority
 */
static void jabber_track_resource(struct gaim_connection *gc,
				  char *buddy,
				  char *res,
				  int priority,
				  int state)
{
	struct jabber_buddy_data *jbd = jabber_find_buddy(gc, buddy);

	if(jbd) {
		char *who;
		jab_res_info jri;
		if(res)
			who = g_strdup_printf("%s/%s", buddy, res);
		else
			who = g_strdup(buddy);
		jri = jabber_find_resource(gc, who);
		g_free(who);
		if(!jri) {
			jri = g_new0(struct jabber_resource_info, 1);
			jri->name = g_strdup(res);
			jri->away_msg = NULL;
			jbd->resources = g_slist_append(jbd->resources, jri);
		}
		jri->priority = priority;
		jri->state = state;
	}
}

/*
 * remove the resource, if it exists
 */
static void jabber_remove_resource(struct gaim_connection *gc, char *buddy, char *res)
{
	struct jabber_buddy_data *jbd = jabber_find_buddy(gc, buddy);
	if(jbd) {
		char *who;
		jab_res_info jri;
		if(res)
			who = g_strdup_printf("%s/%s", buddy, res);
		else
			who = g_strdup(buddy);
		jri = jabber_find_resource(gc, who);
		g_free(who);
		if(jri) {
			if(jri->name)
				g_free(jri->name);
			if(jri->away_msg)
				g_free(jri->away_msg);
			jbd->resources = g_slist_remove(jbd->resources, jri);
			g_free(jri);
		}
	}
}

/*
 * grab the away message for the default resource
 */
static char *jabber_lookup_away(gjconn gjc, char *name)
{
	jab_res_info jri = jabber_find_resource(GJ_GC(gjc), name);

	if(!jri || !jri->away_msg)
		return _("Unknown");

	return jri->away_msg;
}

static void jabber_track_away(gjconn gjc, jpacket p, char *type)
{
	char *show;
	char *vshow = NULL;
	char *status = NULL;
	char *msg = NULL;
	jab_res_info jri = NULL;

	if(!p || !p->from || !p->from->user)
		return;

	jri = jabber_find_resource(GJ_GC(gjc), jid_full(p->from));

	if(!jri)
		return;

	if (type && (strcasecmp(type, "unavailable") == 0)) {
		vshow = _("Unavailable");
	} else {
		if((show = xmlnode_get_tag_data(p->x, "show")) != NULL) {
			if (!strcasecmp(show, "away")) {
				vshow = _("Away");
			} else if (!strcasecmp(show, "chat")) {
				vshow = _("Online");
			} else if (!strcasecmp(show, "xa")) {
				vshow = _("Extended Away");
			} else if (!strcasecmp(show, "dnd")) {
				vshow = _("Do Not Disturb");
			}
		}
	}

	status = g_strdup(xmlnode_get_tag_data(p->x, "status"));

	if(vshow != NULL || status != NULL ) {
		/* kinda hokey, but it works :-) */
		msg = g_strdup_printf("%s%s%s",
			(vshow == NULL? "" : vshow),
			(vshow == NULL || status == NULL? "" : ": "),
			(status == NULL? "" : status));
	} else {
		msg = g_strdup(_("Online"));
	}

	g_free(status);

	if(jri->away_msg)
		g_free(jri->away_msg);

	jri->away_msg = msg;
}

static void jabber_convo_closed(struct gaim_connection *gc, char *name)
{
	jab_res_info jri = jabber_find_resource(gc, name);

	if(jri) {
		if(jri->thread_id)
			g_free(jri->thread_id);

		jri->thread_id = NULL;
	}
}

static void jabber_track_convo_thread(gjconn gjc, char *name, char *thread_id)
{
	jab_res_info jri = jabber_find_resource(GJ_GC(gjc), name);

	if(jri) {
		if(jri->thread_id)
			g_free(jri->thread_id);

		jri->thread_id = g_strdup(thread_id);
	}
}

static char *jabber_get_convo_thread(gjconn gjc, char *name)
{
	char *ct = NULL;
	jab_res_info jri = jabber_find_resource(GJ_GC(gjc), name);

	if(jri) {
		if(jri->thread_id)
			ct = g_strdup(jri->thread_id);
	}
	if(!ct) {
		char buf[1024];
		snprintf(buf, sizeof(buf), "%s%d", name, (int)time(NULL));
		ct = g_strdup(shahash(buf));
	}
	return ct;
}


static time_t iso8601_to_time(char *timestamp)
{
	struct tm t;
	time_t retval = 0;

	if(sscanf(timestamp, "%04d%02d%02dT%02d:%02d:%02d",
		&t.tm_year, &t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min, &t.tm_sec))
	{
		t.tm_year -= 1900;
		t.tm_mon -= 1;
		t.tm_isdst = 0;
		retval = mktime(&t);
#		ifdef HAVE_TM_GMTOFF
			retval += t.tm_gmtoff;
#		else
#		        ifdef HAVE_TIMEZONE
				tzset();	/* making sure */
				retval -= timezone;
#		        endif
#		endif
	}

	return retval;
}

static void jabber_handlemessage(gjconn gjc, jpacket p)
{
	xmlnode y, subj;
	time_t time_sent = time(NULL);
	gboolean typing = FALSE;

	char *from = NULL, *msg = NULL, *type = NULL, *topic = NULL;
	char *thread_id = NULL;
	char *conference_room = NULL;
	char m[BUF_LONG * 2];

	type = xmlnode_get_attrib(p->x, "type");

	if ((y = xmlnode_get_tag(p->x, "thread")))
		thread_id = xmlnode_get_data(y);
	if(!thread_id)
		thread_id = "";

	y = xmlnode_get_firstchild(p->x);

	while(y) {
		if(NSCHECK(y, NS_DELAY)) {
			char *timestamp = xmlnode_get_attrib(y, "stamp");
			time_sent = iso8601_to_time(timestamp);
		} else if(NSCHECK(y, "jabber:x:event")) {
			if(xmlnode_get_tag(y, "composing"))
				typing = TRUE;
		} else if(NSCHECK(y, "jabber:x:conference")) {
			conference_room = xmlnode_get_attrib(y, "jid");
		}
		y = xmlnode_get_nextsibling(y);
	}

	if (!type || !strcasecmp(type, "normal") || !strcasecmp(type, "chat")) {

		from = jid_full(p->from);
		/*
		if ((y = xmlnode_get_tag(p->x, "html"))) {
			msg = xmlnode_get_data(y);
		} else
		*/
		if ((y = xmlnode_get_tag(p->x, "body"))) {
			msg = xmlnode_get_data(y);
		}

		if (!from)
			return;

		if (conference_room) {
			GList *m = NULL;
			char **data;

			data = g_strsplit(conference_room, "@", 2);
			m = g_list_append(m, g_strdup(data[0]));
			m = g_list_append(m, g_strdup(data[1]));
			m = g_list_append(m, g_strdup(gjc->user->user));
			g_strfreev(data);

			serv_got_chat_invite(GJ_GC(gjc), conference_room, from, msg, m);
		} else if (msg) { /* whisper */
			struct jabber_chat *jc;
			g_snprintf(m, sizeof(m), "%s", msg);
			if (((jc = find_existing_chat(GJ_GC(gjc), p->from)) != NULL) && jc->b)
				serv_got_chat_in(GJ_GC(gjc), jc->b->id, p->from->resource, 1, m,
					time_sent);
			else {
				int flags = 0;
				jab_res_info jri = jabber_find_resource(GJ_GC(gjc), from);
				if(jri && typing)
					jri->has_composing = TRUE;
				if (xmlnode_get_tag(p->x, "gaim"))
					flags = IM_FLAG_GAIMUSER;
				jabber_track_convo_thread(gjc, from, thread_id);
				if (find_conversation(from))
					serv_got_im(GJ_GC(gjc), from, m, flags,
						time_sent, -1);
				else {
					if(p->from->user) {
						from = g_strdup_printf("%s@%s", p->from->user,
							p->from->server);
					} else {
						/* server message? */
						from = g_strdup(p->from->server);
					}
					serv_got_im(GJ_GC(gjc), from, m, flags, time_sent, -1);
					g_free(from);
				}
			}
		} else {
			/* a non-message message! */
			from = g_strdup_printf("%s@%s", p->from->user, p->from->server);
			if(typing)
				serv_got_typing(GJ_GC(gjc), from, 0, TYPING);
			else
				serv_got_typing_stopped(GJ_GC(gjc), from);
			g_free(from);
		}

	} else if (!strcasecmp(type, "error")) {
		if ((y = xmlnode_get_tag(p->x, "error"))) {
			type = xmlnode_get_attrib(y, "code");
			msg = xmlnode_get_data(y);
		}

		if (msg) {
			from = g_strdup_printf(_("Jabber Error %s"), type ? type : "");
			do_error_dialog(from, msg, GAIM_ERROR);
			g_free(from);
		}
	} else if (!strcasecmp(type, "groupchat")) {
		struct jabber_chat *jc;
		static int i = 0;

		/*
		if ((y = xmlnode_get_tag(p->x, "html"))) {
			msg = xmlnode_get_data(y);
		} else
		*/
		if ((y = xmlnode_get_tag(p->x, "body"))) {
			msg = xmlnode_get_data(y);
		}

		if ((subj = xmlnode_get_tag(p->x, "subject"))) {
			topic = xmlnode_get_data(subj);
		}

		jc = find_existing_chat(GJ_GC(gjc), p->from);
		if (!jc) {
			/* we're not in this chat. are we supposed to be? */
			if ((jc = find_pending_chat(GJ_GC(gjc), p->from)) != NULL) {
				/* yes, we're supposed to be. so now we are. */
				jc->b = serv_got_joined_chat(GJ_GC(gjc), i++, p->from->user);
				jc->id = jc->b->id;
				jc->state = JCS_ACTIVE;
			} else {
				/* no, we're not supposed to be. */
				g_free(msg);
				return;
			}
		}
		if (p->from->resource) {
			if (!y) {
				if (!find_chat_buddy(jc->b, p->from->resource)) {
					add_chat_buddy(jc->b, p->from->resource, NULL);
				} else if ((y = xmlnode_get_tag(p->x, "status"))) {
					jabber_track_away(gjc, p, NULL);
				}
			} else if (jc->b && msg) {
				char buf[8192];

				if (topic) {
					char tbuf[8192];
					g_snprintf(tbuf, sizeof(tbuf), "%s", topic);
					chat_set_topic(jc->b, p->from->resource, tbuf);
				}

				g_snprintf(buf, sizeof(buf), "%s", msg);
				serv_got_chat_in(GJ_GC(gjc), jc->b->id, p->from->resource, 0, buf,
					time_sent);
			}
		} else { /* message from the server */
			if(jc->b && topic) {
				char tbuf[8192];
				g_snprintf(tbuf, sizeof(tbuf), "%s", topic);
				chat_set_topic(jc->b, "", tbuf);
			}
		}

	} else {
		debug_printf("unhandled message %s\n", type);
	}
}

static void jabber_handlepresence(gjconn gjc, jpacket p)
{
	char *to, *from, *type;
	struct buddy *b = NULL;
	gaim_jid gjid;
	char *buddy;
	xmlnode y;
	char *show;
	int state = 0;
	struct conversation *cnv = NULL;
	struct jabber_chat *jc = NULL;
	int priority = 0;
	struct jabber_buddy_data *jbd;

	to = xmlnode_get_attrib(p->x, "to");
	from = xmlnode_get_attrib(p->x, "from");
	type = xmlnode_get_attrib(p->x, "type");

	if((buddy = get_realwho(gjc, from, FALSE, &gjid)) == NULL)
		return;

	if (gjid->user == NULL) {
		/* FIXME: transport */
		g_free(buddy);
		gaim_jid_free(gjid);
		return;
	}

	jbd = jabber_find_buddy(GJ_GC(gjc), buddy);

	if(jbd->error_msg) {
		g_free(jbd->error_msg);
		jbd->error_msg = NULL;
	}

	if(type && !strcasecmp(type, "error")) {
		state = UC_ERROR;
		if((y = xmlnode_get_tag(p->x, "error")) != NULL) {
			jbd->error_msg = g_strdup_printf(_("Error %s: %s"),
				xmlnode_get_attrib(y, "code"), xmlnode_get_data(y));
		} else {
			jbd->error_msg = g_strdup(_("Unknown Error in presence"));
		}
	} else {
		if ((y = xmlnode_get_tag(p->x, "show"))) {
			show = xmlnode_get_data(y);
			if (!show) {
				state = 0;
			} else if (!strcasecmp(show, "away")) {
				state = UC_AWAY;
			} else if (!strcasecmp(show, "chat")) {
				state = UC_CHAT;
			} else if (!strcasecmp(show, "xa")) {
				state = UC_XA;
			} else if (!strcasecmp(show, "dnd")) {
				state = UC_DND;
			}
		} else {
			state = 0;
		}
	}

	if ((y = xmlnode_get_tag(p->x, "priority")))
		priority = atoi(xmlnode_get_data(y));

	/* um. we're going to check if it's a chat. if it isn't, and there are pending
	 * chats, create the chat. if there aren't pending chats and we don't have the
	 * buddy on our list, simply bail out. */
	if ((cnv = find_chat(GJ_GC(gjc), gjid->user)) == NULL) {
		static int i = 0x70;
		if ((jc = find_pending_chat(GJ_GC(gjc), gjid)) != NULL) {
			jc->b = cnv = serv_got_joined_chat(GJ_GC(gjc), i++, gjid->user);
			jc->id = jc->b->id;
			jc->state = JCS_ACTIVE;
		} else if ((b = find_buddy(GJ_GC(gjc), buddy)) == NULL) {
			g_free(buddy);
			gaim_jid_free(gjid);
			return;
		}
	}

	if (type && (strcasecmp(type, "unavailable") == 0))
		jabber_remove_resource(GJ_GC(gjc), buddy, gjid->resource);
	else {
		jabber_track_resource(GJ_GC(gjc), buddy, gjid->resource, priority, state);

		/* keep track of away msg somewhat the same as the yahoo plugin */
		jabber_track_away(gjc, p, type);
	}


	if (!cnv) {
		/* this is where we handle presence information for "regular" buddies */
		jab_res_info jri = jabber_find_resource(GJ_GC(gjc), buddy);
		if(jri) {
			serv_got_update(GJ_GC(gjc), buddy, 1, 0, b->signon, b->idle, jri->state, 0);
		} else
			serv_got_update(GJ_GC(gjc), buddy, 0, 0, 0, 0, 0, 0);

	} else {
		if (gjid->resource) {
			if (type && (!strcasecmp(type, "unavailable"))) {
				struct jabber_data *jd;
				if (!jc && !(jc = find_existing_chat(GJ_GC(gjc), gjid))) {
					g_free(buddy);
					gaim_jid_free(gjid);
					return;
				}
				jd = jc->gc->proto_data;
				/* if it's not ourselves...*/
				if (strcmp(gjid->resource, jc->gjid->resource) && jc->b) {
					remove_chat_buddy(jc->b, gjid->resource, NULL);
					g_free(buddy);
					gaim_jid_free(gjid);
					return;
				}

				jc->state = JCS_CLOSED;
				serv_got_chat_left(GJ_GC(gjc), jc->id);
				/*
				 * TBD: put back some day?
				jd->chats = g_slist_remove(jd->chats, jc);
				g_free(jc);
				 */
			} else {
				if ((!jc && !(jc = find_existing_chat(GJ_GC(gjc), gjid))) || !jc->b) {
					g_free(buddy);
					gaim_jid_free(gjid);
					return;
				}
				if (!find_chat_buddy(jc->b, gjid->resource)) {
					add_chat_buddy(jc->b, gjid->resource, NULL);
				}
			}
		}
	}

	g_free(buddy);
	gaim_jid_free(gjid);

	return;
}

/*
 * Used only by Jabber accept/deny add stuff just below
 */
struct jabber_add_permit {
	gjconn gjc;
	gchar *user;
};

/*
 * Common part for Jabber accept/deny adds
 *
 * "type" says whether we'll permit/deny the subscribe request
 */
static void jabber_accept_deny_add(struct jabber_add_permit *jap, const char *type)
{
	xmlnode g = xmlnode_new_tag("presence");

	xmlnode_put_attrib(g, "to", jap->user);
	xmlnode_put_attrib(g, "type", type);
	gjab_send(jap->gjc, g);

	xmlnode_free(g);
}

/*
 * Callback from "accept" in do_ask_dialog() invoked by jabber_handles10n()
 */
static void jabber_accept_add(struct jabber_add_permit *jap)
{
	jabber_accept_deny_add(jap, "subscribed");
	/*
	 * If we don't already have the buddy on *our* buddylist,
	 * ask if we want him or her added.
	 */
	if(find_buddy(GJ_GC(jap->gjc), jap->user) == NULL) {
		show_got_added(GJ_GC(jap->gjc), NULL, jap->user, NULL, NULL);
	}
	g_free(jap->user);
	g_free(jap);
}

/*
 * Callback from "deny/cancel" in do_ask_dialog() invoked by jabber_handles10n()
 */
static void jabber_deny_add(struct jabber_add_permit *jap)
{
	jabber_accept_deny_add(jap, "unsubscribed");
	g_free(jap->user);
	g_free(jap);
}

/*
 * Handle subscription requests
 */
static void jabber_handles10n(gjconn gjc, jpacket p)
{
	xmlnode g;
	char *Jid = xmlnode_get_attrib(p->x, "from");
	char *type = xmlnode_get_attrib(p->x, "type");

	g = xmlnode_new_tag("presence");
	xmlnode_put_attrib(g, "to", Jid);

	if (!strcmp(type, "subscribe")) {
		/*
		 * A "subscribe to us" request was received - put up the approval dialog
		 */
		struct jabber_add_permit *jap = g_new0(struct jabber_add_permit, 1);
		gchar *msg = g_strdup_printf(_("The user %s wants to add you to their buddy list."),
				Jid);

		jap->gjc = gjc;
		jap->user = g_strdup(Jid);
		do_ask_dialog(msg, NULL, jap, _("Authorize"), jabber_accept_add, _("Deny"), jabber_deny_add);

		g_free(msg);
		xmlnode_free(g);	/* Never needed it here anyway */
		return;

	} else if (!strcmp(type, "unsubscribe")) {
		/*
		 * An "unsubscribe to us" was received - simply "approve" it
		 */
		xmlnode_put_attrib(g, "type", "unsubscribed");
	} else {
		/*
		 * Did we attempt to subscribe to somebody and they do not exist?
		 */
		if (!strcmp(type, "unsubscribed")) {
			xmlnode y;
			char *status;
			if((y = xmlnode_get_tag(p->x, "status")) && (status = xmlnode_get_data(y)) &&
					!strcmp(status, "Not Found")) {
				char *msg = g_strdup_printf(_("The Jabber user %s does not exist and was therefore "
							      "not added to your roster."), 
							    xmlnode_get_attrib(p->x, "from"));
				do_error_dialog(_("No such user."), msg, GAIM_ERROR );
				g_free(msg);
			}
		}

		xmlnode_free(g);
		return;
	}

	gjab_send(gjc, g);
	xmlnode_free(g);
}

/*
 * Pending subscription to a buddy?
 */
#define BUD_SUB_TO_PEND(sub, ask) ((!strcasecmp((sub), "none") || !strcasecmp((sub), "from")) && \
					(ask) != NULL && !strcasecmp((ask), "subscribe"))

/*
 * Subscribed to a buddy?
 */
#define BUD_SUBD_TO(sub, ask) ((!strcasecmp((sub), "to") || !strcasecmp((sub), "both")) && \
					((ask) == NULL || !strcasecmp((ask), "subscribe")))

/*
 * Pending unsubscription to a buddy?
 */
#define BUD_USUB_TO_PEND(sub, ask) ((!strcasecmp((sub), "to") || !strcasecmp((sub), "both")) && \
					(ask) != NULL && !strcasecmp((ask), "unsubscribe")) 

/*
 * Unsubscribed to a buddy?
 */
#define BUD_USUBD_TO(sub, ask) ((!strcasecmp((sub), "none") || !strcasecmp((sub), "from")) && \
					((ask) == NULL || !strcasecmp((ask), "unsubscribe")))

/*
 * If a buddy is added or removed from the roster on another resource
 * jabber_handlebuddy is called
 *
 * Called with roster item node.
 */
static void jabber_handlebuddy(gjconn gjc, xmlnode x)
{
	xmlnode g;
	char *who, *name, *sub, *ask;
	gaim_jid gjid;
	struct buddy *b = NULL;
	char *buddyname, *groupname = NULL;

	who = xmlnode_get_attrib(x, "jid");
	name = xmlnode_get_attrib(x, "name");
	sub = xmlnode_get_attrib(x, "subscription");
	ask = xmlnode_get_attrib(x, "ask");

	if((buddyname = get_realwho(gjc, who, FALSE, &gjid)) == NULL)
		return;

	/* JFIXME: jabber_handleroster() had a "FIXME: transport" at this
	 * equivilent point.  So...
	 *
	 * We haven't done anything interesting to this point, so we'll
	 * violate Good Coding Structure here by simply bailing out.
	 */
	if(!gjid->user) {
		g_free(buddyname);
		gaim_jid_free(gjid);
		return;
	}
	gaim_jid_free(gjid);

	if((g = xmlnode_get_tag(x, "group")) != NULL) {
		groupname = xmlnode_get_data(g);
	}

	/*
	 * Add or remove a buddy?  Change buddy's alias or group?
	 */
	if (BUD_SUB_TO_PEND(sub, ask) || BUD_SUBD_TO(sub, ask)) {
		if ((b = find_buddy(GJ_GC(gjc), buddyname)) == NULL) {
			debug_printf("adding buddy [4]: %s\n", buddyname);
			b = add_buddy(GJ_GC(gjc), groupname ? groupname : _("Buddies"), buddyname,
				name ? name : buddyname);
			do_export(GJ_GC(gjc));
		} else {
			struct group *c_grp = find_group_by_buddy(GJ_GC(gjc), buddyname);

			/*
			 * If the buddy's in a new group or his/her alias is changed...
			 */
			if(groupname && c_grp && strcmp(c_grp->name, groupname)) {
				int present = b->present;	/* save presence state */
				int uc = b->uc;			/* and away state (?) */
				int idle = b->idle;
				int signon = b->signon;

				/*
				 * seems rude, but it seems to be the only way...
				 */
				remove_buddy(GJ_GC(gjc), c_grp, b);
				b = add_buddy(GJ_GC(gjc), groupname, buddyname,
					name ? name : buddyname);
				do_export(GJ_GC(gjc));
				if(present) {
					serv_got_update(GJ_GC(gjc), buddyname, 1, 0, signon, idle,
							uc, 0);
				}
			} else if(name != NULL && strcmp(b->alias, name)) {
				g_snprintf(b->alias, sizeof(b->alias), "%s", name);
				handle_buddy_rename(b, buddyname);
			}
		}
	}  else if (BUD_USUB_TO_PEND(sub, ask) || BUD_USUBD_TO(sub, ask) || !strcasecmp(sub, "remove")) {
		jabber_remove_gaim_buddy(GJ_GC(gjc), buddyname);
	}

	g_free(buddyname);

}

static void jabber_handleroster(gjconn gjc, xmlnode querynode)
{
	xmlnode x;

	x = xmlnode_get_firstchild(querynode);
	while (x) {
		jabber_handlebuddy(gjc, x);
		x = xmlnode_get_nextsibling(x);
	}

	x = jutil_presnew(0, NULL, "Online");
	gjab_send(gjc, x);
	xmlnode_free(x);
}

static void jabber_handleauthresp(gjconn gjc, jpacket p)
{
	if (jpacket_subtype(p) == JPACKET__RESULT) {
		if (xmlnode_has_children(p->x)) {
			xmlnode query = xmlnode_get_tag(p->x, "query");
			set_login_progress(GJ_GC(gjc), 4, _("Authenticating"));
			if (!xmlnode_get_tag(query, "digest")) {
				g_free(gjc->sid);
				gjc->sid = NULL;
			}
			gjab_auth(gjc);
		} else {
			debug_printf("auth success\n");

			account_online(GJ_GC(gjc));
			serv_finish_login(GJ_GC(gjc));

			if (bud_list_cache_exists(GJ_GC(gjc)))
				do_import(GJ_GC(gjc), NULL);

			((struct jabber_data *)GJ_GC(gjc)->proto_data)->did_import = TRUE;

			gjab_reqroster(gjc);
		}
	} else {
		xmlnode xerr;
		char *errmsg = NULL;
		int errcode = 0;
		struct jabber_data *jd = GJ_GC(gjc)->proto_data;

		debug_printf("auth failed\n");
		xerr = xmlnode_get_tag(p->x, "error");
		if (xerr) {
			char msg[BUF_LONG];
			errmsg = xmlnode_get_data(xerr);
			if (xmlnode_get_attrib(xerr, "code")) {
				errcode = atoi(xmlnode_get_attrib(xerr, "code"));
				g_snprintf(msg, sizeof(msg), "Error %d: %s", errcode, errmsg);
			} else
				g_snprintf(msg, sizeof(msg), "%s", errmsg);
			hide_login_progress(GJ_GC(gjc), msg);
		} else {
			hide_login_progress(GJ_GC(gjc), _("Unknown login error"));
		}

		jd->die = TRUE;
	}
}

static void jabber_handleversion(gjconn gjc, xmlnode iqnode) {
	xmlnode querynode, x;
	char *id, *from;
	char os[1024];
	struct utsname osinfo;

	uname(&osinfo);
	g_snprintf(os, sizeof os, "%s %s %s", osinfo.sysname, osinfo.release, osinfo.machine);

	id = xmlnode_get_attrib(iqnode, "id");
	from = xmlnode_get_attrib(iqnode, "from");

	x = jutil_iqnew(JPACKET__RESULT, NS_VERSION);

	xmlnode_put_attrib(x, "to", from);
	xmlnode_put_attrib(x, "id", id);
	querynode = xmlnode_get_tag(x, "query");
	xmlnode_insert_cdata(xmlnode_insert_tag(querynode, "name"), PACKAGE, -1);
	xmlnode_insert_cdata(xmlnode_insert_tag(querynode, "version"), VERSION, -1);
	xmlnode_insert_cdata(xmlnode_insert_tag(querynode, "os"), os, -1);

	gjab_send(gjc, x);

	xmlnode_free(x);
}

static void jabber_handletime(gjconn gjc, xmlnode iqnode) {
	xmlnode querynode, x;
	char *id, *from;
	time_t now_t;
	struct tm *now;
	char buf[1024];

	time(&now_t);
	now = localtime(&now_t);

	id = xmlnode_get_attrib(iqnode, "id");
	from = xmlnode_get_attrib(iqnode, "from");

	x = jutil_iqnew(JPACKET__RESULT, NS_TIME);

	xmlnode_put_attrib(x, "to", from);
	xmlnode_put_attrib(x, "id", id);
	querynode = xmlnode_get_tag(x, "query");

	strftime(buf, 1024, "%Y%m%dT%T", now);
	xmlnode_insert_cdata(xmlnode_insert_tag(querynode, "utc"), buf, -1);
	strftime(buf, 1024, "%Z", now);
	xmlnode_insert_cdata(xmlnode_insert_tag(querynode, "tz"), buf, -1);
	strftime(buf, 1024, "%d %b %Y %T", now);
	xmlnode_insert_cdata(xmlnode_insert_tag(querynode, "display"), buf, -1);

	gjab_send(gjc, x);

	xmlnode_free(x);
}

struct jabber_file_transfer {
	enum { JFT_SENDFILE_IN, JFT_SENDFILE_OUT } type;
	struct file_transfer *xfer;
	char *from;
	struct g_url *url;
	char *name;
	GString *headers;

	int len;
	int fd;
	int watcher;

	gboolean sentreq;
	gboolean newline;
	gboolean startsaving;

	struct gaim_connection *gc;
};

static struct jabber_file_transfer *find_jft_by_xfer(struct gaim_connection *gc,
				struct file_transfer *xfer) {
	GSList *g = ((struct jabber_data *)gc->proto_data)->file_transfers;
	struct jabber_file_transfer *f = NULL;

	while(g) {
		f = (struct jabber_file_transfer *)g->data;
		if(f->xfer == xfer)
			break;
		g = g->next;
		f = NULL;
	}

	return f;
}

static void jabber_http_recv_callback(gpointer data, gint source, GaimInputCondition condition) {
	struct jabber_file_transfer *jft = data;
	char test;

	jft->fd = source;
	if(!jft->sentreq) {
		char buf[1024];
		g_snprintf(buf, sizeof(buf), "GET /%s HTTP/1.1\r\nHost: %s\r\n\r\n", jft->url->page, jft->url->address);
		write(source, buf, strlen(buf));
#ifndef _WIN32
		fcntl(source, F_SETFL, O_NONBLOCK);
#else
		{
			u_long imode = 1;
			ioctlsocket(source, FIONBIO, (u_long*)&imode);
		}
#endif
		jft->sentreq = TRUE;
		jft->watcher = gaim_input_add(source, GAIM_INPUT_READ, jabber_http_recv_callback,data);
		return;
	}

	if(!jft->startsaving) {
#ifndef _WIN32
		if(read(source, &test, sizeof(test)) > 0 || errno == EWOULDBLOCK) {
			if(errno == EWOULDBLOCK)
#else
		if(recv(source, &test, sizeof(test), 0) > 0 || WSAGetLastError() == WSAEWOULDBLOCK) {
			if(WSAEWOULDBLOCK == WSAGetLastError())
#endif
			{
				errno = 0;
				return;
			}

			jft->headers = g_string_append_c(jft->headers, test);
			if(test == '\r')
				return;
			if(test == '\n') {
				if(jft->newline) {
					gchar *lenstr = strstr(jft->headers->str, "Content-Length: ");
					if(lenstr) {
						sscanf(lenstr, "Content-Length: %d", &jft->len);
					}
					jft->startsaving = TRUE;
				} else
					jft->newline = TRUE;
				return;
			}
			jft->newline = FALSE;
			return;
		} else {
			gaim_input_remove(jft->watcher);
			close(source);
			//FIXME: ft_cancel(NULL, jft->xfer);
		}
		return;
	}

	/* we've parsed the headers, gotten the size, all is good.  now we pass the reception of
	 * the file off to the core, and leave it in it's capable...err...hands?? */
	gaim_input_remove(jft->watcher);
	jft->watcher = 0;
	transfer_in_do(jft->xfer, jft->fd, jft->name, jft->len);
}

static void jabber_file_transfer_cancel(struct gaim_connection *gc, struct file_transfer *xfer) {
	struct jabber_data *jd = gc->proto_data;
	struct jabber_file_transfer *jft = find_jft_by_xfer(gc, xfer);\
	xmlnode x,y;

	jd->file_transfers = g_slist_remove(jd->file_transfers, jft);

	gaim_input_remove(jft->watcher);
	close(jft->fd);

	x = xmlnode_new_tag("iq");
	xmlnode_put_attrib(x, "type", "error");
	xmlnode_put_attrib(x, "to", jft->from);
	y = xmlnode_insert_tag(x, "error");
	/* FIXME: need to handle other kinds of errors here */
	xmlnode_put_attrib(y, "code", "406");
	xmlnode_insert_cdata(y, "File Transfer Refused", -1);

	gjab_send(jd->gjc, x);

	xmlnode_free(x);

	g_string_free(jft->headers, TRUE);
	g_free(jft->from);
	g_free(jft->url);
	g_free(jft->name);

	g_free(jft);
}

static void jabber_file_transfer_done(struct gaim_connection *gc, struct file_transfer *xfer) {
	struct jabber_data *jd = gc->proto_data;
	struct jabber_file_transfer *jft = find_jft_by_xfer(gc, xfer);
	xmlnode x;

	jd->file_transfers = g_slist_remove(jd->file_transfers, jft);

	gaim_input_remove(jft->watcher);
	close(jft->fd);

	x = xmlnode_new_tag("iq");
	xmlnode_put_attrib(x, "type", "result");
	xmlnode_put_attrib(x, "to", jft->from);

	gjab_send(jd->gjc, x);

	xmlnode_free(x);

	g_string_free(jft->headers, TRUE);
	g_free(jft->from);
	g_free(jft->url);
	g_free(jft->name);

	g_free(jft);
}

static void jabber_file_transfer_in(struct gaim_connection *gc, struct file_transfer *xfer, int offset) {
	struct jabber_file_transfer *jft = find_jft_by_xfer(gc, xfer);

	proxy_connect(jft->url->address, jft->url->port, jabber_http_recv_callback, jft);
}

static void jabber_handleoob(gjconn gjc, xmlnode iqnode) {
	struct jabber_file_transfer *jft;
	struct jabber_data *jd = GJ_GC(gjc)->proto_data;
	char *msg = NULL;
	xmlnode querynode = xmlnode_get_tag(iqnode, "query");
	xmlnode urlnode,descnode;

	if(!querynode)
		return;
	urlnode = xmlnode_get_tag(querynode, "url");
	if(!urlnode)
		return;
	descnode = xmlnode_get_tag(querynode, "desc");
	if(descnode)
		msg = xmlnode_get_data(descnode);

	jft = g_new0(struct jabber_file_transfer, 1);
	jft->type = JFT_SENDFILE_IN;
	jft->gc = GJ_GC(gjc);
	jft->url = parse_url(xmlnode_get_data(urlnode));
	jft->from = g_strdup(xmlnode_get_attrib(iqnode, "from"));
	jft->name = g_strdup(g_strrstr(jft->url->page,"/"));
	if (!jft->name)
			jft->name = g_strdup(jft->url->page);
	jft->headers = g_string_new("");
	jft->len = -1;

	jd->file_transfers = g_slist_append(jd->file_transfers, jft);

	jft->xfer = transfer_in_add(GJ_GC(gjc), jft->from, jft->name, jft->len, 1, msg);
}

static void jabber_handlelast(gjconn gjc, xmlnode iqnode) {
	xmlnode x, querytag;
	char *id, *from;
	struct jabber_data *jd = GJ_GC(gjc)->proto_data;
	char idle_time[32];

	id = xmlnode_get_attrib(iqnode, "id");
	from = xmlnode_get_attrib(iqnode, "from");

	x = jutil_iqnew(JPACKET__RESULT, "jabber:iq:last");

	xmlnode_put_attrib(x, "to", from);
	xmlnode_put_attrib(x, "id", id);
	querytag = xmlnode_get_tag(x, "query");
	g_snprintf(idle_time, sizeof idle_time, "%ld", jd->idle ? time(NULL) - jd->idle : 0);
	xmlnode_put_attrib(querytag, "seconds", idle_time);

	gjab_send(gjc, x);
	xmlnode_free(x);
}

/*
 * delete == TRUE: delete found entry
 *
 * returns pointer to (local) copy of value if found, NULL otherwise
 *
 * Note: non-reentrant!  Local static storage re-used on subsequent calls.
 * If you're going to need to keep the returned value, make a copy!
 */
static gchar *jabber_track_queries(GHashTable *queries, gchar *key, gboolean delete)
{
	gpointer my_key, my_val;
	static gchar *ret_val = NULL;

	if(ret_val != NULL) {
		g_free(ret_val);
		ret_val = NULL;
	}

	/* self-protection */
	if(queries != NULL && key != NULL) {
		if(g_hash_table_lookup_extended(queries, key, &my_key, &my_val)) {
			ret_val = g_strdup((gchar *) my_val);
			if(delete) {
				g_hash_table_remove(queries, key);
				g_free(my_key);
				g_free(my_val);
			}
		}
	}

	return(ret_val);
}

static void jabber_handlepacket(gjconn gjc, jpacket p)
{
	char *id;
	switch (p->type) {
	case JPACKET_MESSAGE:
		jabber_handlemessage(gjc, p);
		break;
	case JPACKET_PRESENCE:
		jabber_handlepresence(gjc, p);
		break;
	case JPACKET_IQ:
		debug_printf("jpacket_subtype: %d\n", jpacket_subtype(p));

		id = xmlnode_get_attrib(p->x, "id");
		if (id != NULL && !strcmp(id, IQID_AUTH)) {
			jabber_handleauthresp(gjc, p);
			break;
		}

		if (jpacket_subtype(p) == JPACKET__SET) {
			xmlnode querynode;
			querynode = xmlnode_get_tag(p->x, "query");
			if (NSCHECK(querynode, "jabber:iq:roster")) {
				jabber_handlebuddy(gjc, xmlnode_get_firstchild(querynode));
			} else if(NSCHECK(querynode, "jabber:iq:oob")) {
				jabber_handleoob(gjc, p->x);
			}
		} else if (jpacket_subtype(p) == JPACKET__GET) {
			xmlnode querynode;
			querynode = xmlnode_get_tag(p->x, "query");
			if (NSCHECK(querynode, NS_VERSION)) {
				jabber_handleversion(gjc, p->x);
			} else if (NSCHECK(querynode, NS_TIME)) {
				jabber_handletime(gjc, p->x);
			} else if (NSCHECK(querynode, "jabber:iq:last")) {
				jabber_handlelast(gjc, p->x);
			}
		} else if (jpacket_subtype(p) == JPACKET__RESULT) {
			xmlnode querynode, vcard;
			char *xmlns, *from;

			/*
			 * TBD: ISTM maybe this part could use a serious re-work?
			 */
			from = xmlnode_get_attrib(p->x, "from");
			querynode = xmlnode_get_tag(p->x, "query");
			vcard = xmlnode_get_tag(p->x, "vCard");
			if (!vcard)
				vcard = xmlnode_get_tag(p->x, "VCARD");

			if (NSCHECK(querynode, NS_ROSTER)) {
				jabber_handleroster(gjc, querynode);
			} else if (NSCHECK(querynode, NS_VCARD)) {
				jabber_track_queries(gjc->queries, id, TRUE);	/* delete query track */
				jabber_handlevcard(gjc, querynode, from);
			} else if (vcard) {
				jabber_track_queries(gjc->queries, id, TRUE);	/* delete query track */
				jabber_handlevcard(gjc, vcard, from);
			} else if((xmlns = xmlnode_get_attrib(querynode, "xmlns")) != NULL) {
				debug_printf("jabber:iq:query: %s\n", xmlns);
			} else {
				char *val;

				debug_printf("jabber:iq: %s\n", xmlnode2str(p->x));

				/* handle "null" query results */
				if((val = jabber_track_queries(gjc->queries, id, TRUE)) != NULL) {
					if(strcmp((char *) val, "vCard") == 0) {
						/*
						 * No actual vCard, but there's other stuff.  This
						 * way the user always gets some kind of response.
						 */
						jabber_handlevcard(gjc, NULL, from);
					} else if(!strcmp((char *) val, "change_password")) {
					   char buf[BUF_LONG];
					   sprintf(buf, _("Password successfully changed."));

					   do_error_dialog(buf, NULL, GAIM_INFO);
					}
				}
			}

		} else if (jpacket_subtype(p) == JPACKET__ERROR) {
			xmlnode xerr;
			char *from, *errmsg = NULL;
			int errcode = 0;

			from = xmlnode_get_attrib(p->x, "from");
			xerr = xmlnode_get_tag(p->x, "error");
			if (xerr) {
				errmsg = xmlnode_get_data(xerr);
				if (xmlnode_get_attrib(xerr, "code"))
					errcode = atoi(xmlnode_get_attrib(xerr, "code"));
			}

			from = g_strdup_printf("Jabber Error %d (%s)", errcode, from);
			do_error_dialog(from, errmsg, GAIM_ERROR);
			g_free(from);

		}

		break;
	case JPACKET_S10N:
		jabber_handles10n(gjc, p);
		break;
	default:
		debug_printf("jabber: packet type %d (%s)\n", p->type, xmlnode2str(p->x));
	}

	xmlnode_free(p->x);

	return;
}

static void jabber_handlestate(gjconn gjc, int state)
{
	switch (state) {
	case JCONN_STATE_OFF:
		if(gjc->was_connected) {
			hide_login_progress_error(GJ_GC(gjc), _("Connection lost"));
		} else {
			hide_login_progress(GJ_GC(gjc), _("Unable to connect"));
		}
		signoff(GJ_GC(gjc));
		break;
	case JCONN_STATE_CONNECTED:
		gjc->was_connected = 1;
		set_login_progress(GJ_GC(gjc), 2, _("Connected"));
		break;
	case JCONN_STATE_ON:
		set_login_progress(GJ_GC(gjc), 3, _("Requesting Authentication Method"));
		gjab_reqauth(gjc);
		break;
	default:
		debug_printf("state change: %d\n", state);
	}
	return;
}

static void jabber_login(struct aim_user *user)
{
	struct gaim_connection *gc = new_gaim_conn(user);
	struct jabber_data *jd = gc->proto_data = g_new0(struct jabber_data, 1);
	char *loginname = create_valid_jid(user->username, DEFAULT_SERVER, "GAIM");

	jd->buddies = g_hash_table_new(g_str_hash, g_str_equal);
	jd->chats = NULL;	/* we have no chats yet */

	set_login_progress(gc, 1, _("Connecting"));

	if (!(jd->gjc = gjab_new(loginname, user->password, gc))) {
		g_free(loginname);
		debug_printf("jabber: unable to connect (jab_new failed)\n");
		hide_login_progress(gc, _("Unable to connect"));
		signoff(gc);
		return;
	}

	g_free(loginname);
	gjab_state_handler(jd->gjc, jabber_handlestate);
	gjab_packet_handler(jd->gjc, jabber_handlepacket);
	jd->gjc->queries = g_hash_table_new(g_str_hash, g_str_equal);
	gjab_start(jd->gjc);
}

static gboolean jabber_destroy_hash(gpointer key, gpointer val, gpointer data) {
	g_free(key);
	g_free(val);
	return TRUE;
}

static gboolean jabber_destroy_buddy_hash(gpointer key, gpointer val, gpointer data) {
	struct jabber_buddy_data *jbd = val;
	while (jbd->resources) {
		g_free(((jab_res_info) ((GSList *)jbd->resources)->data)->name);
		if(((jab_res_info) ((GSList *)jbd->resources)->data)->away_msg)
			g_free(((jab_res_info) ((GSList *)jbd->resources)->data)->away_msg);
		g_free(((GSList *)jbd->resources)->data);
		jbd->resources = g_slist_remove(jbd->resources, ((GSList *)jbd->resources)->data);

	}
	if(jbd->error_msg)
		g_free(jbd->error_msg);
	g_free(key);
	g_free(jbd);
	return TRUE;
}


static gboolean jabber_free(gpointer data)
{
	struct jabber_data *jd = data;

	if(jd->gjc != NULL) {
		g_free(jd->gjc->sid);
		gjab_delete(jd->gjc);
		jd->gjc = NULL;
	}
	g_free(jd);

	return FALSE;
}

static void jabber_close(struct gaim_connection *gc)
{
	struct jabber_data *jd = gc->proto_data;

	if(jd) {
		GSList *jcs = jd->chats;

		/* Free-up the jabber_chat struct allocs and the list */
		while (jcs) {
			gaim_jid_free(((struct jabber_chat *)jcs->data)->gjid);
			g_free(jcs->data);
			jcs = jcs->next;
		}
		g_slist_free(jd->chats);

		/* Free-up the buddy data hash */
		if(jd->buddies != NULL)
		{
			g_hash_table_foreach_remove(jd->buddies, jabber_destroy_buddy_hash, NULL);
			g_hash_table_destroy(jd->buddies);
			jd->buddies = NULL;
		}

		/* Free-up the pending queries memories and the list */
		if(jd->gjc != NULL && jd->gjc->queries != NULL) {
			g_hash_table_foreach_remove(jd->gjc->queries, jabber_destroy_hash, NULL);
			g_hash_table_destroy(jd->gjc->queries);
			jd->gjc->queries = NULL;
		}
	}
	if (gc->inpa)
		gaim_input_remove(gc->inpa);

	if(jd) {
		g_timeout_add(0, jabber_free, jd);
		if(jd->gjc != NULL)
			xmlnode_free(jd->gjc->current);
	}
	gc->proto_data = NULL;
}

static int jabber_send_typing(struct gaim_connection *gc, char *who, int typing)
{
	xmlnode x, y;
	char *realwho;
	gjconn gjc = ((struct jabber_data *)gc->proto_data)->gjc;
	jab_res_info jri = jabber_find_resource(gc, who);

	if(!jri || !jri->has_composing)
		return 0;

	if((realwho = get_realwho(gjc, who, FALSE, NULL)) == NULL)
		return 0;

	x = xmlnode_new_tag("message");
	xmlnode_put_attrib(x, "to", realwho);
	xmlnode_insert_tag(x, "gaim");

	y = xmlnode_insert_tag(x, "x");
	xmlnode_put_attrib(y, "xmlns", "jabber:x:event");

	if(typing == TYPING)
		xmlnode_insert_tag(y, "composing");

	gjab_send(((struct jabber_data *)gc->proto_data)->gjc, x);
	xmlnode_free(x);
	g_free(realwho);
	return JABBER_TYPING_NOTIFY_INT;
}

static int jabber_send_im(struct gaim_connection *gc, char *who, char *message, int len, int flags)
{
	xmlnode x, y;
	char *realwho;
	char *thread_id = NULL;
	gjconn gjc = ((struct jabber_data *)gc->proto_data)->gjc;

	if (!who || !message)
		return 0;

	if((realwho = get_realwho(gjc, who, FALSE, NULL)) == NULL)
		return 0;

	x = xmlnode_new_tag("message");
	xmlnode_put_attrib(x, "to", realwho);

	thread_id = jabber_get_convo_thread(gjc, realwho);
	if(thread_id)
	{
		if(strcmp(thread_id, "")) {
			y = xmlnode_insert_tag(x, "thread");
			xmlnode_insert_cdata(y, thread_id, -1);
		}
		g_free(thread_id);
	}

	g_free(realwho);

	xmlnode_insert_tag(x, "gaim");
	xmlnode_put_attrib(x, "type", "chat");

	/* let other clients know we support typing notification */
	y = xmlnode_insert_tag(x, "x");
	xmlnode_put_attrib(y, "xmlns", "jabber:x:event");
	xmlnode_insert_tag(y, "composing");

	if (message && strlen(message)) {
		y = xmlnode_insert_tag(x, "body");
		xmlnode_insert_cdata(y, message, -1);
	}

	gjab_send(((struct jabber_data *)gc->proto_data)->gjc, x);
	xmlnode_free(x);
	return 1;
}

/*
 * Add/update buddy's roster entry on server
 *
 * If "alias" or "group" are NULL, gets them from Gaim's current buddylist values
 * for the buddy.
 */
static void jabber_roster_update(struct gaim_connection *gc, const char *name, const char *alias, const char *group)
{
	xmlnode x, y;
	char *realwho;
	gjconn gjc;
	struct buddy *buddy = NULL;
	struct group *buddy_group = NULL;
	const char *my_alias = NULL;
	const char *my_group = NULL;

	if(gc && gc->proto_data && ((struct jabber_data *)gc->proto_data)->gjc && name) {
		gaim_jid gjid;
		gjc = ((struct jabber_data *)gc->proto_data)->gjc;

		if((realwho = get_realwho(gjc, name, FALSE, &gjid)) == NULL)
			return;

		/* FIXME: transport */
		if(gjid->user == NULL) {
			g_free(realwho);
			gaim_jid_free(gjid);
			return;
		}
		gaim_jid_free(gjid);

		x = jutil_iqnew(JPACKET__SET, NS_ROSTER);
		y = xmlnode_insert_tag(xmlnode_get_tag(x, "query"), "item");
		xmlnode_put_attrib(y, "jid", realwho);

		/*
		 * See if there's an explict (new?) alias for the buddy or we can pull
		 * one out of current Gaim buddylist data for him.
		 */
		if(alias && alias[0] != '\0') {
			my_alias = alias;
		} else if((buddy = find_buddy(gc, realwho)) != NULL && buddy->alias[0]) {
			my_alias = buddy->alias;
		}

		/* If there's an alias for the buddy, it's not 0-length
		 * and it doesn't match his JID, add the "name" attribute.
		 */
		if(my_alias != NULL && my_alias[0] != '\0' && strcmp(realwho, my_alias))
		{
			xmlnode_put_attrib(y, "name", my_alias);
		}

		/*
		 * See if there's an explict (new?) group for the buddy or pull
		 * one out of current Gaim buddylist data for him.
		 */
		if(group && group[0] != '\0') {
			my_group = group;
		} else if((buddy_group = find_group_by_buddy(gc, realwho)) != NULL) {
			my_group = buddy_group->name;
		}

		/*
		 * Send what group the buddy's in along with the roster item.
		 */
		if(my_group != NULL && my_group[0] != '\0') {
			xmlnode z = xmlnode_insert_tag(y, "group");
			xmlnode_insert_cdata(z, my_group, -1);
		}

		gjab_send(((struct jabber_data *)gc->proto_data)->gjc, x);

		xmlnode_free(x);
		g_free(realwho);
	}
}

/*
 * Add/update buddy's alias on server
 *
 * This is just a roster update using existing, local buddylist data
 */
static void jabber_alias_buddy(struct gaim_connection *gc, char *name)
{
	jabber_roster_update(gc, name, NULL, NULL);
}

/*
 * Change buddy's group on server roster
 */
static void jabber_group_change(struct gaim_connection *gc, const char *name, const char *old_group, const char *new_group)
{
	if(old_group && new_group && strcmp(old_group, new_group))
		jabber_roster_update(gc, name, NULL, new_group);
}

/*
 * Group rename
 *
 * Jabber doesn't have "groups," per se.  "Group" is simply a JID attribute.
 * So we iterate through the list of buddies that are in the group and change
 * the group attribute for each of them.
 */
static void jabber_rename_group(struct gaim_connection *gc,
				const char *old_group,
				const char *new_group,
				GList *members)
{
	if(old_group && new_group && strcmp(old_group, new_group))
		while(members) {
			jabber_group_change(gc, (char *)(members->data), old_group, new_group);
			members = members->next;
		}
}

static void jabber_add_buddy(struct gaim_connection *gc, const char *name)
{
	xmlnode x;
	char *realwho;
	gjconn gjc = ((struct jabber_data *)gc->proto_data)->gjc;
	gaim_jid gjid;

	if (!((struct jabber_data *)gc->proto_data)->did_import)
		return;

	/*
	 * If there's no name or the name is ourself
	 */
	if(!name || !strcmp(gc->username, name))
		return;

	if((realwho = get_realwho(gjc, name, FALSE, &gjid)) == NULL) {
		char *msg = g_strdup_printf(_("The user %s is an invalid Jabber I.D. and was "
					      "therefore not added."),  name);
		do_error_dialog("Unable to add buddy.", _("Jabber Error"), GAIM_ERROR);
		g_free(msg);
		jabber_remove_gaim_buddy(gc, name);
		return;
	}

	/* FIXME: transport */
	if(gjid->user == NULL) {
		g_free(realwho);
		gaim_jid_free(gjid);
		return;
	}
	gaim_jid_free(gjid);

	x = xmlnode_new_tag("presence");
	xmlnode_put_attrib(x, "to", realwho);
	xmlnode_put_attrib(x, "type", "subscribe");
	gjab_send(((struct jabber_data *)gc->proto_data)->gjc, x);
	xmlnode_free(x);

	jabber_roster_update(gc, realwho, NULL, NULL);

	g_free(realwho);
}

static void jabber_remove_buddy(struct gaim_connection *gc, char *name, char *group)
{
	xmlnode x;
	char *realwho;
	gjconn gjc = ((struct jabber_data *)gc->proto_data)->gjc;

	if(!name || (realwho = get_realwho(gjc, name, FALSE, NULL)) == NULL)
		return;

	x = xmlnode_new_tag("presence");
	xmlnode_put_attrib(x, "to", realwho);
	xmlnode_put_attrib(x, "type", "unsubscribe");
	gjab_send(((struct jabber_data *)gc->proto_data)->gjc, x);
	g_free(realwho);
	xmlnode_free(x);
}

/*
 * Remove a buddy item from the roster entirely
 */
static void jabber_remove_buddy_roster_item(struct gaim_connection *gc, char *name)
{
	gjconn gjc = ((struct jabber_data *)gc->proto_data)->gjc;
	char *realwho;

	if((realwho = get_realwho(gjc, name, FALSE, NULL)) != NULL) {
		xmlnode x = jutil_iqnew(JPACKET__SET, NS_ROSTER);
		xmlnode y = xmlnode_insert_tag(xmlnode_get_tag(x, "query"), "item");
		xmlnode_put_attrib(y, "jid", realwho);
		xmlnode_put_attrib(y, "subscription", "remove");
		gjab_send(((struct jabber_data *)gc->proto_data)->gjc, x);
		g_free(realwho);
		xmlnode_free(x);
	}
}

/*
 * Unsubscribe a buddy from our presence
 */
static void jabber_unsubscribe_buddy_from_us(struct gaim_connection *gc, char *name)
{
	gjconn gjc = ((struct jabber_data *)gc->proto_data)->gjc;
	char *realwho;

	if((realwho = get_realwho(gjc, name, FALSE, NULL)) != NULL) {
		xmlnode g = xmlnode_new_tag("presence");
		xmlnode_put_attrib(g, "to", realwho);
		xmlnode_put_attrib(g, "type", "unsubscribed");
		gjab_send(gjc, g);
		xmlnode_free(g);
	}
}

/*
 * Common code for setting ourselves invisible/visible to buddy
 */
static void jabber_invisible_to_buddy_common(struct gaim_connection *gc, char *name, gboolean invisible)
{
	gjconn gjc = ((struct jabber_data *)gc->proto_data)->gjc;
	char *realwho;

	if((realwho = get_realwho(gjc, name, FALSE, NULL)) != NULL) {
		struct jabber_buddy_data *jbd = jabber_find_buddy(gc, realwho);
		xmlnode g = xmlnode_new_tag("presence");

		xmlnode_put_attrib(g, "to", realwho);

		if(invisible)
			xmlnode_put_attrib(g, "type", "invisible");

		gjab_send(gjc, g);

		g_free(realwho);
		xmlnode_free(g);

		if(jbd) {
			if(invisible) {
				jbd->invisible |= JABBER_BUD_INVIS;
			} else {
				jbd->invisible &= ~JABBER_BUD_INVIS;
			}
		}
	}
}

/*
 * Make ourselves temporarily invisible to a buddy
 */
static void jabber_invisible_to_buddy(struct gaim_connection *gc, char *name)
{
	jabber_invisible_to_buddy_common(gc, name, TRUE);
}

/*
 * Make ourselves visible to a buddy
 */
static void jabber_visible_to_buddy(struct gaim_connection *gc, char *name)
{
	jabber_invisible_to_buddy_common(gc, name, FALSE);
}

/*
 * Function used by the g_hash_table_foreach() in invisible_to_all_buddies() to
 * actually set the status.
 *
 * key is unused
 * value is the pointer to the jabber_buddy_data struct
 * data is gboolean: TRUE (invisible) or FALSE (not invisible)
 */
static void set_invisible_to_buddy_status(gpointer key, gpointer val, gpointer data) {
	struct jabber_buddy_data *jbd = val;
	gboolean invisible = (gboolean) data;

	if(jbd) {
		if(invisible) {
			jbd->invisible = JABBER_SERV_INVIS | JABBER_BUD_INVIS;
		} else {
			/*
			 * If we've asserted server-level invisibility, cancelling
			 * it removes explicit buddy invisibility settings too.
			 */
			if(jbd->invisible & JABBER_SERV_INVIS)
				jbd->invisible = JABBER_NOT_INVIS;
		}
	}
}

/*
 * Show we've set ourselves invisible/visible to all buddies on the server
 *
 * Used when we set server-wide invisibility so that individual buddy menu
 * entries show the proper option.
 */
static void invisible_to_all_buddies(struct gaim_connection *gc, gboolean invisible)
{
	struct jabber_data *jd = gc->proto_data;

	if(jd->buddies != NULL)
		g_hash_table_foreach(jd->buddies, set_invisible_to_buddy_status, (gpointer) invisible);
}

static char **jabber_list_icon(int uc)
{
	switch (uc) {
	case UC_AWAY:
		return available_away_xpm;
	case UC_CHAT:
		return available_chat_xpm;
	case UC_XA:
		return available_xa_xpm;
	case UC_DND:
		return available_dnd_xpm;
	case UC_ERROR:
		return available_error_xpm;
	default:
		return available_xpm;
	}
}

static GList *jabber_chat_info(struct gaim_connection *gc)
{
	gjconn gjc = ((struct jabber_data *)gc->proto_data)->gjc;

	static char *confserv = NULL;	/* this pointer must be persistent */
	gchar *server;

	GList *m = NULL;
	struct proto_chat_entry *pce;

	/* This is a scientific wild-ass guess...
	 *
	 * If there are more than two "components" to the current server name,
	 * lop-off the left-most component and replace with "conference."
	 */
	if(confserv != NULL) {
		g_free(confserv);	/* dispose of the old value */
	}

	if((server = g_strdup(gjc->user->server)) == NULL) {
		confserv = g_strdup(DEFAULT_GROUPCHAT);
	} else {
		gchar **splits, **index;
		gchar *tmp;
		int cnt = 0;


		index = splits = g_strsplit(server, ".", -1);	/* split the connected server */

		while(*(index++))	/* index to the end--counting the parts */
			++cnt;

		/*
		 * If we've more than two parts, point to the second part.  Else point
		 * to the start.
		 */
		if(cnt > 2) {
			index -= cnt;
		} else {
			index = splits;
		}

		/* Put it together */
		confserv = g_strjoin(".", "conference", (tmp = g_strjoinv(".", index)), NULL);

		g_free(server);		/* we don't need this stuff no more */
		g_free(tmp);
		g_strfreev(splits);
	}

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("Room:");
	m = g_list_append(m, pce);

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("Server:");
	pce->def = confserv;
	m = g_list_append(m, pce);

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("Handle:");
	pce->def = gjc->user->user;
	m = g_list_append(m, pce);

	return m;
}

static void jabber_join_chat(struct gaim_connection *gc, GList *data)
{
	xmlnode x;
	char *realwho;
	gjconn gjc = ((struct jabber_data *)gc->proto_data)->gjc;
	GSList *jcs = ((struct jabber_data *)gc->proto_data)->chats;
	struct jabber_chat *jc;
	gaim_jid gjid;

	if (!data || !data->next || !data->next->next)
		return;

	realwho = create_valid_jid(data->data, data->next->data, data->next->next->data);
	debug_printf("%s\n", realwho);

	if((gjid = gaim_jid_new(realwho)) == NULL) {
		char *msg = g_strdup_printf("The Jabber I.D. %s is invalid.", realwho);
		do_error_dialog(_("Unable to join chat"), msg, GAIM_ERROR);
		g_free(msg);
		g_free(realwho);
		return;
	}

	if((jc = find_any_chat(gc, gjid)) != NULL) {
		switch(jc->state) {
			case JCS_PENDING:
				debug_printf("attempt to re-join already pending Jabber chat! (ignoring)\n");
				g_free(realwho);	/* yuck! */
				gaim_jid_free(gjid);
				return;
			case JCS_ACTIVE:
				debug_printf("attempt to re-join already active Jabber chat! (ignoring)\n");
				g_free(realwho);	/* yuck! */
				gaim_jid_free(gjid);
				return;
			case JCS_CLOSED:
				debug_printf("rejoining previously closed Jabber chat\n");
				break;
			default:
				debug_printf("found Jabber chat in unknown state! (ignoring)\n");
				g_free(realwho);	/* yuck! */
				gaim_jid_free(gjid);
				return;
		}
	} else {
		debug_printf("joining completely new Jabber chat\n");
		jc = g_new0(struct jabber_chat, 1);
		jc->gjid = gjid;
		jc->gc = gc;
		((struct jabber_data *)gc->proto_data)->chats = g_slist_append(jcs, jc);
		add_buddy(gc, _("Chats"), realwho, realwho);
	}

	jc->state = JCS_PENDING;

	x = jutil_presnew(0, realwho, NULL);
	gjab_send(gjc, x);
	xmlnode_free(x);
	g_free(realwho);
}

static void jabber_chat_invite(struct gaim_connection *gc, int id, const char *message, const char *name)
{
	xmlnode x, y;
	struct jabber_data *jd = gc->proto_data;
	gjconn gjc = jd->gjc;
	struct jabber_chat *jc = NULL;
	char *realwho, *subject;

	if(!name || (realwho = get_realwho(gjc, name, FALSE, NULL)) == NULL)
		return;

	/* find which chat we're inviting to */
	if(jabber_find_chat_by_convo_id(gc, id, &jc) != 0)
		return;

	x = xmlnode_new_tag("message");
	xmlnode_put_attrib(x, "to", realwho);

	g_free(realwho);

	y = xmlnode_insert_tag(x, "x");
	xmlnode_put_attrib(y, "xmlns", "jabber:x:conference");
	subject = g_strdup_printf("%s@%s", jc->gjid->user, jc->gjid->server);
	xmlnode_put_attrib(y, "jid", subject);
	g_free(subject);

	if (message && strlen(message)) {
		y = xmlnode_insert_tag(x, "body");
		xmlnode_insert_cdata(y, message, -1);
	}

	gjab_send(((struct jabber_data *)gc->proto_data)->gjc, x);
	xmlnode_free(x);
}

static void jabber_chat_leave(struct gaim_connection *gc, int id)
{
	struct jabber_data *jd = gc->proto_data;
	gjconn gjc = jd->gjc;
	struct jabber_chat *jc = NULL;
	char *chatname;
	xmlnode x;

	/* Find out which chat we're leaving */
	if(jabber_find_chat_by_convo_id(gc, id, &jc) != 0)
		return;

	chatname = g_strdup_printf("%s@%s", jc->gjid->user, jc->gjid->server);
	x = jutil_presnew(0, chatname, NULL);
	g_free(chatname);
	xmlnode_put_attrib(x, "type", "unavailable");
	gjab_send(gjc, x);
	xmlnode_free(x);
	jc->b = NULL;
}

static int jabber_chat_send(struct gaim_connection *gc, int id, char *message)
{
	xmlnode x, y;
	struct jabber_chat *jc = NULL;
	char *chatname;
	int retval = 0;

	/* Find out which chat we're sending to */
	if((retval = jabber_find_chat_by_convo_id(gc, id, &jc)) != 0)
		return(retval);

	x = xmlnode_new_tag("message");
	xmlnode_put_attrib(x, "from", jc->gjid->full);
	chatname = g_strdup_printf("%s@%s", jc->gjid->user, jc->gjid->server);
	xmlnode_put_attrib(x, "to", chatname);
	g_free(chatname);
	xmlnode_put_attrib(x, "type", "groupchat");

	if (message && strlen(message) > strlen("/topic ") &&
			!g_strncasecmp(message, "/topic ", strlen("/topic "))) {
		char buf[8192];
		y = xmlnode_insert_tag(x, "subject");
		xmlnode_insert_cdata(y, message + strlen("/topic "), -1);
		y = xmlnode_insert_tag(x, "body");
		g_snprintf(buf, sizeof(buf), "/me has changed the subject to: %s", message + strlen("/topic"));
		xmlnode_insert_cdata(y, buf, -1);
	} else if (message && strlen(message)) {
		y = xmlnode_insert_tag(x, "body");
		xmlnode_insert_cdata(y, message, -1);
	}

	gjab_send(((struct jabber_data *)gc->proto_data)->gjc, x);
	xmlnode_free(x);
	return 0;
}

static void jabber_chat_whisper(struct gaim_connection *gc, int id, char *who, char *message)
{
	xmlnode x, y;
	struct jabber_chat *jc = NULL;
	char *chatname;

	/* Find out which chat we're whispering to */
	if(jabber_find_chat_by_convo_id(gc, id, &jc) != 0)
		return;

	x = xmlnode_new_tag("message");
	xmlnode_put_attrib(x, "from", jc->gjid->full);
	chatname = g_strdup_printf("%s@%s/%s", jc->gjid->user, jc->gjid->server, who);
	xmlnode_put_attrib(x, "to", chatname);
	g_free(chatname);
	xmlnode_put_attrib(x, "type", "normal");

	if (message && strlen(message)) {
		y = xmlnode_insert_tag(x, "body");
		xmlnode_insert_cdata(y, message, -1);
	}

	gjab_send(((struct jabber_data *)gc->proto_data)->gjc, x);
	xmlnode_free(x);
}

static char *jabber_normalize(const char *s)
{
	static char buf[BUF_LEN];
	char *t, *u;
	int x = 0;

	g_return_val_if_fail((s != NULL), NULL);

	/* Somebody called us with s == NULL once... */
	if(s == NULL) {
		return(NULL);
	} else {
		u = t = g_strdup(s);

		g_strdown(t);

		while (*t && (x < BUF_LEN - 1)) {
			if (*t != ' ')
				buf[x++] = *t;
			t++;
		}
		buf[x] = '\0';
		g_free(u);

		if (!strchr(buf, '@')) {
			strcat(buf, "@jabber.org"); /* this isn't always right, but eh */
		} else if ((u = strchr(strchr(buf, '@'), '/')) != NULL) {
			*u = '\0';
		}

		return buf;
	}
}

static void jabber_get_info(struct gaim_connection *gc, char *who) {
	xmlnode x;
	char *id;
	char *realwho;
	struct jabber_data *jd = gc->proto_data;
	gjconn gjc = jd->gjc;

	if((realwho = get_realwho(gjc, who, TRUE, NULL)) == NULL)
		return;

	x = jutil_iqnew(JPACKET__GET, NS_VCARD);
	xmlnode_put_attrib(x, "to", realwho);

	g_free(realwho);

	id = gjab_getid(gjc);
	xmlnode_put_attrib(x, "id", id);

	g_hash_table_insert(jd->gjc->queries, g_strdup(id), g_strdup("vCard"));

	gjab_send(gjc, x);

	xmlnode_free(x);
}

static void jabber_get_error_msg(struct gaim_connection *gc, char *who) {
	struct jabber_data *jd = gc->proto_data;
	gjconn gjc = jd->gjc;
	gchar **str_arr = (gchar **) g_new(gpointer, 3);
	gchar **ap = str_arr;
	gchar *realwho, *final;
	struct jabber_buddy_data *jbd;

	if((realwho = get_realwho(gjc, who, FALSE, NULL)) == NULL) {
		g_strfreev(str_arr);
		return;
	}

	jbd = jabber_find_buddy(gc, realwho);

	*ap++ = g_strdup_printf("<B>Jabber ID:</B> %s<BR>\n", realwho);
	*ap++ = g_strdup_printf("<B>Error:</B> %s<BR>\n", jbd->error_msg);
	*ap = NULL;

	final= g_strjoinv(NULL, str_arr);

	g_strfreev(str_arr);

	g_show_info_text(gc, realwho, 2, final, NULL);
	g_free(realwho);
	g_free(final);
}

static void jabber_get_away_msg(struct gaim_connection *gc, char *who) {
	struct jabber_data *jd = gc->proto_data;
	gjconn gjc = jd->gjc;
	int num_resources;
	gaim_jid gjid;
	char *buddy = get_realwho(gjc, who, FALSE, &gjid);
	struct jabber_buddy_data *jbd = jabber_find_buddy(gc, buddy);
	gchar **str_arr;
	gchar **ap;
	gchar *realwho, *final;
	GSList *resources;
	int i;

	if(!buddy)
		return;

	if(!gjid->resource) {
		num_resources = g_slist_length(jbd->resources);
		resources = jbd->resources;
	} else {
		num_resources = 1;
		resources = jbd->resources;
		while(strcasecmp(((jab_res_info)resources->data)->name, gjid->resource))
			resources = resources->next;
	}

	gaim_jid_free(gjid);

	/* space for all elements: Jabber I.D. + "status" + NULL (list terminator) */
	str_arr = (gchar **) g_new(gpointer, num_resources*2 + 1);
	ap = str_arr;

	for(i=0; i<num_resources; i++)
	{
		jab_res_info jri = resources->data;
		realwho = g_strdup_printf("%s/%s", buddy, jri->name);
		*ap++ = g_strdup_printf("<B>Jabber ID:</B> %s<BR>\n", realwho);
		*ap++ = g_strdup_printf("<B>Status:</B> %s<BR>\n", jabber_lookup_away(gjc, realwho));
		g_free(realwho);
		resources = resources->next;
	}

	*ap = NULL;

	g_free(buddy);

	final= g_strjoinv(NULL, str_arr);
	g_strfreev(str_arr);

	g_show_info_text(gc, who, 2, final, NULL);
	g_free(final);

}

static void jabber_get_cb_info(struct gaim_connection *gc, int cid, char *who) {
	struct jabber_chat *jc = NULL;
	char *realwho;

	/* Find out which chat */
	if(jabber_find_chat_by_convo_id(gc, cid, &jc) != 0)
		return;

	realwho = g_strdup_printf("%s@%s/%s", jc->gjid->user, jc->gjid->server, who);

	jabber_get_info(gc, realwho);
	g_free(realwho);
}

static void jabber_get_cb_away_msg(struct gaim_connection *gc, int cid, char *who) {
	struct jabber_chat *jc = NULL;
	char *realwho;

	/* Find out which chat */
	if(jabber_find_chat_by_convo_id(gc, cid, &jc) != 0)
		return;

	realwho = g_strdup_printf("%s@%s/%s", jc->gjid->user, jc->gjid->server, who);

	jabber_get_away_msg(gc, realwho);
	g_free(realwho);

}

static GList *jabber_buddy_menu(struct gaim_connection *gc, char *who) {
	GList *m = NULL;
	struct proto_buddy_menu *pbm;
	struct buddy *b = find_buddy(gc, who);

	if(b->uc == UC_ERROR)
	{
		pbm = g_new0(struct proto_buddy_menu, 1);
		pbm->label = _("View Error Msg");
		pbm->callback = jabber_get_error_msg;
		pbm->gc = gc;
		m = g_list_append(m, pbm);
	} else {
		gjconn gjc = ((struct jabber_data *)gc->proto_data)->gjc;
		char *realwho = get_realwho(gjc, who, FALSE, NULL);
		struct jabber_buddy_data *jbd = jabber_find_buddy(gc, realwho);

		g_free(realwho);

		pbm = g_new0(struct proto_buddy_menu, 1);
		pbm->label = _("Get Info");
		pbm->callback = jabber_get_info;
		pbm->gc = gc;
		m = g_list_append(m, pbm);
		pbm = g_new0(struct proto_buddy_menu, 1);
		pbm->label = _("Get Away Msg");
		pbm->callback = jabber_get_away_msg;
		pbm->gc = gc;
		m = g_list_append(m, pbm);

		pbm = g_new0(struct proto_buddy_menu, 1);
		if(jbd && (jbd->invisible & JABBER_BUD_INVIS)) {
			pbm->label = _("Un-hide From");
			pbm->callback = jabber_visible_to_buddy;
		} else {
			pbm->label = _("Temporarily Hide From");
			pbm->callback = jabber_invisible_to_buddy;
		}
		pbm->gc = gc;
		m = g_list_append(m, pbm);
	}

	return m;
}

/*
 * Jabber protocol-specific "edit buddy menu" item(s)
 */
static GList *jabber_edit_buddy_menu(struct gaim_connection *gc, char *who) {
	GList *m = NULL;
	struct proto_buddy_menu *pbm;

	pbm = g_new0(struct proto_buddy_menu, 1);
	pbm->label = _("Get Info");
	pbm->callback = jabber_get_info;
	pbm->gc = gc;
	m = g_list_append(m, pbm);
	pbm = g_new0(struct proto_buddy_menu, 1);
	pbm->label = _("Remove From Roster");
	pbm->callback = jabber_remove_buddy_roster_item;
	pbm->gc = gc;
	m = g_list_append(m, pbm);
	pbm = g_new0(struct proto_buddy_menu, 1);
	pbm->label = _("Cancel Presence Notification");
	pbm->callback = jabber_unsubscribe_buddy_from_us;
	pbm->gc = gc;
	m = g_list_append(m, pbm);

	return m;
}

static GList *jabber_away_states(struct gaim_connection *gc) {
	GList *m = NULL;

	m = g_list_append(m, "Online");
	m = g_list_append(m, "Chatty");
	m = g_list_append(m, "Away");
	m = g_list_append(m, "Extended Away");
	m = g_list_append(m, "Do Not Disturb");
	m = g_list_append(m, "Invisible");
	m = g_list_append(m, GAIM_AWAY_CUSTOM);

	return m;
}

static void jabber_set_away(struct gaim_connection *gc, char *state, char *message)
{
	xmlnode x, y;
	struct jabber_data *jd = gc->proto_data;
	gjconn gjc = jd->gjc;
	GSList *jcs;
	struct jabber_chat *jc;
	char *chatname;
	gboolean invisible = FALSE;

	if (gc->away) {
		g_free(gc->away);
		gc->away = NULL;
	}

	x = xmlnode_new_tag("presence");

	if (!strcmp(state, GAIM_AWAY_CUSTOM)) {
		/* oh goody. Gaim is telling us what to do. */
		if (message) {
			/* Gaim wants us to be away */
			y = xmlnode_insert_tag(x, "show");
			xmlnode_insert_cdata(y, "away", -1);
			y = xmlnode_insert_tag(x, "status");
			xmlnode_insert_cdata(y, message, -1);
			gc->away = g_strdup(message);
		} else {
			/* Gaim wants us to not be away */
			/* but for Jabber, we can just send presence with no other information. */
		}
	} else {
		/* state is one of our own strings. it won't be NULL. */
		if (!strcmp(state, "Online")) {
			/* once again, we don't have to put anything here */
		} else if (!strcmp(state, "Chatty")) {
			y = xmlnode_insert_tag(x, "show");
			xmlnode_insert_cdata(y, "chat", -1);
			gc->away = g_strdup("");
		} else if (!strcmp(state, "Away")) {
			y = xmlnode_insert_tag(x, "show");
			xmlnode_insert_cdata(y, "away", -1);
			gc->away = g_strdup("");
		} else if (!strcmp(state, "Extended Away")) {
			y = xmlnode_insert_tag(x, "show");
			xmlnode_insert_cdata(y, "xa", -1);
			gc->away = g_strdup("");
		} else if (!strcmp(state, "Do Not Disturb")) {
			y = xmlnode_insert_tag(x, "show");
			xmlnode_insert_cdata(y, "dnd", -1);
			gc->away = g_strdup("");
		} else if (!strcmp(state, "Invisible")) {
			xmlnode_put_attrib(x, "type", "invisible");
			gc->away = g_strdup("");
			invisible = TRUE;
		}
	}

	gjab_send(gjc, x);	/* Notify "individuals" */

	/*
	 * As of jabberd-1.4.2: simply sending presence to the server doesn't result in
	 * it being propagated to conference rooms.  So we wade thru the list of chats,
	 * sending our new presence status to each and every one.
	 */
	for(jcs = jd->chats; jcs; jcs = jcs->next) {
		jc = jcs->data;
		if(jc->state == JCS_ACTIVE) {
			xmlnode_put_attrib(x, "from", jc->gjid->full);
			chatname = g_strdup_printf("%s@%s", jc->gjid->user, jc->gjid->server);
			xmlnode_put_attrib(x, "to", chatname);
			gjab_send(gjc, x);
			g_free(chatname);
		}
	}

	xmlnode_free(x);

	invisible_to_all_buddies(gc, invisible);
}

static void jabber_set_idle(struct gaim_connection *gc, int idle) {
	struct jabber_data *jd = (struct jabber_data *)gc->proto_data;
	debug_printf("jabber_set_idle: setting idle %i\n", idle);
	jd->idle = idle ? time(NULL) - idle : idle;
}

static void jabber_keepalive(struct gaim_connection *gc) {
	struct jabber_data *jd = (struct jabber_data *)gc->proto_data;
	gjab_send_raw(jd->gjc, "  \t  ");
}

/*---------------------------------------*/
/* Jabber "set info" (vCard) support     */
/*---------------------------------------*/

/*
 * V-Card format:
 *
 *  <vCard prodid='' version='' xmlns=''>
 *    <FN></FN>
 *    <N>
 *	<FAMILY/>
 *	<GIVEN/>
 *    </N>
 *    <NICKNAME/>
 *    <URL/>
 *    <ADR>
 *	<STREET/>
 *	<EXTADD/>
 *	<LOCALITY/>
 *	<REGION/>
 *	<PCODE/>
 *	<COUNTRY/>
 *    </ADR>
 *    <TEL/>
 *    <EMAIL/>
 *    <ORG>
 *	<ORGNAME/>
 *	<ORGUNIT/>
 *    </ORG>
 *    <TITLE/>
 *    <ROLE/>
 *    <DESC/>
 *    <BDAY/>
 *  </vCard>
 *
 * See also:
 *
 *	http://docs.jabber.org/proto/html/vcard-temp.html
 *	http://www.vcard-xml.org/dtd/vCard-XML-v2-20010520.dtd
 */

/*
 * Cross-reference user-friendly V-Card entry labels to vCard XML tags
 * and attributes.
 *
 * Order is (or should be) unimportant.  For example: we have no way of
 * knowing in what order real data will arrive.
 *
 * Format: Label, Pre-set text, "visible" flag, "editable" flag, XML tag
 *         name, XML tag's parent tag "path" (relative to vCard node).
 *
 *         List is terminated by a NULL label pointer.
 *
 *	   Entries with no label text, but with XML tag and parent tag
 *	   entries, are used by V-Card XML construction routines to
 *	   "automagically" construct the appropriate XML node tree.
 *
 * Thoughts on future direction/expansion
 *
 *	This is a "simple" vCard.
 *
 *	It is possible for nodes other than the "vCard" node to have
 *      attributes.  Should that prove necessary/desirable, add an
 *      "attributes" pointer to the vcard_template struct, create the
 *      necessary tag_attr structs, and add 'em to the vcard_dflt_data
 *      array.
 *
 *	The above changes will (obviously) require changes to the vCard
 *      construction routines.
 */

struct vcard_template {
	char *label;			/* label text pointer */
	char *text;			/* entry text pointer */
	int  visible;			/* should entry field be "visible?" */
	int  editable;			/* should entry field be editable? */
	char *tag;			/* tag text */
	char *ptag;			/* parent tag "path" text */
	char *url;			/* vCard display format if URL */
} vcard_template_data[] = {
	{N_("Full Name"),          NULL, TRUE, TRUE, "FN",        NULL,  NULL},
	{N_("Family Name"),        NULL, TRUE, TRUE, "FAMILY",    "N",   NULL},
	{N_("Given Name"),         NULL, TRUE, TRUE, "GIVEN",     "N",   NULL},
	{N_("Nickname"),           NULL, TRUE, TRUE, "NICKNAME",  NULL,  NULL},
	{N_("URL"),                NULL, TRUE, TRUE, "URL",       NULL,  "<A HREF=\"%s\">%s</A>"},
	{N_("Street Address"),     NULL, TRUE, TRUE, "STREET",    "ADR", NULL},
	{N_("Extended Address"),   NULL, TRUE, TRUE, "EXTADD",    "ADR", NULL},
	{N_("Locality"),           NULL, TRUE, TRUE, "LOCALITY",  "ADR", NULL},
	{N_("Region"),             NULL, TRUE, TRUE, "REGION",    "ADR", NULL},
	{N_("Postal Code"),        NULL, TRUE, TRUE, "PCODE",     "ADR", NULL},
	{N_("Country"),            NULL, TRUE, TRUE, "COUNTRY",   "ADR", NULL},
	{N_("Telephone"),          NULL, TRUE, TRUE, "TELEPHONE", NULL,  NULL},
	{N_("Email"),              NULL, TRUE, TRUE, "EMAIL",     NULL,  "<A HREF=\"mailto:%s\">%s</A>"},
	{N_("Organization Name"),  NULL, TRUE, TRUE, "ORGNAME",   "ORG", NULL},
	{N_("Organization Unit"),  NULL, TRUE, TRUE, "ORGUNIT",   "ORG", NULL},
	{N_("Title"),              NULL, TRUE, TRUE, "TITLE",     NULL,  NULL},
	{N_("Role"),               NULL, TRUE, TRUE, "ROLE",      NULL,  NULL},
	{N_("Birthday"),           NULL, TRUE, TRUE, "BDAY",      NULL,  NULL},
	{N_("Description"),        NULL, TRUE, TRUE, "DESC",      NULL,  NULL},
	{"", NULL, TRUE, TRUE, "N",     NULL, NULL},
	{"", NULL, TRUE, TRUE, "ADR",   NULL, NULL},
	{"", NULL, TRUE, TRUE, "ORG",   NULL, NULL},
	{NULL, NULL, 0, 0, NULL, NULL, NULL}
};

/*
 * The "vCard" tag's attibute list...
 */
struct tag_attr {
	char *attr;
	char *value;
} vcard_tag_attr_list[] = {
	{"prodid",   "-//HandGen//NONSGML vGen v1.0//EN"},
	{"version",  "2.0",                             },
	{"xmlns",    "vcard-temp",                      },
	{NULL, NULL},
};


/*
 * V-Card user instructions
 */
static char *multi_entry_instructions =
	N_("All items below are optional. Enter only the information with which you feel comfortable");
static char *entries_title = N_("User Identity");

/*
 * Used by routines to parse an XML-encoded string into an xmlnode tree
 */
typedef struct {
	XML_Parser parser;
	xmlnode current;
} *xmlstr2xmlnode_parser, xmlstr2xmlnode_parser_struct;


/*
 * Display a Jabber vCard
 */
static void jabber_handlevcard(gjconn gjc, xmlnode querynode, char *from)
{
	struct gaim_connection *gc = GJ_GC(gjc);
	char *cdata, *status;
	struct vcard_template *vc_tp = vcard_template_data;

	/* space for all vCard elements + Jabber I.D. + "status" + NULL (list terminator) */
	gchar **str_arr = (gchar **) g_new(gpointer,
				(sizeof(vcard_template_data)/sizeof(struct vcard_template)) + 3);
	gchar **ap = str_arr;
	gchar *buddy, *final;

	if((buddy = get_realwho(gjc, from, TRUE, NULL)) == NULL) {
		g_strfreev(str_arr);
		return;
	}

	*ap++ = g_strdup_printf("<B>Jabber ID:</B> %s<BR>\n", buddy);

	for(vc_tp = vcard_template_data; vc_tp->label != NULL; ++vc_tp) {
		if(strcmp(vc_tp->tag, "DESC") == 0)
			continue;	/* special handling later */
		if(vc_tp->ptag == NULL) {
			cdata = xmlnode_get_tag_data(querynode, vc_tp->tag);
		} else {
			gchar *tag = g_strdup_printf("%s/%s", vc_tp->ptag, vc_tp->tag);
			cdata = xmlnode_get_tag_data(querynode, tag);
			g_free(tag);
		}
		if(cdata != NULL) {
			if(vc_tp->url == NULL) {
				*ap++ = g_strdup_printf("<B>%s:</B> %s<BR>\n", vc_tp->label, cdata);
			} else {
				gchar *fmt = g_strdup_printf("<B>%%s:</B> %s<BR>\n", vc_tp->url);
				*ap++ = g_strdup_printf(fmt, vc_tp->label, cdata, cdata);
				g_free(fmt);
			}
		}
	}

	status = jabber_lookup_away(gjc, buddy);

	*ap++ = g_strdup_printf("<B>Status:</B> %s<BR>\n", status);

	/*
	 * "Description" handled as a special case: get a copy of the
	 * string and HTML-ize.
	 */
	if((cdata = xmlnode_get_tag_data(querynode, "DESC")) != NULL) {
		gchar *tmp = g_strdup_printf("<HR>%s<BR>", cdata);
		*ap++ = strdup_withhtml(tmp);
		g_free(tmp);
	}

	*ap = NULL;

	final= g_strjoinv(NULL, str_arr);
	g_strfreev(str_arr);

	g_show_info_text(gc, buddy, 2, final, NULL);
	g_free(buddy);
	g_free(final);
}

/*
 * Used by XML_Parse on parsing CDATA
 */
static void xmlstr2xmlnode_charData(void *userdata, const char *s, int slen)
{
	xmlstr2xmlnode_parser xmlp = (xmlstr2xmlnode_parser) userdata;

	if (xmlp->current)
		xmlnode_insert_cdata(xmlp->current, s, slen);
}

/*
 * Used by XML_Parse to start or append to an xmlnode
 */
static void xmlstr2xmlnode_startElement(void *userdata, const char *name, const char **attribs)
{
	xmlnode x;
	xmlstr2xmlnode_parser xmlp = (xmlstr2xmlnode_parser) userdata;

	if (xmlp->current) {
		/* Append the node to the current one */
		x = xmlnode_insert_tag(xmlp->current, name);
		xmlnode_put_expat_attribs(x, attribs);

		xmlp->current = x;
	} else {
		x = xmlnode_new_tag(name);
		xmlnode_put_expat_attribs(x, attribs);
		xmlp->current = x;
	}
}

/*
 * Used by XML_Parse to end an xmlnode
 */
static void xmlstr2xmlnode_endElement(void *userdata, const char *name)
{
	xmlstr2xmlnode_parser xmlp = (xmlstr2xmlnode_parser) userdata;
	xmlnode x;

	if (xmlp->current != NULL && (x = xmlnode_get_parent(xmlp->current)) != NULL) {
		xmlp->current = x;
	}
}

/*
 * Parse an XML-encoded string into an xmlnode tree
 *
 * Caller is responsible for freeing the returned xmlnode
 */
static xmlnode xmlstr2xmlnode(char *xmlstring)
{
	xmlstr2xmlnode_parser my_parser = g_new(xmlstr2xmlnode_parser_struct, 1);
	xmlnode x = NULL;

	my_parser->parser = XML_ParserCreate(NULL);
	my_parser->current = NULL;

	XML_SetUserData(my_parser->parser, (void *)my_parser);
	XML_SetElementHandler(my_parser->parser, xmlstr2xmlnode_startElement, xmlstr2xmlnode_endElement);
	XML_SetCharacterDataHandler(my_parser->parser, xmlstr2xmlnode_charData);
	XML_Parse(my_parser->parser, xmlstring, strlen(xmlstring), 0);

	x = my_parser->current;

	XML_ParserFree(my_parser->parser);
	g_free(my_parser);

	return(x);
}

/*
 * Insert a tag node into an xmlnode tree, recursively inserting parent tag
 * nodes as necessary
 *
 * Returns pointer to inserted node
 *
 * Note to hackers: this code is designed to be re-entrant (it's recursive--it
 * calls itself), so don't put any "static"s in here!
 */
static xmlnode insert_tag_to_parent_tag(xmlnode start, const char *parent_tag, const char *new_tag)
{
	xmlnode x = NULL;

	/*
	 * If the parent tag wasn't specified, see if we can get it
	 * from the vCard template struct.
	 */
	if(parent_tag == NULL) {
		struct vcard_template *vc_tp = vcard_template_data;

		while(vc_tp->label != NULL) {
			if(strcmp(vc_tp->tag, new_tag) == 0) {
				parent_tag = vc_tp->ptag;
				break;
			}
			++vc_tp;
		}
	}

	/*
	 * If we have a parent tag...
	 */
	if(parent_tag != NULL ) {
		/*
		 * Try to get the parent node for a tag
		 */
		if((x = xmlnode_get_tag(start, parent_tag)) == NULL) {
			/*
			 * Descend?
			 */
			char *grand_parent = strcpy(g_malloc(strlen(parent_tag) + 1), parent_tag);
			char *parent;

			if((parent = strrchr(grand_parent, '/')) != NULL) {
				*(parent++) = '\0';
				x = insert_tag_to_parent_tag(start, grand_parent, parent);
			} else {
				x = xmlnode_insert_tag(start, grand_parent);
			}
			g_free(grand_parent);
		} else {
			/*
			 * We found *something* to be the parent node.
			 * Note: may be the "root" node!
			 */
			xmlnode y;
			if((y = xmlnode_get_tag(x, new_tag)) != NULL) {
				return(y);
			}
		}
	}

	/*
	 * insert the new tag into its parent node
	 */
	return(xmlnode_insert_tag((x == NULL? start : x), new_tag));
}

/*
 * Find the tag name for a label
 *
 * Returns NULL on not found
 */
static char *tag_for_label(const char *label)
{
	struct vcard_template *vc_tp = vcard_template_data;
	char *p = NULL;

	for(vc_tp = vcard_template_data; vc_tp->label != NULL; ++vc_tp) {
		if(strcmp(label, vc_tp->label) == 0) {
			p = vc_tp->tag;
			break;
		}
	}

	return(p);
}

/*
 * Send vCard info to Jabber server
 */
static void jabber_set_info(struct gaim_connection *gc, char *info)
{
	xmlnode x, vc_node;
	char *id;
	struct jabber_data *jd = gc->proto_data;
	gjconn gjc = jd->gjc;

	x = xmlnode_new_tag("iq");
	xmlnode_put_attrib(x, "type", "set");

	id = gjab_getid(gjc);

	xmlnode_put_attrib(x, "id", id);

	/*
	 * Send only if there's actually any *information* to send
	 */
	if((vc_node = xmlstr2xmlnode(info)) != NULL && xmlnode_get_name(vc_node) != NULL &&
			g_strncasecmp(xmlnode_get_name(vc_node), "vcard", 5) == 0) {
		xmlnode_insert_tag_node(x, vc_node);
		debug_printf("jabber: vCard packet: %s\n", xmlnode2str(x));
		gjab_send(gjc, x);
	}

	xmlnode_free(x);
}

/*
 * This is the callback from the "ok clicked" for "set vCard"
 *
 * Formats GSList data into XML-encoded string and returns a pointer
 * to said string.
 *
 * g_free()'ing the returned string space is the responsibility of
 * the caller.
 */
static gchar *jabber_format_info(MultiEntryDlg *b)
{
	xmlnode vc_node;
	GSList *list;
	MultiEntryData *med;
	MultiTextData *mtd;
	char *p;

	struct tag_attr *tag_attr;

	vc_node = xmlnode_new_tag("vCard");

	for(tag_attr = vcard_tag_attr_list; tag_attr->attr != NULL; ++tag_attr)
		xmlnode_put_attrib(vc_node, tag_attr->attr, tag_attr->value);

	for(list = b->multi_entry_items; list != NULL; list = list->next) {
		med = (MultiEntryData *) list->data;
		if(med->label != NULL && med->text != NULL && (med->text)[0] != '\0') {
			if((p = tag_for_label(med->label)) != NULL) {
				xmlnode xp;
				if((xp = insert_tag_to_parent_tag(vc_node, NULL, p)) != NULL) {
					xmlnode_insert_cdata(xp, med->text, -1);
				}
			}
		}
	}

	for(list = b->multi_text_items; list != NULL; list = list->next) {
		mtd = (MultiTextData *) list->data;
		if(mtd->label != NULL && mtd->text != NULL && (mtd->text)[0] != '\0') {
			if((p = tag_for_label(mtd->label)) != NULL) {
				xmlnode xp;
				if((xp = insert_tag_to_parent_tag(vc_node, NULL, p)) != NULL) {
					xmlnode_insert_cdata(xp, mtd->text, -1);
				}
			}
		}
	}


	p = g_strdup(xmlnode2str(vc_node));
	xmlnode_free(vc_node);

	return(p);
}

/*
 * This gets executed by the proto action
 *
 * Creates a new MultiEntryDlg struct, gets the XML-formatted user_info
 * string (if any) into GSLists for the (multi-entry) edit dialog and
 * calls the set_vcard dialog.
 */
static void jabber_setup_set_info(struct gaim_connection *gc)
{
	MultiEntryData *data;
	const struct vcard_template *vc_tp;
	char *user_info;
	MultiEntryDlg *b = multi_entry_dialog_new();
	char *cdata;
	xmlnode x_vc_data = NULL;
	struct aim_user *tmp = gc->user;
	b->user = tmp;


	/*
	 * Get existing, XML-formatted, user info
	 */
	if((user_info = g_malloc(strlen(tmp->user_info) + 1)) != NULL) {
		strcpy(user_info, tmp->user_info);
		x_vc_data = xmlstr2xmlnode(user_info);
	}

	/*
	 * Set up GSLists for edit with labels from "template," data from user info
	 */
	for(vc_tp = vcard_template_data; vc_tp->label != NULL; ++vc_tp) {
		if((vc_tp->label)[0] == '\0')
			continue;
		if(vc_tp->ptag == NULL) {
			cdata = xmlnode_get_tag_data(x_vc_data, vc_tp->tag);
		} else {
			gchar *tag = g_strdup_printf("%s/%s", vc_tp->ptag, vc_tp->tag);
			cdata = xmlnode_get_tag_data(x_vc_data, tag);
			g_free(tag);
		}
		if(strcmp(vc_tp->tag, "DESC") == 0) {
			multi_text_list_update(&(b->multi_text_items),
				vc_tp->label, cdata, TRUE);
		} else {
			data = multi_entry_list_update(&(b->multi_entry_items),
				vc_tp->label, cdata, TRUE);
			data->visible = vc_tp->visible;
			data->editable = vc_tp->editable;
		}
	}


	if(x_vc_data != NULL) {
		xmlnode_free(x_vc_data);
	} else {
		/*
		 * Early Beta versions had a different user_info storage format--let's
		 * see if that works.
		 *
		 * This goes away RSN.
		 */
		const char *record_separator = "<BR>";
		const char *field_separator = ": ";
		gchar **str_list, **str_list_ptr, **str_list2;

		if((str_list = g_strsplit(user_info, record_separator, 0)) != NULL) {
			for(str_list_ptr = str_list; *str_list_ptr != NULL; ++str_list_ptr) {
				str_list2 = g_strsplit(*str_list_ptr, field_separator, 2);
				if(str_list2[0] != NULL && str_list2[1] != NULL) {
					g_strstrip(str_list2[0]);
					g_strstrip(str_list2[1]);
					/* this is ugly--so far */
					if(strcmp(str_list2[0], "Description") == 0) {
						multi_text_list_update(&(b->multi_text_items),
							str_list2[0], str_list2[1], FALSE);
					} else {
						multi_entry_list_update(&(b->multi_entry_items),
							str_list2[0], str_list2[1], FALSE);
					}
				}
				g_strfreev(str_list2);
			}
			g_strfreev(str_list);
		}
	}

	if(user_info != NULL) {
		g_free(user_info);
	}

	b->title = _("Gaim - Edit Jabber vCard");
	b->role = "set_info";
	b->instructions->text = g_strdup(multi_entry_instructions);
	b->entries_title = g_strdup(entries_title);

	b->custom = (void *) jabber_format_info;

	show_set_vcard(b);
}

/*---------------------------------------*/
/* End Jabber "set info" (vCard) support */
/*---------------------------------------*/

/*----------------------------------------*/
/* Jabber "user registration" support     */
/*----------------------------------------*/

/*
 * Three of the following four functions duplicate much of what
 * exists elsewhere:
 *
 *	jabber_handleregresp()
 *	gjab_reqreg()
 *	jabber_handle_registration_state()
 *
 * It may be that an additional flag could be added to one of
 * the "local" structs and the duplicated code modified to
 * account for it--thus eliminating the duplication.  Then again:
 * doing it the way it is may be much cleaner.
 *
 * TBD: Code to support requesting additional information server
 * wants at registration--incl. dialog.
 */

/*
 * Like jabber_handlepacket(), only different
 */
static void jabber_handleregresp(gjconn gjc, jpacket p)
{
	if (jpacket_subtype(p) == JPACKET__RESULT) {
		xmlnode querynode;

		if((querynode = xmlnode_get_tag(p->x, "query")) != NULL) {
			char *xmlns;

			/* we damn well *better* have this! */
			if((xmlns = xmlnode_get_attrib(querynode, "xmlns")) != NULL &&
				strcmp(xmlns, NS_REGISTER) == 0) {

				char *tag;
				xmlnode child = xmlnode_get_firstchild(querynode);

				debug_printf("got registration requirments response!\n");

				while(child != NULL) {
					if((tag = xmlnode_get_name(child)) != NULL) {
						char *data;

						fprintf(stderr, "DBG: got node: \"%s\"\n", tag);
						fflush(stderr);

						if((data = xmlnode_get_data(child)) != NULL) {
							fprintf(stderr, "DBG: got data: \"%s\"\n", data);
							fflush(stderr);
						}
					}
					child = xmlnode_get_nextsibling(child);
				}
			}
		} else {
			debug_printf("registration successful!\n");

			hide_login_progress_notice(GJ_GC(gjc), _("Server Registration successful!"));
			/*
			 * TBD: is this the correct way to do away with a
			 * gaim_connection and all its associated memory
			 * allocs, etc.?
			 */
			signoff(GJ_GC(gjc));
		}

	} else {
		xmlnode xerr;
		char *errmsg = NULL;
		int errcode = 0;
		struct jabber_data *jd = GJ_GC(gjc)->proto_data;

		debug_printf("registration failed\n");
		xerr = xmlnode_get_tag(p->x, "error");
		if (xerr) {
			char msg[BUF_LONG];
			errmsg = xmlnode_get_data(xerr);
			if (xmlnode_get_attrib(xerr, "code")) {
				errcode = atoi(xmlnode_get_attrib(xerr, "code"));
				g_snprintf(msg, sizeof(msg), "Error %d: %s", errcode, errmsg);
			} else
				g_snprintf(msg, sizeof(msg), "%s", errmsg);
			hide_login_progress(GJ_GC(gjc), msg);
		} else {
			hide_login_progress(GJ_GC(gjc), _("Unknown registration error"));
		}

		jd->die = TRUE;
	}
}

/*
 * Like gjab_reqauth(), only different
 */
static void gjab_reqreg(gjconn gjc)
{
	xmlnode x, y, z;
	char *user;

	if (!gjc)
		return;

	x = jutil_iqnew(JPACKET__SET, NS_REGISTER);
	y = xmlnode_get_tag(x, "query");

	user = gjc->user->user;

	if (user) {
		z = xmlnode_insert_tag(y, "username");
		xmlnode_insert_cdata(z, user, -1);
	}
	z = xmlnode_insert_tag(y, "password");
	xmlnode_insert_cdata(z, gjc->pass, -1);

	debug_printf("jabber: registration packet: %s\n", xmlnode2str(x));
	gjab_send(gjc, x);
	xmlnode_free(x);
}

/*
 * Like jabber_handlestate(), only different
 */
static void jabber_handle_registration_state(gjconn gjc, int state)
{
	switch (state) {
	case JCONN_STATE_OFF:
		if(gjc->was_connected) {
			hide_login_progress_error(GJ_GC(gjc), _("Connection lost"));
		} else {
			hide_login_progress(GJ_GC(gjc), _("Unable to connect"));
		}
		signoff(GJ_GC(gjc));
		break;
	case JCONN_STATE_CONNECTED:
		gjc->was_connected = 1;
		/*
		 * TBD?
		set_login_progress(GJ_GC(gjc), 2, _("Connected"));
		 */
		break;
	case JCONN_STATE_ON:
		/*
		 * TBD?
		set_login_progress(GJ_GC(gjc), 3, _("Requesting Authentication Method"));
		 */
		gjab_reqreg(gjc);
		/*
		 * TBD: A work-in-progress
		gjab_reqregreqs(gjc);
		 */
		break;
	default:
		debug_printf("state change: %d\n", state);
	}
	return;
}

/*
 * Like jabber_login(), only different
 */
void jabber_register_user(struct aim_user *au)
{
	struct gaim_connection *gc = new_gaim_conn(au);
	struct jabber_data *jd = gc->proto_data = g_new0(struct jabber_data, 1);
	char *loginname = create_valid_jid(au->username, DEFAULT_SERVER, "GAIM");

	/*
	 * These do nothing during registration
	 */
	jd->buddies = NULL;
	jd->chats = NULL;

	if ((jd->gjc = gjab_new(loginname, au->password, gc)) == NULL) {
		g_free(loginname);
		debug_printf("jabber: unable to connect (jab_new failed)\n");
		hide_login_progress(gc, _("Unable to connect"));
		signoff(gc);
	} else {
		gjab_state_handler(jd->gjc, jabber_handle_registration_state);
		gjab_packet_handler(jd->gjc, jabber_handleregresp);
		jd->gjc->queries = NULL;
		gjab_start(jd->gjc);
	}

	g_free(loginname);
}

/*----------------------------------------*/
/* End Jabber "user registration" support */
/*----------------------------------------*/

static void jabber_do_action(struct gaim_connection *gc, char *act)
{
	if (!strcmp(act, _("Set User Info"))) {
		jabber_setup_set_info(gc);
	/*
	} else if (!strcmp(act, _("Set Dir Info"))) {
		show_set_dir(gc);
	 */
	} else if (!strcmp(act, _("Change Password"))) {
		show_change_passwd(gc);
	}
}

static GList *jabber_actions()
{
	GList *m = NULL;

	m = g_list_append(m, _("Set User Info"));
	/*
	m = g_list_append(m, _("Set Dir Info"));
	 */
	m = g_list_append(m, _("Change Password"));

	return m;
}

static struct prpl *my_protocol = NULL;

G_MODULE_EXPORT void jabber_init(struct prpl *ret)
{
	/* the NULL's aren't required but they're nice to have */
	struct proto_user_opt *puo;
	ret->protocol = PROTO_JABBER;
	ret->options = OPT_PROTO_UNIQUE_CHATNAME | OPT_PROTO_CHAT_TOPIC;
	ret->name = g_strdup("Jabber");
	ret->list_icon = jabber_list_icon;
	ret->away_states = jabber_away_states;
	ret->actions = jabber_actions;
	ret->do_action = jabber_do_action;
	ret->buddy_menu = jabber_buddy_menu;
	ret->edit_buddy_menu = jabber_edit_buddy_menu;
	ret->login = jabber_login;
	ret->close = jabber_close;
	ret->send_im = jabber_send_im;
	ret->set_info = jabber_set_info;
	ret->get_info = jabber_get_info;
	ret->get_cb_info = jabber_get_cb_info;
	ret->get_cb_away = jabber_get_cb_away_msg;
	ret->set_away = jabber_set_away;
	ret->set_dir = NULL;
	ret->get_dir = NULL;
	ret->dir_search = NULL;
	ret->set_idle = jabber_set_idle;
	ret->change_passwd = jabber_change_passwd;
	ret->add_buddy = jabber_add_buddy;
	ret->add_buddies = NULL;
	ret->remove_buddy = jabber_remove_buddy;
	ret->add_permit = NULL;
	ret->add_deny = NULL;
	ret->rem_permit = NULL;
	ret->rem_deny = NULL;
	ret->set_permit_deny = NULL;
	ret->warn = NULL;
	ret->chat_info = jabber_chat_info;
	ret->join_chat = jabber_join_chat;
	ret->chat_invite = jabber_chat_invite;
	ret->chat_leave = jabber_chat_leave;
	ret->chat_whisper = jabber_chat_whisper;
	ret->chat_send = jabber_chat_send;
	ret->keepalive = jabber_keepalive;
	ret->normalize = jabber_normalize;
	ret->register_user = jabber_register_user;
	ret->alias_buddy = jabber_alias_buddy;
	ret->group_buddy = jabber_group_change;
	ret->send_typing = jabber_send_typing;
	ret->convo_closed = jabber_convo_closed;
	ret->rename_group = jabber_rename_group;
	ret->file_transfer_out = NULL; /* TODO */
	ret->file_transfer_in = jabber_file_transfer_in;
	ret->file_transfer_data_chunk = NULL; /* TODO */
	ret->file_transfer_done = jabber_file_transfer_done;
	ret->file_transfer_cancel = jabber_file_transfer_cancel;

	puo = g_new0(struct proto_user_opt, 1);
	puo->label = g_strdup(_("Port:"));
	puo->def = g_strdup("5222");
	puo->pos = USEROPT_PORT;
	ret->user_opts = g_list_append(ret->user_opts, puo);

	my_protocol = ret;
}

#ifndef STATIC

G_MODULE_EXPORT void gaim_prpl_init(struct prpl *prpl)
{
	jabber_init(prpl);
	prpl->plug->desc.api_version = PLUGIN_API_VERSION;
}

#endif

/*
 * Local variables:
 * c-indentation-style: k&r
 * c-basic-offset: 8
 * indent-tabs-mode: notnil
 * End:
 *
 * vim: shiftwidth=8:
 */
