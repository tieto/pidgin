#include <gnt.h>
#include <gntbox.h>
#include <gntbutton.h>
#include <gntcombobox.h>
#include <gntlabel.h>

int main()
{
	GntWidget *box, *combo, *button;
	GntWidget *hbox;

	gnt_init();
	
	box = gnt_box_new(FALSE, TRUE);
	gnt_widget_set_name(box, "box");
	gnt_box_set_alignment(GNT_BOX(box), GNT_ALIGN_MID);
	gnt_box_set_pad(GNT_BOX(box), 0);

	gnt_box_set_toplevel(GNT_BOX(box), TRUE);
	gnt_box_set_title(GNT_BOX(box), "Checkbox");

	hbox = gnt_box_new(FALSE, FALSE);
	gnt_box_set_pad(GNT_BOX(hbox), 0);
	gnt_box_set_alignment(GNT_BOX(hbox), GNT_ALIGN_MID);
	gnt_widget_set_name(hbox, "upper");

	combo = gnt_combo_box_new();
	gnt_combo_box_add_data(GNT_COMBO_BOX(combo), "1", "1");
	gnt_combo_box_add_data(GNT_COMBO_BOX(combo), "2", "2");
	gnt_combo_box_add_data(GNT_COMBO_BOX(combo), "3", "3abcdefghijklmnopqrstuvwxyz");
	gnt_combo_box_add_data(GNT_COMBO_BOX(combo), "4", "4");
	gnt_combo_box_add_data(GNT_COMBO_BOX(combo), "5", "5");
	gnt_combo_box_add_data(GNT_COMBO_BOX(combo), "6", "6");

	gnt_box_add_widget(GNT_BOX(hbox), gnt_label_new("Select"));
	gnt_box_add_widget(GNT_BOX(hbox), combo);

	gnt_box_add_widget(GNT_BOX(box), hbox);

	hbox = gnt_box_new(TRUE, FALSE);
	gnt_box_set_alignment(GNT_BOX(hbox), GNT_ALIGN_MID);
	gnt_widget_set_name(hbox, "lower");

	button = gnt_button_new("OK");
	gnt_box_add_widget(GNT_BOX(hbox), button);

	gnt_box_add_widget(GNT_BOX(box), hbox);

	gnt_widget_show(box);

	gnt_main();

	gnt_quit();

	return 0;
}

