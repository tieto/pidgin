/*
 * gaim
 *
 * Copyright (C) 2003,      Sean Egan    <sean.egan@binghamton.edu>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <unistd.h>
#else
#include <direct.h>
#endif
#include <ctype.h>
#include "gaim.h"
#include "prpl.h"
#include "list.h"

#ifdef _WIN32
#include "win32dep.h"
#endif

#define PATHSIZE 1024

struct gaim_buddy_list *gaimbuddylist = NULL;
static struct gaim_blist_ui_ops *blist_ui_ops = NULL;

/*****************************************************************************
 * Private Utility functions                                                 *
 *****************************************************************************/
static GaimBlistNode *gaim_blist_get_last_sibling(GaimBlistNode *node)
{
	GaimBlistNode *n = node;
	if (!n)
		return NULL;
	while (n->next)
		n = n->next;
	return n;
}
static GaimBlistNode *gaim_blist_get_last_child(GaimBlistNode *node)
{
	if (!node)
		return NULL;
	return gaim_blist_get_last_sibling(node->child);
}

/*****************************************************************************
 * Public API functions                                                      *
 *****************************************************************************/

struct gaim_buddy_list *gaim_blist_new()
{
	struct gaim_buddy_list *gbl = g_new0(struct gaim_buddy_list, 1);

	gbl->ui_ops = gaim_get_blist_ui_ops();

	if (gbl->ui_ops != NULL && gbl->ui_ops->new_list != NULL)
		gbl->ui_ops->new_list(gbl);

	return gbl;
}

void
gaim_set_blist(struct gaim_buddy_list *list)
{
	gaimbuddylist = list;
}

struct gaim_buddy_list *
gaim_get_blist(void)
{
	return gaimbuddylist;
}

void  gaim_blist_show () 
{
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	if (ops)
		ops->show(gaimbuddylist);
}

void gaim_blist_destroy()
{
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	if (ops)
		ops->destroy(gaimbuddylist);
}

void  gaim_blist_set_visible (gboolean show)
{
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	if (ops)
		ops->set_visible(gaimbuddylist, show);
}

void  gaim_blist_update_buddy_status (struct buddy *buddy, int status)
{
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	buddy->uc = status;
	if (ops)
		ops->update(gaimbuddylist, (GaimBlistNode*)buddy);
}

static gboolean presence_update_timeout_cb(struct buddy *buddy) {
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;

	if(buddy->present == GAIM_BUDDY_SIGNING_ON)
		buddy->present = GAIM_BUDDY_ONLINE;
	else if(buddy->present == GAIM_BUDDY_SIGNING_OFF)
		buddy->present = GAIM_BUDDY_OFFLINE;

	buddy->timer = 0;

	if (ops)
		ops->update(gaimbuddylist, (GaimBlistNode*)buddy);

	return FALSE;
}

void gaim_blist_update_buddy_presence(struct buddy *buddy, int presence) {
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	gboolean do_timer = FALSE;

	if (!GAIM_BUDDY_IS_ONLINE(buddy) && presence) {
		buddy->present = GAIM_BUDDY_SIGNING_ON;
		do_timer = TRUE;
	} else if(GAIM_BUDDY_IS_ONLINE(buddy) && !presence) {
		buddy->present = GAIM_BUDDY_SIGNING_OFF;
		do_timer = TRUE;
	}

	if(do_timer) {
		if(buddy->timer > 0)
			g_source_remove(buddy->timer);
		buddy->timer = g_timeout_add(10000, (GSourceFunc)presence_update_timeout_cb, buddy);
	}

	if (ops)
		ops->update(gaimbuddylist, (GaimBlistNode*)buddy);
}


void  gaim_blist_update_buddy_idle (struct buddy *buddy, int idle)
{
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	buddy->idle = idle;
	if (ops)
		ops->update(gaimbuddylist, (GaimBlistNode*)buddy);
}
void  gaim_blist_update_buddy_evil (struct buddy *buddy, int warning)
{
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	buddy->evil = warning;
	if (ops)
		ops->update(gaimbuddylist,(GaimBlistNode*)buddy);
}
void gaim_blist_update_buddy_icon(struct buddy *buddy) {
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	if(ops)
		ops->update(gaimbuddylist, (GaimBlistNode*)buddy);
}
void  gaim_blist_rename_buddy (struct buddy *buddy, const char *name)
{
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
       	g_free(buddy->name);
	buddy->name = g_strdup(name);
	if (ops)
		ops->update(gaimbuddylist, (GaimBlistNode*)buddy);
}
void  gaim_blist_alias_buddy (struct buddy *buddy, const char *alias)
{
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	struct gaim_conversation *conv;

	g_free(buddy->alias);

	buddy->alias = g_strdup(alias);

	if (ops)
		ops->update(gaimbuddylist, (GaimBlistNode*)buddy);

	conv = gaim_find_conversation_with_account(buddy->name, buddy->account);

	if (conv)
		gaim_conversation_autoset_title(conv);
}

void gaim_blist_rename_group(struct group *group, const char *name)
{
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	g_free(group->name);
	group->name = g_strdup(name);
	if (ops)
		ops->update(gaimbuddylist, (GaimBlistNode*)group);
}
struct buddy *gaim_buddy_new(struct gaim_account *account, const char *screenname, const char *alias)
{
	struct buddy *b;
	struct gaim_blist_ui_ops *ops;

	b = g_new0(struct buddy, 1);
	b->account = account;
	b->name  = g_strdup(screenname);
	b->alias = g_strdup(alias);
	b->settings = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	((GaimBlistNode*)b)->type = GAIM_BLIST_BUDDY_NODE;

	ops = gaim_get_blist_ui_ops();

	if (ops != NULL && ops->new_node != NULL)
		ops->new_node((GaimBlistNode *)b);

