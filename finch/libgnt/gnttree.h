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
/**
 * SECTION:gnttree
 * @section_id: libgnt-gnttree
 * @short_description: <filename>gnttree.h</filename>
 * @title: Tree
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

	/*< private >*/
	void (*gnt_reserved1)(void);
	void (*gnt_reserved2)(void);
	void (*gnt_reserved3)(void);
	void (*gnt_reserved4)(void);
};

G_BEGIN_DECLS

/**
 * gnt_tree_get_gtype:
 *
 * Returns: The GType for GntTree
 */
GType gnt_tree_get_gtype(void);

/**
 * gnt_tree_new:
 *
 * Create a tree with one column.
 *
 * Returns: The newly created tree
 *
 * See gnt_tree_new_with_columns().
 */
GntWidget * gnt_tree_new(void);

/**
 * gnt_tree_new_with_columns:
 * @columns:  Number of columns
 *
 * Create a tree with a specified number of columns.
 *
 * See gnt_tree_new().
 *
 * Returns:  The newly created tree
 */
GntWidget * gnt_tree_new_with_columns(int columns);

/**
 * gnt_tree_set_visible_rows:
 * @tree:  The tree
 * @rows:  The number of rows
 *
 * The number of rows the tree should display at a time.
 */
void gnt_tree_set_visible_rows(GntTree *tree, int rows);

/**
 * gnt_tree_get_visible_rows:
 * @tree:  The tree
 *
 * Get the number visible rows.
 *
 * Returns:  The number of visible rows
 */
int gnt_tree_get_visible_rows(GntTree *tree);

/**
 * gnt_tree_scroll:
 * @tree:   The tree
 * @count:  If positive, the tree will be scrolled down by count rows,
 *               otherwise, it will be scrolled up by count rows.
 *
 * Scroll the contents of the tree.
 */
void gnt_tree_scroll(GntTree *tree, int count);

/**
 * gnt_tree_add_row_after:
 * @tree:    The tree
 * @key:     The key for the row
 * @row:     The row to insert
 * @parent:  The key for the parent row
 * @bigbro:  The key for the row to insert the new row after.
 *
 * Insert a row in the tree.
 *
 * See gnt_tree_create_row(), gnt_tree_add_row_last(), gnt_tree_add_choice().
 *
 * Returns:  The inserted row
 */
GntTreeRow * gnt_tree_add_row_after(GntTree *tree, void *key, GntTreeRow *row, void *parent, void *bigbro);

/**
 * gnt_tree_add_row_last:
 * @tree:    The tree
 * @key:     The key for the row
 * @row:     The row to insert
 * @parent:  The key for the parent row
 *
 * Insert a row at the end of the tree.
 *
 * See gnt_tree_create_row(), gnt_tree_add_row_after(), gnt_tree_add_choice().
 *
 * Returns: The inserted row
 */
GntTreeRow * gnt_tree_add_row_last(GntTree *tree, void *key, GntTreeRow *row, void *parent);

/**
 * gnt_tree_get_selection_data:
 * @tree:  The tree
 *
 * Get the key for the selected row.
 *
 * Returns:   The key for the selected row
 */
gpointer gnt_tree_get_selection_data(GntTree *tree);

/**
 * gnt_tree_get_selection_text:
 * @tree:  The tree
 *
 * Get the text displayed for the selected row.
 *
 * See gnt_tree_get_row_text_list(), gnt_tree_get_selection_text_list().
 *
 * Returns:  The text, which needs to be freed by the caller
 */
char * gnt_tree_get_selection_text(GntTree *tree);

/**
 * gnt_tree_get_row_text_list:
 * @tree:  The tree
 * @key:   A key corresponding to the row in question. If key
 *              is %NULL, the text list for the selected row will
 *              be returned.
 *
 * Get a list of text for a row.
 *
 * See gnt_tree_get_selection_text_list(), gnt_tree_get_selection_text().
 *
 * Returns: A list of texts of a row. The list and its data should be
 *         freed by the caller. The caller should make sure that if
 *         any column of the tree contains binary data, it's not freed.
 */
GList * gnt_tree_get_row_text_list(GntTree *tree, gpointer key);

/**
 * gnt_tree_row_get_key:
 * @tree:   The tree
 * @row:    The GntTreeRow object
 *
 * Get the key of a row.
 *
 * Returns: The key of the row.
 *
 * Since: 2.8.0 (gnt), 2.7.2 (pidgin)
 */
gpointer gnt_tree_row_get_key(GntTree *tree, GntTreeRow *row);

