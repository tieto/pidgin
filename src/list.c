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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "gaim.h"
#include "prpl.h"

#define PATHSIZE 1024

void remove_buddy(struct gaim_connection *gc, struct group *rem_g, struct buddy *rem_b)
{
	GSList *grp;
	GSList *mem;

	struct group *delg;
	struct buddy *delb;

	/* we assume that gc is not NULL and that the buddy exists somewhere within the
	 * gc's buddy list, therefore we can safely remove it. we need to ensure this
	 * via the UI
	 */

	grp = g_slist_find(gc->groups, rem_g);
	delg = (struct group *)grp->data;
	mem = delg->members;

	mem = g_slist_find(mem, rem_b);
	delb = (struct buddy *)mem->data;

	delg->members = g_slist_remove(delg->members, delb);

	ui_remove_buddy(gc, rem_g, rem_b);

	g_free(rem_b);

	/* we don't flush buddy list to cache because in the case of remove_group that would
	 * mean writing to the buddy list file once for each buddy, plus one more time */
}

void remove_group(struct gaim_connection *gc, struct group *rem_g)
{
	GSList *grp;
	GSList *mem;
	GList *tmp = NULL;

	struct group *delg;
	struct buddy *delb;

	/* we assume that the group actually does exist within the gc, and that the gc is not NULL.
	 * the UI is responsible for this */

	grp = g_slist_find(gc->groups, rem_g);
	delg = (struct group *)grp->data;
	mem = delg->members;

	while (delg->members) {
		delb = (struct buddy *)delg->members->data;
		tmp = g_list_append(tmp, g_strdup(delb->name));
		remove_buddy(gc, delg, delb);	/* this should take care of removing
						   the group_show if necessary */
	}

	gc->groups = g_slist_remove(gc->groups, delg);

	serv_remove_buddies(gc, tmp);
	while (tmp) {
		g_free(tmp->data);
		tmp = g_list_remove(tmp, tmp->data);
	}

	ui_remove_group(gc, rem_g);

	g_free(rem_g);

	/* don't flush buddy list to cache in order to be consistent with remove_buddy,
	 * mostly. remove_group is only called from one place, so we'll let it handle it. */
}

struct buddy *add_buddy(struct gaim_connection *gc, char *group, char *buddy, char *show)
{
	struct buddy *b;
	struct group *g;
	char *good;

	if (!g_slist_find(connections, gc))
		return NULL;

	if ((b = find_buddy(gc, buddy)) != NULL)
		return b;

	g = find_group(gc, group);

	if (g == NULL)
		g = add_group(gc, group);

	b = (struct buddy *)g_new0(struct buddy, 1);

	if (!b)
		return NULL;

	b->gc = gc;
	b->present = 0;

	if (gc->prpl->normalize)
		good = (*gc->prpl->normalize)(buddy);
	else
		good = buddy;

	g_snprintf(b->name, sizeof(b->name), "%s", good);
	g_snprintf(b->show, sizeof(b->show), "%s", show ? (show[0] ? show : good) : good);

	g->members = g_slist_append(g->members, b);

	b->idle = 0;
	b->caps = 0;

	ui_add_buddy(gc, g, b);

	return b;
}

struct group *add_group(struct gaim_connection *gc, char *group)
{
	struct group *g = find_group(gc, group);
	if (g)
		return g;
	if (!g_slist_find(connections, gc))
		return NULL;
	g = (struct group *)g_new0(struct group, 1);
	if (!g)
		return NULL;

	g->gc = gc;
	strncpy(g->name, group, sizeof(g->name));
	gc->groups = g_slist_append(gc->groups, g);

	g->members = NULL;

	ui_add_group(gc, g);

	return g;
}

struct group *find_group(struct gaim_connection *gc, char *group)
{
	struct group *g;
	GSList *grp;
	GSList *c = connections;
	struct gaim_connection *z;
	char *grpname = g_malloc(strlen(group) + 1);