	return b;
}
void  gaim_blist_add_buddy (struct buddy *buddy, struct group *group, GaimBlistNode *node)
{
	GaimBlistNode *n = node, *bnode = (GaimBlistNode*)buddy;
	struct group *g = group;
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	gboolean save = FALSE;

	if (!n) {
		if (!g) {
			g = gaim_group_new(_("Buddies"));
			gaim_blist_add_group(g, NULL);
		}
		n = gaim_blist_get_last_child((GaimBlistNode*)g);
	} else {
		g = (struct group*)n->parent;
	}

	/* if we're moving to overtop of ourselves, do nothing */
	if(bnode == n)
		return;

	if (bnode->parent) {
		/* This buddy was already in the list and is
		 * being moved.
		 */
		if(bnode->next)
			bnode->next->prev = bnode->prev;
		if(bnode->prev)
			bnode->prev->next = bnode->next;
		if(bnode->parent->child == bnode)
			bnode->parent->child = bnode->next;

		ops->remove(gaimbuddylist, bnode);

		if (bnode->parent != ((GaimBlistNode*)g))
			serv_move_buddy(buddy, (struct group*)bnode->parent, g);
		save = TRUE;
	}

	if (n) {
		if(n->next)
			n->next->prev = (GaimBlistNode*)buddy;
		((GaimBlistNode*)buddy)->next = n->next;
		((GaimBlistNode*)buddy)->prev = n;
		((GaimBlistNode*)buddy)->parent = n->parent;
		n->next = (GaimBlistNode*)buddy;
	} else {
		((GaimBlistNode*)g)->child = (GaimBlistNode*)buddy;
		((GaimBlistNode*)buddy)->next = NULL;
		((GaimBlistNode*)buddy)->prev = NULL;
		((GaimBlistNode*)buddy)->parent = (GaimBlistNode*)g;
	}

	if (ops)
		ops->update(gaimbuddylist, (GaimBlistNode*)buddy);
	if (save)
		gaim_blist_save();
}

struct group *gaim_group_new(const char *name)
{
	struct group *g = gaim_find_group(name);

	if (!g) {
		struct gaim_blist_ui_ops *ops;
		g= g_new0(struct group, 1);
		g->name = g_strdup(name);
		((GaimBlistNode*)g)->type = GAIM_BLIST_GROUP_NODE;

		ops = gaim_get_blist_ui_ops();

		if (ops != NULL && ops->new_node != NULL)
			ops->new_node((GaimBlistNode *)g);

	}
	return g;
}

void  gaim_blist_add_group (struct group *group, GaimBlistNode *node)
{
	struct gaim_blist_ui_ops *ops;
	GaimBlistNode *gnode = (GaimBlistNode*)group;
	gboolean save = FALSE;

	if (!gaimbuddylist)
		gaimbuddylist = gaim_blist_new();
	ops = gaimbuddylist->ui_ops;

	if (!gaimbuddylist->root) {
		gaimbuddylist->root = gnode;
		return;
	}

	if (!node)
		node = gaim_blist_get_last_sibling(gaimbuddylist->root);

	/* if we're moving to overtop of ourselves, do nothing */
	if(gnode == node)
		return;

	if (gaim_find_group(group->name)) {
		/* This is just being moved */

		ops->remove(gaimbuddylist, (GaimBlistNode*)group);

		if(gnode == gaimbuddylist->root)
			gaimbuddylist->root = gnode->next;
		if(gnode->prev)
			gnode->prev->next = gnode->next;
		if(gnode->next)
			gnode->next->prev = gnode->prev;

		save = TRUE;
	}

	gnode->next = node->next;
	gnode->prev = node;
	if(node->next)
		node->next->prev = gnode;
	node->next = gnode;

	if (ops) {
		ops->update(gaimbuddylist, gnode);
		for(node = gnode->child; node; node = node->next)
			ops->update(gaimbuddylist, node);
	}
	if (save)
		gaim_blist_save();
}

void  gaim_blist_remove_buddy (struct buddy *buddy)
{
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;

	GaimBlistNode *gnode, *node = (GaimBlistNode*)buddy;
	struct group *group;

	gnode = node->parent;
	group = (struct group *)gnode;

	if(gnode->child == node)
		gnode->child = node->next;
	if (node->prev)
		node->prev->next = node->next;
	if (node->next)
		node->next->prev = node->prev;

	ops->remove(gaimbuddylist, node);
	g_hash_table_destroy(buddy->settings);
	g_free(buddy->name);
	g_free(buddy->alias);
	g_free(buddy);
}

void  gaim_blist_remove_group (struct group *group)
{
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	GaimBlistNode *node = (GaimBlistNode*)group;

	if(node->child) {
		char *buf;
		int count = 0;
		GaimBlistNode *child = node->child;

		while(child) {
			count++;
			child = child->next;
		}

		buf = g_strdup_printf(_("%d buddies from group %s were not "
					"removed because their accounts were not logged in.  These "
					"buddies, and the group were not removed.\n"),
				count, group->name);
		do_error_dialog(_("Group Not Removed"), buf, GAIM_ERROR);
		g_free(buf);
		return;
	}

	if(gaimbuddylist->root == node)
		gaimbuddylist->root = node->next;
	if (node->prev)
		node->prev->next = node->next;
	if (node->next)
		node->next->prev = node->prev;

	ops->remove(gaimbuddylist, node);
	g_free(group->name);
	g_free(group);
}

char *gaim_get_buddy_alias_only(struct buddy *b) {
        if(!b)
                return NULL;
        if(b->alias && b->alias[0])
                return b->alias;
        else if((misc_options & OPT_MISC_USE_SERVER_ALIAS) && b->server_alias)
                return b->server_alias;
        return NULL;
}

char *  gaim_get_buddy_alias (struct buddy *buddy)
{
	char *ret = gaim_get_buddy_alias_only(buddy);
        if(!ret)
                return buddy ? buddy->name : _("Unknown");
        return ret;

}

