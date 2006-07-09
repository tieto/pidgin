#include <gnt.h>
#include <gntbox.h>
#include <gntcombobox.h>
#include <gntlabel.h>

int main()
{
	GntWidget *box, *combo, *button;

	gnt_init();
	
	box = gnt_box_new(FALSE, FALSE);

	gnt_box_set_toplevel(GNT_BOX(box), TRUE);
	gnt_box_set_title(GNT_BOX(box), "Checkbox");

	combo = gnt_combo_box_new();
	gnt_combo_box_add_data(GNT_COMBO_BOX(combo), "1", "1");
	gnt_combo_box_add_data(GNT_COMBO_BOX(combo), "2", "2");
	gnt_combo_box_add_data(GNT_COMBO_BOX(combo), "3", "3abcdefghijklmnopqrstuvwxyz");
	gnt_combo_box_add_data(GNT_COMBO_BOX(combo), "4", "4");
	gnt_combo_box_add_data(GNT_COMBO_BOX(combo), "5", "5");
	gnt_combo_box_add_data(GNT_COMBO_BOX(combo), "6", "6");

	gnt_box_add_widget(GNT_BOX(box), gnt_label_new("Select"));
	gnt_box_add_widget(GNT_BOX(box), combo);

	button = gnt_button_new("OK");
	gnt_box_add_widget(GNT_BOX(box), button);

	gnt_widget_show(box);

	gnt_main();

	gnt_quit();

	return 0;
}