/**
 * gnt_tree_row_get_next:
 * @tree: The tree
 * @row:  The GntTreeRow object
 *
 * Get the next row.
 *
 * Returns: The next row.
 *
 * Since: 2.8.0 (gnt), 2.7.2 (pidgin)
 */
GntTreeRow * gnt_tree_row_get_next(GntTree *tree, GntTreeRow *row);

/**
 * gnt_tree_row_get_prev:
 * @tree: The tree
 * @row:  The GntTreeRow object
 *
 * Get the previous row.
 *
 * Returns: The previous row.
 *
 * Since: 2.8.0 (gnt), 2.7.2 (pidgin)
 */
GntTreeRow * gnt_tree_row_get_prev(GntTree *tree, GntTreeRow *row);

/**
 * gnt_tree_row_get_child:
 * @tree: The tree
 * @row:  The GntTreeRow object
 *
 * Get the child row.
 *
 * Returns: The child row.
 *
 * Since: 2.8.0 (gnt), 2.7.2 (pidgin)
 */
GntTreeRow * gnt_tree_row_get_child(GntTree *tree, GntTreeRow *row);

/**
 * gnt_tree_row_get_parent:
 * @tree: The tree
 * @row:  The GntTreeRow object
 *
 * Get the parent row.
 *
 * Returns: The parent row.
 *
 * Since: 2.8.0 (gnt), 2.7.2 (pidgin)
 */
GntTreeRow * gnt_tree_row_get_parent(GntTree *tree, GntTreeRow *row);

/**
 * gnt_tree_get_selection_text_list:
 * @tree:  The tree
 *
 * Get a list of text of the current row.
 *
 * See gnt_tree_get_row_text_list(), gnt_tree_get_selection_text().
 *
 * Returns: A list of texts of the currently selected row. The list
 *         and its data should be freed by the caller. The caller
 *         should make sure that if any column of the tree contains
 *         binary data, it's not freed.
 */
GList * gnt_tree_get_selection_text_list(GntTree *tree);

/**
 * gnt_tree_get_rows:
 * @tree:  The tree
 *
 * Returns the list of rows in the tree.
 *
 * Returns: The list of the rows. The list should not be modified by the caller.
 */
GList *gnt_tree_get_rows(GntTree *tree);

/**
 * gnt_tree_remove:
 * @tree:  The tree
 * @key:   The key for the row to remove
 *
 * Remove a row from the tree.
 */
void gnt_tree_remove(GntTree *tree, gpointer key);

/**
 * gnt_tree_remove_all:
 * @tree:  The tree
 *
 * Remove all the item from the tree.
 */
void gnt_tree_remove_all(GntTree *tree);

/**
 * gnt_tree_get_selection_visible_line:
 * @tree:  The tree
 *
 * Get the visible line number of the selected row.
 *
 * Returns:  The line number of the currently selected row
 */
int gnt_tree_get_selection_visible_line(GntTree *tree);

/**
 * gnt_tree_change_text:
 * @tree:   The tree
 * @key:    The key for the row
 * @colno:  The index of the column
 * @text:   The new text
 *
 * Change the text of a column in a row.
 */
void gnt_tree_change_text(GntTree *tree, gpointer key, int colno, const char *text);

/**
 * gnt_tree_add_choice:
 * @tree:    The tree
 * @key:     The key for the row
 * @row:     The row to add
 * @parent:  The parent of the row, or %NULL
 * @bigbro:  The row to insert after, or %NULL
 *
 * Add a checkable item in the tree.
 *
 * See gnt_tree_create_row(), gnt_tree_create_row_from_list(),
 *     gnt_tree_add_row_last(), gnt_tree_add_row_after().
 *
 * Returns:  The row inserted.
 */
GntTreeRow * gnt_tree_add_choice(GntTree *tree, void *key, GntTreeRow *row, void *parent, void *bigbro);

/**
 * gnt_tree_set_choice:
 * @tree:   The tree
 * @key:    The key for the row
 * @set:    %TRUE if the item should be checked, %FALSE if not
 *
 * Set whether a checkable item is checked or not.
 */
void gnt_tree_set_choice(GntTree *tree, void *key, gboolean set);

/**
 * gnt_tree_get_choice:
 * @tree:  The tree
 * @key:   The key for the row
 *
 * Return whether a row is selected or not, where the row is a checkable item.
 *
 * Returns:    %TRUE if the row is checked, %FALSE otherwise.
 */
