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

#ifdef _WIN32
#include "win32dep.h"
#endif

#define PATHSIZE 1024

/* This guy does its best to convert a string to UTF-8 from an unknown
 * encoding by checking the locale and trying some sane defaults ...
 * if everything fails it returns NULL. */
static char *whatever_to_utf8(const char *str);

void remove_buddy(struct buddy *rem_b)
{
	if(rem_b->user->gc) {
		struct group *rem_g = find_group_by_buddy(rem_b);

		ui_remove_buddy(rem_b);

		rem_g->members = g_slist_remove(rem_g->members, rem_b);

		g_hash_table_destroy(rem_b->settings);

		g_free(rem_b);
	} else {
		char *buf = g_strdup_printf(_("%s was not removed from your buddy "
					"list, because your account (%s) is not logged in."),
					rem_b->name, rem_b->user->username);
		do_error_dialog(_("Buddy Not Removed"), buf, GAIM_ERROR);
		g_free(buf);
	}
}

void remove_group(struct group *rem_g)
{
	GSList *users;

	for(users = aim_users; users; users = users->next) {
		struct aim_user *user = users->data;
		if(user->gc) {
			GList *tmp = NULL;
			GSList *buds = rem_g->members;

			while (buds) {
				struct buddy *delb = (struct buddy *)buds->data;
				buds = buds->next;

				if(delb->user == user) {
					tmp = g_list_append(tmp, g_strdup(delb->name));
					remove_buddy(delb);	/* this should take care of removing
										   the group_show if necessary */
				}
			}

			if(tmp)
				serv_remove_buddies(user->gc, tmp, rem_g->name);

			while (tmp) {
				g_free(tmp->data);
				tmp = g_list_remove(tmp, tmp->data);
			}
		}
	}

	if(rem_g->members) {
		char *buf = g_strdup_printf(_("%d buddies from group %s were not "
					"removed because their accounts were not logged in.  These "
					"buddies, and the group were not removed.\n"),
				g_slist_length(rem_g->members), rem_g->name);
		do_error_dialog(_("Group Not Removed"), buf, GAIM_ERROR);
		g_free(buf);

		return;
	}

	ui_remove_group(rem_g);

	groups = g_slist_remove(groups, rem_g);

	g_free(rem_g);

	/* don't flush buddy list to cache in order to be consistent with remove_buddy,
	 * mostly. remove_group is only called from one place, so we'll let it handle it. */
}

struct buddy *add_buddy(struct aim_user *user, const char *group, const char *buddy, const char *show)
{
	struct buddy *b;
	struct group *g;
	const char *good;

	if ((b = find_buddy(user, buddy)) != NULL)
		return b;

	g = find_group(group);

	if (g == NULL)
		g = add_group(group);

	b = (struct buddy *)g_new0(struct buddy, 1);

	if (!b)
		return NULL;

	b->user = user;
	b->present = 0;

	b->settings = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	good = buddy;

	g_snprintf(b->name, sizeof(b->name), "%s", good);
	if(show && show[0])
		g_snprintf(b->alias, sizeof(b->alias), "%s", show);
	else
		b->alias[0] = '\0';

	g->members = g_slist_append(g->members, b);

	b->idle = 0;
	b->caps = 0;

	ui_add_buddy(user->gc, g, b);

	return b;
}

struct group *add_group(const char *group)
{
	struct group *g = find_group(group);
	if (g)
		return g;
	g = (struct group *)g_new0(struct group, 1);
	if (!g)
		return NULL;

	strncpy(g->name, group, sizeof(g->name));
	groups = g_slist_append(groups, g);

	g->members = NULL;

	ui_add_group(g);

	return g;
}

struct group *find_group(const char *group)
{
	struct group *g;
	GSList *grp;
	char *grpname = g_strdup(normalize(group));

	grp = groups;
	while (grp) {
		g = (struct group *)grp->data;
		if (!g_strcasecmp(normalize (g->name), grpname)) {
			g_free(grpname);
			return g;
		}
		grp = g_slist_next(grp);
	}
	g_free(grpname);
	return NULL;
}

struct group *find_group_by_buddy(struct buddy *b)
{
	GSList *grp = groups;

	while(grp) {
		struct group *g = grp->data;
		if(g_slist_find(g->members, b))
			return g;
		grp = grp->next;
	}
	return NULL;
}

