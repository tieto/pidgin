/**
 * GNT - The GLib Ncurses Toolkit
 *
 * GNT is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This library is free software; you can redistribute it and/or modify
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#include "gntinternal.h"
#undef GNT_LOG_DOMAIN
#define GNT_LOG_DOMAIN "Keys"

#include "gntkeys.h"

#include <glib.h>
#include <stdlib.h>
#include <string.h>

char *gnt_key_cup;
char *gnt_key_cdown;
char *gnt_key_cleft;
char *gnt_key_cright;

static const char *term;
static GHashTable *specials;

void gnt_init_keys()
{
	const char *controls[] = {"", "c-", "ctrl-", "ctr-", "ctl-", NULL};
	const char *alts[] = {"", "alt-", "a-", "m-", "meta-", NULL};
	int c, a, ch;
	char key[32];

	if (term == NULL) {
		term = getenv("TERM");
		if (!term)
			term = "";  /* Just in case */
	}

	if (strstr(term, "xterm") == term || strcmp(term, "rxvt") == 0) {
		gnt_key_cup    = "\033" "[1;5A";
		gnt_key_cdown  = "\033" "[1;5B";
		gnt_key_cright = "\033" "[1;5C";
		gnt_key_cleft  = "\033" "[1;5D";
	} else if (strstr(term, "screen") == term || strcmp(term, "rxvt-unicode") == 0) {
		gnt_key_cup    = "\033" "Oa";
		gnt_key_cdown  = "\033" "Ob";
		gnt_key_cright = "\033" "Oc";
		gnt_key_cleft  = "\033" "Od";
	}

	specials = g_hash_table_new(g_str_hash, g_str_equal);

#define INSERT_KEY(k, code) do { \
		g_hash_table_insert(specials, g_strdup(k), g_strdup(code)); \
		gnt_keys_add_combination(code); \
	} while (0)

	INSERT_KEY("home",     GNT_KEY_HOME);
	INSERT_KEY("end",      GNT_KEY_END);
	INSERT_KEY("pageup",   GNT_KEY_PGUP);
	INSERT_KEY("pagedown", GNT_KEY_PGDOWN);
	INSERT_KEY("insert",   GNT_KEY_INS);
	INSERT_KEY("delete",   GNT_KEY_DEL);
	INSERT_KEY("back_tab", GNT_KEY_BACK_TAB);

	INSERT_KEY("left",   GNT_KEY_LEFT);
	INSERT_KEY("right",  GNT_KEY_RIGHT);
	INSERT_KEY("up",     GNT_KEY_UP);
	INSERT_KEY("down",   GNT_KEY_DOWN);

	INSERT_KEY("tab",    "\t");
	INSERT_KEY("escape", "\033");
	INSERT_KEY("space", " ");
	INSERT_KEY("return", GNT_KEY_ENTER);
	INSERT_KEY("menu",   GNT_KEY_POPUP);

	INSERT_KEY("f1",   GNT_KEY_F1);
	INSERT_KEY("f2",   GNT_KEY_F2);
	INSERT_KEY("f3",   GNT_KEY_F3);
	INSERT_KEY("f4",   GNT_KEY_F4);
	INSERT_KEY("f5",   GNT_KEY_F5);
	INSERT_KEY("f6",   GNT_KEY_F6);
	INSERT_KEY("f7",   GNT_KEY_F7);
	INSERT_KEY("f8",   GNT_KEY_F8);
	INSERT_KEY("f9",   GNT_KEY_F9);
	INSERT_KEY("f10",  GNT_KEY_F10);
	INSERT_KEY("f11",  GNT_KEY_F11);
	INSERT_KEY("f12",  GNT_KEY_F12);

#define REM_LENGTH  (sizeof(key) - (cur - key))
#define INSERT_COMB(k, code) do { \
		snprintf(key, sizeof(key), "%s%s%s", controls[c], alts[a], k);  \
		INSERT_KEY(key, code);  \
	} while (0)
#define INSERT_COMB_CODE(k, c1, c2) do { \
		char __[32]; \
		snprintf(__, sizeof(__), "%s%s", c1, c2); \
		INSERT_COMB(k, __); \
	} while (0)

	/* Lower-case alphabets */
	for (a = 0, c = 0; controls[c]; c++, a = 0) {
		if (c) {
			INSERT_COMB("up",    gnt_key_cup);
			INSERT_COMB("down",  gnt_key_cdown);
			INSERT_COMB("left",  gnt_key_cleft);
			INSERT_COMB("right", gnt_key_cright);
		}

		for (a = 0; alts[a]; a++) {
			for (ch = 0; ch < 26; ch++) {
				char str[2] = {'a' + ch, 0}, code[4] = "\0\0\0\0";
				int ind = 0;
				if (a)
					code[ind++] = '\033';
				code[ind] = (c ? 1 : 'a') + ch;
				INSERT_COMB(str, code);
			}
			if (c == 0 && a) {
				INSERT_COMB("tab", "\033\t");
				INSERT_COMB_CODE("up", "\033", GNT_KEY_UP);
				INSERT_COMB_CODE("down", "\033", GNT_KEY_DOWN);
				INSERT_COMB_CODE("left", "\033", GNT_KEY_LEFT);
				INSERT_COMB_CODE("right", "\033", GNT_KEY_RIGHT);
			}
		}
	}
	c = 0;
	for (a = 0; alts[a]; a++) {
		/* Upper-case alphabets */
		for (ch = 0; ch < 26; ch++) {
			char str[2] = {'A' + ch, 0}, code[] = {'\033', 'A' + ch, 0};
			INSERT_COMB(str, code);
		}
		/* Digits */
		for (ch = 0; ch < 10; ch++) {
			char str[2] = {'0' + ch, 0}, code[] = {'\033', '0' + ch, 0};
			INSERT_COMB(str, code);
		}
	}
}

