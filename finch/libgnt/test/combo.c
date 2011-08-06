#include <gnt.h>
#include <gntbox.h>
#include <gntbutton.h>
#include <gntcheckbox.h>
#include <gntcombobox.h>
#include <gntentry.h>
#include <gntlabel.h>
#include <gntslider.h>

static void
button_activated(GntWidget *b, GntComboBox *combo)
{
	GntWidget *w = b->parent;

	gnt_box_add_widget(GNT_BOX(w),
			gnt_label_new(gnt_combo_box_get_selected_data(GNT_COMBO_BOX(combo))));
	fprintf(stderr, "%s\n", gnt_combo_box_get_selected_data(GNT_COMBO_BOX(combo)));
	gnt_box_readjust(GNT_BOX(w->parent));
}

static void
update_label(GntSlider *slider, int current_value, GntEntry *entry)
{
	char value[256];
	g_snprintf(value, sizeof(value), "%d/%d", current_value, slider->max);
	/*gnt_label_set_text(label, value);*/
	/*gnt_widget_draw(GNT_WIDGET(label));*/
	gnt_entry_set_text(entry, value);
}

int main()
{
	GntWidget *box, *combo, *button;
	GntWidget *hbox;

#ifdef STANDALONE
	freopen(".error", "w", stderr);
	gnt_init();
#endif

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
	gnt_combo_box_add_data(GNT_COMBO_BOX(combo), "7", "7");
	gnt_combo_box_add_data(GNT_COMBO_BOX(combo), "8", "8");
	gnt_combo_box_add_data(GNT_COMBO_BOX(combo), "9", "9");

	GntWidget *l = gnt_label_new("Select");
	gnt_box_add_widget(GNT_BOX(hbox), l);
	gnt_widget_set_size(l, 0, 1);
	gnt_box_add_widget(GNT_BOX(hbox), combo);

	gnt_box_add_widget(GNT_BOX(box), hbox);

	hbox = gnt_box_new(TRUE, FALSE);
	gnt_box_set_alignment(GNT_BOX(hbox), GNT_ALIGN_MID);
	gnt_widget_set_name(hbox, "lower");

	button = gnt_button_new("OK");
	gnt_box_add_widget(GNT_BOX(hbox), button);
	g_signal_connect(G_OBJECT(button), "activate", G_CALLBACK(button_activated), combo);

	gnt_box_add_widget(GNT_BOX(box), hbox);

	gnt_box_add_widget(GNT_BOX(box), gnt_check_box_new("check box"));

	GntWidget *e = gnt_entry_new(NULL);
	gnt_box_add_widget(GNT_BOX(box), e);

	GntWidget *slider = gnt_slider_new(TRUE, 0, 100);
	gnt_slider_set_value(GNT_SLIDER(slider), 50);
	gnt_box_add_widget(GNT_BOX(box), slider);
	g_signal_connect(G_OBJECT(slider), "changed", G_CALLBACK(update_label), e);
	gnt_slider_reflect_label(GNT_SLIDER(slider), GNT_LABEL(l));

	gnt_widget_show(box);

	gnt_widget_show(gnt_file_sel_new());

#ifdef STANDALONE
	gnt_main();

	gnt_quit();
#endif

	return 0;
}

