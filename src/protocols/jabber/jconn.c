/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  Jabber
 *  Copyright (C) 1998-1999 The Jabber Team http://jabber.org/
 */

#include "jabber.h"

#ifdef _WIN32
#include "win32dep.h"
#endif

/* local macros for launching event handlers */
#define STATE_EVT(arg) if(j->on_state) { (j->on_state)(j, (arg) ); }

/* prototypes of the local functions */
static void startElement(void *userdata, const char *name, const char **attribs);
static void endElement(void *userdata, const char *name);
static void charData(void *userdata, const char *s, int slen);

/*
 *  jab_new -- initialize a new jabber connection
 *
 *  parameters
 *      user -- jabber id of the user
 *      pass -- password of the user
 *
 *  results
 *      a pointer to the connection structure
 *      or NULL if allocations failed
 */
jconn jab_new(char *user, char *pass)
{
    pool p;
    jconn j;

    if(!user) return(NULL);

    p = pool_new();
    if(!p) return(NULL);
    j = pmalloc_x(p, sizeof(jconn_struct), 0);
    if(!j) return(NULL);
    j->p = p;

    j->user = jid_new(p, user);
    j->pass = pstrdup(p, pass);

    j->state = JCONN_STATE_OFF;
    j->id = 1;
    j->fd = -1;

    return j;
}

/*
 *  jab_delete -- free a jabber connection
 *
 *  parameters
 *      j -- connection
 *
 */
void jab_delete(jconn j)
{
    if(!j) return;

    jab_stop(j);
    pool_free(j->p);
}

/*
 *  jab_state_handler -- set callback handler for state change
 *
 *  parameters
 *      j -- connection
 *      h -- name of the handler function
 */
void jab_state_handler(jconn j, jconn_state_h h)
{
    if(!j) return;

    j->on_state = h;
}

/*
 *  jab_packet_handler -- set callback handler for incoming packets
 *
 *  parameters
 *      j -- connection
 *      h -- name of the handler function
 */
void jab_packet_handler(jconn j, jconn_packet_h h)
{
    if(!j) return;

    j->on_packet = h;
}


/*
 *  jab_start -- start connection
 *
 *  parameters
 *      j -- connection
 *
 */
void jab_start(jconn j)
{
    xmlnode x;
    char *t,*t2;

    if(!j || j->state != JCONN_STATE_OFF) return;

    j->parser = XML_ParserCreate(NULL);
    XML_SetUserData(j->parser, (void *)j);
    XML_SetElementHandler(j->parser, startElement, endElement);
    XML_SetCharacterDataHandler(j->parser, charData);

    j->fd = make_netsocket(5222, j->user->server, NETSOCKET_CLIENT);
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
    jab_send_raw(j,"<?xml version='1.0'?>");
    jab_send_raw(j,t);
    xmlnode_free(x);

    j->state = JCONN_STATE_ON;
    STATE_EVT(JCONN_STATE_ON)

}

/*
 *  jab_stop -- stop connection
 *
 *  parameters
 *      j -- connection
 */
void jab_stop(jconn j)
{
    if(!j || j->state == JCONN_STATE_OFF) return;

    j->state = JCONN_STATE_OFF;
    close(j->fd);
    j->fd = -1;
    XML_ParserFree(j->parser);
}

/*
 *  jab_getfd -- get file descriptor of connection socket
 *
 *  parameters
 *      j -- connection
 *
 *  returns
 *      fd of the socket or -1 if socket was not connected
 */
int jab_getfd(jconn j)
{
    if(j)
        return j->fd;
    else
        return -1;
}

/*
 *  jab_getjid -- get jid structure of user
 *
 *  parameters
 *      j -- connection
 */
jid jab_getjid(jconn j)
{
    if(j)
        return(j->user);
    else
        return NULL;
}

/*  jab_getsid -- get stream id
 *  This is the id of server's <stream:stream> tag and used for
 *  digest authorization.
 *
 *  parameters
 *      j -- connection
 */
char *jab_getsid(jconn j)
{
    if(j)
        return(j->sid);
    else
        return NULL;
}

/*
 *  jab_getid -- get a unique id
 *
 *  parameters
 *      j -- connection
 */
char *jab_getid(jconn j)
{
    snprintf(j->idbuf, 8, "%d", j->id++);
    return &j->idbuf[0];
}

/*
 *  jab_send -- send xml data
 *
 *  parameters
 *      j -- connection
 *      x -- xmlnode structure
 */
void jab_send(jconn j, xmlnode x)
{
    if (j && j->state != JCONN_STATE_OFF)
    {
	    char *buf = xmlnode2str(x);
	    if (buf) write(j->fd, buf, strlen(buf));
#ifdef JDEBUG
	    printf ("out: %s\n", buf);
#endif
    }
}

/*
 *  jab_send_raw -- send a string
 *
 *  parameters
 *      j -- connection
 *      str -- xml string
 */
