/*
 * gaim
 *
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
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
#include "internal.h"
#include "conversation.h"
#include "debug.h"
#include "multi.h"
#include "notify.h"
#include "prpl.h"
#include "request.h"
#include "util.h"

/* XXX */
#include "gtkinternal.h"
#include "gtkconv.h"
#include "ui.h"

const char *
gaim_prpl_num_to_id(GaimProtocol protocol)
{
	g_return_val_if_fail(protocol >= 0 && protocol < GAIM_PROTO_UNTAKEN, NULL);

	switch (protocol)
	{
		case GAIM_PROTO_TOC:      return "prpl-toc";     break;
		case GAIM_PROTO_OSCAR:    return "prpl-oscar";   break;
		case GAIM_PROTO_YAHOO:    return "prpl-yahoo";   break;
		case GAIM_PROTO_ICQ:      return "prpl-icq";     break;
		case GAIM_PROTO_MSN:      return "prpl-msn";     break;
		case GAIM_PROTO_IRC:      return "prpl-irc";     break;
		case GAIM_PROTO_JABBER:   return "prpl-jabber";  break;
		case GAIM_PROTO_NAPSTER:  return "prpl-napster"; break;
		case GAIM_PROTO_ZEPHYR:   return "prpl-zephyr";  break;
		case GAIM_PROTO_GADUGADU: return "prpl-gg";      break;
		case GAIM_PROTO_MOO:      return "prpl-moo";     break;
		case GAIM_PROTO_TREPIA:   return "prpl-trepia";  break;
		case GAIM_PROTO_BLOGGER:  return "prpl-blogger"; break;

		default:
			break;
	}

	return NULL;
}

GaimProtocol
gaim_prpl_id_to_num(const char *id)
{
	g_return_val_if_fail(id != NULL, -1);

	if      (!strcmp(id, "prpl-toc"))     return GAIM_PROTO_TOC;
	else if (!strcmp(id, "prpl-oscar"))   return GAIM_PROTO_OSCAR;
	else if (!strcmp(id, "prpl-yahoo"))   return GAIM_PROTO_YAHOO;
	else if (!strcmp(id, "prpl-icq"))     return GAIM_PROTO_ICQ;
	else if (!strcmp(id, "prpl-msn"))     return GAIM_PROTO_MSN;
	else if (!strcmp(id, "prpl-irc"))     return GAIM_PROTO_IRC;
	else if (!strcmp(id, "prpl-jabber"))  return GAIM_PROTO_JABBER;
	else if (!strcmp(id, "prpl-napster")) return GAIM_PROTO_NAPSTER;
	else if (!strcmp(id, "prpl-zephyr"))  return GAIM_PROTO_ZEPHYR;
	else if (!strcmp(id, "prpl-gg"))      return GAIM_PROTO_GADUGADU;
	else if (!strcmp(id, "prpl-moo"))     return GAIM_PROTO_MOO;
	else if (!strcmp(id, "prpl-trepia"))  return GAIM_PROTO_TREPIA;
	else if (!strcmp(id, "prpl-blogger")) return GAIM_PROTO_BLOGGER;

	return -1;
}

GaimPlugin *
gaim_find_prpl(GaimProtocol type)
{
	GList *l;
	GaimPlugin *plugin;

	for (l = gaim_plugins_get_protocols(); l != NULL; l = l->next) {
		plugin = (GaimPlugin *)l->data;

		/* Just In Case (TM) */
		if (GAIM_IS_PROTOCOL_PLUGIN(plugin)) {

			if (GAIM_PLUGIN_PROTOCOL_INFO(plugin)->protocol == type)
				return plugin;
		}
	}

	return NULL;
}

struct got_add {
	GaimConnection *gc;
	char *who;
	char *alias;
};

static void dont_add(struct got_add *ga)
{
	g_free(ga->who);
	if (ga->alias)
		g_free(ga->alias);
	g_free(ga);
}

static void do_add(struct got_add *ga)
{
	if (g_list_find(gaim_connections_get_all(), ga->gc))
		show_add_buddy(ga->gc, ga->who, NULL, ga->alias);
	dont_add(ga);
}

void show_got_added(GaimConnection *gc, const char *id,
		    const char *who, const char *alias, const char *msg)
{
	GaimAccount *account;
	char buf[BUF_LONG];
	struct got_add *ga;
	GaimBuddy *b;

	account = gaim_connection_get_account(gc);
	b = gaim_find_buddy(gc->account, who);

	ga = g_new0(struct got_add, 1);
	ga->gc    = gc;
	ga->who   = g_strdup(who);
	ga->alias = (alias ? g_strdup(alias) : NULL);


	g_snprintf(buf, sizeof(buf), _("%s%s%s%s has made %s his or her buddy%s%s%s"),
		   who,
		   alias ? " (" : "",
		   alias ? alias : "",
		   alias ? ")" : "",
		   (id
			? id
			: (gaim_connection_get_display_name(gc)
			   ? gaim_connection_get_display_name(gc)
			   : gaim_account_get_username(account))),
		   msg ? ": " : ".",
		   msg ? msg : "",
		   b ? "" : _("\n\nDo you wish to add him or her to your buddy list?"));

	if (b) {
		gaim_notify_info(NULL, NULL, _("Gaim - Information"), buf);
	}
	else
		gaim_request_action(NULL, NULL, _("Add buddy to your list?"), buf,
							0, ga, 2,
							_("Add"), G_CALLBACK(do_add),
							_("Cancel"), G_CALLBACK(dont_add));
}

