/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
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
#include "../config.h"
#endif


#include <gtk/gtk.h>
#ifdef MAX
#undef MAX
#endif
#ifdef MIN
#undef MIN
#endif
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include "multi.h"
#include "prpl.h"
#include "gaim.h"
#include <jabber/jabber.h>

/* The priv member of gjconn's is a gaim_connection for now. */
#define GJ_GC(x) ((struct gaim_connection *)(x)->priv)

#define IQ_NONE -1
#define IQ_AUTH 0
#define IQ_ROSTER 1

#define USEROPT_SERVER   0
#define USEROPT_PORT     1
#define USEROPT_RESOURCE 2

typedef struct gjconn_struct
{
        /* Core structure */
        pool        p;             /* Memory allocation pool */
        int         state;     /* Connection state flag */
        int         fd;            /* Connection file descriptor */
        jid         user;      /* User info */
        char        *pass;     /* User passwd */

        /* Stream stuff */
        int         id;        /* id counter for jab_getid() function */
        char        idbuf[9];  /* temporary storage for jab_getid() */
        char        *sid;      /* stream id from server, for digest auth */
        XML_Parser  parser;    /* Parser instance */
        xmlnode     current;   /* Current node in parsing instance.. */

        /* Event callback ptrs */
        void (*on_state)(struct gjconn_struct *j, int state);
        void (*on_packet)(struct gjconn_struct *j, jpacket p);

        void *priv;

} *gjconn, gjconn_struct;

typedef void (*gjconn_state_h)(gjconn j, int state);
typedef void (*gjconn_packet_h)(gjconn j, jpacket p);

static gjconn gjab_new(char *user, char *pass, void *priv);
static void gjab_delete(gjconn j);
static void gjab_state_handler(gjconn j, gjconn_state_h h);
static void gjab_packet_handler(gjconn j, gjconn_packet_h h);
static void gjab_start(gjconn j, int port);
static void gjab_stop(gjconn j);
static int gjab_getfd(gjconn j);
static jid gjab_getjid(gjconn j);
static char *gjab_getsid(gjconn j);
static char *gjab_getid(gjconn j);
static void gjab_send(gjconn j, xmlnode x);
static void gjab_send_raw(gjconn j, const char *str);
static void gjab_recv(gjconn j);
static char *gjab_auth(gjconn j);

struct jabber_data {
        gjconn jc;
};

static char *jabber_name() {
	return "Jabber";
}

char *name() {
	return "Jabber";
}

char *description() {
	return "Allows gaim to use the Jabber protocol";
}

#define STATE_EVT(arg) if(j->on_state) { (j->on_state)(j, (arg) ); }

static gjconn gjab_new(char *user, char *pass, void *priv)
{
    pool p;
    gjconn j;

    if(!user) 
            return(NULL);

    p = pool_new();
    if(!p) 
            return(NULL);
    j = pmalloc_x(p, sizeof(gjconn_struct), 0);
    if(!j) 
            return(NULL);
    j->p = p;

    j->user = jid_new(p, user);
    j->pass = pstrdup(p, pass);

    j->state = JCONN_STATE_OFF;
    j->id = 1;
    j->fd = -1;

    j->priv = priv;

    return j;
}

static void gjab_delete(gjconn j)
{
    if(!j) 
            return;

    gjab_stop(j);
    pool_free(j->p);
}

static void gjab_state_handler(gjconn j, gjconn_state_h h)
{
    if(!j)      
            return;

    j->on_state = h;
}

static void gjab_packet_handler(gjconn j, gjconn_packet_h h)
{
    if(!j)
            return;

    j->on_packet = h;
}

static void gjab_stop(gjconn j)
{
    if(!j || j->state == JCONN_STATE_OFF) 
            return;

    j->state = JCONN_STATE_OFF;
    close(j->fd);
    j->fd = -1;
    XML_ParserFree(j->parser);
}

static int gjab_getfd(gjconn j)
{
    if(j)
        return j->fd;
    else
        return -1;
}

