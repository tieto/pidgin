/*
 * Gaim's oscar protocol plugin
 * This file is the legal property of its developers.
 * Please see the AUTHORS file distributed alongside this file.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/**
 * Low-level connection handling.
 *
 * Does all this gloriously nifty connection handling stuff...
 *
 */

#include "oscar.h"

#ifndef _WIN32
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#ifdef _WIN32
#include "win32dep.h"
#endif

/**
 * In OSCAR, every connection has a set of SNAC groups associated
 * with it.  These are the groups that you can send over this connection
 * without being guaranteed a "Not supported" SNAC error.
 *
 * The grand theory of things says that these associations transcend
 * what libfaim calls "connection types" (conn->type).  You can probably
 * see the elegance here, but since I want to revel in it for a bit, you
 * get to hear it all spelled out.
 *
 * So let us say that you have your core BOS connection running.  One
 * of your modules has just given you a SNAC of the group 0x0004 to send
 * you.  Maybe an IM destined for some twit in Greenland.  So you start
 * at the top of your connection list, looking for a connection that
 * claims to support group 0x0004.  You find one.  Why, that neat BOS
 * connection of yours can do that.  So you send it on its way.
 *
 * Now, say, that fellow from Greenland has friends and they all want to
 * meet up with you in a lame chat room.  This has landed you a SNAC
 * in the family 0x000e and you have to admit you're a bit lost.  You've
 * searched your connection list for someone who wants to make your life
 * easy and deliver this SNAC for you, but there isn't one there.
 *
 * Here comes the good bit.  Without even letting anyone know, particularly
 * the module that decided to send this SNAC, and definitely not that twit
 * in Greenland, you send out a service request.  In this request, you have
 * marked the need for a connection supporting group 0x000e.  A few seconds
 * later, you receive a service redirect with an IP address and a cookie in
 * it.  Great, you say.  Now I have something to do.  Off you go, making
 * that connection.  One of the first things you get from this new server
 * is a message saying that indeed it does support the group you were looking
 * for.  So you continue and send rate confirmation and all that.
 *
 * Then you remember you had that SNAC to send, and now you have a means to
 * do it, and you do, and everyone is happy.  Except the Greenlander, who is
 * still stuck in the bitter cold.
 *
 * Oh, and this is useful for building the Migration SNACs, too.  In the
 * future, this may help convince me to implement rate limit mitigation
 * for real.  We'll see.
 *
 * Just to make me look better, I'll say that I've known about this great
 * scheme for quite some time now.  But I still haven't convinced myself
 * to make libfaim work that way.  It would take a fair amount of effort,
 * and probably some client API changes as well.  (Whenever I don't want
 * to do something, I just say it would change the client API.  Then I
 * instantly have a couple of supporters of not doing it.)
 *
 * Generally, addgroup is only called by the internal handling of the
 * server ready SNAC.  So if you want to do something before that, you'll
 * have to be more creative.  That is done rather early, though, so I don't
 * think you have to worry about it.  Unless you're me.  I care deeply
 * about such inane things.
 *
 */
void
aim_conn_addgroup(OscarConnection *conn, guint16 group)
{
	aim_conn_inside_t *ins = (aim_conn_inside_t *)conn->inside;
	struct snacgroup *sg;

	sg = g_new(struct snacgroup, 1);

	gaim_debug_misc("oscar", "adding group 0x%04x\n", group);
	sg->group = group;

	sg->next = ins->groups;
	ins->groups = sg;
}

OscarConnection *
aim_conn_findbygroup(OscarSession *sess, guint16 group)
{
	GList *cur;;

	for (cur = sess->oscar_connections; cur; cur = cur->next)
	{
		OscarConnection *conn;
		aim_conn_inside_t *ins;
		struct snacgroup *sg;

		conn = cur->data;
		ins = (aim_conn_inside_t *)conn->inside;

		for (sg = ins->groups; sg; sg = sg->next)
		{
			if (sg->group == group)
				return conn;
		}
	}

	return NULL;
}

