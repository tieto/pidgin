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

/* This is defined in oscar.h, but only when !FAIM_INTERNAL, since the rest of
 * the library is not allowed to call it. */
faim_export void aim_conn_kill(OscarSession *sess, OscarConnection **deadconn);

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
faim_internal void aim_conn_addgroup(OscarConnection *conn, guint16 group)
{
	aim_conn_inside_t *ins = (aim_conn_inside_t *)conn->inside;
	struct snacgroup *sg;

	if (!(sg = malloc(sizeof(struct snacgroup))))
		return;

	gaim_debug_misc("oscar", "adding group 0x%04x\n", group);
	sg->group = group;

	sg->next = ins->groups;
	ins->groups = sg;

	return;
}

faim_export OscarConnection *aim_conn_findbygroup(OscarSession *sess, guint16 group)
{
	OscarConnection *cur;

	for (cur = sess->connlist; cur; cur = cur->next) {
		aim_conn_inside_t *ins = (aim_conn_inside_t *)cur->inside;
		struct snacgroup *sg;

		for (sg = ins->groups; sg; sg = sg->next) {
			if (sg->group == group)
				return cur;
		}
	}

	return NULL;
}

static void connkill_snacgroups(struct snacgroup **head)
{
	struct snacgroup *sg;

	for (sg = *head; sg; ) {
		struct snacgroup *tmp;

		tmp = sg->next;
		free(sg);
		sg = tmp;
	}

	*head = NULL;

	return;
}

static void connkill_rates(struct rateclass **head)
{
	struct rateclass *rc;

	for (rc = *head; rc; ) {
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

	*head = NULL;

	return;
}

static void connkill_real(OscarSession *sess, OscarConnection **deadconn)
{

	aim_rxqueue_cleanbyconn(sess, *deadconn);
	aim_tx_cleanqueue(sess, *deadconn);

	if ((*deadconn)->fd != -1)
		aim_conn_close(*deadconn);

	/*
	 * This will free ->internal if it necessary...
	 */
	if ((*deadconn)->type == AIM_CONN_TYPE_CHAT)
		aim_conn_kill_chat(sess, *deadconn);

	if ((*deadconn)->inside) {
		aim_conn_inside_t *inside = (aim_conn_inside_t *)(*deadconn)->inside;

		connkill_snacgroups(&inside->groups);
		connkill_rates(&inside->rates);

		free(inside);
	}

	free(*deadconn);
	*deadconn = NULL;

	return;
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

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x04, 0)))
		return -ENOMEM;

	aim_tx_enqueue(sess, fr);

	return 0;
}

/**
 * Clears out the connection list, killing remaining connections.
 *
 * @param sess Session to be cleared.
 */
static void aim_connrst(OscarSession *sess)
{

	if (sess->connlist) {
		OscarConnection *cur = sess->connlist, *tmp;

		/* Attempt to send the log-off packet */
		if (cur->type == AIM_CONN_TYPE_BOS)
			aim_flap_close(sess, cur);

		while (cur) {
			tmp = cur->next;
			aim_conn_close(cur);
			connkill_real(sess, &cur);
			cur = tmp;
		}
	}

	sess->connlist = NULL;

	return;
}

/**
 * Initializes and/or resets a connection structure to the default values.
 *
 * @param deadconn Connection to be reset.
 */
static void aim_conn_init(OscarConnection *deadconn)
{

	if (!deadconn)
		return;

	deadconn->fd = -1;
	deadconn->subtype = -1;
	deadconn->type = -1;
	deadconn->seqnum = 0;
	deadconn->lastactivity = 0;
	deadconn->forcedlatency = 0;
	deadconn->handlerlist = NULL;
	deadconn->priv = NULL;
	memset(deadconn->inside, 0, sizeof(aim_conn_inside_t));

	return;
}

/**
 * Allocate a new empty connection structure.
 *
 * @param sess Session
 * @return Returns the new connection structure.
 */
static OscarConnection *aim_conn_getnext(OscarSession *sess)
{
	OscarConnection *newconn;

	if (!(newconn = malloc(sizeof(OscarConnection))))
		return NULL;
	memset(newconn, 0, sizeof(OscarConnection));

	if (!(newconn->inside = malloc(sizeof(aim_conn_inside_t)))) {
		free(newconn);
		return NULL;
	}
	memset(newconn->inside, 0, sizeof(aim_conn_inside_t));

	aim_conn_init(newconn);

	newconn->next = sess->connlist;
	sess->connlist = newconn;

	return newconn;
}

/**
 * Close, clear, and free a connection structure. Should never be
 * called from within libfaim.
 *
 * @param sess Session for the connection.
 * @param deadconn Connection to be freed.
 */