static jid gjab_getjid(gjconn j)
{
    if(j)
        return(j->user);
    else
        return NULL;
}

static char *gjab_getsid(gjconn j)
{
    if(j)
        return(j->sid);
    else
        return NULL;
}

static char *gjab_getid(gjconn j)
{
    snprintf(j->idbuf, 8, "%d", j->id++);
    return &j->idbuf[0];
}

static void gjab_send(gjconn j, xmlnode x)
{
    if (j && j->state != JCONN_STATE_OFF) {
            char *buf = xmlnode2str(x);
            if (buf) 
                    write(j->fd, buf, strlen(buf));
            debug_printf("gjab_send: %s\n", buf);
    }
}

static void gjab_send_raw(gjconn j, const char *str)
{
        if (j && j->state != JCONN_STATE_OFF) {
                write(j->fd, str, strlen(str));
                debug_printf("gjab_send_raw: %s\n", str);
        }
}

static void gjab_reqroster(gjconn j)
{
        xmlnode x;
        char *id;

        x = jutil_iqnew(JPACKET__GET, NS_ROSTER);
        id = gjab_getid(j);
        xmlnode_put_attrib(x, "id", id);

        gjab_send(j, x);
        xmlnode_free(x);
}

static char *gjab_auth(gjconn j)
{
    xmlnode x,y,z;
    char *hash, *user, *id;

    if(!j) 
            return NULL;

    x = jutil_iqnew(JPACKET__SET, NS_AUTH);
    id = gjab_getid(j);
    xmlnode_put_attrib(x, "id", id);
    y = xmlnode_get_tag(x,"query");

    user = j->user->user;

    if (user) {
            z = xmlnode_insert_tag(y, "username");
            xmlnode_insert_cdata(z, user, -1);
    }

    z = xmlnode_insert_tag(y, "resource");
    xmlnode_insert_cdata(z, j->user->resource, -1);

    if (j->sid) {
            z = xmlnode_insert_tag(y, "digest");
            hash = pmalloc(x->p, strlen(j->sid)+strlen(j->pass)+1);
            strcpy(hash, j->sid);
            strcat(hash, j->pass);
            hash = shahash(hash);
            xmlnode_insert_cdata(z, hash, 40);
    } else {
            z = xmlnode_insert_tag(y, "password");
            xmlnode_insert_cdata(z, j->pass, -1);
    }

    gjab_send(j, x);
    xmlnode_free(x);

    return id;
}

static void gjab_recv(gjconn j)
{
        static char buf[4096];
        int len;

        if(!j || j->state == JCONN_STATE_OFF)
                return;

        if ((len = read(j->fd, buf, sizeof(buf)-1))) {
                buf[len] = '\0';
                debug_printf("input: %s\n", buf);
                XML_Parse(j->parser, buf, len, 0);
        } else if (len < 0) {
                STATE_EVT(JCONN_STATE_OFF);
                gjab_stop(j);
        }
}

static void startElement(void *userdata, const char *name, const char **attribs)
{
    xmlnode x;
    gjconn j = (gjconn)userdata;

    if(j->current) {
            /* Append the node to the current one */
            x = xmlnode_insert_tag(j->current, name);
            xmlnode_put_expat_attribs(x, attribs);

            j->current = x;
    } else {
            x = xmlnode_new_tag(name);
            xmlnode_put_expat_attribs(x, attribs);
            if(strcmp(name, "stream:stream") == 0) {
                    /* special case: name == stream:stream */
                    /* id attrib of stream is stored for digest auth */
                    j->sid = xmlnode_get_attrib(x, "id");
                    /* STATE_EVT(JCONN_STATE_AUTH) */
            } else {
                    j->current = x;
            }
    }
}

