#include "gntbutton.h"
#include "gnt.h"
#include "gntkeys.h"
#include "gnttree.h"
#include "gntbox.h"
#include "gntentry.h"
#include "gntlabel.h"

int main()
{
	gnt_init();

	GntWidget *label = gnt_label_new("So wassup dudes and dudettes!!\nSo this is, like,\nthe third line!! \\o/");
	GntWidget *vbox, *hbox, *tree;
	WINDOW *test;

	box(stdscr, 0, 0);
	wrefresh(stdscr);

	vbox = gnt_box_new(FALSE, FALSE);
	hbox = gnt_box_new(FALSE, TRUE);

	gnt_widget_set_name(vbox, "vbox");
	gnt_widget_set_name(hbox, "hbox");

	gnt_box_add_widget(GNT_BOX(hbox), label);
	gnt_box_add_widget(GNT_BOX(hbox), vbox);

	gnt_box_add_widget(GNT_BOX(hbox), gnt_entry_new("a"));

	tree = gnt_tree_new();
	gnt_box_add_widget(GNT_BOX(hbox), tree);

	gnt_tree_add_row_after(GNT_TREE(tree), "a", "a", NULL, NULL);
	gnt_tree_add_row_after(GNT_TREE(tree), "c", "c", NULL, NULL);
	gnt_tree_add_row_after(GNT_TREE(tree), "d", "d", NULL, NULL);
	gnt_tree_add_row_after(GNT_TREE(tree), "e", "e", "a", NULL);
	gnt_tree_add_row_after(GNT_TREE(tree), "b", "b", "d", NULL);

	GNT_WIDGET_UNSET_FLAGS(hbox, GNT_WIDGET_NO_BORDER | GNT_WIDGET_NO_SHADOW);
	gnt_box_set_title(GNT_BOX(hbox), "This is the title …");

	gnt_widget_show(hbox);

	gnt_main();

	return 0;
}

