#include "gntbutton.h"
#include "gnt.h"
#include "gntkeys.h"
#include "gnttree.h"
#include "gntbox.h"
#include "gntentry.h"
#include "gntlabel.h"

static void
toggled(GntWidget *tree, gpointer key, gpointer null)
{
	GntWidget *w = gnt_box_new(FALSE, FALSE);

	gnt_box_set_toplevel(GNT_BOX(w), TRUE);

	gnt_box_add_widget(GNT_BOX(w),
			gnt_label_new(gnt_tree_get_choice(GNT_TREE(tree), key) ? "Selected" : "NOT"));
	gnt_widget_show(w);
}

int main()
{
#ifdef STANDALONE
	freopen(".error", "w", stderr);
	gnt_init();
#endif
	
	GntWidget *label = gnt_label_new("So wassup dudes and dudettes!!\u4e0a1\u6d772\u67003\u4f4e4\u67085\nSo this is, like,\nthe third line!! \\o/");
	GntWidget *vbox, *hbox, *tree, *box, *button;
	WINDOW *test;

	vbox = gnt_box_new(FALSE, FALSE);
	hbox = gnt_box_new(FALSE, TRUE);
	gnt_box_set_alignment(GNT_BOX(hbox), GNT_ALIGN_MID);

	gnt_widget_set_name(vbox, "vbox");
	gnt_widget_set_name(hbox, "hbox");

	gnt_box_add_widget(GNT_BOX(hbox), label);

	GntWidget *entry = gnt_entry_new("a");
	gnt_widget_set_name(entry, "entry");
	gnt_box_add_widget(GNT_BOX(hbox), entry);

	box = gnt_box_new(FALSE, FALSE);
	tree = gnt_tree_new();
	gnt_tree_set_compare_func(GNT_TREE(tree), g_utf8_collate);
	gnt_widget_set_name(tree, "tree");
	gnt_box_add_widget(GNT_BOX(box), tree);
	gnt_box_add_widget(GNT_BOX(hbox), box);

	gnt_tree_add_row_after(GNT_TREE(tree), "c", gnt_tree_create_row(GNT_TREE(tree), "c"), NULL, NULL);
	gnt_tree_add_row_after(GNT_TREE(tree), "a", gnt_tree_create_row(GNT_TREE(tree), "a"), NULL, NULL);
	gnt_tree_add_row_after(GNT_TREE(tree), "z", gnt_tree_create_row(GNT_TREE(tree), "z"), "a", NULL);
	gnt_tree_add_row_after(GNT_TREE(tree), "y", gnt_tree_create_row(GNT_TREE(tree), "y"), "a", NULL);
	gnt_tree_add_row_after(GNT_TREE(tree), "g", gnt_tree_create_row(GNT_TREE(tree), "g"), "a", NULL);
	gnt_tree_add_row_after(GNT_TREE(tree), "d", gnt_tree_create_row(GNT_TREE(tree), "d"), NULL, NULL);
	gnt_tree_add_row_after(GNT_TREE(tree), "x", gnt_tree_create_row(GNT_TREE(tree), "x"), "a", NULL);
	gnt_tree_add_row_after(GNT_TREE(tree), "k", gnt_tree_create_row(GNT_TREE(tree), "k"), "a", NULL);
	gnt_tree_add_row_after(GNT_TREE(tree), "e", gnt_tree_create_row(GNT_TREE(tree), "e"), "a", NULL);
	gnt_tree_add_choice(GNT_TREE(tree), "b", gnt_tree_create_row(GNT_TREE(tree), "b"), "d", NULL);

	GNT_WIDGET_UNSET_FLAGS(hbox, GNT_WIDGET_NO_BORDER | GNT_WIDGET_NO_SHADOW);
	gnt_box_set_title(GNT_BOX(hbox), "\u4e0a\u6d77\u6700\u4f4e\u6708\u5de5 \u4e0a\u6d77\u6700\u4f4e\u6708\u5de5 ……\u4e0a\u6d77\u6700\u4f4e\u6708\u5de5 …");

	g_signal_connect(G_OBJECT(tree), "toggled", G_CALLBACK(toggled), NULL);

	button = gnt_button_new("one");
	gnt_widget_set_name(button, "one");
	gnt_box_add_widget(GNT_BOX(vbox), button);

	button = gnt_button_new("two");
	gnt_widget_set_name(button, "two");
	gnt_box_add_widget(GNT_BOX(vbox), button);

	button = gnt_button_new("three");
	gnt_widget_set_name(button, "three");
	gnt_box_add_widget(GNT_BOX(vbox), button);

	gnt_box_add_widget(GNT_BOX(hbox), vbox);
	
	gnt_widget_show(hbox);

#ifdef STANDALONE
	gnt_main();

	gnt_quit();
#endif

	return 0;
}

