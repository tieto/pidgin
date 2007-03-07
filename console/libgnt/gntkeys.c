#include "gntkeys.h"

#include <glib.h>
#include <stdlib.h>
#include <string.h>

char *gnt_key_cup;
char *gnt_key_cdown;
char *gnt_key_cleft;
char *gnt_key_cright;

static const char *term;

void gnt_init_keys()
{
	if (term == NULL) {
		term = getenv("TERM");
		if (!term)
			term = "";  /* Just in case */
	}

	if (strcmp(term, "xterm") == 0 || strcmp(term, "rxvt") == 0) {
		gnt_key_cup    = "\033" "[1;5A";
		gnt_key_cdown  = "\033" "[1;5B";
		gnt_key_cright = "\033" "[1;5C";
		gnt_key_cleft  = "\033" "[1;5D";
	} else if (strcmp(term, "screen") == 0 || strcmp(term, "rxvt-unicode") == 0) {
		gnt_key_cup    = "\033" "Oa";
		gnt_key_cdown  = "\033" "Ob";
		gnt_key_cright = "\033" "Oc";
		gnt_key_cleft  = "\033" "Od";
	}
}

void gnt_keys_refine(char *text)
{
	if (*text == 27 && *(text + 1) == '[' && *(text + 3) == '\0' &&
			(*(text + 2) >= 'A' && *(text + 2) <= 'D')) {
		/* Apparently this is necessary for urxvt and screen and xterm */
		if (strcmp(term, "screen") == 0 || strcmp(term, "rxvt-unicode") == 0 ||
				strcmp(term, "xterm") == 0)
			*(text + 1) = 'O';
	} else if (*(unsigned char*)text == 195) {
		if (*(text + 2) == 0 && strcmp(term, "xterm") == 0) {
			*(text) = 27;
			*(text + 1) -= 64;  /* Say wha? */
		}
	}
}

/**
 * The key-bindings will be saved in a tree. When a keystroke happens, GNT will
 * find the longest sequence that matches a binding and return the length.
 */
#define SIZE 256

#define HAS_CHILD     1 << 0
struct _node
{
	struct _node *next[SIZE];
	int ref;
	int flags;
};

static struct _node root = {.ref = 1, .flags = HAS_CHILD};

static void add_path(struct _node *node, const char *path)
{
	struct _node *n = NULL;
	if (!path || !*path)
		return;
	while (*path && node->next[*path]) {
		node = node->next[*path];
		node->ref++;
		path++;
	}
	if (!*path)
		return;
	n = g_new0(struct _node, 1);
	n->ref = 1;
	node->next[*path++] = n;
	node->flags |= HAS_CHILD;
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
	next = node->next[*path];
	if (!next)
		return;
	del_path(next, path + 1);
	next->ref--;
	if (next->ref == 0) {
		node->next[*path] = NULL;
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

	while (*path && n->next[*path] && (n->flags & HAS_CHILD)) {
		n = n->next[*path++];
		depth++;
	}

	if (n->flags & HAS_CHILD)
		depth = 0;
	return depth;
}

static void
print_path(struct _node *node, int depth)
{
	int i;
	for (i = 0; i < SIZE; i++) {
		if (node->next[i]) {
			g_printerr("%*c (%d:%d)\n", depth, i, node->next[i]->ref,
						node->next[i]->flags);
			print_path(node->next[i], depth + 1);
		}
	}
}

/* this is purely for debugging purposes. */
void gnt_keys_print_combinations()
{
	g_printerr("--------\n");
	print_path(&root, 1);
	g_printerr("--------\n");
}