static void endElement(void *userdata, const char *name)
{
    gjconn j = (gjconn)userdata;
    xmlnode x;
    jpacket p;

    if(j->current == NULL) {
        /* we got </stream:stream> */
        STATE_EVT(JCONN_STATE_OFF)
        return;
    }

    x = xmlnode_get_parent(j->current);

    if(!x) {
            /* it is time to fire the event */
            p = jpacket_new(j->current);

            if(j->on_packet)
                    (j->on_packet)(j, p);
            else
                    xmlnode_free(j->current);
    }

    j->current = x;
}

static void charData(void *userdata, const char *s, int slen)
{
    gjconn j = (gjconn)userdata;

    if (j->current)
        xmlnode_insert_cdata(j->current, s, slen);
}

static void gjab_start(gjconn j, int port)
{
    xmlnode x;
    char *t,*t2;

    if(!j || j->state != JCONN_STATE_OFF) 
            return;

    j->parser = XML_ParserCreate(NULL);
    XML_SetUserData(j->parser, (void *)j);
    XML_SetElementHandler(j->parser, startElement, endElement);
    XML_SetCharacterDataHandler(j->parser, charData);

    j->fd = make_netsocket(port, j->user->server, NETSOCKET_CLIENT);
    if(j->fd < 0) {
        STATE_EVT(JCONN_STATE_OFF)
        return;
    }
    j->state = JCONN_STATE_CONNECTED;
    STATE_EVT(JCONN_STATE_CONNECTED)

    /* start stream */
    x = jutil_header(NS_CLIENT, j->user->server);
    t = xmlnode2str(x);
    /* this is ugly, we can create the string here instead of jutil_header */
    /* what do you think about it? -madcat */
    t2 = strstr(t,"/>");
    *t2++ = '>';
    *t2 = '\0';
    gjab_send_raw(j,"<?xml version='1.0'?>");
    gjab_send_raw(j,t);
    xmlnode_free(x);

    j->state = JCONN_STATE_ON;
    STATE_EVT(JCONN_STATE_ON)
}

static void jabber_callback(gpointer data, gint source, GdkInputCondition condition) {
	struct gaim_connection *gc = (struct gaim_connection *)data;
	struct jabber_data *jd = (struct jabber_data *)gc->proto_data;

        debug_printf("jabber_callback!\n");

        gjab_recv(jd->jc);
}

static void jabber_handlemessage(gjconn j, jpacket p)
{
  xmlnode y;
  char *x, *m;

  char *from = NULL, *msg = NULL;

  from = jid_full(p->from);
  if ((y = xmlnode_get_tag(p->x, "body"))) {
    msg = xmlnode_get_data(y);
  }

  if (!from || !msg) {
    return;
  }

  x = strchr(from, '@');
  if (x) {
	  *x++ = '\0';
	  m = strchr(x, '/');
	  *m = '\0';
	  if (strcmp(j->user->server, x)) {
		  x--;
		  *x = '@';
		  *m = '/';
	  }
  }
  debug_printf("jabber: msg from %s: %s\n", from, msg);

  serv_got_im(GJ_GC(j), from, msg, 0);

  return;
}

static void jabber_handlepresence(gjconn j, jpacket p)
{
  char *to, *from, *type;
  struct buddy *b;

  to = xmlnode_get_attrib(p->x, "to");
  from = xmlnode_get_attrib(p->x, "from");
  type = xmlnode_get_attrib(p->x, "type");

  debug_printf("jabber: presence: %s, %s %s\n", to, from, type);

  if (!(b = find_buddy(GJ_GC(j), from)))
          add_buddy(GJ_GC(j), "Extra", from, from);

  if (type && (strcasecmp(type, "unavailable") == 0))
          serv_got_update(GJ_GC(j), from, 0, 0, 0, 0, 0, 0);
  else
          serv_got_update(GJ_GC(j), from, 1, 0, 0, 0, 0, 0);

  return;
}

