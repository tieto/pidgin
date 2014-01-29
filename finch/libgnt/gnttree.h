/**
 * @file gnttree.h Tree API
 * @ingroup gnt
 */
/*
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

typedef struct _GntTree			GntTree;
typedef struct _GntTreePriv		GntTreePriv;
typedef struct _GntTreeClass		GntTreeClass;

typedef struct _GntTreeRow		GntTreeRow;
typedef struct _GntTreeCol		GntTreeCol;

typedef enum {
	GNT_TREE_COLUMN_INVISIBLE    = 1 << 0,
	GNT_TREE_COLUMN_FIXED_SIZE   = 1 << 1,
	GNT_TREE_COLUMN_BINARY_DATA  = 1 << 2,
	GNT_TREE_COLUMN_RIGHT_ALIGNED = 1 << 3,
} GntTreeColumnFlag;

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
		int width_ratio;
		GntTreeColumnFlag flags;
	} *columns;             /* Would a GList be better? */
	gboolean show_title;
	gboolean show_separator; /* Whether to show column separators */

	GntTreePriv *priv;
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
 * @return The GType for GntTree
 */
GType gnt_tree_get_gtype(void);

/**
 * Create a tree with one column.
 *
 * @return The newly created tree
 *
 * @see gnt_tree_new_with_columns
 */
GntWidget * gnt_tree_new(void);

/**
 * Create a tree with a specified number of columns.
 *
 * @param columns  Number of columns
 *
 * @return  The newly created tree
 *
 * @see gnt_tree_new
 */
GntWidget * gnt_tree_new_with_columns(int columns);

/**
 * The number of rows the tree should display at a time.
 *
 * @param tree  The tree
 * @param rows  The number of rows
 */
void gnt_tree_set_visible_rows(GntTree *tree, int rows);

/**
 * Get the number visible rows.
 *
 * @param tree  The tree
 *
 * @return  The number of visible rows
 */
int gnt_tree_get_visible_rows(GntTree *tree);

/**
 * Scroll the contents of the tree.
 *
 * @param tree   The tree
 * @param count  If positive, the tree will be scrolled down by count rows,
 *               otherwise, it will be scrolled up by count rows.
 */
void gnt_tree_scroll(GntTree *tree, int count);

/**
 * Insert a row in the tree.
 *
 * @param tree    The tree
 * @param key     The key for the row
 * @param row     The row to insert
 * @param parent  The key for the parent row
 * @param bigbro  The key for the row to insert the new row after.
 *
 * @return  The inserted row
 *
 * @see gnt_tree_create_row
 * @see gnt_tree_add_row_last
 * @see gnt_tree_add_choice
 */
GntTreeRow * gnt_tree_add_row_after(GntTree *tree, void *key, GntTreeRow *row, void *parent, void *bigbro);

/**
 * Insert a row at the end of the tree.
 *
 * @param tree    The tree
 * @param key     The key for the row
 * @param row     The row to insert
 * @param parent  The key for the parent row
 *
 * @return The inserted row
 *
 * @see gnt_tree_create_row
 * @see gnt_tree_add_row_after
 * @see gnt_tree_add_choice
 */
GntTreeRow * gnt_tree_add_row_last(GntTree *tree, void *key, GntTreeRow *row, void *parent);

/**
 * Get the key for the selected row.
 *
 * @param tree  The tree
 *
 * @return   The key for the selected row
 */
gpointer gnt_tree_get_selection_data(GntTree *tree);

/**
 * Get the text displayed for the selected row.
 *
 * @param tree  The tree
 *
 * @return  The text, which needs to be freed by the caller
 * @see gnt_tree_get_row_text_list
 * @see gnt_tree_get_selection_text_list
 */
char * gnt_tree_get_selection_text(GntTree *tree);

/**
 * Get a list of text for a row.
 *
 * @param tree  The tree
 * @param key   A key corresponding to the row in question. If key
 *              is @c NULL, the text list for the selected row will
 *              be returned.
 *
 * @return A list of texts of a row. The list and its data should be
 *         freed by the caller. The caller should make sure that if
 *         any column of the tree contains binary data, it's not freed.
 * @see gnt_tree_get_selection_text_list
 * @see gnt_tree_get_selection_text
 */