struct buddy *find_buddy(struct aim_user *user, const char *who)
{
	struct group *g;
	struct buddy *b;
	GSList *grp;
	GSList *mem;
	char *whoname = NULL;
	char *(*norm)(const char *);

	grp = groups;
	while (grp) {
		g = (struct group *)grp->data;

		mem = g->members;
		while (mem) {
			b = (struct buddy *)mem->data;
			/*
			norm = (b->user->gc && b->user->gc->prpl->normalize) ? b->user->gc->prpl->normalize : normalize;
			*/
			norm = normalize;
			whoname = g_strdup(norm(who));
			if ((b->user == user || !user) && !strcmp(norm(b->name), whoname)) {
				g_free(whoname);
				return b;
			}
			g_free(whoname);
			mem = mem->next;
		}
		grp = g_slist_next(grp);
	}
	return NULL;
}

void parse_toc_buddy_list(struct aim_user *user, char *config)
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
				utf8 = whatever_to_utf8(c + 2);
				if (utf8 == NULL) {
					g_strlcpy(current, _("Invalid Groupname"), sizeof(current));
				} else {
					g_strlcpy(current, utf8, sizeof(current));
					g_free(utf8);
				}
				if (!find_group(current)) {
					add_group(current);
				}
			} else if (*c == 'b') { /*&& !find_buddy(user, c + 2)) {*/
				char nm[80], sw[BUDDY_ALIAS_MAXLEN], *a, *utf8 = NULL;

				if ((a = strchr(c + 2, ':')) != NULL) {
					*a++ = '\0';		/* nul the : */
				}

				g_strlcpy(nm, c + 2, sizeof(nm));
				if (a) {
					utf8 = whatever_to_utf8(a);
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
				
				if (!find_buddy(user, nm)) {
					add_buddy(user, current, nm, sw);
					bud = g_list_append(bud, nm);
				}
			} else if (*c == 'p') {
				gaim_privacy_permit_add(user, c + 2);
			} else if (*c == 'd') {
				gaim_privacy_deny_add(user, c + 2);
			} else if (!strncmp("toc", c, 3)) {
				sscanf(c + strlen(c) - 1, "%d", &user->permdeny);
				debug_printf("permdeny: %d\n", user->permdeny);
				if (user->permdeny == 0)
					user->permdeny = 1;
			} else if (*c == 'm') {
				sscanf(c + 2, "%d", &user->permdeny);
				debug_printf("permdeny: %d\n", user->permdeny);
				if (user->permdeny == 0)
					user->permdeny = 1;
			}
		} while ((c = strtok(NULL, "\n")));

		if(user->gc) {
			if(bud)
				serv_add_buddies(user->gc, bud);
			serv_set_permit_deny(user->gc);
		}
		g_list_free(bud);
	}
}

void toc_build_config(struct aim_user *user, char *s, int len, gboolean show)
{
	GSList *grp = groups;
	GSList *mem;
	struct group *g;
	struct buddy *b;
	GSList *plist = user->permit;
	GSList *dlist = user->deny;

	int pos = 0;

	if (!user->permdeny)
		user->permdeny = 1;

	pos += g_snprintf(&s[pos], len - pos, "m %d\n", user->permdeny);
	while (len > pos && grp) {
		g = (struct group *)grp->data;
		if(gaim_group_on_account(g, user)) {
			pos += g_snprintf(&s[pos], len - pos, "g %s\n", g->name);
			mem = g->members;
			while (len > pos && mem) {
				b = (struct buddy *)mem->data;
				if(b->user == user) {
					pos += g_snprintf(&s[pos], len - pos, "b %s%s%s\n", b->name,
							(show && b->alias[0]) ? ":" : "",
							(show && b->alias[0]) ? b->alias : "");
				}
				mem = mem->next;
			}
		}
		grp = g_slist_next(grp);
	}

	while (len > pos && plist) {
		pos += g_snprintf(&s[pos], len - pos, "p %s\n", (char *)plist->data);
		plist = plist->next;
	}

	while (len > pos && dlist) {
		pos += g_snprintf(&s[pos], len - pos, "d %s\n", (char *)dlist->data);
		dlist = dlist->next;
	}
}

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

