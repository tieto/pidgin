#ifndef GNT_TREE_H
#define GNT_TREE_H

#include "gntwidget.h"
#include "gnt.h"
#include "gntcolors.h"
#include "gntkeys.h"

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

	int current;	/* current selection */

	int top;		/* The index in 'list' of the topmost visible item */
	int bottom;		/* The index in 'list' of the bottommost visible item */
	
	GntTreeRow *root; /* The root of all evil */
	
	GList *list;	/* List of GntTreeRow s */
	GHashTable *hash;	/* XXX: We may need this for quickly referencing the rows */
};

struct _GnTreeClass
{
	GntWidgetClass parent;

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

void gnt_tree_add_row_after(GntTree *tree, void *key, const char *text, void *parent, void *bigbro);

gpointer gnt_tree_get_selection_data(GntTree *tree);

int gnt_tree_get_selection_index(GntTree *tree);

void gnt_tree_remove(GntTree *tree, gpointer key);

G_END_DECLS

#endif /* GNT_TREE_H */
