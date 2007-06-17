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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef GNT_TREE_H
#define GNT_TREE_H

#include "gntwidget.h"
#include "gnt.h"
#include "gntcolors.h"
#include "gntkeys.h"
#include "gnttextview.h"

#define GNT_TYPE_TREE				(gnt_tree_get_gtype())
#define GNT_TREE(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), GNT_TYPE_TREE, GntTree))
#define GNT_TREE_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), GNT_TYPE_TREE, GntTreeClass))
#define GNT_IS_TREE(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), GNT_TYPE_TREE))
#define GNT_IS_TREE_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GNT_TYPE_TREE))
#define GNT_TREE_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GNT_TYPE_TREE, GntTreeClass))

#define GNT_TREE_FLAGS(obj)				(GNT_TREE(obj)->priv.flags)
#define GNT_TREE_SET_FLAGS(obj, flags)		(GNT_TREE_FLAGS(obj) |= flags)
#define GNT_TREE_UNSET_FLAGS(obj, flags)	(GNT_TREE_FLAGS(obj) &= ~(flags))

typedef struct _GntTree			GntTree;
typedef struct _GntTreePriv		GntTreePriv;
typedef struct _GntTreeClass		GntTreeClass;

typedef struct _GntTreeRow		GntTreeRow;
typedef struct _GntTreeCol		GntTreeCol;

struct _GntTree
{
	GntWidget parent;

	GntTreeRow *current;    /* current selection */

	GntTreeRow *top;        /* The topmost visible item */
	GntTreeRow *bottom;     /* The bottommost visible item */
	
	GntTreeRow *root;       /* The root of all evil */
	
	GList *list;            /* List of GntTreeRow s */
	GHashTable *hash;       /* We need this for quickly referencing the rows */
	guint (*hash_func)(gconstpointer);
	gboolean (*hash_eq_func)(gconstpointer, gconstpointer);
	GDestroyNotify key_destroy;
	GDestroyNotify value_destroy;

	int ncol;               /* No. of columns */
	struct _GntTreeColInfo
	{
		int width;
		char *title;
		gboolean invisible;
	} *columns;             /* Would a GList be better? */
	gboolean show_title;
	gboolean show_separator; /* Whether to show column separators */

	GString *search;
	int search_timeout;

	GCompareFunc compare;
};

struct _GntTreeClass
{
	GntWidgetClass parent;

	void (*selection_changed)(GntTreeRow *old, GntTreeRow * current);
	void (*toggled)(GntTree *tree, gpointer key);

	void (*gnt_reserved1)(void);
	void (*gnt_reserved2)(void);
	void (*gnt_reserved3)(void);
	void (*gnt_reserved4)(void);
};

G_BEGIN_DECLS

/**
 * 
 *
 * @return
 */
GType gnt_tree_get_gtype(void);

/**
 * 
 *
 * @return
 */
GntWidget * gnt_tree_new(void);

      /* A tree with just one column */

/**
 * 
 * @param columns
 *
 * @return
 */
GntWidget * gnt_tree_new_with_columns(int columns);

/**
 * 
 * @param tree
 * @param rows
 */
void gnt_tree_set_visible_rows(GntTree *tree, int rows);

/**
 * 
 * @param tree
 *
 * @return
 */
int gnt_tree_get_visible_rows(GntTree *tree);

/**
 * 
 * @param tree
 * @param count
 */
void gnt_tree_scroll(GntTree *tree, int count);

/**
 * 
 * @param tree
 * @param key
 * @param row
 * @param parent
 * @param bigbro
 *
 * @return
 */
GntTreeRow * gnt_tree_add_row_after(GntTree *tree, void *key, GntTreeRow *row, void *parent, void *bigbro);

/**
 * 
 * @param tree
 * @param key
 * @param row
 * @param parent
 *
 * @return
 */
GntTreeRow * gnt_tree_add_row_last(GntTree *tree, void *key, GntTreeRow *row, void *parent);

/**
 * 
 * @param tree
 *
 * @return
 */
gpointer gnt_tree_get_selection_data(GntTree *tree);