static void jabber_handleroster(gjconn j, xmlnode querynode)
{
        xmlnode x;

        x = xmlnode_get_firstchild(querynode);
        while (x) {
                xmlnode g;
                char *jid, *name, *sub, *ask;

                jid = xmlnode_get_attrib(x, "jid");
                name = xmlnode_get_attrib(x, "name");
                if (name)
                        debug_printf("name = %s\n", name);
                sub = xmlnode_get_attrib(x, "subscription");
                ask = xmlnode_get_attrib(x, "ask");

                if (ask) {
                        /* XXX do something */
                        debug_printf("jabber: unhandled subscription request (%s/%s/%s/%s)\n", jid, name, sub, ask);
                }

                if ((g = xmlnode_get_firstchild(x))) {
                        while (g) {
                                if (strncasecmp(xmlnode_get_name(g), "group", 5) == 0) {
                                        struct buddy *b;
                                        char *groupname;

                                        groupname = xmlnode_get_data(xmlnode_get_firstchild(g));
                                        if (!(b = find_buddy(GJ_GC(j), jid))) {
                                                debug_printf("adding buddy: %s\n", jid);
                                                b = add_buddy(GJ_GC(j), groupname, jid, name?name:jid);
                                        } else {
                                                debug_printf("updating buddy: %s/%s\n", jid, name);
                                                g_snprintf(b->name, sizeof(b->name), "%s", jid);
                                                g_snprintf(b->show, sizeof(b->show), "%s", name?name:jid);
                                        }
                                        //serv_got_update(GJ_GC(j), b->name, 1, 0, 0, 0, 0, 0);
                                }
                                g = xmlnode_get_nextsibling(g);
                        }
                } else {
                        struct buddy *b;
                        if (!(b = find_buddy(GJ_GC(j), jid))) {
                                b = add_buddy(GJ_GC(j), "Extra", jid, name?name:jid);
                        }
                }

                x = xmlnode_get_nextsibling(x);
        }
}

static void jabber_handlepacket(gjconn j, jpacket p)
{
  switch (p->type) {
  case JPACKET_MESSAGE:
          jabber_handlemessage(j, p);
          break;
  case JPACKET_PRESENCE:
          jabber_handlepresence(j, p);
          break;
  case JPACKET_IQ: {

          if (jpacket_subtype(p) == JPACKET__RESULT) {
                  xmlnode querynode;
                  char *xmlns;

                  querynode = xmlnode_get_tag(p->x, "query");
                  xmlns = xmlnode_get_attrib(querynode, "xmlns");

                  /* XXX this just doesn't look right */
                  if (!xmlns || NSCHECK(querynode, NS_AUTH)) {
                          xmlnode x;

                          debug_printf("auth success\n");
                          x = jutil_presnew(0, NULL, NULL);
                          gjab_send(j, x);
                          xmlnode_free(x);
                          
                          account_online(GJ_GC(j));
                          serv_finish_login(GJ_GC(j));
                          
                          gjab_reqroster(j);

                  } else if (NSCHECK(querynode, NS_ROSTER)) {
                          jabber_handleroster(j, querynode);
                  } else {
                          debug_printf("jabber:iq:query: %s\n", xmlns);
                  }

          } else {
                  xmlnode x;

                  debug_printf("auth failed\n");
                  x = xmlnode_get_tag(p->x, "error");
                  if (x) {
                          debug_printf("error %d: %s\n\n", 
                                       atoi(xmlnode_get_attrib(x, "code")), 
                                       xmlnode_get_data(xmlnode_get_firstchild(x)));
                          hide_login_progress(GJ_GC(j), xmlnode_get_data(xmlnode_get_firstchild(x)));

                  } else
                          hide_login_progress(GJ_GC(j), "unknown error");

                  signoff(GJ_GC(j));
          }
          break;
  }
  default:
          debug_printf("jabber: packet type %d (%s)\n", p->type, xmlnode2str(p->x));
  }

  xmlnode_free(p->x);

  return;
}

