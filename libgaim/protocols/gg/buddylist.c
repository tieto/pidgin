/**
 * @file buddylist.c
 *
 * gaim
 *
 * Copyright (C) 2005  Bartosz Oler <bartosz@bzimage.us>
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


#include <libgadu.h>

#include "gg.h"
#include "gg-utils.h"
#include "buddylist.h"


/* void ggp_buddylist_send(GaimConnection *gc) {{{ */
void ggp_buddylist_send(GaimConnection *gc)
{
	GGPInfo *info = gc->proto_data;
	GaimAccount *account = gaim_connection_get_account(gc);

	GaimBuddyList *blist;
	GaimBlistNode *gnode, *cnode, *bnode;
	GaimBuddy *buddy;
	uin_t *userlist = NULL;
	gchar *types = NULL;
	int size = 0;

	if ((blist = gaim_get_blist()) == NULL)
	    return;

	for (gnode = blist->root; gnode != NULL; gnode = gnode->next) {
		if (!GAIM_BLIST_NODE_IS_GROUP(gnode))
			continue;

		for (cnode = gnode->child; cnode != NULL; cnode = cnode->next) {
			if (!GAIM_BLIST_NODE_IS_CONTACT(cnode))
				continue;

			for (bnode = cnode->child; bnode != NULL; bnode = bnode->next) {
				if (!GAIM_BLIST_NODE_IS_BUDDY(bnode))
					continue;

				buddy = (GaimBuddy *)bnode;

				if (buddy->account != account)
					continue;

				size++;
				userlist = (uin_t *) g_renew(uin_t, userlist, size);
				types    = (gchar *) g_renew(gchar, types, size);
				userlist[size - 1] = ggp_str_to_uin(buddy->name);
				types[size - 1]    = GG_USER_NORMAL;
				gaim_debug_info("gg", "ggp_buddylist_send: adding %d\n",
						userlist[size - 1]);
			}
		}
	}

	if (userlist) {
		int ret = gg_notify_ex(info->session, userlist, types, size);
		g_free(userlist);
		g_free(types);

		gaim_debug_info("gg", "send: ret=%d; size=%d\n", ret, size);
	}
}
/* }}} */

/* void ggp_buddylist_load(GaimConnection *gc, char *buddylist) {{{ */
void ggp_buddylist_load(GaimConnection *gc, char *buddylist)
{
	GaimBuddy *buddy;
	GaimGroup *group;
	gchar **users_tbl;
	int i;

	/*
	 * XXX: Limit of entries in a buddylist that will be imported.
	 */
	users_tbl = g_strsplit(buddylist, "\r\n", 200);

	for (i = 0; users_tbl[i] != NULL; i++) {
		gchar **data_tbl;
		gchar *name, *show, *g;

		if (strlen(users_tbl[i]) == 0)
			continue;

		data_tbl = g_strsplit(users_tbl[i], ";", 8);

		show = charset_convert(data_tbl[3], "CP1250", "UTF-8");
		name = data_tbl[6];
		if (NULL == name || '\0' == *name) {
			gaim_debug_warning("gg",
				"Something is wrong on line %d of the buddylist. Skipping.\n",
				i + 1);
			continue;
		}

		if (NULL == show || '\0' == *show) {
			show = g_strdup(name);
		}

		gaim_debug_info("gg", "got buddy: name=%s show=%s\n", name, show);

		if (gaim_find_buddy(gaim_connection_get_account(gc), name)) {
			g_free(show);
			g_strfreev(data_tbl);
			continue;
		}

		g = g_strdup("Gadu-Gadu");

		if (strlen(data_tbl[5])) {
			/* Hard limit to at most 50 groups */
			gchar **group_tbl = g_strsplit(data_tbl[5], ",", 50);
			if (strlen(group_tbl[0]) > 0) {
				g_free(g);
				g = g_strdup(group_tbl[0]);
			}
			g_strfreev(group_tbl);
		}

		buddy = gaim_buddy_new(gaim_connection_get_account(gc), name,
				       strlen(show) ? show : NULL);

		if (!(group = gaim_find_group(g))) {
			group = gaim_group_new(g);
			gaim_blist_add_group(group, NULL);
		}

		gaim_blist_add_buddy(buddy, NULL, group, NULL);
		g_free(g);

		g_free(show);
		g_strfreev(data_tbl);
	}
	g_strfreev(users_tbl);

	ggp_buddylist_send(gc);

}
/* }}} */