gboolean gnt_tree_get_choice(GntTree *tree, void *key);

/**
 * gnt_tree_set_row_flags:
 * @tree:   The tree
 * @key:    The key for the row
 * @flags:  The flags to set
 *
 * Set flags for the text in a row in the tree.
 */
void gnt_tree_set_row_flags(GntTree *tree, void *key, GntTextFormatFlags flags);

/**
 * gnt_tree_set_row_color:
 * @tree:   The tree
 * @key:    The key for the row
 * @color:  The color
 *
 * Set color for the text in a row in the tree.
 *
 * Since: 2.4.0
 */
void gnt_tree_set_row_color(GntTree *tree, void *key, int color);

/**
 * gnt_tree_set_selected:
 * @tree:  The tree
 * @key:   The key of the row to select
 *
 * Select a row.
 */
void gnt_tree_set_selected(GntTree *tree , void *key);

/**
 * gnt_tree_create_row:
 * @tree: The tree
 * @...:  A string for each column in the tree
 *
 * Create a row to insert in the tree.
 *
 * See gnt_tree_create_row_from_list(), gnt_tree_add_row_after(),
 *     gnt_tree_add_row_last(), gnt_tree_add_choice().
 *
 * Returns:   The row
 */
GntTreeRow * gnt_tree_create_row(GntTree *tree, ...);

/**
 * gnt_tree_create_row_from_list:
 * @tree:  The tree
 * @list:  The list containing the text for each column
 *
 * Create a row from a list of text.
 *
 * See gnt_tree_create_row(), gnt_tree_add_row_after(), gnt_tree_add_row_last(),
 *     gnt_tree_add_choice().
 *
 * Returns:   The row
 */
GntTreeRow * gnt_tree_create_row_from_list(GntTree *tree, GList *list);

/**
 * gnt_tree_set_col_width:
 * @tree:   The tree
 * @col:    The index of the column
 * @width:  The width for the column
 *
 * Set the width of a column in the tree.
 *
 * See gnt_tree_set_column_width_ratio(), gnt_tree_set_column_resizable()
 */
void gnt_tree_set_col_width(GntTree *tree, int col, int width);

/**
 * gnt_tree_set_column_title:
 * @tree:   The tree
 * @index:  The index of the column
 * @title:  The title for the column
 *
 * Set the title for a column.
 *
 * See gnt_tree_set_column_titles(), gnt_tree_set_show_title().
 *
 * Since: 2.0.0 (gnt), 2.1.0 (pidgin)
 */
void gnt_tree_set_column_title(GntTree *tree, int index, const char *title);

/**
 * gnt_tree_set_column_titles:
 * @tree:  The tree
 * @...:   One title for each column in the tree
 *
 * Set the titles of the columns
 *
 * See gnt_tree_set_column_title(), gnt_tree_set_show_title().
 */
void gnt_tree_set_column_titles(GntTree *tree, ...);

/**
 * gnt_tree_set_show_title:
 * @tree:  The tree
 * @set:   If %TRUE, the column titles are displayed
 *
 * Set whether to display the title of the columns.
 *
 * See gnt_tree_set_column_title(), gnt_tree_set_column_titles().
 */
void gnt_tree_set_show_title(GntTree *tree, gboolean set);

/**
 * gnt_tree_set_compare_func:
 * @tree:  The tree
 * @func:  The comparison function, which is used to compare
 *              the keys
 *
 * Set the compare function for sorting the data.
 *
 * See gnt_tree_sort_row().
 */
void gnt_tree_set_compare_func(GntTree *tree, GCompareFunc func);

/**
 * gnt_tree_set_expanded:
 * @tree:      The tree
 * @key:       The key of the row
 * @expanded:  Whether to expand the child rows
 *
 * Set whether a row, which has child rows, should be expanded.
 */
void gnt_tree_set_expanded(GntTree *tree, void *key, gboolean expanded);

/**
 * gnt_tree_set_show_separator:
 * @tree:  The tree
 * @set:   If %TRUE, the column separators are displayed
 *
 * Set whether to show column separators.
 */
void gnt_tree_set_show_separator(GntTree *tree, gboolean set);

/**
 * gnt_tree_sort_row:
 * @tree:  The tree
 * @row:   The row to sort
 *
 * Sort a row in the tree.
 *
 * See gnt_tree_set_compare_func().
 */
void gnt_tree_sort_row(GntTree *tree, void *row);

/**
 * gnt_tree_adjust_columns:
 * @tree:  The tree
 *
 * Automatically adjust the width of the columns in the tree.
 */
