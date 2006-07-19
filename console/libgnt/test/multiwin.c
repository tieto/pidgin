#include "gnt.h"
#include "gntbutton.h"
#include "gntentry.h"
#include "gntkeys.h"
#include "gntlabel.h"
#include "gnttree.h"
#include "gntbox.h"

gboolean show(GntWidget *w)
{
	return FALSE;
}

int main()
{
#ifdef STANDALONE
	freopen(".error", "w", stderr);
	gnt_init();
#endif

	GntWidget *hbox, *tree, *box2;

	hbox = gnt_box_new(FALSE, TRUE);
	box2 = gnt_box_new(FALSE, TRUE);

	gnt_widget_set_name(hbox, "hbox");
	gnt_widget_set_name(box2, "box2");

	tree = gnt_tree_new();
	gnt_widget_set_name(tree, "tree");
	gnt_box_add_widget(GNT_BOX(hbox), tree);

	gnt_box_set_toplevel(GNT_BOX(hbox), TRUE);
	gnt_box_set_title(GNT_BOX(hbox), "Testing the tree widget");

	gnt_box_set_toplevel(GNT_BOX(box2), TRUE);
	gnt_box_set_title(GNT_BOX(box2), "On top");

	gnt_box_add_widget(GNT_BOX(box2), gnt_label_new("asdasd"));
	gnt_box_add_widget(GNT_BOX(box2), gnt_entry_new(NULL));

	gnt_widget_show(hbox);
	gnt_widget_set_position(box2, 35, 15);
	gnt_widget_show(box2);

	gnt_tree_add_row_after(GNT_TREE(tree), "a", gnt_tree_create_row(GNT_TREE(tree), "a"), NULL, NULL);
	gnt_tree_add_row_after(GNT_TREE(tree), "c", gnt_tree_create_row(GNT_TREE(tree), "c"), NULL, NULL);
	gnt_tree_add_row_after(GNT_TREE(tree), "d", gnt_tree_create_row(GNT_TREE(tree), "d"), NULL, NULL);
	gnt_tree_add_row_after(GNT_TREE(tree), "e", gnt_tree_create_row(GNT_TREE(tree), "e"), "a", NULL);
	gnt_tree_add_row_after(GNT_TREE(tree), "b", gnt_tree_create_row(GNT_TREE(tree), "b"), "d", NULL);

	gnt_tree_add_choice(GNT_TREE(tree), "1", gnt_tree_create_row(GNT_TREE(tree), "1"), NULL, NULL);
	gnt_tree_add_row_after(GNT_TREE(tree), "2", gnt_tree_create_row(GNT_TREE(tree), "2"), NULL, NULL);
	gnt_tree_add_row_after(GNT_TREE(tree), "3", gnt_tree_create_row(GNT_TREE(tree), "3"), NULL, NULL);
	gnt_tree_add_row_after(GNT_TREE(tree), "4", gnt_tree_create_row(GNT_TREE(tree), "4"), "a", NULL);
	gnt_tree_add_row_after(GNT_TREE(tree), "5", gnt_tree_create_row(GNT_TREE(tree), "5"), "d", NULL);

	gnt_tree_add_row_after(GNT_TREE(tree), "6", gnt_tree_create_row(GNT_TREE(tree), "6"), "4", NULL);

	gnt_tree_set_row_flags(GNT_TREE(tree), "e", GNT_TEXT_FLAG_DIM);

	gnt_tree_set_selected(GNT_TREE(tree), "2");

	g_timeout_add(5000, (GSourceFunc)show, box2);

#ifdef STANDALONE
	gnt_main();

	gnt_quit();
#endif

	return 0;
}