GList * gnt_tree_get_row_text_list(GntTree *tree, gpointer key);

/**
 * Get the key of a row.
 *
 * @param tree   The tree
 * @param row    The GntTreeRow object
 *
 * @return The key of the row.
 * @since 2.8.0 (gnt), 2.7.2 (pidgin)
 */
gpointer gnt_tree_row_get_key(GntTree *tree, GntTreeRow *row);

/**
 * Get the next row.
 *
 * @param tree The tree
 * @param row  The GntTreeRow object
 *
 * @return The next row.
 * @since 2.8.0 (gnt), 2.7.2 (pidgin)
 */
GntTreeRow * gnt_tree_row_get_next(GntTree *tree, GntTreeRow *row);

/**
 * Get the previous row.
 *
 * @param tree The tree
 * @param row  The GntTreeRow object
 *
 * @return The previous row.
 * @since 2.8.0 (gnt), 2.7.2 (pidgin)
 */
GntTreeRow * gnt_tree_row_get_prev(GntTree *tree, GntTreeRow *row);

/**
 * Get the child row.
 *
 * @param tree The tree
 * @param row  The GntTreeRow object
 *
 * @return The child row.
 * @since 2.8.0 (gnt), 2.7.2 (pidgin)
 */
GntTreeRow * gnt_tree_row_get_child(GntTree *tree, GntTreeRow *row);

/**
 * Get the parent row.
 *
 * @param tree The tree
 * @param row  The GntTreeRow object
 *
 * @return The parent row.
 * @since 2.8.0 (gnt), 2.7.2 (pidgin)
 */
GntTreeRow * gnt_tree_row_get_parent(GntTree *tree, GntTreeRow *row);

/**
 * Get a list of text of the current row.
 *
 * @param tree  The tree
 *
 * @return A list of texts of the currently selected row. The list
 *         and its data should be freed by the caller. The caller
 *         should make sure that if any column of the tree contains
 *         binary data, it's not freed.
 * @see gnt_tree_get_row_text_list
 * @see gnt_tree_get_selection_text
 */
GList * gnt_tree_get_selection_text_list(GntTree *tree);

/**
 * Returns the list of rows in the tree.
 *
 * @param tree  The tree
 *
 * @return The list of the rows. The list should not be modified by the caller.
 */
GList *gnt_tree_get_rows(GntTree *tree);

/**
 * Remove a row from the tree.
 *
 * @param tree  The tree
 * @param key   The key for the row to remove
 */
void gnt_tree_remove(GntTree *tree, gpointer key);

/**
 * Remove all the item from the tree.
 *
 * @param tree  The tree
 */
void gnt_tree_remove_all(GntTree *tree);

/**
 * Get the visible line number of the selected row.
 *
 * @param tree  The tree
 *
 * @return  The line number of the currently selected row
 */
int gnt_tree_get_selection_visible_line(GntTree *tree);

/**
 * Change the text of a column in a row.
 *
 * @param tree   The tree
 * @param key    The key for the row
 * @param colno  The index of the column
 * @param text   The new text
 */
void gnt_tree_change_text(GntTree *tree, gpointer key, int colno, const char *text);

/**
 * Add a checkable item in the tree.
 *
 * @param tree    The tree
 * @param key     The key for the row
 * @param row     The row to add
 * @param parent  The parent of the row, or @c NULL
 * @param bigbro  The row to insert after, or @c NULL
 *
 * @return  The row inserted.
 *
 * @see gnt_tree_create_row
 * @see gnt_tree_create_row_from_list
 * @see gnt_tree_add_row_last
 * @see gnt_tree_add_row_after
 */
GntTreeRow * gnt_tree_add_choice(GntTree *tree, void *key, GntTreeRow *row, void *parent, void *bigbro);

/**
 * Set whether a checkable item is checked or not.
 *
 * @param tree   The tree
 * @param key    The key for the row
 * @param set    @c TRUE if the item should be checked, @c FALSE if not
 */
void gnt_tree_set_choice(GntTree *tree, void *key, gboolean set);