void gnt_tree_adjust_columns(GntTree *tree);

/**
 * gnt_tree_set_hash_fns:
 * @tree:  The tree
 * @hash:  The hashing function
 * @eq:    The function to compare keys
 * @kd:    The function to use to free the keys when a row is removed
 *              from the tree
 *
 * Set the hash functions to use to hash, compare and free the keys.
 */
void gnt_tree_set_hash_fns(GntTree *tree, gpointer hash, gpointer eq, gpointer kd);

/**
 * gnt_tree_set_column_visible:
 * @tree:  The tree
 * @col:   The index of the column
 * @vis:   If %FALSE, the column will not be displayed
 *
 * Set whether a column is visible or not.
 * This can be useful when, for example, we want to store some data
 * which we don't want/need to display.
 */
void gnt_tree_set_column_visible(GntTree *tree, int col, gboolean vis);

/**
 * gnt_tree_set_column_resizable:
 * @tree:  The tree
 * @col:   The index of the column
 * @res:   If %FALSE, the column will not be resized when the
 *              tree is resized
 *
 * Set whether a column can be resized to keep the same ratio when the
 * tree is resized.
 *
 * See gnt_tree_set_col_width(), gnt_tree_set_column_width_ratio().
 *
 * Since: 2.0.0 (gnt), 2.1.0 (pidgin)
 */
void gnt_tree_set_column_resizable(GntTree *tree, int col, gboolean res);

/**
 * gnt_tree_set_column_is_binary:
 * @tree:  The tree
 * @col:   The index of the column
 * @bin:   %TRUE if the data for the column is binary
 *
 * Set whether data in a column should be considered as binary data, and
 * not as strings. A column containing binary data will be display empty text.
 */
void gnt_tree_set_column_is_binary(GntTree *tree, int col, gboolean bin);

/**
 * gnt_tree_set_column_is_right_aligned:
 * @tree:  The tree
 * @col:   The index of the column
 * @right: %TRUE if the text in the column should be right aligned
 *
 * Set whether text in a column should be right-aligned.
 *
 * Since: 2.0.0 (gnt), 2.1.0 (pidgin)
 */
void gnt_tree_set_column_is_right_aligned(GntTree *tree, int col, gboolean right);

/**
 * gnt_tree_set_column_width_ratio:
 * @tree:   The tree
 * @cols:   Array of widths. The width must have the same number
 *               of entries as the number of columns in the tree, or
 *               end with a negative value for a column-width.
 *
 * Set column widths to use when calculating column widths after a tree
 * is resized.
 *
 * See gnt_tree_set_col_width(), gnt_tree_set_column_resizable().
 *
 * Since: 2.0.0 (gnt), 2.1.0 (pidgin)
 */
void gnt_tree_set_column_width_ratio(GntTree *tree, int cols[]);

/**
 * gnt_tree_set_search_column:
 * @tree:   The tree
 * @col:    The index of the column
 *
 * Set the column to use for typeahead searching.
 *
 * Since: 2.0.0 (gnt), 2.1.0 (pidgin)
 */
void gnt_tree_set_search_column(GntTree *tree, int col);

/**
 * gnt_tree_is_searching:
 * @tree:   The tree
 *
 * Check whether the user is currently in the middle of a search.
 *
 * Returns:  %TRUE if the user is searching, %FALSE otherwise.
 *
 * Since: 2.0.0 (gnt), 2.1.0 (pidgin)
 */
gboolean gnt_tree_is_searching(GntTree *tree);

/**
 * gnt_tree_set_search_function:
 * @tree:  The tree
 * @func:  The custom search function. The search function is
 *              sent the tree itself, the key of a row, the search
 *              string and the content of row in the search column.
 *              If the function returns %TRUE, the row is dislayed,
 *              otherwise it's not.
 *
 * Set a custom search function.
 *
 * Since: 2.0.0 (gnt), 2.1.0 (pidgin)
 */
void gnt_tree_set_search_function(GntTree *tree,
		gboolean (*func)(GntTree *tree, gpointer key, const char *search, const char *current));

/**
 * gnt_tree_get_parent_key:
 * @tree:  The tree
 * @key:   The key for the row.
 *
 * Get the parent key for a row.
 *
 * Returns: The key of the parent row.
 *
 * Since: 2.4.0
 */
gpointer gnt_tree_get_parent_key(GntTree *tree, gpointer key);

G_END_DECLS

#endif /* GNT_TREE_H */