static void
connkill_snacgroups(struct snacgroup *head)
{
	struct snacgroup *sg;
	for (sg = head; sg; )
	{
		struct snacgroup *tmp;

		tmp = sg->next;
		free(sg);
		sg = tmp;
	}
}

static void
connkill_rates(struct rateclass *head)
{
	struct rateclass *rc;

	for (rc = head; rc; )
	{
		struct rateclass *tmp;
		struct snacpair *sp;

		tmp = rc->next;

		for (sp = rc->members; sp; ) {
			struct snacpair *tmpsp;

			tmpsp = sp->next;
			free(sp);
			sp = tmpsp;
		}
		free(rc);

		rc = tmp;
	}
}

void
oscar_connection_destroy(OscarSession *sess, OscarConnection *conn)
{
	aim_rxqueue_cleanbyconn(sess, conn);
	aim_tx_cleanqueue(sess, conn);

	if (conn->fd != -1)
		aim_conn_close(sess, conn);

	/*
	 * This will free ->internal if it necessary...
	 */
	if (conn->type == AIM_CONN_TYPE_CHAT)
		aim_conn_kill_chat(sess, conn);

	if (conn->inside != NULL)
	{
		aim_conn_inside_t *inside = (aim_conn_inside_t *)conn->inside;

		connkill_snacgroups(inside->groups);
		connkill_rates(inside->rates);

		free(inside);
	}

	gaim_circ_buffer_destroy(conn->buffer_outgoing);
	g_free(conn);

	sess->oscar_connections = g_list_remove(sess->oscar_connections, conn);
}

/**
 * This sends an empty channel 4 SNAC.  This is sent to signify
 * that we're logging off.  This shouldn't really be necessary--
 * usually the AIM server will detect that the TCP connection has
 * been destroyed.
 */
static int
aim_flap_close(OscarSession *sess, OscarConnection *conn)
{
	FlapFrame *fr;

	if (!sess || !conn)
		return -EINVAL;

	if (!(fr = flap_frame_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x04, 0)))
		return -ENOMEM;

	aim_tx_enqueue(sess, fr);

	return 0;
}

/**
 * Allocate a new empty connection structure.
 *
 * @param sess The oscar session associated with this connection.
 * @return Returns the new connection structure.
 */
static OscarConnection *
aim_conn_getnext(OscarSession *sess)
{
	OscarConnection *conn;

	conn = g_new0(OscarConnection, 1);
	conn->inside = g_new0(aim_conn_inside_t, 1);
	conn->buffer_outgoing = gaim_circ_buffer_new(-1);
	conn->fd = -1;
	conn->subtype = -1;
	conn->type = -1;
	conn->seqnum = 0;
	conn->lastactivity = 0;
	conn->handlerlist = NULL;

	sess->oscar_connections = g_list_prepend(sess->oscar_connections, conn);

	return conn;
}

/**
 * Close, clear, and free a connection structure. Should never be
 * called from within libfaim.
 *
 * @param sess Session for the connection.
 * @param deadconn Connection to be freed.
 */
void
aim_conn_kill(OscarSession *sess, OscarConnection *conn)
{
	if (!conn)
		return;

	oscar_connection_destroy(sess, conn);
}

/**
 * Close (but not free) a connection.
 *
 * This leaves everything untouched except for clearing the
 * handler list and setting the fd to -1 (used to recognize
 * dead connections).  It will also remove cookies if necessary.
 *
 * @param conn The connection to close.
 */
void
aim_conn_close(OscarSession *sess, OscarConnection *conn)
{
	if (conn->type == AIM_CONN_TYPE_BOS)
		aim_flap_close(sess, conn);

	if (conn->fd >= 0)
		close(conn->fd);

	conn->fd = -1;

	if (conn->handlerlist)
		aim_clearhandlers(conn);
}

/**
 * Locates a connection of the specified type in the
 * specified session.
 *
 * XXX - Except for rendezvous, all uses of this should be removed and
 * aim_conn_findbygroup() should be used instead.
 *
 * @param sess The session to search.
 * @param type The type of connection to look for.
 * @return Returns the first connection found of the given target type,
 *         or NULL if none could be found.
 */
