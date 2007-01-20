/**
 * @file sendqueue.c
 *
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
 */

#include "connection.h"
#include "debug.h"
#include "internal.h"
#include "notify.h"
#include "prefs.h"
#include "request.h"

#include "header_info.h"
#include "qq_proxy.h"
#include "sendqueue.h"

#define QQ_RESEND_MAX               8	/* max resend per packet */

typedef struct _gc_and_packet gc_and_packet;

struct _gc_and_packet {
	GaimConnection *gc;
	qq_sendpacket *packet;
};

/* Remove a packet with send_seq from sendqueue */
void qq_sendqueue_remove(qq_data *qd, guint16 send_seq)
{
	GList *list;
	qq_sendpacket *p;

	list = qd->sendqueue;
	while (list != NULL) {
		p = (qq_sendpacket *) (list->data);
		if (p->send_seq == send_seq) {
			qd->sendqueue = g_list_remove(qd->sendqueue, p);
			g_free(p->buf);
			g_free(p);
			break;
		}
		list = list->next;
	}
}
		
/* clean up sendqueue and free all contents */
void qq_sendqueue_free(qq_data *qd)
{
	qq_sendpacket *p;
	gint i;

	i = 0;
	while (qd->sendqueue != NULL) {
		p = (qq_sendpacket *) (qd->sendqueue->data);
		qd->sendqueue = g_list_remove(qd->sendqueue, p);
		g_free(p->buf);
		g_free(p);
		i++;
	}
	gaim_debug(GAIM_DEBUG_INFO, "QQ", "%d packets in sendqueue are freed!\n", i);
}

/* FIXME We shouldn't be dropping packets, but for now we have to because
 * somewhere we're generating invalid packets that the server won't ack.
 * Given enough time, a buildup of those packets would crash the client. */
gboolean qq_sendqueue_timeout_callback(gpointer data)
{
	GaimConnection *gc;
	qq_data *qd;
	GList *list;
	qq_sendpacket *p;
	time_t now;
	gint wait_time;

	gc = (GaimConnection *) data;
	qd = (qq_data *) gc->proto_data;
	now = time(NULL);
	list = qd->sendqueue;

	/* empty queue, return TRUE so that timeout continues functioning */
	if (qd->sendqueue == NULL)
		return TRUE;

	while (list != NULL) {	/* remove all packet whose resend_times == -1 */
		p = (qq_sendpacket *) list->data;
		if (p->resend_times == -1) {	/* to remove */
			qd->sendqueue = g_list_remove(qd->sendqueue, p);
			g_free(p->buf);
			g_free(p);
			list = qd->sendqueue;
		} else {
			list = list->next;
		}
	}

	list = qd->sendqueue;
	while (list != NULL) {
		p = (qq_sendpacket *) list->data;
		if (p->resend_times == QQ_RESEND_MAX) {	/* reach max */
			switch (p->cmd) {
			case QQ_CMD_KEEP_ALIVE:
				if (qd->logged_in) {
					gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Connection lost!\n");
					gaim_connection_error(gc, _("Connection lost"));
					qd->logged_in = FALSE;
				}
				p->resend_times = -1;
				break;
			case QQ_CMD_LOGIN:
			case QQ_CMD_REQUEST_LOGIN_TOKEN:
				if (!qd->logged_in)	/* cancel login progress */
					gaim_connection_error(gc, _("Login failed, no reply"));
				p->resend_times = -1;
				break;
			default:{
				gaim_debug(GAIM_DEBUG_WARNING, "QQ", 
					"%s packet sent %d times but not acked. Not resending it.\n", 
					qq_get_cmd_desc(p->cmd), QQ_RESEND_MAX);
				}
				p->resend_times = -1;
			}
		} else {	/* resend_times < QQ_RESEND_MAX, so sent it again */
			wait_time = (gint) (QQ_SENDQUEUE_TIMEOUT / 1000);
			if (difftime(now, p->sendtime) > (wait_time * (p->resend_times + 1))) {
				qq_proxy_write(qd, p->buf, p->len);
				p->resend_times++;
				gaim_debug(GAIM_DEBUG_INFO,
					   "QQ", "<<< [%05d] send again for %d times!\n", 
					   p->send_seq, p->resend_times);
			}
		}
		list = list->next;
	}
	return TRUE;		/* if we return FALSE, the timeout callback stops functioning */
}