	strcpy(grpname, normalize (group));
	if (gc) {
		if (!g_slist_find(connections, gc))
			return NULL;
		grp = gc->groups;
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
	} else {
		while (c) {
			z = (struct gaim_connection *)c->data;
			grp = z->groups;
			while (grp) {
				g = (struct group *)grp->data;
				if (!g_strcasecmp(normalize (g->name), grpname)) {
					g_free(grpname);
					return g;
				}
				grp = g_slist_next(grp);
			}

			c = c->next;
		}
		g_free(grpname);
		return NULL;
	}
}

struct group *find_group_by_buddy(struct gaim_connection *gc, char *who)
{
	struct group *g;
	struct buddy *b;
	GSList *grp;
	GSList *mem;
	char *whoname;
	char *(*norm)(const char *);

	if (gc) {
		if (gc->prpl->normalize)
			norm = gc->prpl->normalize;
		else
			norm = normalize;
		whoname = g_strdup((*norm)(who));
		grp = gc->groups;
		while (grp) {
			g = (struct group *)grp->data;

			mem = g->members;
			while (mem) {
				b = (struct buddy *)mem->data;
				if (!strcmp((*norm)(b->name), whoname)) {
					g_free(whoname);
					return g;
				}
				mem = mem->next;
			}
			grp = g_slist_next(grp);
		}
		g_free(whoname);
		return NULL;
	} else {
		GSList *c = connections;
		struct gaim_connection *z;
		while (c) {
			z = (struct gaim_connection *)c->data;
			if (z->prpl->normalize)
				norm = z->prpl->normalize;
			else
				norm = normalize;
			whoname = g_strdup((*norm)(who));
			grp = z->groups;
			while (grp) {
				g = (struct group *)grp->data;

				mem = g->members;
				while (mem) {
					b = (struct buddy *)mem->data;
					if (!strcmp((*norm)(b->name), whoname)) {
						g_free(whoname);
						return g;
					}
					mem = mem->next;
				}
				grp = g_slist_next(grp);
			}
			c = c->next;
			g_free(whoname);
		}
		return NULL;
	}
}

struct buddy *find_buddy(struct gaim_connection *gc, char *who)
{
	struct group *g;
	struct buddy *b;
	GSList *grp;
	GSList *c;
	struct gaim_connection *z;
	GSList *mem;
	char *whoname;
	char *(*norm)(const char *);

	if (gc) {
		if (!g_slist_find(connections, gc))
			return NULL;
		if (gc->prpl->normalize)
			norm = gc->prpl->normalize;
		else
			norm = normalize;
		whoname = g_strdup((*norm)(who));
		grp = gc->groups;
		while (grp) {
			g = (struct group *)grp->data;

			mem = g->members;
			while (mem) {
				b = (struct buddy *)mem->data;
				if (!strcmp((*norm)(b->name), whoname)) {
					g_free(whoname);
					return b;
				}
				mem = mem->next;
			}
			grp = g_slist_next(grp);
		}
		g_free(whoname);
		return NULL;
	} else {
		c = connections;
		while (c) {
			z = (struct gaim_connection *)c->data;
			if (z->prpl->normalize)
				norm = z->prpl->normalize;
			else
				norm = normalize;
			whoname = g_strdup((*norm)(who));
			grp = z->groups;
			while (grp) {
				g = (struct group *)grp->data;

				mem = g->members;
				while (mem) {
					b = (struct buddy *)mem->data;
					if (!strcmp((*norm)(b->name), whoname)) {
						g_free(whoname);
						return b;
					}
					mem = mem->next;
				}
				grp = g_slist_next(grp);
			}
			c = c->next;
			g_free(whoname);
		}
		return NULL;
	}
}

