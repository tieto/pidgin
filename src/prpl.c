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
#include "debug.h"
#include "multi.h"
#include "notify.h"
#include "prpl.h"
#include "request.h"
#include "ui.h"
#include "util.h"

/* XXX */
#include "gtkconv.h"

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

struct icon_data {
	GaimConnection *gc;
	char *who;
	void *data;
	int len;
};

static GList *icons = NULL;

static gint find_icon_data(gconstpointer a, gconstpointer b)
{
	const struct icon_data *x = a;
	const struct icon_data *y = b;

	return ((x->gc != y->gc) || gaim_utf8_strcasecmp(x->who, y->who));
}

void set_icon_data(GaimConnection *gc, const char *who, void *data, int len)
{
	GaimConversation *conv;
	struct icon_data tmp;
	GList *l;
	struct icon_data *id;
	struct buddy *b;
	/* i'm going to vent here a little bit about normalize().  normalize()
	 * uses a static buffer, so when we call functions that use normalize() from
	 * functions that use normalize(), whose parameters are the result of running
	 * normalize(), bad things happen.  To prevent some of this, we're going
	 * to make a copy of what we get from normalize(), so we know nothing else
	 * touches it, and buddy icons don't go to the wrong person.  Some day I
	 * will kill normalize(), and dance on its grave.  That will be a very happy
	 * day for everyone.
	 *                                    --ndw
	 */
	char *realwho = g_strdup(normalize(who));
	tmp.gc = gc;
	tmp.who = realwho;
	tmp.data=NULL;
	tmp.len = 0;
	l = g_list_find_custom(icons, &tmp, find_icon_data);
	id = l ? l->data : NULL;

	if (id) {
		g_free(id->data);
		if (!data) {
			icons = g_list_remove(icons, id);
			g_free(id->who);
			g_free(id);
			g_free(realwho);
			return;
		}
	} else if (data) {
		id = g_new0(struct icon_data, 1);
		icons = g_list_append(icons, id);
		id->gc = gc;
		id->who = g_strdup(realwho);
	} else {
		g_free(realwho);
		return;
	}

	gaim_debug(GAIM_DEBUG_MISC, "prpl", "Got icon for %s (length %d)\n",
			   realwho, len);

	id->data = g_memdup(data, len);
	id->len = len;

	/* Update the buddy icon for this user. */
	conv = gaim_find_conversation(realwho);

	/* XXX Buddy Icon should probalby be part of struct buddy instead of this weird global
	 * linked list stuff. */

	if ((b = gaim_find_buddy(gc->account, realwho)) != NULL) {
		char *random = g_strdup_printf("%x", g_random_int());
		char *filename = g_build_filename(gaim_user_dir(), "icons", random,
				NULL);
		char *dirname = g_build_filename(gaim_user_dir(), "icons", NULL);
		char *old_icon = gaim_buddy_get_setting(b, "buddy_icon");
		FILE *file = NULL;

		g_free(random);

		if(!g_file_test(dirname, G_FILE_TEST_IS_DIR)) {
			gaim_debug(GAIM_DEBUG_INFO, "buddy icons",
					   "Creating icon cache directory.\n");

			if(mkdir(dirname, S_IRUSR | S_IWUSR | S_IXUSR) < 0)
				gaim_debug(GAIM_DEBUG_ERROR, "buddy icons",
						   "Unable to create directory %s: %s\n",
						   dirname, strerror(errno));
		}

		g_free(dirname);

		file = fopen(filename, "wb");
		if (file) {
			fwrite(data, 1, len, file);
			fclose(file);
		}

		if(old_icon) {
			unlink(old_icon);
			g_free(old_icon);
		}

		gaim_buddy_set_setting(b, "buddy_icon", filename);
		gaim_blist_save();

		g_free(filename);

		gaim_blist_update_buddy_icon(b);
	}

	if (conv != NULL && gaim_conversation_get_gc(conv) == gc)
		gaim_gtkconv_update_buddy_icon(conv);

	g_free(realwho);
}

void remove_icon_data(GaimConnection *gc)
{
	GList *list = icons;
	struct icon_data *id;

	while (list) {
		id = list->data;
		if (id->gc == gc) {
			g_free(id->data);
			g_free(id->who);
			list = icons = g_list_remove(icons, id);
			g_free(id);
		} else
			list = list->next;
	}
}

void *get_icon_data(GaimConnection *gc, const char *who, int *len)
{
	struct icon_data tmp = { gc, normalize(who), NULL, 0 };
	GList *l = g_list_find_custom(icons, &tmp, find_icon_data);
	struct icon_data *id = l ? l->data : NULL;

	if (id) {
		*len = id->len;
		return id->data;
	}

	*len = 0;
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
	struct buddy *b;

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