struct buddy *gaim_find_buddy(struct gaim_account *account, const char *name)
{
	GaimBlistNode *group;
	GaimBlistNode *buddy;
	char *norm_name = g_strdup(normalize(name));

	if (!gaimbuddylist)
		return NULL;

	group = gaimbuddylist->root;
	while (group) {
		buddy = group->child;
		while (buddy) {
			if (!gaim_utf8_strcasecmp(normalize(((struct buddy*)buddy)->name), norm_name) && account == ((struct buddy*)buddy)->account) {
				g_free(norm_name);
				return (struct buddy*)buddy;
			}
			buddy = buddy->next;
		}
		group = group->next;
	}
	g_free(norm_name);
	return NULL;
}

struct group *gaim_find_group(const char *name)
{
	GaimBlistNode *node;
	if (!gaimbuddylist)
		return NULL;
	node = gaimbuddylist->root;
	while(node) {
		if (!strcmp(((struct group*)node)->name, name))
			return (struct group*)node;
		node = node->next;
	}
	return NULL;
}
struct group *gaim_find_buddys_group(struct buddy *buddy)
{
	if (!buddy)
		return NULL;
	return (struct group*)(((GaimBlistNode*)buddy)->parent);
}

GSList *gaim_group_get_accounts(struct group *g)
{
	GSList *l = NULL;
	GaimBlistNode *child = ((GaimBlistNode *)g)->child;

	while (child) {
		if (!g_slist_find(l, ((struct buddy*)child)->account))
			l = g_slist_append(l, ((struct buddy*)child)->account);
		child = child->next;
	}
	return l;
}

void gaim_blist_remove_account(struct gaim_account *account)
{
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	GaimBlistNode *group = gaimbuddylist->root;
	GaimBlistNode *buddy;
	if (!gaimbuddylist)
		return;
	while (group) {
		buddy = group->child;
		while (buddy) {
			if (account == ((struct buddy*)buddy)->account) {
				((struct buddy*)buddy)->present = GAIM_BUDDY_OFFLINE;
				if(ops)
					ops->remove(gaimbuddylist, buddy);
			}
			buddy = buddy->next;
		}
		group = group->next;
	}
}

void parse_toc_buddy_list(struct gaim_account *account, char *config)
{
	char *c;
	char current[256];
	GList *bud = NULL;


	if (config != NULL) {

		/* skip "CONFIG:" (if it exists) */
		c = strncmp(config + 6 /* sizeof(struct sflap_hdr) */ , "CONFIG:", strlen("CONFIG:")) ?
			strtok(config, "\n") :
			strtok(config + 6 /* sizeof(struct sflap_hdr) */  + strlen("CONFIG:"), "\n");
		do {
			if (c == NULL)
				break;
			if (*c == 'g') {
				char *utf8 = NULL;
				utf8 = gaim_try_conv_to_utf8(c + 2);
				if (utf8 == NULL) {
					g_strlcpy(current, _("Invalid Groupname"), sizeof(current));
				} else {
					g_strlcpy(current, utf8, sizeof(current));
					g_free(utf8);
				}
				if (!gaim_find_group(current)) {
					struct group *g = gaim_group_new(current);
					gaim_blist_add_group(g, NULL);
				}
			} else if (*c == 'b') { /*&& !gaim_find_buddy(user, c + 2)) {*/
				char nm[80], sw[388], *a, *utf8 = NULL;

				if ((a = strchr(c + 2, ':')) != NULL) {
					*a++ = '\0';		/* nul the : */
				}

				g_strlcpy(nm, c + 2, sizeof(nm));
				if (a) {
					utf8 = gaim_try_conv_to_utf8(a);
					if (utf8 == NULL) {
						debug_printf ("Failed to convert alias for '%s' to UTF-8\n", nm);
					}
				}
				if (utf8 == NULL) {
					sw[0] = '\0';
				} else {
					/* This can leave a partial sequence at the end,
					 * but who cares? */
					g_strlcpy(sw, utf8, sizeof(sw));
					g_free(utf8);
				}

				if (!gaim_find_buddy(account, nm)) {
					struct buddy *b = gaim_buddy_new(account, nm, sw);
					struct group *g = gaim_find_group(current);
					gaim_blist_add_buddy(b, g, NULL);
					bud = g_list_append(bud, g_strdup(nm));
				}
			} else if (*c == 'p') {
				gaim_privacy_permit_add(account, c + 2);
			} else if (*c == 'd') {
				gaim_privacy_deny_add(account, c + 2);
			} else if (!strncmp("toc", c, 3)) {
				sscanf(c + strlen(c) - 1, "%d", &account->permdeny);
				debug_printf("permdeny: %d\n", account->permdeny);
				if (account->permdeny == 0)
					account->permdeny = 1;
			} else if (*c == 'm') {
				sscanf(c + 2, "%d", &account->permdeny);
				debug_printf("permdeny: %d\n", account->permdeny);
				if (account->permdeny == 0)
					account->permdeny = 1;
			}
		} while ((c = strtok(NULL, "\n")));

		if(account->gc) {
			if(bud) {
				GList *node = bud;
				serv_add_buddies(account->gc, bud);
				while(node) {
					g_free(node->data);
					node = node->next;
				}
			}
			serv_set_permit_deny(account->gc);
		}
		g_list_free(bud);
	}
}