OscarConnection *
aim_getconn_type(OscarSession *sess, int type)
{
	GList *cur;

	for (cur = sess->oscar_connections; cur; cur = cur->next)
	{
		OscarConnection *conn;
		conn = cur->data;
		if ((conn->type == type) &&
				!(conn->status & AIM_CONN_STATUS_INPROGRESS))
			return conn;
	}

	return NULL;
}

OscarConnection *
aim_getconn_type_all(OscarSession *sess, int type)
{
	GList *cur;

	for (cur = sess->oscar_connections; cur; cur = cur->next)
	{
		OscarConnection *conn;
		conn = cur->data;
		if (conn->type == type)
			return conn;
	}

	return NULL;
}

/**
 * Clone an OscarConnection.
 *
 * A new connection is allocated, and the values are filled in
 * appropriately.
 *
 * @param sess The session containing this connection.
 * @param src The connection to clone.
 * @return Returns a pointer to the new OscarConnection, or %NULL on error.
 */
OscarConnection *
aim_cloneconn(OscarSession *sess, OscarConnection *src)
{
	OscarConnection *conn;

	if (!(conn = aim_conn_getnext(sess)))
		return NULL;

	conn->fd = src->fd;
	conn->type = src->type;
	conn->subtype = src->subtype;
	conn->seqnum = src->seqnum;
	conn->internal = src->internal;
	conn->lastactivity = src->lastactivity;
	conn->sessv = src->sessv;
	aim_clonehandlers(sess, conn, src);

	if (src->inside) {
		/*
		 * XXX should clone this section as well, but since currently
		 * this function only gets called for some of that rendezvous
		 * crap, and not on SNAC connections, its probably okay for
		 * now.
		 *
		 */
	}

	return conn;
}

/**
 * Opens a new connection to the specified dest host of specified
 * type, using the proxy settings if available.  If @host is %NULL,
 * the connection is allocated and returned, but no connection
 * is made.
 *
 * FIXME: Return errors in a more sane way.
 *
 * @param sess Session to create connection in
 * @param type Type of connection to create
 */
OscarConnection *
oscar_connection_new(OscarSession *sess, int type)
{
	OscarConnection *conn;

	if (!(conn = aim_conn_getnext(sess)))
		return NULL;

	conn->sessv = (void *)sess;
	conn->type = type;

	conn->fd = -1;
	conn->status = 0;
	return conn;
}

/**
 * Determine if a connection is connecting.
 *
 * @param conn Connection to examine.
 * @return Returns nonzero if the connection is in the process of
 *         connecting (or if it just completed and
 *         aim_conn_completeconnect() has yet to be called on it).
 */
int
aim_conn_isconnecting(OscarConnection *conn)
{

	if (!conn)
		return 0;

	return !!(conn->status & AIM_CONN_STATUS_INPROGRESS);
}

/*
 * XXX this is nearly as ugly as proxyconnect().
 */
int
aim_conn_completeconnect(OscarSession *sess, OscarConnection *conn)
{
	if (!conn || (conn->fd == -1))
		return -1;

	if (!(conn->status & AIM_CONN_STATUS_INPROGRESS))
		return -1;

	fcntl(conn->fd, F_SETFL, 0);

	conn->status &= ~AIM_CONN_STATUS_INPROGRESS;

	/* Flush out the queues if there was something waiting for this conn  */
	aim_tx_flushqueue(sess);

	return 0;
}

OscarSession *
aim_conn_getsess(OscarConnection *conn)
{

	if (!conn)
		return NULL;

	return (OscarSession *)conn->sessv;
}

/**
 * No-op.  This sends an empty channel 5 SNAC.  WinAIM 4.x and higher
 * sends these _every minute_ to keep the connection alive.
 */
int
aim_flap_nop(OscarSession *sess, OscarConnection *conn)
{
	FlapFrame *fr;

	if (!(fr = flap_frame_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x05, 0)))
		return -ENOMEM;

	aim_tx_enqueue(sess, fr);

	/* clean out SNACs over 60sec old */
	aim_cleansnacs(sess, 60);

	return 0;
}