static gchar *get_screenname_filename(const char *name)
{
	gchar **split;
	gchar *good;

	split = g_strsplit(name, G_DIR_SEPARATOR_S, -1);
	good = g_strjoinv(NULL, split);
	g_strfreev(split);

	g_strup(good);

	return good;
}

static gboolean gaim_blist_read(const char *filename);

void do_import(struct aim_user *user, const char *filename)
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
		char *g_screenname = get_screenname_filename(user->username);
		char *file = gaim_user_dir();
		int protocol = (user->protocol == PROTO_OSCAR) ? (isalpha(user->username[0]) ? PROTO_TOC : PROTO_ICQ): user->protocol;

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

	if (!g_strncasecmp(first, "<xml", strlen("<xml"))) {
		/* new gaim XML buddy list */
		gaim_blist_read(path);
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
	} else if (first[0] == 'm') {
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
		parse_toc_buddy_list(user, buf->str);
		g_string_free(buf, TRUE);
	}
}

static gboolean is_blocked(struct buddy *b)
{
	struct aim_user *user = b->user;

	if (user->permdeny == PERMIT_ALL)
		return FALSE;

	if (user->permdeny == PERMIT_NONE) {
		if (user->gc && g_strcasecmp(b->name, user->gc->username))
			return TRUE;
		else
			return FALSE;
	}

	if (user->permdeny == PERMIT_SOME) {
		char *x = g_strdup(normalize(b->name));
		GSList *s = user->permit;
		while (s) {
			if (!g_strcasecmp(x, normalize(s->data)))
				break;
			s = s->next;
		}
		g_free(x);
		if (s)
			return FALSE;
		return TRUE;
	}

	if (user->permdeny == DENY_SOME) {
		char *x = g_strdup(normalize(b->name));
		GSList *s = user->deny;
		while (s) {
			if (!g_strcasecmp(x, normalize(s->data)))
				break;
			s = s->next;
		}
		g_free(x);
		if (s)
			return TRUE;
		return FALSE;
	}

	return FALSE;
}

void signoff_blocked(struct gaim_connection *gc)
{
	GSList *g = groups;
	while (g) {
		GSList *m = ((struct group *)g->data)->members;
		while (m) {
			struct buddy *b = m->data;
			if (is_blocked(b))
				serv_got_update(gc, b->name, 0, 0, 0, 0, 0, 0);
			m = m->next;
		}
		g = g->next;
	}
}

char *get_buddy_alias_only(struct buddy *b) {
	if(!b)
		return NULL;
	if(b->alias[0])
		return b->alias;
	else if((misc_options & OPT_MISC_USE_SERVER_ALIAS) && b->server_alias[0])
		return b->server_alias;
	return NULL;
}


char *get_buddy_alias(struct buddy *b) {
	char *ret = get_buddy_alias_only(b);
	if(!ret)
		return b ? b->name : _("Unknown");
	return ret;
}

GSList *gaim_group_get_accounts(struct group *g) {
	GSList *buds = g->members;
	GSList *ret = NULL;
	while(buds) {
		struct buddy *b = buds->data;
		if(!g_slist_find(ret, b->user))
			ret = g_slist_append(ret, b->user);
		buds = buds->next;
	}
	return ret;
}