#if 0
/* translate an AIM 3 buddylist (*.lst) to a Gaim buddylist */
static GString *translate_lst(FILE *src_fp)
{
	char line[BUF_LEN], *line2;
	char *name;
	int i;

	GString *dest = g_string_new("m 1\n");

	while (fgets(line, BUF_LEN, src_fp)) {
		line2 = g_strchug(line);
		if (strstr(line2, "group") == line2) {
			name = strpbrk(line2, " \t\n\r\f") + 1;
			dest = g_string_append(dest, "g ");
			for (i = 0; i < strcspn(name, "\n\r"); i++)
				if (name[i] != '\"')
					dest = g_string_append_c(dest, name[i]);
			dest = g_string_append_c(dest, '\n');
		}
		if (strstr(line2, "buddy") == line2) {
			name = strpbrk(line2, " \t\n\r\f") + 1;
			dest = g_string_append(dest, "b ");
			for (i = 0; i < strcspn(name, "\n\r"); i++)
				if (name[i] != '\"')
					dest = g_string_append_c(dest, name[i]);
			dest = g_string_append_c(dest, '\n');
		}
	}

	return dest;
}


/* translate an AIM 4 buddylist (*.blt) to Gaim format */
static GString *translate_blt(FILE *src_fp)
{
	int i;
	char line[BUF_LEN];
	char *buddy;

	GString *dest = g_string_new("m 1\n");

	while (strstr(fgets(line, BUF_LEN, src_fp), "Buddy") == NULL);
	while (strstr(fgets(line, BUF_LEN, src_fp), "list") == NULL);

	while (1) {
		fgets(line, BUF_LEN, src_fp); g_strchomp(line);
		if (strchr(line, '}') != NULL)
			break;

		if (strchr(line, '{') != NULL) {
			/* Syntax starting with "<group> {" */

			dest = g_string_append(dest, "g ");
			buddy = g_strchug(strtok(line, "{"));
			for (i = 0; i < strlen(buddy); i++)
				if (buddy[i] != '\"')
					dest = g_string_append_c(dest, buddy[i]);
			dest = g_string_append_c(dest, '\n');
			while (strchr(fgets(line, BUF_LEN, src_fp), '}') == NULL) {
				gboolean pounce = FALSE;
				char *e;
				g_strchomp(line);
				buddy = g_strchug(line);
				debug_printf("\nbuddy: \"%s\"\n\n", buddy);
				dest = g_string_append(dest, "b ");
				if (strchr(buddy, '{') != NULL) {
					/* buddy pounce, etc */
					char *pos = strchr(buddy, '{') - 1;
					*pos = 0;
					pounce = TRUE;
				}
				if ((e = strchr(buddy, '\"')) != NULL) {
					*e = '\0';
					buddy++;
				}
				dest = g_string_append(dest, buddy);
				dest = g_string_append_c(dest, '\n');
				if (pounce)
					do
						fgets(line, BUF_LEN, src_fp);
					while (!strchr(line, '}'));
			}
		} else {

			/* Syntax "group buddy buddy ..." */
			buddy = g_strchug(strtok(line, " \n"));
			dest = g_string_append(dest, "g ");
			if (strchr(buddy, '\"') != NULL) {
				dest = g_string_append(dest, &buddy[1]);
				dest = g_string_append_c(dest, ' ');
				buddy = g_strchug(strtok(NULL, " \n"));
				while (strchr(buddy, '\"') == NULL) {
					dest = g_string_append(dest, buddy);
					dest = g_string_append_c(dest, ' ');
					buddy = g_strchug(strtok(NULL, " \n"));
				}
				buddy[strlen(buddy) - 1] = '\0';
				dest = g_string_append(dest, buddy);
			} else {
				dest = g_string_append(dest, buddy);
			}
			dest = g_string_append_c(dest, '\n');
			while ((buddy = g_strchug(strtok(NULL, " \n"))) != NULL) {
				dest = g_string_append(dest, "b ");
				if (strchr(buddy, '\"') != NULL) {
					dest = g_string_append(dest, &buddy[1]);
					dest = g_string_append_c(dest, ' ');
					buddy = g_strchug(strtok(NULL, " \n"));
					while (strchr(buddy, '\"') == NULL) {
						dest = g_string_append(dest, buddy);
						dest = g_string_append_c(dest, ' ');
						buddy = g_strchug(strtok(NULL, " \n"));
					}
					buddy[strlen(buddy) - 1] = '\0';
					dest = g_string_append(dest, buddy);
				} else {
					dest = g_string_append(dest, buddy);
				}
				dest = g_string_append_c(dest, '\n');
			}
		}
	}

	return dest;
}

static GString *translate_gnomeicu(FILE *src_fp)
{
	char line[BUF_LEN];
	GString *dest = g_string_new("m 1\ng Buddies\n");

	while (strstr(fgets(line, BUF_LEN, src_fp), "NewContacts") == NULL);

	while (fgets(line, BUF_LEN, src_fp)) {
		char *eq;
		g_strchomp(line);
		if (line[0] == '\n' || line[0] == '[')
			break;
		eq = strchr(line, '=');
		if (!eq)
			break;
		*eq = ':';
		eq = strchr(eq, ',');
		if (eq)
			*eq = '\0';
		dest = g_string_append(dest, "b ");
		dest = g_string_append(dest, line);
		dest = g_string_append_c(dest, '\n');
	}

	return dest;
}
#endif

static gchar *get_screenname_filename(const char *name)
{
	gchar **split;
	gchar *good;
	gchar *ret;

	split = g_strsplit(name, G_DIR_SEPARATOR_S, -1);
	good = g_strjoinv(NULL, split);
	g_strfreev(split);

	ret = g_utf8_strup(good, -1);

	g_free(good);

	return ret;
}

static gboolean gaim_blist_read(const char *filename);