static void jabber_handlestate(gjconn j, int state)
{
  switch (state) {
  case JCONN_STATE_OFF:
          debug_printf("jabber: connection closed\n");
          hide_login_progress(GJ_GC(j), "Unable to connect");
          signoff(GJ_GC(j));
          break;
  case JCONN_STATE_CONNECTED:
          debug_printf("jabber: connected.\n");
          set_login_progress(GJ_GC(j), 1, "Connected");
          break;
  case JCONN_STATE_ON:
          debug_printf("jabber: logging in...\n");
          set_login_progress(GJ_GC(j), 1, "Logging in...");
          gjab_auth(j);
          break;
  default:
          debug_printf("state change: %d\n", state);
  }
  return;
}

static void jabber_login(struct aim_user *user) {
	struct gaim_connection *gc = new_gaim_conn(user);
	struct jabber_data *jd = gc->proto_data = g_new0(struct jabber_data, 1);
	char *tmp;
	int port;

	set_login_progress(gc, 1, "Connecting");
	while (gtk_events_pending())
		gtk_main_iteration();

	tmp = g_strdup_printf("%s@%s/%s", user->username,
		user->proto_opt[USEROPT_SERVER][0] ? user->proto_opt[USEROPT_SERVER] : "jabber.com",
		user->proto_opt[USEROPT_RESOURCE][0] ? user->proto_opt[USEROPT_RESOURCE] : "GAIM");

        debug_printf("jabber_login (u=%s)\n", tmp);

        if (!(jd->jc = gjab_new(tmp, user->password, gc))) {
		debug_printf("jabber: unable to connect (jab_new failed)\n");
		hide_login_progress(gc, "Unable to connect");
		signoff(gc);
		g_free(jd);
		g_free(tmp);
                return;
        }
	g_free(tmp);

        gjab_state_handler(jd->jc, jabber_handlestate);
        gjab_packet_handler(jd->jc, jabber_handlepacket);
	port = user->proto_opt[USEROPT_PORT][0] ? atoi(user->proto_opt[USEROPT_PORT]) : 5222;
        gjab_start(jd->jc, port);


	gc->inpa = gdk_input_add(jd->jc->fd, 
                                 GDK_INPUT_READ | GDK_INPUT_EXCEPTION,
                                 jabber_callback, gc);

        return;
}

static void jabber_close(struct gaim_connection *gc) {
	struct jabber_data *jd = gc->proto_data;
	gdk_input_remove(gc->inpa);
	gjab_stop(jd->jc);
	g_free(jd);
}

static void jabber_send_im(struct gaim_connection *gc, char *who, char *message, int away) {
        xmlnode x, y;
	char *realwho;
	gjconn j = ((struct jabber_data *)gc->proto_data)->jc;

        if (!who || !message)
                return;

        x = xmlnode_new_tag("message");
	if (!strchr(who, '@'))
		realwho = g_strdup_printf("%s@%s", who, j->user->server);
	else
		realwho = g_strdup(who);
        xmlnode_put_attrib(x, "to", realwho);
	g_free(realwho);

        xmlnode_put_attrib(x, "type", "chat");

        if (message && strlen(message)) {
                y = xmlnode_insert_tag(x, "body");
                xmlnode_insert_cdata(y, message, -1);
        }

        gjab_send(((struct jabber_data *)gc->proto_data)->jc, x);
}

static void jabber_print_option(GtkEntry *entry, struct aim_user *user) {
	int entrynum;

	entrynum = (int) gtk_object_get_user_data(GTK_OBJECT(entry));

	if (entrynum == USEROPT_SERVER)
		g_snprintf(user->proto_opt[USEROPT_SERVER],
				sizeof(user->proto_opt[USEROPT_SERVER]),
				"%s", gtk_entry_get_text(entry));
	else if (entrynum == USEROPT_PORT)
		g_snprintf(user->proto_opt[USEROPT_PORT],
				sizeof(user->proto_opt[USEROPT_PORT]),
				"%s", gtk_entry_get_text(entry));
	else if (entrynum == USEROPT_RESOURCE)
		g_snprintf(user->proto_opt[USEROPT_RESOURCE],
				sizeof(user->proto_opt[USEROPT_RESOURCE]),
				"%s", gtk_entry_get_text(entry));
}