void parse_toc_buddy_list(struct gaim_connection *gc, char *config)
{
	char *c;
	char current[256];
	char *name;
	GList *bud;
	int how_many = 0;

	bud = NULL;

	if (config != NULL) {

		/* skip "CONFIG:" (if it exists) */
		c = strncmp(config + 6 /* sizeof(struct sflap_hdr) */ , "CONFIG:", strlen("CONFIG:")) ?
		    strtok(config, "\n") :
		    strtok(config + 6 /* sizeof(struct sflap_hdr) */  + strlen("CONFIG:"), "\n");
		do {
			if (c == NULL)
				break;
			if (*c == 'g') {
				strncpy(current, c + 2, sizeof(current));
				if (!find_group(gc, current)) {
					add_group(gc, current);
					how_many++;
				}
			} else if (*c == 'b' && !find_buddy(gc, c + 2)) {
				char nm[80], sw[80], *tmp = c + 2;
				int i = 0;
				while (*tmp != ':' && *tmp)
					nm[i++] = *tmp++;
				if (*tmp == ':')
					*tmp++ = '\0';
				nm[i] = '\0';
				i = 0;
				while (*tmp)
					sw[i++] = *tmp++;
				sw[i] = '\0';
				if (!find_buddy(gc, nm)) {
					add_buddy(gc, current, nm, sw);
					how_many++;
					bud = g_list_append(bud, c + 2);
				}
			} else if (*c == 'p') {
				GSList *d = gc->permit;
				char *n;
				name = g_malloc(strlen(c + 2) + 2);
				g_snprintf(name, strlen(c + 2) + 1, "%s", c + 2);
				n = g_strdup(normalize (name));
				while (d) {
					if (!g_strcasecmp(n, normalize (d->data)))
						 break;
					d = d->next;
				}
				g_free(n);
				if (!d) {
					gc->permit = g_slist_append(gc->permit, name);
					how_many++;
				} else
					g_free(name);
			} else if (*c == 'd') {
				GSList *d = gc->deny;
				char *n;
				name = g_malloc(strlen(c + 2) + 2);
				g_snprintf(name, strlen(c + 2) + 1, "%s", c + 2);
				n = g_strdup(normalize (name));
				while (d) {
					if (!g_strcasecmp(n, normalize (d->data)))
						 break;
					d = d->next;
				}
				g_free(n);
				if (!d) {
					gc->deny = g_slist_append(gc->deny, name);
					how_many++;
				} else
					g_free(name);
			} else if (!strncmp("toc", c, 3)) {
				sscanf(c + strlen(c) - 1, "%d", &gc->permdeny);
				debug_printf("permdeny: %d\n", gc->permdeny);
				if (gc->permdeny == 0)
					gc->permdeny = 1;
			} else if (*c == 'm') {
				sscanf(c + 2, "%d", &gc->permdeny);
				debug_printf("permdeny: %d\n", gc->permdeny);
				if (gc->permdeny == 0)
					gc->permdeny = 1;
			}
		} while ((c = strtok(NULL, "\n")));

		if (bud != NULL) {
			serv_add_buddies(gc, bud);
			g_list_free(bud);
		}
		serv_set_permit_deny(gc);
	}

	if (how_many != 0)
		do_export(gc);
}