static void do_import(struct gaim_account *account, const char *filename)
{
	GString *buf = NULL;
	char first[64];
	char path[PATHSIZE];
	int len;
	FILE *f;
	struct stat st;

	if (filename) {
		g_snprintf(path, sizeof(path), "%s", filename);
	} else {
		char *g_screenname = get_screenname_filename(account->username);
		char *file = gaim_user_dir();
		int protocol = (account->protocol == PROTO_OSCAR) ? (isalpha(account->username[0]) ? PROTO_TOC : PROTO_ICQ): account->protocol;

		if (file != (char *)NULL) {
			sprintf(path, "%s" G_DIR_SEPARATOR_S "%s.%d.blist", file, g_screenname, protocol);
			g_free(g_screenname);
		} else {
			g_free(g_screenname);
			return;
		}
	}

	if (stat(path, &st)) {
		debug_printf("Unable to stat %s.\n", path);
		return;
	}

	if (!(f = fopen(path, "r"))) {
		debug_printf("Unable to open %s.\n", path);
		return;
	}

	fgets(first, 64, f);

	if ((first[0] == '\n') || (first[0] == '\r' && first[1] == '\n'))
		fgets(first, 64, f);

#if 0
	if (!g_strncasecmp(first, "<xml", strlen("<xml"))) {
		/* new gaim XML buddy list */
		gaim_blist_read(path);
		
		/* We really don't need to bother doing stuf like translating AIM 3 buddy lists anymore */
		
	} else if (!g_strncasecmp(first, "Config {", strlen("Config {"))) {
		/* AIM 4 buddy list */
		debug_printf("aim 4\n");
		rewind(f);
		buf = translate_blt(f);
	} else if (strstr(first, "group") != NULL) {
		/* AIM 3 buddy list */
		debug_printf("aim 3\n");
		rewind(f);
		buf = translate_lst(f);
	} else if (!g_strncasecmp(first, "[User]", strlen("[User]"))) {
		/* GnomeICU (hopefully) */
		debug_printf("gnomeicu\n");
		rewind(f);
		buf = translate_gnomeicu(f);

	} else 
#endif
		if (first[0] == 'm') {
			/* Gaim buddy list - no translation */
			char buf2[BUF_LONG * 2];
			buf = g_string_new("");
			rewind(f);
			while (1) {
				len = fread(buf2, 1, BUF_LONG * 2 - 1, f);
				if (len <= 0)
					break;
			buf2[len] = '\0';
			buf = g_string_append(buf, buf2);
			if (len != BUF_LONG * 2 - 1)
				break;
			}
		}
	
	fclose(f);

	if (buf) {
		buf = g_string_prepend(buf, "toc_set_config {");
		buf = g_string_append(buf, "}\n");
		parse_toc_buddy_list(account, buf->str);
		g_string_free(buf, TRUE);
	}
}

gboolean gaim_group_on_account(struct group *g, struct gaim_account *account) {
	GaimBlistNode *bnode;
	for(bnode = g->node.child; bnode; bnode = bnode->next) {
		struct buddy *b = (struct buddy *)bnode;
		if(!GAIM_BLIST_NODE_IS_BUDDY(bnode))
			continue;
		if((!account && b->account->gc) || b->account == account)
			return TRUE;
	}
	return FALSE;
}

static gboolean blist_safe_to_write = FALSE;

static char *blist_parser_group_name = NULL;
static char *blist_parser_person_name = NULL;
static char *blist_parser_account_name = NULL;
static int blist_parser_account_protocol = 0;
static char *blist_parser_buddy_name = NULL;
static char *blist_parser_buddy_alias = NULL;
static char *blist_parser_setting_name = NULL;
static char *blist_parser_setting_value = NULL;
static GHashTable *blist_parser_buddy_settings = NULL;
static int blist_parser_privacy_mode = 0;
static enum {
	BLIST_TAG_GAIM,
	BLIST_TAG_BLIST,
	BLIST_TAG_GROUP,
	BLIST_TAG_PERSON,
	BLIST_TAG_BUDDY,
	BLIST_TAG_NAME,
	BLIST_TAG_ALIAS,
	BLIST_TAG_SETTING,
	BLIST_TAG_PRIVACY,
	BLIST_TAG_ACCOUNT,
	BLIST_TAG_PERMIT,
	BLIST_TAG_BLOCK,
	BLIST_TAG_IGNORE
} blist_parser_current_tag;
static gboolean blist_parser_error_occurred = FALSE;

static void blist_start_element_handler (GMarkupParseContext *context,
		const gchar *element_name,
		const gchar **attribute_names,
		const gchar **attribute_values,
		gpointer user_data,
		GError **error) {
	int i;

	if(!strcmp(element_name, "gaim")) {
		blist_parser_current_tag = BLIST_TAG_GAIM;
	} else if(!strcmp(element_name, "blist")) {
		blist_parser_current_tag = BLIST_TAG_BLIST;
	} else if(!strcmp(element_name, "group")) {
		blist_parser_current_tag = BLIST_TAG_GROUP;
		for(i=0; attribute_names[i]; i++) {
			if(!strcmp(attribute_names[i], "name")) {
				g_free(blist_parser_group_name);
				blist_parser_group_name = g_strdup(attribute_values[i]);
			}
		}
		if(blist_parser_group_name) {
			struct group *g = gaim_group_new(blist_parser_group_name);
			gaim_blist_add_group(g,NULL);
		}
	} else if(!strcmp(element_name, "person")) {
		blist_parser_current_tag = BLIST_TAG_PERSON;
		for(i=0; attribute_names[i]; i++) {
			if(!strcmp(attribute_names[i], "name")) {
				g_free(blist_parser_person_name);
				blist_parser_person_name = g_strdup(attribute_values[i]);
			}
		}
	} else if(!strcmp(element_name, "buddy")) {
		blist_parser_current_tag = BLIST_TAG_BUDDY;
		for(i=0; attribute_names[i]; i++) {
			if(!strcmp(attribute_names[i], "account")) {
				g_free(blist_parser_account_name);
				blist_parser_account_name = g_strdup(attribute_values[i]);
			} else if(!strcmp(attribute_names[i], "protocol")) {
				blist_parser_account_protocol = atoi(attribute_values[i]);
			}
		}
	} else if(!strcmp(element_name, "name")) {
		blist_parser_current_tag = BLIST_TAG_NAME;
	} else if(!strcmp(element_name, "alias")) {
		blist_parser_current_tag = BLIST_TAG_ALIAS;
	} else if(!strcmp(element_name, "setting")) {
		blist_parser_current_tag = BLIST_TAG_SETTING;
		for(i=0; attribute_names[i]; i++) {
			if(!strcmp(attribute_names[i], "name")) {
				g_free(blist_parser_setting_name);
				blist_parser_setting_name = g_strdup(attribute_values[i]);
			}
		}
	} else if(!strcmp(element_name, "privacy")) {
		blist_parser_current_tag = BLIST_TAG_PRIVACY;
	} else if(!strcmp(element_name, "account")) {
		blist_parser_current_tag = BLIST_TAG_ACCOUNT;
		for(i=0; attribute_names[i]; i++) {
			if(!strcmp(attribute_names[i], "protocol"))
				blist_parser_account_protocol = atoi(attribute_values[i]);
			else if(!strcmp(attribute_names[i], "mode"))
				blist_parser_privacy_mode = atoi(attribute_values[i]);
			else if(!strcmp(attribute_names[i], "name")) {
				g_free(blist_parser_account_name);
				blist_parser_account_name = g_strdup(attribute_values[i]);
			}
		}
	} else if(!strcmp(element_name, "permit")) {
		blist_parser_current_tag = BLIST_TAG_PERMIT;
	} else if(!strcmp(element_name, "block")) {
		blist_parser_current_tag = BLIST_TAG_BLOCK;
	} else if(!strcmp(element_name, "ignore")) {
		blist_parser_current_tag = BLIST_TAG_IGNORE;
	}
}