void jab_send_raw(jconn j, const char *str)
{
    if (j && j->state != JCONN_STATE_OFF)
        write(j->fd, str, strlen(str));
#ifdef JDEBUG
    printf ("out: %s\n", str);
#endif
}

/*
 *  jab_recv -- read and parse incoming data
 *
 *  parameters
 *      j -- connection
 */
void jab_recv(jconn j)
{
    static char buf[4096];
    int len;

    if(!j || j->state == JCONN_STATE_OFF)
        return;

    len = read(j->fd, buf, sizeof(buf)-1);
    if(len>0)
    {
        buf[len] = '\0';
#ifdef JDEBUG
        printf (" in: %s\n", buf);
#endif
        XML_Parse(j->parser, buf, len, 0);
    }
    else if(len<0)
    {
        STATE_EVT(JCONN_STATE_OFF);
        jab_stop(j);
    }
}

/*
 *  jab_poll -- check socket for incoming data
 *
 *  parameters
 *      j -- connection
 *      timeout -- poll timeout
 */
void jab_poll(jconn j, int timeout)
{
    fd_set fds;
    struct timeval tv;

    if (!j || j->state == JCONN_STATE_OFF)
        return;

    FD_ZERO(&fds);
    FD_SET(j->fd, &fds);

    if (timeout < 0)
    {
        if (select(j->fd + 1, &fds, NULL, NULL, NULL) > 0)
            jab_recv(j);
    }
    else
    {
	    tv.tv_sec  = 0;
	    tv.tv_usec = timeout;
	    if (select(j->fd + 1, &fds, NULL, NULL, &tv) > 0)
	        jab_recv(j);
    }
}

/*
 *  jab_auth -- authorize user
 *
 *  parameters
 *      j -- connection
 *
 *  returns
 *      id of the iq packet
 */
char *jab_auth(jconn j)
{
    xmlnode x,y,z;
    char *hash, *user, *id;

    if(!j) return(NULL);

    x = jutil_iqnew(JPACKET__SET, NS_AUTH);
    id = jab_getid(j);
    xmlnode_put_attrib(x, "id", id);
    y = xmlnode_get_tag(x,"query");

    user = j->user->user;

    if (user)
    {
        z = xmlnode_insert_tag(y, "username");
        xmlnode_insert_cdata(z, user, -1);
    }

    z = xmlnode_insert_tag(y, "resource");
    xmlnode_insert_cdata(z, j->user->resource, -1);

    if (j->sid)
    {
        z = xmlnode_insert_tag(y, "digest");
        hash = pmalloc(x->p, strlen(j->sid)+strlen(j->pass)+1);
        strcpy(hash, j->sid);
        strcat(hash, j->pass);
        hash = shahash(hash);
        xmlnode_insert_cdata(z, hash, 40);
    }
    else
    {
	z = xmlnode_insert_tag(y, "password");
	xmlnode_insert_cdata(z, j->pass, -1);
    }

    jab_send(j, x);
    xmlnode_free(x);
    return id;
}

/*
 *  jab_reg -- register user
 *
 *  parameters
 *      j -- connection
 *
 *  returns
 *      id of the iq packet
 */
char *jab_reg(jconn j)
{
    xmlnode x,y,z;
    char *user, *id;

    if (!j) return(NULL);

    x = jutil_iqnew(JPACKET__SET, NS_REGISTER);
    id = jab_getid(j);
    xmlnode_put_attrib(x, "id", id);
    y = xmlnode_get_tag(x,"query");

    user = j->user->user;

    if (user)
    {
        z = xmlnode_insert_tag(y, "username");
        xmlnode_insert_cdata(z, user, -1);
    }

    z = xmlnode_insert_tag(y, "resource");
    xmlnode_insert_cdata(z, j->user->resource, -1);

    if (j->pass)
    {
	z = xmlnode_insert_tag(y, "password");
	xmlnode_insert_cdata(z, j->pass, -1);
    }

    jab_send(j, x);
    xmlnode_free(x);
    j->state = JCONN_STATE_ON;
    STATE_EVT(JCONN_STATE_ON)
    return id;
}


/* local functions */

static void startElement(void *userdata, const char *name, const char **attribs)
{
    xmlnode x;
    jconn j = (jconn)userdata;

    if(j->current)
    {
        /* Append the node to the current one */
        x = xmlnode_insert_tag(j->current, name);
        xmlnode_put_expat_attribs(x, attribs);

        j->current = x;
    }
    else
    {
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
    jconn j = (jconn)userdata;
    xmlnode x;
    jpacket p;

    if(j->current == NULL) {
        /* we got </stream:stream> */
        STATE_EVT(JCONN_STATE_OFF)
        return;
    }

    x = xmlnode_get_parent(j->current);

    if(x == NULL)
    {
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
    jconn j = (jconn)userdata;

    if (j->current)
        xmlnode_insert_cdata(j->current, s, slen);
}
