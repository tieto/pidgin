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

typedef struct _GnTree			GntTree;
typedef struct _GnTreePriv		GntTreePriv;
typedef struct _GnTreeClass		GntTreeClass;

typedef struct _GnTreeRow		GntTreeRow;

struct _GnTree
{
	GntWidget parent;

	GntTreeRow *current;	/* current selection */

	GntTreeRow *top;		/* The topmost visible item */
	GntTreeRow *bottom;		/* The bottommost visible item */
	
	GntTreeRow *root; /* The root of all evil */
	
	GList *list;	/* List of GntTreeRow s */
	GHashTable *hash;	/* XXX: We may need this for quickly referencing the rows */
};

struct _GnTreeClass
{
	GntWidgetClass parent;

	void (*selection_changed)(int old, int current);

	void (*gnt_reserved1)(void);
	void (*gnt_reserved2)(void);
	void (*gnt_reserved3)(void);
	void (*gnt_reserved4)(void);
};

G_BEGIN_DECLS

GType gnt_tree_get_gtype(void);

GntWidget *gnt_tree_new();

void gnt_tree_set_visible_rows(GntTree *tree, int rows);

int gnt_tree_get_visible_rows(GntTree *tree);

void gnt_tree_scroll(GntTree *tree, int count);

GntTreeRow *gnt_tree_add_row_after(GntTree *tree, void *key, const char *text, void *parent, void *bigbro);

gpointer gnt_tree_get_selection_data(GntTree *tree);

void gnt_tree_remove(GntTree *tree, gpointer key);

/* Returns the visible line number of the selected row */
int gnt_tree_get_selection_visible_line(GntTree *tree);

void gnt_tree_change_text(GntTree *tree, gpointer key, const char *text);

GntTreeRow *gnt_tree_add_choice(GntTree *tree, void *key, const char *text, void *parent, void *bigbro);

void gnt_tree_set_choice(GntTree *tree, void *key, gboolean set);

gboolean gnt_tree_get_choice(GntTree *tree, void *key);

void gnt_tree_set_row_flags(GntTree *tree, void *key, GntTextFormatFlags flags);

G_END_DECLS

#endif /* GNT_TREE_H */