static void blist_end_element_handler(GMarkupParseContext *context,
		const gchar *element_name, gpointer user_data, GError **error) {
	if(!strcmp(element_name, "gaim")) {
	} else if(!strcmp(element_name, "blist")) {
		blist_parser_current_tag = BLIST_TAG_GAIM;
	} else if(!strcmp(element_name, "group")) {
		blist_parser_current_tag = BLIST_TAG_BLIST;
	} else if(!strcmp(element_name, "person")) {
		blist_parser_current_tag = BLIST_TAG_GROUP;
		g_free(blist_parser_person_name);
		blist_parser_person_name = NULL;
	} else if(!strcmp(element_name, "buddy")) {
		struct gaim_account *account = gaim_account_find(blist_parser_account_name,
				blist_parser_account_protocol);
		if(account) {
			struct buddy *b = gaim_buddy_new(account, blist_parser_buddy_name, blist_parser_buddy_alias);
			struct group *g = gaim_find_group(blist_parser_group_name);
			gaim_blist_add_buddy(b,g,NULL);
			if(blist_parser_buddy_settings) {
				g_hash_table_destroy(b->settings);
				b->settings = blist_parser_buddy_settings;
			}
		}
		blist_parser_current_tag = BLIST_TAG_PERSON;
		g_free(blist_parser_buddy_name);
		blist_parser_buddy_name = NULL;
		g_free(blist_parser_buddy_alias);
		blist_parser_buddy_alias = NULL;
		g_free(blist_parser_account_name);
		blist_parser_account_name = NULL;
		blist_parser_buddy_settings = NULL;
	} else if(!strcmp(element_name, "name")) {
		blist_parser_current_tag = BLIST_TAG_BUDDY;
	} else if(!strcmp(element_name, "alias")) {
		blist_parser_current_tag = BLIST_TAG_BUDDY;
	} else if(!strcmp(element_name, "setting")) {
		if(!blist_parser_buddy_settings)
			blist_parser_buddy_settings = g_hash_table_new_full(g_str_hash,
					g_str_equal, g_free, g_free);
		if(blist_parser_setting_name && blist_parser_setting_value) {
			g_hash_table_replace(blist_parser_buddy_settings,
					g_strdup(blist_parser_setting_name),
					g_strdup(blist_parser_setting_value));
		}
		g_free(blist_parser_setting_name);
		g_free(blist_parser_setting_value);
		blist_parser_setting_name = NULL;
		blist_parser_setting_value = NULL;
		blist_parser_current_tag = BLIST_TAG_BUDDY;
	} else if(!strcmp(element_name, "privacy")) {
		blist_parser_current_tag = BLIST_TAG_GAIM;
	} else if(!strcmp(element_name, "account")) {
		struct gaim_account *account = gaim_account_find(blist_parser_account_name,
				blist_parser_account_protocol);
		if(account) {
			account->permdeny = blist_parser_privacy_mode;
		}
		blist_parser_current_tag = BLIST_TAG_PRIVACY;
		g_free(blist_parser_account_name);
		blist_parser_account_name = NULL;
	} else if(!strcmp(element_name, "permit")) {
		struct gaim_account *account = gaim_account_find(blist_parser_account_name,
				blist_parser_account_protocol);
		if(account) {
			gaim_privacy_permit_add(account, blist_parser_buddy_name);
		}
		blist_parser_current_tag = BLIST_TAG_ACCOUNT;
		g_free(blist_parser_buddy_name);
		blist_parser_buddy_name = NULL;
	} else if(!strcmp(element_name, "block")) {
		struct gaim_account *account = gaim_account_find(blist_parser_account_name,
				blist_parser_account_protocol);
		if(account) {
			gaim_privacy_deny_add(account, blist_parser_buddy_name);
		}
		blist_parser_current_tag = BLIST_TAG_ACCOUNT;
		g_free(blist_parser_buddy_name);
		blist_parser_buddy_name = NULL;
	} else if(!strcmp(element_name, "ignore")) {
		/* we'll apparently do something with this later */
		blist_parser_current_tag = BLIST_TAG_ACCOUNT;
	}
}

