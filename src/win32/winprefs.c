/*
 *  winprefs.c
 *  
 *  Author: Herman Bloggs <hermanator12002@yahoo.com>
 *  Date: November, 2002
 *  Description: Windows only preferences page
 */
#include <gtk/gtk.h>
#include "gaim.h"
#include "win32dep.h"

/*
 *  PROTOS
 */
extern GtkWidget *gaim_button(const char*, guint*, int, GtkWidget*);

static void im_alpha_change(GtkWidget *w, gpointer data) {
	int val = gtk_range_get_value(GTK_RANGE(w));
	wgaim_set_imalpha(val);
}

GtkWidget *wgaim_winprefs_page() {
	GtkWidget *ret;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label, *slider;
	GtkWidget *button;
	GtkWidget *trans_box;

	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 12);

	/* transparency options */
	vbox = make_frame (ret, _("Transparency"));
	button = gaim_button(_("_IM window transparency"), &wgaim_options, OPT_WGAIM_IMTRANS, vbox);
	trans_box =  gtk_vbox_new(FALSE, 18);
	if (!(wgaim_options & OPT_WGAIM_IMTRANS))
		gtk_widget_set_sensitive(GTK_WIDGET(trans_box), FALSE);
	gtk_widget_show(trans_box);
	gaim_button(_("_Show slider bar in IM window"), &wgaim_options, OPT_WGAIM_SHOW_IMTRANS, trans_box);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive),  trans_box);
	gtk_box_pack_start(GTK_BOX(vbox), trans_box, FALSE, FALSE, 5);

	/* transparency slider */
	hbox = gtk_hbox_new(FALSE, 5);

	label = gtk_label_new(_("Default Opacity:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);

	slider = gtk_hscale_new_with_range(50,255,1);
	gtk_range_set_value(GTK_RANGE(slider), wgaim_get_imalpha());
	gtk_widget_set_usize(GTK_WIDGET(slider), 200, -1);
	
	gtk_signal_connect(GTK_OBJECT(slider), "value-changed", GTK_SIGNAL_FUNC(im_alpha_change), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), slider, FALSE, TRUE, 5);

	gtk_widget_show_all(hbox);

	gtk_box_pack_start(GTK_BOX(trans_box), hbox, FALSE, FALSE, 5);
	
	/* If this version of Windows dosn't support Transparency, grey out options */
	if(!wgaim_has_transparency()) {
		gtk_widget_set_sensitive(GTK_WIDGET(vbox), FALSE);
	}

	gtk_widget_show_all(ret);
	return ret;
}