/**
 * Return whether a row is selected or not, where the row is a checkable item.
 *
 * @param tree  The tree
 * @param key   The key for the row
 *
 * @return    @c TRUE if the row is checked, @c FALSE otherwise.
 */
gboolean gnt_tree_get_choice(GntTree *tree, void *key);

/**
 * Set flags for the text in a row in the tree.
 *
 * @param tree   The tree
 * @param key    The key for the row
 * @param flags  The flags to set
 */
void gnt_tree_set_row_flags(GntTree *tree, void *key, GntTextFormatFlags flags);

/**
 * Set color for the text in a row in the tree.
 *
 * @param tree   The tree
 * @param key    The key for the row
 * @param color  The color
 * @since 2.4.0
 */
void gnt_tree_set_row_color(GntTree *tree, void *key, int color);

/**
 * Select a row.
 *
 * @param tree  The tree
 * @param key   The key of the row to select
 */
void gnt_tree_set_selected(GntTree *tree , void *key);

/**
 * Create a row to insert in the tree.
 *
 * @param tree The tree
 * @param ...  A string for each column in the tree
 *
 * @return   The row
 *
 * @see gnt_tree_create_row_from_list
 * @see gnt_tree_add_row_after
 * @see gnt_tree_add_row_last
 * @see gnt_tree_add_choice
 */
GntTreeRow * gnt_tree_create_row(GntTree *tree, ...);

/**
 * Create a row from a list of text.
 *
 * @param tree  The tree
 * @param list  The list containing the text for each column
 *
 * @return   The row
 *
 * @see gnt_tree_create_row
 * @see gnt_tree_add_row_after
 * @see gnt_tree_add_row_last
 * @see gnt_tree_add_choice
 */
GntTreeRow * gnt_tree_create_row_from_list(GntTree *tree, GList *list);

/**
 * Set the width of a column in the tree.
 *
 * @param tree   The tree
 * @param col    The index of the column
 * @param width  The width for the column
 *
 * @see gnt_tree_set_column_width_ratio
 * @see gnt_tree_set_column_resizable
 */
void gnt_tree_set_col_width(GntTree *tree, int col, int width);

/**
 * Set the title for a column.
 *
 * @param tree   The tree
 * @param index  The index of the column
 * @param title  The title for the column
 *
 * @see gnt_tree_set_column_titles
 * @see gnt_tree_set_show_title
 *
 * @since 2.0.0 (gnt), 2.1.0 (pidgin)
 */
void gnt_tree_set_column_title(GntTree *tree, int index, const char *title);

/**
 * Set the titles of the columns
 *
 * @param tree  The tree
 * @param ...   One title for each column in the tree
 *
 * @see gnt_tree_set_column_title
 * @see gnt_tree_set_show_title
 */
void gnt_tree_set_column_titles(GntTree *tree, ...);

/**
 * Set whether to display the title of the columns.
 *
 * @param tree  The tree
 * @param set   If @c TRUE, the column titles are displayed
 *
 * @see gnt_tree_set_column_title
 * @see gnt_tree_set_column_titles
 */
void gnt_tree_set_show_title(GntTree *tree, gboolean set);

/**
 * Set the compare function for sorting the data.
 *
 * @param tree  The tree
 * @param func  The comparison function, which is used to compare
 *              the keys
 *
 * @see gnt_tree_sort_row
 */
void gnt_tree_set_compare_func(GntTree *tree, GCompareFunc func);

/**
 * Set whether a row, which has child rows, should be expanded.
 *
 * @param tree      The tree
 * @param key       The key of the row
 * @param expanded  Whether to expand the child rows
 */
void gnt_tree_set_expanded(GntTree *tree, void *key, gboolean expanded);

/**
 * Set whether to show column separators.
 *
 * @param tree  The tree
 * @param set   If @c TRUE, the column separators are displayed
 */
void gnt_tree_set_show_separator(GntTree *tree, gboolean set);

/**
 * Sort a row in the tree.
 *
 * @param tree  The tree
 * @param row   The row to sort
 *
 * @see gnt_tree_set_compare_func
 */