static void blist_text_handler(GMarkupParseContext *context, const gchar *text,
		gsize text_len, gpointer user_data, GError **error) {
	switch(blist_parser_current_tag) {
		case BLIST_TAG_NAME:
			blist_parser_buddy_name = g_strndup(text, text_len);
			break;
		case BLIST_TAG_ALIAS:
			blist_parser_buddy_alias = g_strndup(text, text_len);
			break;
		case BLIST_TAG_PERMIT:
		case BLIST_TAG_BLOCK:
		case BLIST_TAG_IGNORE:
			blist_parser_buddy_name = g_strndup(text, text_len);
			break;
		case BLIST_TAG_SETTING:
			blist_parser_setting_value = g_strndup(text, text_len);
			break;
		default:
			break;
	}
}

static void blist_error_handler(GMarkupParseContext *context, GError *error,
		gpointer user_data) {
	blist_parser_error_occurred = TRUE;
	debug_printf("error parsing blist.xml: %s\n", error->message);
}

static GMarkupParser blist_parser = {
	blist_start_element_handler,
	blist_end_element_handler,
	blist_text_handler,
	NULL,
	blist_error_handler
};

static gboolean gaim_blist_read(const char *filename) {
	gchar *contents = NULL;
	gsize length;
	GMarkupParseContext *context;
	GError *error = NULL;

	debug_printf("gaim_blist_read: reading %s\n", filename);
	if(!g_file_get_contents(filename, &contents, &length, &error)) {
		debug_printf("error reading blist: %s\n", error->message);
		g_error_free(error);
		return FALSE;
	}

	context = g_markup_parse_context_new(&blist_parser, 0, NULL, NULL);

	if(!g_markup_parse_context_parse(context, contents, length, NULL)) {
		g_markup_parse_context_free(context);
		g_free(contents);
		return FALSE;
	}

	if(!g_markup_parse_context_end_parse(context, NULL)) {
		debug_printf("error parsing blist\n");
		g_markup_parse_context_free(context);
		g_free(contents);
		return FALSE;
	}

	g_markup_parse_context_free(context);
	g_free(contents);

	if(blist_parser_error_occurred)
		return FALSE;

	debug_printf("gaim_blist_read: finished reading %s\n", filename);

	return TRUE;
}

void gaim_blist_load() {
	GSList *accts;
	char *user_dir = gaim_user_dir();
	char *filename;
	char *msg;

	blist_safe_to_write = TRUE;

	if(!user_dir)
		return;

	filename = g_build_filename(user_dir, "blist.xml", NULL);

	if(g_file_test(filename, G_FILE_TEST_EXISTS)) {
		if(!gaim_blist_read(filename)) {
			msg = g_strdup_printf(_("An error was encountered parsing your "
						"buddy list.  It has not been loaded."));
			do_error_dialog(_("Buddy List Error"), msg, GAIM_ERROR);
			g_free(msg);
		}
	} else if(g_slist_length(gaim_accounts)) {
		/* rob wants to inform the user that their buddy lists are
		 * being converted */
		msg = g_strdup_printf(_("Gaim is converting your old buddy lists "
					"to a new format, which will now be located at %s"),
				filename);
		do_error_dialog(_("Converting Buddy List"), msg, GAIM_INFO);
		g_free(msg);

		/* now, let gtk actually display the dialog before we start anything */
		while(gtk_events_pending())
			gtk_main_iteration();

		/* read in the old lists, then save to the new format */
		for(accts = gaim_accounts; accts; accts = accts->next) {
			do_import(accts->data, NULL);
		}
		gaim_blist_save();
	}

	g_free(filename);
}

static void blist_print_buddy_settings(gpointer key, gpointer data,
		gpointer user_data) {
	char *key_val;
	char *data_val;
	FILE *file = user_data;

	if(!key || !data)
		return;

	key_val = g_markup_escape_text(key, -1);
	data_val = g_markup_escape_text(data, -1);

	fprintf(file, "\t\t\t\t\t<setting name=\"%s\">%s</setting>\n", key_val,
			data_val);
	g_free(key_val);
	g_free(data_val);
}

static void gaim_blist_write(FILE *file, struct gaim_account *exp_acct) {
	GSList *accounts, *buds;
	GaimBlistNode *gnode,*bnode;
	struct group *group;
	struct buddy *bud;
	fprintf(file, "<?xml version='1.0' encoding='UTF-8' ?>\n");
	fprintf(file, "<gaim version=\"1\">\n");
	fprintf(file, "\t<blist>\n");

	for(gnode = gaimbuddylist->root; gnode; gnode = gnode->next) {
		if(!GAIM_BLIST_NODE_IS_GROUP(gnode))
			continue;
		group = (struct group *)gnode;
		if(!exp_acct || gaim_group_on_account(group, exp_acct)) {
			char *group_name = g_markup_escape_text(group->name, -1);
			fprintf(file, "\t\t<group name=\"%s\">\n", group_name);
			for(bnode = gnode->child; bnode; bnode = bnode->next) {
				if(!GAIM_BLIST_NODE_IS_BUDDY(bnode))
					continue;
				bud = (struct buddy *)bnode;
				if(!exp_acct || bud->account == exp_acct) {
					char *bud_name = g_markup_escape_text(bud->name, -1);
					char *bud_alias = NULL;
					char *acct_name = g_markup_escape_text(bud->account->username, -1);
					if(bud->alias)
						bud_alias= g_markup_escape_text(bud->alias, -1);
					fprintf(file, "\t\t\t<person name=\"%s\">\n",
							bud_alias ? bud_alias : bud_name);
					fprintf(file, "\t\t\t\t<buddy protocol=\"%d\" "
							"account=\"%s\">\n", bud->account->protocol,
							acct_name);
					fprintf(file, "\t\t\t\t\t<name>%s</name>\n", bud_name);
					if(bud_alias) {
						fprintf(file, "\t\t\t\t\t<alias>%s</alias>\n",
								bud_alias);
					}
					g_hash_table_foreach(bud->settings,
							blist_print_buddy_settings, file);
					fprintf(file, "\t\t\t\t</buddy>\n");
					fprintf(file, "\t\t\t</person>\n");
					g_free(bud_name);
					g_free(bud_alias);
					g_free(acct_name);
				}
			}
			fprintf(file, "\t\t</group>\n");
			g_free(group_name);
		}
	}

	fprintf(file, "\t</blist>\n");
	fprintf(file, "\t<privacy>\n");

	for(accounts = gaim_accounts; accounts; accounts = accounts->next) {
		struct gaim_account *account = accounts->data;
		char *acct_name = g_markup_escape_text(account->username, -1);
		if(!exp_acct || account == exp_acct) {
			fprintf(file, "\t\t<account protocol=\"%d\" name=\"%s\" "
					"mode=\"%d\">\n", account->protocol, acct_name, account->permdeny);
			for(buds = account->permit; buds; buds = buds->next) {
				char *bud_name = g_markup_escape_text(buds->data, -1);
				fprintf(file, "\t\t\t<permit>%s</permit>\n", bud_name);
				g_free(bud_name);
			}
			for(buds = account->deny; buds; buds = buds->next) {
				char *bud_name = g_markup_escape_text(buds->data, -1);
				fprintf(file, "\t\t\t<block>%s</block>\n", bud_name);
				g_free(bud_name);
			}
			fprintf(file, "\t\t</account>\n");
		}
		g_free(acct_name);
	}

	fprintf(file, "\t</privacy>\n");
	fprintf(file, "</gaim>\n");
}