/* void ggp_buddylist_offline(GaimConnection *gc) {{{ */
void ggp_buddylist_offline(GaimConnection *gc)
{
	GaimAccount *account = gaim_connection_get_account(gc);
	GaimBuddyList *blist;
	GaimBlistNode *gnode, *cnode, *bnode;
	GaimBuddy *buddy;

	if ((blist = gaim_get_blist()) == NULL)
		return;

	for (gnode = blist->root; gnode != NULL; gnode = gnode->next) {
		if (!GAIM_BLIST_NODE_IS_GROUP(gnode))
			continue;

		for (cnode = gnode->child; cnode != NULL; cnode = cnode->next) {
			if (!GAIM_BLIST_NODE_IS_CONTACT(cnode))
				continue;

			for (bnode = cnode->child; bnode != NULL; bnode = bnode->next) {
				if (!GAIM_BLIST_NODE_IS_BUDDY(bnode))
					continue;

				buddy = (GaimBuddy *)bnode;

				if (buddy->account != account)
					continue;

				gaim_prpl_got_user_status(
					gaim_connection_get_account(gc),
					buddy->name, "offline", NULL);

				gaim_debug_info("gg",
					"ggp_buddylist_offline: gone: %s\n",
					buddy->name);
			}
		}
	}
}
/* }}} */

/* char *ggp_buddylist_dump(GaimAccount *account) {{{ */
char *ggp_buddylist_dump(GaimAccount *account)
{
	GaimBuddyList *blist;
	GaimBlistNode *gnode, *cnode, *bnode;
	GaimGroup *group;
	GaimBuddy *buddy;

	char *buddylist = g_strdup("");
	char *ptr;

	if ((blist = gaim_get_blist()) == NULL)
		return NULL;

	for (gnode = blist->root; gnode != NULL; gnode = gnode->next) {
		if (!GAIM_BLIST_NODE_IS_GROUP(gnode))
			continue;

		group = (GaimGroup *)gnode;

		for (cnode = gnode->child; cnode != NULL; cnode = cnode->next) {
			if (!GAIM_BLIST_NODE_IS_CONTACT(cnode))
				continue;

			for (bnode = cnode->child; bnode != NULL; bnode = bnode->next) {
				gchar *newdata, *name, *alias, *gname;
				gchar *cp_alias, *cp_gname;

				if (!GAIM_BLIST_NODE_IS_BUDDY(bnode))
					continue;

				buddy = (GaimBuddy *)bnode;
				if (buddy->account != account)
					continue;

				name = buddy->name;
				alias = buddy->alias ? buddy->alias : buddy->name;
				gname = group->name;

				cp_gname = charset_convert(gname, "UTF-8", "CP1250");
				cp_alias = charset_convert(alias, "UTF-8", "CP1250");
				newdata = g_strdup_printf(
						"%s;%s;%s;%s;%s;%s;%s;%s%s\r\n",
						cp_alias, cp_alias, cp_alias, cp_alias,
						"", cp_gname, name, "", "");

				ptr = buddylist;
				buddylist = g_strconcat(ptr, newdata, NULL);

				g_free(newdata);
				g_free(ptr);
				g_free(cp_gname);
				g_free(cp_alias);
			}
		}
	}

	return buddylist;
}
/* }}} */


/* vim: set ts=8 sts=0 sw=8 noet: */