void gnt_keys_refine(char *text)
{
	while (*text == 27 && *(text + 1) == 27)
		text++;
	if (*text == 27 && *(text + 1) == '[' &&
			(*(text + 2) >= 'A' && *(text + 2) <= 'D')) {
		/* Apparently this is necessary for urxvt and screen and xterm */
		if (strstr(term, "screen") == term || strcmp(term, "rxvt-unicode") == 0 ||
				strstr(term, "xterm") == term ||
				strstr(term, "vt100") == term)
			*(text + 1) = 'O';
	} else if (*(unsigned char*)text == 195) {
		if (*(text + 2) == 0 && strstr(term, "xterm") == term) {
			*(text) = 27;
			*(text + 1) -= 64;  /* Say wha? */
		}
	}
}

const char *gnt_key_translate(const char *name)
{
	return name ? g_hash_table_lookup(specials, name) : NULL;
}

typedef struct {
	const char *name;
	const char *key;
} gntkey;

static void
get_key_name(gpointer key, gpointer value, gpointer data)
{
	gntkey *k = data;
	if (k->name)
		return;
	if (g_utf8_collate(value, k->key) == 0)
		k->name = key;
}

const char *gnt_key_lookup(const char *key)
{
	gntkey k = {NULL, key};
	g_hash_table_foreach(specials, get_key_name, &k);
	return k.name;
}

/**
 * The key-bindings will be saved in a tree. When a keystroke happens, GNT will
 * find the sequence that matches a binding and return the length.
 * A sequence should not be a prefix of another sequence. If it is, then only
 * the shortest one will be processed. If we want to change that, we will need
 * to allow getting the k-th prefix that matches the input, and pay attention
 * to the return value of gnt_wm_process_input in gntmain.c.
 */
#define SIZE 256

#define IS_END         1 << 0
struct _node
{
	struct _node *next[SIZE];
	int ref;
	int flags;
};

static struct _node root = {.ref = 1, .flags = 0};

static void add_path(struct _node *node, const char *path)
{
	struct _node *n = NULL;
	if (!path || !*path) {
		node->flags |= IS_END;
		return;
	}
	while (*path && node->next[(unsigned char)*path]) {
		node = node->next[(unsigned char)*path];
		node->ref++;
		path++;
	}
	if (!*path)
		return;
	n = g_new0(struct _node, 1);
	n->ref = 1;
	node->next[(unsigned char)*path++] = n;
	add_path(n, path);
}

void gnt_keys_add_combination(const char *path)
{
	add_path(&root, path);
}

static void del_path(struct _node *node, const char *path)
{
	struct _node *next = NULL;

	if (!*path)
		return;
	next = node->next[(unsigned char)*path];
	if (!next)
		return;
	del_path(next, path + 1);
	next->ref--;
	if (next->ref == 0) {
		node->next[(unsigned char)*path] = NULL;
		g_free(next);
	}
}

void gnt_keys_del_combination(const char *path)
{
	del_path(&root, path);
}

int gnt_keys_find_combination(const char *path)
{
	int depth = 0;
	struct _node *n = &root;

	root.flags &= ~IS_END;
	while (*path && n->next[(unsigned char)*path] && !(n->flags & IS_END)) {
		if (!g_ascii_isspace(*path) &&
				!g_ascii_iscntrl(*path) &&
				!g_ascii_isgraph(*path))
			return 0;
		n = n->next[(unsigned char)*path++];
		depth++;
	}

	if (!(n->flags & IS_END))
		depth = 0;
	return depth;
}

static void
print_path(struct _node *node, int depth)
{
	int i;
	for (i = 0; i < SIZE; i++) {
		if (node->next[i]) {
			g_printerr("%*c (%d:%d)\n", depth * 4, i, node->next[i]->ref,
						node->next[i]->flags);
			print_path(node->next[i], depth + 1);
		}
	}
}

/* this is purely for debugging purposes. */
void gnt_keys_print_combinations(void);
void gnt_keys_print_combinations()
{
	g_printerr("--------\n");
	print_path(&root, 1);
	g_printerr("--------\n");
}