void gaim_blist_save() {
	FILE *file;
	char *user_dir = gaim_user_dir();
	char *filename;
	char *filename_real;

	if(!user_dir)
		return;
	if(!blist_safe_to_write) {
		debug_printf("AHH!! tried to write the blist before we read it!\n");
		return;
	}

	file = fopen(user_dir, "r");
	if(!file)
		mkdir(user_dir, S_IRUSR | S_IWUSR | S_IXUSR);
	else
		fclose(file);

	filename = g_build_filename(user_dir, "blist.xml.save", NULL);

	if((file = fopen(filename, "w"))) {
		gaim_blist_write(file, NULL);
		fclose(file);
		chmod(filename, S_IRUSR | S_IWUSR);
	} else {
		debug_printf("unable to write %s\n", filename);
	}

	filename_real = g_build_filename(user_dir, "blist.xml", NULL);

	if(rename(filename, filename_real) < 0)
		debug_printf("error renaming %s to %s\n", filename, filename_real);


	g_free(filename);
	g_free(filename_real);
}

gboolean gaim_privacy_permit_add(struct gaim_account *account, const char *who) {
	GSList *d = account->permit;
	char *n = g_strdup(normalize(who));
	while(d) {
		if(!gaim_utf8_strcasecmp(n, normalize(d->data)))
			break;
		d = d->next;
	}
	g_free(n);
	if(!d) {
		account->permit = g_slist_append(account->permit, g_strdup(who));
		return TRUE;
	}

	return FALSE;
}

gboolean gaim_privacy_permit_remove(struct gaim_account *account, const char *who) {
	GSList *d = account->permit;
	char *n = g_strdup(normalize(who));
	while(d) {
		if(!gaim_utf8_strcasecmp(n, normalize(d->data)))
			break;
		d = d->next;
	}
	g_free(n);
	if(d) {
		account->permit = g_slist_remove(account->permit, d->data);
		g_free(d->data);
		return TRUE;
	}
	return FALSE;
}

gboolean gaim_privacy_deny_add(struct gaim_account *account, const char *who) {
	GSList *d = account->deny;
	char *n = g_strdup(normalize(who));
	while(d) {
		if(!gaim_utf8_strcasecmp(n, normalize(d->data)))
			break;
		d = d->next;
	}
	g_free(n);
	if(!d) {
		account->deny = g_slist_append(account->deny, g_strdup(who));
		return TRUE;
	}

	return FALSE;
}

gboolean gaim_privacy_deny_remove(struct gaim_account *account, const char *who) {
	GSList *d = account->deny;
	char *n = g_strdup(normalize(who));
	while(d) {
		if(!gaim_utf8_strcasecmp(n, normalize(d->data)))
			break;
		d = d->next;
	}
	g_free(n);
	if(d) {
		account->deny = g_slist_remove(account->deny, d->data);
		g_free(d->data);
		return TRUE;
	}
	return FALSE;
}

void gaim_buddy_set_setting(struct buddy *b, const char *key,
		const char *value) {
	if(!b)
		return;
	g_hash_table_replace(b->settings, g_strdup(key), g_strdup(value));
}

char *gaim_buddy_get_setting(struct buddy *b, const char *key) {
	if(!b)
		return NULL;
	return g_strdup(g_hash_table_lookup(b->settings, key));
}

void gaim_set_blist_ui_ops(struct gaim_blist_ui_ops *ops)
{
	blist_ui_ops = ops;
}

struct gaim_blist_ui_ops *
gaim_get_blist_ui_ops(void)
{
	return blist_ui_ops;
}

int gaim_blist_get_group_size(struct group *group, gboolean offline) {
	GaimBlistNode *node;
	int count = 0;

	if(!group)
		return 0;

	for(node = group->node.child; node; node = node->next) {
		if(GAIM_BLIST_NODE_IS_BUDDY(node)) {
			struct buddy *b = (struct buddy *)node;
			if(b->account->gc || offline)
				count++;
		}
	}

	return count;
}

int gaim_blist_get_group_online_count(struct group *group) {
	GaimBlistNode *node;
	int count = 0;

	if(!group)
		return 0;

	for(node = group->node.child; node; node = node->next) {
		if(GAIM_BLIST_NODE_IS_BUDDY(node)) {
			struct buddy *b = (struct buddy *)node;
			if(GAIM_BUDDY_IS_ONLINE(b))
				count++;
		}
	}

	return count;
}