/* Returned string needs to be freed */
/**
 * 
 * @param tree
 *
 * @return
 */
char * gnt_tree_get_selection_text(GntTree *tree);

/**
 * 
 * @param tree
 *
 * @return
 */
GList * gnt_tree_get_selection_text_list(GntTree *tree);

/**
 *
 * @param tree
 *
 * @constreturn
 */
GList *gnt_tree_get_rows(GntTree *tree);

/**
 * 
 * @param tree
 * @param key
 */
void gnt_tree_remove(GntTree *tree, gpointer key);

/**
 * 
 * @param tree
 */
void gnt_tree_remove_all(GntTree *tree);

/* Returns the visible line number of the selected row */
/**
 * 
 * @param tree
 *
 * @return
 */
int gnt_tree_get_selection_visible_line(GntTree *tree);

/**
 * 
 * @param tree
 * @param key
 * @param colno
 * @param text
 */
void gnt_tree_change_text(GntTree *tree, gpointer key, int colno, const char *text);

/**
 * 
 * @param tree
 * @param key
 * @param row
 * @param parent
 * @param bigbro
 *
 * @return
 */
GntTreeRow * gnt_tree_add_choice(GntTree *tree, void *key, GntTreeRow *row, void *parent, void *bigbro);

/**
 * 
 * @param tree
 * @param key
 * @param set
 */
void gnt_tree_set_choice(GntTree *tree, void *key, gboolean set);

/**
 * 
 * @param tree
 * @param key
 *
 * @return
 */
gboolean gnt_tree_get_choice(GntTree *tree, void *key);

/**
 * 
 * @param tree
 * @param key
 * @param flags
 */
void gnt_tree_set_row_flags(GntTree *tree, void *key, GntTextFormatFlags flags);

/**
 * 
 * @param key
 */
void gnt_tree_set_selected(GntTree *tree , void *key);

/**
 * 
 * @param tree
 *
 * @return
 */
GntTreeRow * gnt_tree_create_row(GntTree *tree, ...);

/**
 * 
 * @param tree
 * @param list
 *
 * @return
 */
GntTreeRow * gnt_tree_create_row_from_list(GntTree *tree, GList *list);

/**
 * 
 * @param tree
 * @param col
 * @param width
 */
void gnt_tree_set_col_width(GntTree *tree, int col, int width);

/**
 * 
 * @param tree
 */
void gnt_tree_set_column_titles(GntTree *tree, ...);

/**
 * 
 * @param tree
 * @param set
 */
void gnt_tree_set_show_title(GntTree *tree, gboolean set);

/**
 * 
 * @param tree
 * @param func
 */
void gnt_tree_set_compare_func(GntTree *tree, GCompareFunc func);

/**
 * 
 * @param tree
 * @param key
 * @param expanded
 */
void gnt_tree_set_expanded(GntTree *tree, void *key, gboolean expanded);

/**
 * 
 * @param tree
 * @param set
 */
void gnt_tree_set_show_separator(GntTree *tree, gboolean set);

/**
 * 
 * @param tree
 * @param row
 */
void gnt_tree_sort_row(GntTree *tree, void *row);

/* This will try to automatically adjust the width of the columns in the tree */
/**
 * 
 * @param tree
 */
void gnt_tree_adjust_columns(GntTree *tree);

/**
 * 
 * @param tree
 * @param hash
 * @param eq
 * @param kd
 */
void gnt_tree_set_hash_fns(GntTree *tree, gpointer hash, gpointer eq, gpointer kd);

/* This can be useful when, for example, we want to store some data
 * which we don't want/need to display. */
/**
 * 
 * @param tree
 * @param col
 * @param vis
 */
void gnt_tree_set_column_visible(GntTree *tree, int col, gboolean vis);

G_END_DECLS

/* The following functions should NOT be used by applications. */

/* This should be called by the subclasses of GntTree's in their _new function */
/**
 * 
 * @param tree
 * @param col
 */
void _gnt_tree_init_internals(GntTree *tree, int col);

#endif /* GNT_TREE_H */