gboolean gaim_group_on_account(struct group *g, struct aim_user *user) {
	GSList *buds = g->members;
	while(buds) {
		struct buddy *b = buds->data;
		if((!user && b->user->gc) || b->user == user)
			return TRUE;
		buds = buds->next;
	}
	return FALSE;
}


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
			if(!strcmp(attribute_names[i], "name"))
				blist_parser_group_name = g_strdup(attribute_values[i]);
		}
		if(blist_parser_group_name) {
			add_group(blist_parser_group_name);
		}
	} else if(!strcmp(element_name, "person")) {
		blist_parser_current_tag = BLIST_TAG_PERSON;
		for(i=0; attribute_names[i]; i++) {
			if(!strcmp(attribute_names[i], "name"))
				blist_parser_person_name = g_strdup(attribute_values[i]);
		}
	} else if(!strcmp(element_name, "buddy")) {
		blist_parser_current_tag = BLIST_TAG_BUDDY;
		for(i=0; attribute_names[i]; i++) {
			if(!strcmp(attribute_names[i], "account"))
				blist_parser_account_name = g_strdup(attribute_values[i]);
			else if(!strcmp(attribute_names[i], "protocol"))
				blist_parser_account_protocol = atoi(attribute_values[i]);
		}
	} else if(!strcmp(element_name, "name")) {
		blist_parser_current_tag = BLIST_TAG_NAME;
	} else if(!strcmp(element_name, "alias")) {
		blist_parser_current_tag = BLIST_TAG_ALIAS;
	} else if(!strcmp(element_name, "setting")) {
		blist_parser_current_tag = BLIST_TAG_SETTING;
		for(i=0; attribute_names[i]; i++) {
			if(!strcmp(attribute_names[i], "name"))
				blist_parser_setting_name = g_strdup(attribute_values[i]);
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
			else if(!strcmp(attribute_names[i], "name"))
				blist_parser_account_name = g_strdup(attribute_values[i]);
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
		struct aim_user *user = find_user(blist_parser_account_name,
				blist_parser_account_protocol);
		if(user) {
			struct buddy *b = add_buddy(user, blist_parser_group_name,
					blist_parser_buddy_name, blist_parser_buddy_alias);
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
		blist_parser_current_tag = BLIST_TAG_BUDDY;
	} else if(!strcmp(element_name, "privacy")) {
		blist_parser_current_tag = BLIST_TAG_GAIM;
	} else if(!strcmp(element_name, "account")) {
		struct aim_user *user = find_user(blist_parser_account_name,
				blist_parser_account_protocol);
		if(user) {
			user->permdeny = blist_parser_privacy_mode;
		}
		blist_parser_current_tag = BLIST_TAG_PRIVACY;
		g_free(blist_parser_account_name);
		blist_parser_account_name = NULL;
	} else if(!strcmp(element_name, "permit")) {
		struct aim_user *user = find_user(blist_parser_account_name,
				blist_parser_account_protocol);
		if(user) {
			gaim_privacy_permit_add(user, blist_parser_buddy_name);
		}
		blist_parser_current_tag = BLIST_TAG_ACCOUNT;
	} else if(!strcmp(element_name, "block")) {
		struct aim_user *user = find_user(blist_parser_account_name,
				blist_parser_account_protocol);
		if(user) {
			gaim_privacy_deny_add(user, blist_parser_buddy_name);
		}
		blist_parser_current_tag = BLIST_TAG_ACCOUNT;
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

	return TRUE;
}

void gaim_blist_load() {
	GSList *accts;
	char *user_dir = gaim_user_dir();
	char *filename;
	char *msg;

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
	} else if(g_slist_length(aim_users)) {
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
		for(accts = aim_users; accts; accts = accts->next) {
			do_import(accts->data, NULL);
		}
		gaim_blist_save();
	}

	g_free(filename);
}

static void blist_print_buddy_settings(gpointer key, gpointer data,
		gpointer user_data) {
	char *key_val = g_markup_escape_text(key, -1);
	char *data_val = g_markup_escape_text(data, -1);
	FILE *file = user_data;
	fprintf(file, "\t\t\t\t\t<setting name=\"%s\">%s</setting>\n", key_val,
			data_val);
	g_free(key_val);
	g_free(data_val);
}

static void gaim_blist_write(FILE *file, struct aim_user *user) {
	GSList *grps, *buds, *users;
	fprintf(file, "<?xml version='1.0' encoding='UTF-8' ?>\n");
	fprintf(file, "<gaim version=\"1\">\n");
	fprintf(file, "\t<blist>\n");

	for(grps = groups; grps; grps = grps->next) {
		struct group *g = grps->data;
		if(!user || gaim_group_on_account(g, user)) {
			char *group_name = g_markup_escape_text(g->name, -1);
			fprintf(file, "\t\t<group name=\"%s\">\n", group_name);
			for(buds = g->members; buds; buds = buds->next) {
				struct buddy *b = buds->data;
				if(!user || b->user == user) {
					char *bud_name = g_markup_escape_text(b->name, -1);
					char *bud_alias = NULL;
					char *acct_name = g_markup_escape_text(b->user->username, -1);
					if(b->alias[0])
						bud_alias= g_markup_escape_text(b->alias, -1);
					fprintf(file, "\t\t\t<person name=\"%s\">\n",
							bud_alias ? bud_alias : bud_name);
					fprintf(file, "\t\t\t\t<buddy protocol=\"%d\" "
							"account=\"%s\">\n", b->user->protocol,
							acct_name);
					fprintf(file, "\t\t\t\t\t<name>%s</name>\n", bud_name);
					if(bud_alias) {
						fprintf(file, "\t\t\t\t\t<alias>%s</alias>\n",
								bud_alias);
					}
					g_hash_table_foreach(b->settings,
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

	for(users = aim_users; users; users = users->next) {
		struct aim_user *usr = users->data;
		char *acct_name = g_markup_escape_text(usr->username, -1);
		if(!user || usr == user) {
			fprintf(file, "\t\t<account protocol=\"%d\" name=\"%s\" "
					"mode=\"%d\">\n", usr->protocol, acct_name, usr->permdeny);
			for(buds = usr->permit; buds; buds = buds->next) {
				char *bud_name = g_markup_escape_text(buds->data, -1);
				fprintf(file, "\t\t\t<permit>%s</permit>\n", bud_name);
				g_free(bud_name);
			}
			for(buds = usr->deny; buds; buds = buds->next) {
				char *bud_name = g_markup_escape_text(buds->data, -1);
				fprintf(file, "\t\t\t<block>%s</block>\n", bud_name);
				g_free(bud_name);
			}
			fprintf(file, "\t\t</account>\n");
		}
	}

	fprintf(file, "\t</privacy>\n");
	fprintf(file, "</gaim>\n");
}

void gaim_blist_save() {
	FILE *file;
	char *user_dir = gaim_user_dir();
	char *filename;

	if(!user_dir)
		return;

	file = fopen(user_dir, "r");
	if(!file)
		mkdir(user_dir, S_IRUSR | S_IWUSR | S_IXUSR);
	else
		fclose(file);

	filename = g_build_filename(user_dir, "blist.xml", NULL);

	if((file = fopen(filename, "w"))) {
		gaim_blist_write(file, NULL);
		fclose(file);
		chmod(filename, S_IRUSR | S_IWUSR);
	} else {
		debug_printf("unable to write %s\n", filename);
	}

	g_free(filename);
}

gboolean gaim_privacy_permit_add(struct aim_user *user, const char *who) {
	GSList *d = user->permit;
	char *n = g_strdup(normalize(who));
	while(d) {
		if(!g_strcasecmp(n, normalize(d->data)))
			break;
		d = d->next;
	}
	g_free(n);
	if(!d) {
		user->permit = g_slist_append(user->permit, g_strdup(who));
		return TRUE;
	}

	return FALSE;
}

gboolean gaim_privacy_permit_remove(struct aim_user *user, const char *who) {
	GSList *d = user->permit;
	char *n = g_strdup(normalize(who));
	while(d) {
		if(!g_strcasecmp(n, normalize(d->data)))
			break;
		d = d->next;
	}
	g_free(n);
	if(d) {
		user->permit = g_slist_remove(user->permit, d->data);
		g_free(d->data);
		return TRUE;
	}
	return FALSE;
}

gboolean gaim_privacy_deny_add(struct aim_user *user, const char *who) {
	GSList *d = user->deny;
	char *n = g_strdup(normalize(who));
	while(d) {
		if(!g_strcasecmp(n, normalize(d->data)))
			break;
		d = d->next;
	}
	g_free(n);
	if(!d) {
		user->deny = g_slist_append(user->deny, g_strdup(who));
		return TRUE;
	}

	return FALSE;
}

gboolean gaim_privacy_deny_remove(struct aim_user *user, const char *who) {
	GSList *d = user->deny;
	char *n = g_strdup(normalize(who));
	while(d) {
		if(!g_strcasecmp(n, normalize(d->data)))
			break;
		d = d->next;
	}
	g_free(n);
	if(d) {
		user->deny = g_slist_remove(user->deny, d->data);
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

static char *whatever_to_utf8(const char *str)
{
	int converted;
	char *utf8;

	if (g_utf8_validate(str, -1, NULL)) {
		return g_strdup(str);
	}

	utf8 = g_locale_to_utf8(str, -1, &converted, NULL, NULL);
	if (utf8 && converted == strlen (str)) {
		return(utf8);
	} else if (utf8) {
		g_free(utf8);
	}

	utf8 = g_convert(str, -1, "UTF-8", "ISO-8859-15", &converted, NULL, NULL);
	if (utf8 && converted == strlen (str)) {
		return(utf8);
	} else if (utf8) {
		g_free(utf8);
	}

	return(NULL);
}