faim_export void aim_conn_kill(OscarSession *sess, OscarConnection **deadconn)
{
	OscarConnection *cur, **prev;

	if (!deadconn || !*deadconn)
		return;

	for (prev = &sess->connlist; (cur = *prev); ) {
		if (cur == *deadconn) {
			*prev = cur->next;
			break;
		}
		prev = &cur->next;
	}

	if (!cur)
		return; /* oops */

	connkill_real(sess, &cur);

	return;
}

/**
 * Close (but not free) a connection.
 *
 * This leaves everything untouched except for clearing the 
 * handler list and setting the fd to -1 (used to recognize
 * dead connections).  It will also remove cookies if necessary.
 *
 * Why only if fd >= 3?  Seems rather implementation specific...
 * fd's do not have to be distributed in a particular order, do they?
 *
 * @param deadconn The connection to close.
 */
faim_export void aim_conn_close(OscarConnection *deadconn)
{
	aim_rxcallback_t userfunc;

	if (deadconn->fd >= 0)
		close(deadconn->fd);

	deadconn->fd = -1;

	if ((userfunc = aim_callhandler(deadconn->sessv, deadconn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNDEAD)))
		userfunc(deadconn->sessv, NULL, deadconn);

	if (deadconn->handlerlist)
		aim_clearhandlers(deadconn);

	return;
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
faim_export OscarConnection *aim_getconn_type(OscarSession *sess, int type)
{
	OscarConnection *cur;

	for (cur = sess->connlist; cur; cur = cur->next) {
		if ((cur->type == type) &&
				!(cur->status & AIM_CONN_STATUS_INPROGRESS))
			break;
	}

	return cur;
}

faim_export OscarConnection *aim_getconn_type_all(OscarSession *sess, int type)
{
	OscarConnection *cur;

	for (cur = sess->connlist; cur; cur = cur->next) {
		if (cur->type == type)
			break;
	}

	return cur;
}

/* If you pass -1 for the fd, you'll get what you ask for.  Gibberish. */
faim_export OscarConnection *aim_getconn_fd(OscarSession *sess, int fd)
{
	OscarConnection *cur;

	for (cur = sess->connlist; cur; cur = cur->next) {
		if (cur->fd == fd)
			break;
	}

	return cur;
}

/**
 * Clone an OscarConnection.
 *
 * A new connection is allocated, and the values are filled in
 * appropriately. Note that this function sets the new connnection's
 * ->priv pointer to be equal to that of its parent: only the pointer
 * is copied, not the data it points to.
 *
 * @param sess The session containing this connection.
 * @param src The connection to clone.
 * @return Returns a pointer to the new OscarConnection, or %NULL on error.
 */
faim_internal OscarConnection *aim_cloneconn(OscarSession *sess, OscarConnection *src)
{
	OscarConnection *conn;

	if (!(conn = aim_conn_getnext(sess)))
		return NULL;

	conn->fd = src->fd;
	conn->type = src->type;
	conn->subtype = src->subtype;
	conn->seqnum = src->seqnum;
	conn->priv = src->priv;
	conn->internal = src->internal;
	conn->lastactivity = src->lastactivity;
	conn->forcedlatency = src->forcedlatency;
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
faim_export OscarConnection *aim_newconn(OscarSession *sess, int type)
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
 * Searches @sess for the passed connection.
 *
 * @param sess Session in which to look.
 * @param conn Connection to look for.
 * @return Returns 1 if the passed connection is present, zero otherwise.
 */
faim_export int aim_conn_in_sess(OscarSession *sess, OscarConnection *conn)
{
	OscarConnection *cur;

	for (cur = sess->connlist; cur; cur = cur->next) {
		if (cur == conn)
			return 1;
	}

	return 0;
}

/**
 * Set a forced latency value for connection.  Basically causes
 * @newval seconds to be spent between transmits on a connection.
 *
 * This is my lame attempt at overcoming not understanding the rate
 * limiting.
 *
 * XXX: This should really be replaced with something that scales and
 * backs off like the real rate limiting does.
 *
 * @param conn Conn to set latency for.
 * @param newval Number of seconds to force between transmits.
 * @return Returns -1 if the connection does not exist, zero otherwise.
 */
faim_export int aim_conn_setlatency(OscarConnection *conn, int newval)
{

	if (!conn)
		return -1;

	conn->forcedlatency = newval;
	conn->lastactivity = 0; /* reset this just to make sure */

	return 0;
}

/**
 * Initializes a session structure by setting the initial values
 * stuff in the OscarSession struct.
 *
 * @param sess Session to initialize.
 * @param nonblocking Set to true if you want connections to be non-blocking.
 */
faim_export void aim_session_init(OscarSession *sess, guint8 nonblocking)
{

	if (!sess)
		return;

	memset(sess, 0, sizeof(OscarSession));
	aim_connrst(sess);
	sess->queue_outgoing = NULL;
	sess->queue_incoming = NULL;
	aim_initsnachash(sess);
	sess->msgcookies = NULL;
	sess->nonblocking = nonblocking;
	sess->modlistv = NULL;
	sess->snacid_next = 0x00000001;

	sess->locate.userinfo = NULL;
	sess->locate.torequest = NULL;
	sess->locate.requested = NULL;
	sess->locate.waiting_for_response = FALSE;
	sess->ssi.received_data = 0;
	sess->ssi.numitems = 0;
	sess->ssi.official = NULL;
	sess->ssi.local = NULL;
	sess->ssi.pending = NULL;
	sess->ssi.timestamp = (time_t)0;
	sess->ssi.waiting_for_ack = 0;
	sess->icq_info = NULL;
	sess->authinfo = NULL;
	sess->emailinfo = NULL;
	sess->peer_connections = NULL;

	/*
	 * This must always be set.  Default to the queue-based
	 * version for back-compatibility.
	 */
	aim_tx_setenqueue(sess, AIM_TX_QUEUED, NULL);

	/*
	 * Register all the modules for this session...
	 */
	aim__registermodule(sess, misc_modfirst); /* load the catch-all first */
	aim__registermodule(sess, service_modfirst);
	aim__registermodule(sess, locate_modfirst);
	aim__registermodule(sess, buddylist_modfirst);
	aim__registermodule(sess, msg_modfirst);
	aim__registermodule(sess, adverts_modfirst);
	aim__registermodule(sess, invite_modfirst);
	aim__registermodule(sess, admin_modfirst);
	aim__registermodule(sess, popups_modfirst);
	aim__registermodule(sess, bos_modfirst);
	aim__registermodule(sess, search_modfirst);
	aim__registermodule(sess, stats_modfirst);
	aim__registermodule(sess, translate_modfirst);
	aim__registermodule(sess, chatnav_modfirst);
	aim__registermodule(sess, chat_modfirst);
	aim__registermodule(sess, odir_modfirst);
	aim__registermodule(sess, bart_modfirst);
	/* missing 0x11 - 0x12 */
	aim__registermodule(sess, ssi_modfirst);
	/* missing 0x14 */
	aim__registermodule(sess, icq_modfirst); /* XXX - Make sure this isn't sent for AIM */
	/* missing 0x16 */
	aim__registermodule(sess, auth_modfirst);
	aim__registermodule(sess, email_modfirst);

	return;
}

/**
 * Logoff and deallocate a session.
 *
 * @param sess Session to kill
 */
faim_export void aim_session_kill(OscarSession *sess)
{
	aim_cleansnacs(sess, -1);

	aim_logoff(sess);

	aim__shutdownmodules(sess);

	return;
}

/**
 * Determine if a connection is connecting.
 *
 * @param conn Connection to examine.
 * @return Returns nonzero if the connection is in the process of
 *         connecting (or if it just completed and
 *         aim_conn_completeconnect() has yet to be called on it).
 */
faim_export int aim_conn_isconnecting(OscarConnection *conn)
{

	if (!conn)
		return 0;

	return !!(conn->status & AIM_CONN_STATUS_INPROGRESS);
}

/*
 * XXX this is nearly as ugly as proxyconnect().
 */
faim_export int aim_conn_completeconnect(OscarSession *sess, OscarConnection *conn)
{
	aim_rxcallback_t userfunc;

	if (!conn || (conn->fd == -1))
		return -1;

	if (!(conn->status & AIM_CONN_STATUS_INPROGRESS))
		return -1;

	fcntl(conn->fd, F_SETFL, 0);

	conn->status &= ~AIM_CONN_STATUS_INPROGRESS;

	if ((userfunc = aim_callhandler(sess, conn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNCOMPLETE)))
		userfunc(sess, NULL, conn);

	/* Flush out the queues if there was something waiting for this conn  */
	aim_tx_flushqueue(sess);

	return 0;
}

faim_export OscarSession *aim_conn_getsess(OscarConnection *conn)
{

	if (!conn)
		return NULL;

	return (OscarSession *)conn->sessv;
}

/**
 * Close -ALL- open connections.
 *
 * @param sess The session.
 * @return Zero.
 */
faim_export int aim_logoff(OscarSession *sess)
{
	aim_connrst(sess);  /* in case we want to connect again */

	return 0;
}

/**
 * No-op.  This sends an empty channel 5 SNAC.  WinAIM 4.x and higher
 * sends these _every minute_ to keep the connection alive.
 */
faim_export int aim_flap_nop(OscarSession *sess, OscarConnection *conn)
{
	FlapFrame *fr;

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x05, 0)))
		return -ENOMEM;

	aim_tx_enqueue(sess, fr);

	/* clean out SNACs over 60sec old */
	aim_cleansnacs(sess, 60);

	return 0;
}