static void jabber_user_opts(GtkWidget *book, struct aim_user *user) {
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *entry;

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_notebook_append_page(GTK_NOTEBOOK(book), vbox,
				 gtk_label_new("Jabber Options"));
	gtk_widget_show(vbox);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new("Server:");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	entry = gtk_entry_new();
	gtk_box_pack_end(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
	gtk_object_set_user_data(GTK_OBJECT(entry), (void *)USEROPT_SERVER);
	gtk_signal_connect(GTK_OBJECT(entry), "changed",
			   GTK_SIGNAL_FUNC(jabber_print_option), user);
	if (user->proto_opt[USEROPT_SERVER][0])
		gtk_entry_set_text(GTK_ENTRY(entry), user->proto_opt[USEROPT_SERVER]);
	else
		gtk_entry_set_text(GTK_ENTRY(entry), "jabber.com");
	gtk_widget_show(entry);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new("Port:");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	entry = gtk_entry_new();
	gtk_box_pack_end(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
	gtk_object_set_user_data(GTK_OBJECT(entry), (void *)USEROPT_PORT);
	gtk_signal_connect(GTK_OBJECT(entry), "changed",
			   GTK_SIGNAL_FUNC(jabber_print_option), user);
	if (user->proto_opt[USEROPT_PORT][0])
		gtk_entry_set_text(GTK_ENTRY(entry), user->proto_opt[USEROPT_PORT]);
	else
		gtk_entry_set_text(GTK_ENTRY(entry), "5222");
	gtk_widget_show(entry);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new("Resource:");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	entry = gtk_entry_new();
	gtk_box_pack_end(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
	gtk_object_set_user_data(GTK_OBJECT(entry), (void *)USEROPT_RESOURCE);
	gtk_signal_connect(GTK_OBJECT(entry), "changed",
			   GTK_SIGNAL_FUNC(jabber_print_option), user);
	if (user->proto_opt[USEROPT_RESOURCE][0])
		gtk_entry_set_text(GTK_ENTRY(entry), user->proto_opt[USEROPT_RESOURCE]);
	else
		gtk_entry_set_text(GTK_ENTRY(entry), "GAIM");
	gtk_widget_show(entry);
}

static struct prpl *my_protocol = NULL;

void Jabber_init(struct prpl *ret) {
	/* the NULL's aren't required but they're nice to have */
	ret->protocol = PROTO_JABBER;
	ret->name = jabber_name;
	ret->list_icon = NULL;
	ret->action_menu = NULL;
	ret->user_opts = jabber_user_opts;
	ret->login = jabber_login;
	ret->close = jabber_close;
	ret->send_im = jabber_send_im;
	ret->set_info = NULL;
	ret->get_info = NULL;
	ret->set_away = NULL;
	ret->get_away_msg = NULL;
	ret->set_dir = NULL;
	ret->get_dir = NULL;
	ret->dir_search = NULL;
	ret->set_idle = NULL;
	ret->change_passwd = NULL;
	ret->add_buddy = NULL;
	ret->add_buddies = NULL;
	ret->remove_buddy = NULL;
	ret->add_permit = NULL;
	ret->add_deny = NULL;
	ret->rem_permit = NULL;
	ret->rem_deny = NULL;
	ret->set_permit_deny = NULL;
	ret->warn = NULL;
	ret->accept_chat = NULL;
	ret->join_chat = NULL;
	ret->chat_invite = NULL;
	ret->chat_leave = NULL;
	ret->chat_whisper = NULL;
	ret->chat_send = NULL;
	ret->keepalive = NULL;

	my_protocol = ret;
}

char *gaim_plugin_init(GModule *handle) {
	load_protocol(Jabber_init);
	return NULL;
}

void gaim_plugin_remove() {
	struct prpl *p = find_prpl(PROTO_JABBER);
	if (p == my_protocol)
		unload_protocol(p);
}