void toc_build_config(struct gaim_connection *gc, char *s, int len, gboolean show)
{
	GSList *grp = gc->groups;
	GSList *mem;
	struct group *g;
	struct buddy *b;
	GSList *plist = gc->permit;
	GSList *dlist = gc->deny;

	int pos = 0;

	if (!gc->permdeny)
		gc->permdeny = 1;

	pos += g_snprintf(&s[pos], len - pos, "m %d\n", gc->permdeny);
	while (len > pos && grp) {
		g = (struct group *)grp->data;
		pos += g_snprintf(&s[pos], len - pos, "g %s\n", g->name);
		mem = g->members;
		while (len > pos && mem) {
			b = (struct buddy *)mem->data;
			pos += g_snprintf(&s[pos], len - pos, "b %s%s%s\n", b->name,
					  (show && strcmp(b->name, b->show)) ? ":" : "",
					  (show && strcmp(b->name, b->show)) ? b->show : "");
			mem = mem->next;
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
static void translate_lst(FILE *src_fp, char *dest)
{
	char line[BUF_LEN], *line2;
	char *name;
	int i;

	sprintf(dest, "m 1\n");

	while (fgets(line, BUF_LEN, src_fp)) {
		line2 = g_strchug(line);
		if (strstr(line2, "group") == line2) {
			name = strpbrk(line2, " \t\n\r\f") + 1;
			strcat(dest, "g ");
			for (i = 0; i < strcspn(name, "\n\r"); i++)
				if (name[i] != '\"')
					strncat(dest, &name[i], 1);
			strcat(dest, "\n");
		}
		if (strstr(line2, "buddy") == line2) {
			name = strpbrk(line2, " \t\n\r\f") + 1;
			strcat(dest, "b ");
			for (i = 0; i < strcspn(name, "\n\r"); i++)
				if (name[i] != '\"')
					strncat(dest, &name[i], 1);
			strcat(dest, "\n");
		}
	}

	return;
}


/* translate an AIM 4 buddylist (*.blt) to Gaim format */
static void translate_blt(FILE *src_fp, char *dest)
{
	int i;
	char line[BUF_LEN];
	char *buddy;

	sprintf(dest, "m 1\n");

	while (strstr(fgets(line, BUF_LEN, src_fp), "Buddy") == NULL);
	while (strstr(fgets(line, BUF_LEN, src_fp), "list") == NULL);

	while (1) {
		fgets(line, BUF_LEN, src_fp); g_strchomp(line);
		if (strchr(line, '}') != NULL)
			break;

		if (strchr(line, '{') != NULL) {
			/* Syntax starting with "<group> {" */

			strcat(dest, "g ");
			buddy = g_strchug(strtok(line, "{"));
			for (i = 0; i < strlen(buddy); i++) {
				if (buddy[i] != '\"') {
					strncat(dest, &buddy[i], 1);
				}
			}
			strcat(dest, "\n");
			while (strchr(fgets(line, BUF_LEN, src_fp), '}') == NULL) {
				gboolean pounce = FALSE;
				g_strchomp(line);
				buddy = g_strchug(line);
				debug_printf("\nbuddy: \"%s\"\n\n", buddy);
				strcat(dest, "b ");
				if (strchr(buddy, '{') != NULL) {
					/* buddy pounce, etc */
					char *pos = strchr(buddy, '{') - 1;
					*pos = 0;
					pounce = TRUE;
				}
				if (strchr(buddy, '\"') != NULL) {
					buddy++;
					strncat(dest, buddy, strchr(buddy, '\"') - buddy);
				} else
					strcat(dest, buddy);
				strcat(dest, "\n");
				if (pounce)
					do
						fgets(line, BUF_LEN, src_fp);
					while (!strchr(line, '}'));
			}
		} else {

			/* Syntax "group buddy buddy ..." */
			buddy = g_strchug(strtok(line, " \n"));
			strcat(dest, "g ");
			if (strchr(buddy, '\"') != NULL) {
				strcat(dest, &buddy[1]);
				strcat(dest, " ");
				buddy = g_strchug(strtok(NULL, " \n"));
				while (strchr(buddy, '\"') == NULL) {
					strcat(dest, buddy);
					strcat(dest, " ");
					buddy = g_strchug(strtok(NULL, " \n"));
				}
				strncat(dest, buddy, strlen(buddy) - 1);
			} else {
				strcat(dest, buddy);
			}
			strcat(dest, "\n");
			while ((buddy = g_strchug(strtok(NULL, " \n"))) != NULL) {
				strcat(dest, "b ");
				if (strchr(buddy, '\"') != NULL) {
					strcat(dest, &buddy[1]);
					strcat(dest, " ");
					buddy = g_strchug(strtok(NULL, " \n"));
					while (strchr(buddy, '\"') == NULL) {
						strcat(dest, buddy);
						strcat(dest, " ");
						buddy = g_strchug(strtok(NULL, " \n"));
					}
					strncat(dest, buddy, strlen(buddy) - 1);
				} else {
					strcat(dest, buddy);
				}
				strcat(dest, "\n");
			}
		}
	}

	return;
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

/* see if a buddy list cache file for this user exists */

gboolean bud_list_cache_exists(struct gaim_connection *gc)
{
	gboolean ret = FALSE;
	char path[PATHSIZE];
	char *file;
	struct stat sbuf;
	char *g_screenname;

	g_screenname = get_screenname_filename(gc->username);

	file = gaim_user_dir();
	if (file != (char *)NULL) {
		g_snprintf(path, sizeof path, "%s/%s.%d.blist", file, g_screenname, gc->protocol);
		if (!stat(path, &sbuf)) {
			debug_printf("%s exists.\n", path);
			ret = TRUE;
		} else {
			char path2[PATHSIZE];
			debug_printf("%s does not exist.\n", path);
			g_snprintf(path2, sizeof path2, "%s/%s.blist", file, g_screenname);
			if (!stat(path2, &sbuf)) {
				debug_printf("%s exists, moving to %s\n", path2, path);
				if (rename(path2, path))
					debug_printf("rename didn't work!\n");
				else
					ret = TRUE;
			}
		}
		g_free(file);
	}
	g_free(g_screenname);
	return ret;
}

void do_import(struct gaim_connection *gc, char *filename)
{
	char *buf = g_malloc(BUF_LONG * 2);
	char *buf2;
	char *first = g_malloc(64);
	char *file;
	char path[PATHSIZE];
	char *g_screenname;
	int len;
	FILE *f;

	if (filename) {
		g_snprintf(path, sizeof(path), "%s", filename);
	} else {
		g_screenname = get_screenname_filename(gc->username);

		file = gaim_user_dir();
		if (file != (char *)NULL) {
			sprintf(path, "%s/%s.%d.blist", file, g_screenname, gc->protocol);
			g_free(file);
			g_free(g_screenname);
		} else {
			g_free(g_screenname);
			g_free(buf);
			g_free(first);
			return;
		}
	}

	if (!(f = fopen(path, "r"))) {
		debug_printf("Unable to open %s.\n", path);
		g_free(buf);
		g_free(first);
		return;
	}

	fgets(first, 64, f);

	/* AIM 4 buddy list */
	if (!g_strncasecmp(first, "Config {", strlen("Config {"))) {
		debug_printf("aim 4\n");
		rewind(f);
		translate_blt(f, buf);
		debug_printf("%s\n", buf);
		buf2 = buf;
		buf = g_malloc(8193);
		g_snprintf(buf, 8192, "toc_set_config {%s}\n", buf2);
		g_free(buf2);
		/* AIM 3 buddy list */
	} else if (strstr(first, "group") != NULL) {
		debug_printf("aim 3\n");
		rewind(f);
		translate_lst(f, buf);
		debug_printf("%s\n", buf);
		buf2 = buf;
		buf = g_malloc(8193);
		g_snprintf(buf, 8192, "toc_set_config {%s}\n", buf2);
		g_free(buf2);
		/* Gaim buddy list - no translation */
	} else if (first[0] == 'm') {
		rewind(f);
		len = fread(buf, 1, BUF_LONG * 2, f);
		buf[len] = '\0';
		buf2 = buf;
		buf = g_malloc(8193);
		g_snprintf(buf, 8192, "toc_set_config {%s}\n", buf2);
		g_free(buf2);
		/* Something else */
	} else {
		g_free(buf);
		g_free(first);
		fclose(f);
		return;
	}

	parse_toc_buddy_list(gc, buf);

	fclose(f);

	g_free(buf);
	g_free(first);
}

void do_export(struct gaim_connection *g)
{
	FILE *dir;
	FILE *f;
	char buf[32 * 1024];
	char *file;
	char path[PATHSIZE];
	char *g_screenname;

	file = gaim_user_dir();
	if (!file)
		return;

	strcpy(buf, file);
	dir = fopen(buf, "r");
	if (!dir)
		mkdir(buf, S_IRUSR | S_IWUSR | S_IXUSR);
	else
		fclose(dir);

	g_screenname = get_screenname_filename(g->username);

	sprintf(path, "%s/%s.%d.blist", file, g_screenname, g->protocol);
	if ((f = fopen(path, "w"))) {
		debug_printf("writing %s\n", path);
		toc_build_config(g, buf, 8192 - 1, TRUE);
		fprintf(f, "%s\n", buf);
		fclose(f);
		chmod(buf, S_IRUSR | S_IWUSR);
	} else {
		debug_printf("unable to write %s\n", path);
	}

	g_free(g_screenname);
	g_free(file);
}