void gnt_tree_sort_row(GntTree *tree, void *row);

/**
 * Automatically adjust the width of the columns in the tree.
 *
 * @param tree  The tree
 */
void gnt_tree_adjust_columns(GntTree *tree);

/**
 * Set the hash functions to use to hash, compare and free the keys.
 *
 * @param tree  The tree
 * @param hash  The hashing function
 * @param eq    The function to compare keys
 * @param kd    The function to use to free the keys when a row is removed
 *              from the tree
 */
void gnt_tree_set_hash_fns(GntTree *tree, gpointer hash, gpointer eq, gpointer kd);

/**
 * Set whether a column is visible or not.
 * This can be useful when, for example, we want to store some data
 * which we don't want/need to display.
 *
 * @param tree  The tree
 * @param col   The index of the column
 * @param vis   If @c FALSE, the column will not be displayed
 */
void gnt_tree_set_column_visible(GntTree *tree, int col, gboolean vis);

/**
 * Set whether a column can be resized to keep the same ratio when the
 * tree is resized.
 *
 * @param tree  The tree
 * @param col   The index of the column
 * @param res   If @c FALSE, the column will not be resized when the
 *              tree is resized
 *
 * @see gnt_tree_set_col_width
 * @see gnt_tree_set_column_width_ratio
 *
 * @since 2.0.0 (gnt), 2.1.0 (pidgin)
 */
void gnt_tree_set_column_resizable(GntTree *tree, int col, gboolean res);

/**
 * Set whether data in a column should be considered as binary data, and
 * not as strings. A column containing binary data will be display empty text.
 *
 * @param tree  The tree
 * @param col   The index of the column
 * @param bin   @c TRUE if the data for the column is binary
 */
void gnt_tree_set_column_is_binary(GntTree *tree, int col, gboolean bin);

/**
 * Set whether text in a column should be right-aligned.
 *
 * @param tree  The tree
 * @param col   The index of the column
 * @param right @c TRUE if the text in the column should be right aligned
 *
 * @since 2.0.0 (gnt), 2.1.0 (pidgin)
 */
void gnt_tree_set_column_is_right_aligned(GntTree *tree, int col, gboolean right);

/**
 * Set column widths to use when calculating column widths after a tree
 * is resized.
 *
 * @param tree   The tree
 * @param cols   Array of widths. The width must have the same number
 *               of entries as the number of columns in the tree, or
 *               end with a negative value for a column-width.
 *
 * @see gnt_tree_set_col_width
 * @see gnt_tree_set_column_resizable
 *
 * @since 2.0.0 (gnt), 2.1.0 (pidgin)
 */
void gnt_tree_set_column_width_ratio(GntTree *tree, int cols[]);

/**
 * Set the column to use for typeahead searching.
 *
 * @param tree   The tree
 * @param col    The index of the column
 *
 * @since 2.0.0 (gnt), 2.1.0 (pidgin)
 */
void gnt_tree_set_search_column(GntTree *tree, int col);

/**
 * Check whether the user is currently in the middle of a search.
 *
 * @param tree   The tree
 * @return  @c TRUE if the user is searching, @c FALSE otherwise.
 *
 * @since 2.0.0 (gnt), 2.1.0 (pidgin)
 */
gboolean gnt_tree_is_searching(GntTree *tree);

/**
 * Set a custom search function.
 *
 * @param tree  The tree
 * @param func  The custom search function. The search function is
 *              sent the tree itself, the key of a row, the search
 *              string and the content of row in the search column.
 *              If the function returns @c TRUE, the row is dislayed,
 *              otherwise it's not.
 *
 * @since 2.0.0 (gnt), 2.1.0 (pidgin)
 */
void gnt_tree_set_search_function(GntTree *tree,
		gboolean (*func)(GntTree *tree, gpointer key, const char *search, const char *current));

/**
 * Get the parent key for a row.
 *
 * @param  tree  The tree
 * @param  key   The key for the row.
 *
 * @return The key of the parent row.
 * @since 2.4.0
 */
gpointer gnt_tree_get_parent_key(GntTree *tree, gpointer key);

G_END_DECLS

#endif /* GNT_TREE_H */
