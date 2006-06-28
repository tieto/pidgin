#include "gntbutton.h"
#include "gnt.h"
#include "gntkeys.h"
#include "gnttree.h"
#include "gntbox.h"

gboolean show(GntWidget *w)
{
	gnt_widget_draw(w);

	return TRUE;
}

int main()
{
	gnt_init();

	GntWidget *hbox, *tree, *box2;

	box(stdscr, 0, 0);
	wrefresh(stdscr);

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

	gnt_box_add_widget(GNT_BOX(box2), GNT_WIDGET(gnt_label_new("asdasd")));

	gnt_widget_show(hbox);
	gnt_widget_set_position(box2, 5, 5);
	gnt_widget_show(box2);

	gnt_tree_add_row_after(GNT_TREE(tree), "a", "a", NULL, NULL);
	gnt_tree_add_row_after(GNT_TREE(tree), "c", "c", NULL, NULL);
	gnt_tree_add_row_after(GNT_TREE(tree), "d", "d", NULL, NULL);
	gnt_tree_add_row_after(GNT_TREE(tree), "e", "e", "a", NULL);
	gnt_tree_add_row_after(GNT_TREE(tree), "b", "b", "d", NULL);

	gnt_tree_add_row_after(GNT_TREE(tree), "1", "1", NULL, NULL);
	gnt_tree_add_row_after(GNT_TREE(tree), "2", "2", NULL, NULL);
	gnt_tree_add_row_after(GNT_TREE(tree), "3", "3", NULL, NULL);
	gnt_tree_add_row_after(GNT_TREE(tree), "4", "4", "a", NULL);
	gnt_tree_add_row_after(GNT_TREE(tree), "5", "5", "d", NULL);

	gnt_tree_add_row_after(GNT_TREE(tree), "6", "6", "4", NULL);
	g_timeout_add(1000, (GSourceFunc)show, hbox);

	gnt_main();

	return 0;
}

