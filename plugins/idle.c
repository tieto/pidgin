/* a nifty little plugin to set your idle time to whatever you want it to be.
 * useful for almost nothing. mostly just a demo plugin. but it's fun to have
 * 40-day idle times.
 */

#include "config.h"

#include "gaim.h"
#include "multi.h"
#include "gtkplugin.h"
#include <sys/time.h>

#define IDLE_PLUGIN_ID "gtk-idle"

static struct gaim_connection *gc = NULL;

static void set_idle(GtkWidget *button, GtkWidget *spinner) {
	time_t t;
	int tm = CLAMP(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinner)), 0, G_MAXINT);
	if (!gc) {
		return;
	}
	gaim_debug(GAIM_DEBUG_INFO, "idle",
			   "setting idle time for %s to %d\n", gc->username, tm);
	time(&t);
	t -= 60 * tm;
	gc->lastsent = t;
	serv_set_idle(gc, 60 * tm);
	gc->is_idle = 0;
}

static void sel_gc(GtkWidget *opt, struct gaim_connection *g) {
	gc = g;
}

static void make_connect_menu(GtkWidget *box) {
	GtkWidget *optmenu, *menu, *opt;
	GSList *c = connections;
	struct gaim_connection *g;

	optmenu = gtk_option_menu_new();
	gtk_box_pack_start(GTK_BOX(box), optmenu, FALSE, FALSE, 5);

	menu = gtk_menu_new();

	while (c) {
		g = (struct gaim_connection *)c->data;
		opt = gtk_menu_item_new_with_label(g->username);
		g_signal_connect(G_OBJECT(opt), "activate",
				   G_CALLBACK(sel_gc), g);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), opt);
		gtk_widget_show(opt);
		c = g_slist_next(c);
	}

	gtk_option_menu_remove_menu(GTK_OPTION_MENU(optmenu));
	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu), menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(optmenu), 0);

	if (connections)
		gc = connections->data;
	else
		gc = NULL;
}

static GtkWidget *
get_config_frame(GaimPlugin *plugin)
{
	GtkWidget *ret;
	GtkWidget *frame, *label;
	GtkWidget *vbox, *hbox;
	GtkAdjustment *adj;
	GtkWidget *spinner, *button;

	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width(GTK_CONTAINER(ret), 12);

	frame = make_frame(ret, _("Idle Time"));
	
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

	label = gtk_label_new(_("Set"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);

	make_connect_menu(hbox);

	label = gtk_label_new(_("idle for"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);

	adj = (GtkAdjustment *)gtk_adjustment_new(10, 0, G_MAXINT, 1, 0, 0);
	spinner = gtk_spin_button_new(adj, 0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), spinner, TRUE, TRUE, 0);

	label = gtk_label_new(_("minutes."));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);

	hbox = gtk_hbox_new(TRUE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

	button = gtk_button_new_with_mnemonic(_("_Set"));
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 5);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(set_idle), spinner);

	gtk_widget_show_all(ret);

	return ret;
}

static GaimGtkPluginUiInfo ui_info =
{
	get_config_frame
};

static GaimPluginInfo info =
{
	2,
	GAIM_PLUGIN_STANDARD,
	GAIM_GTK_PLUGIN_TYPE,
	0,
	NULL,
	GAIM_PRIORITY_DEFAULT,
	IDLE_PLUGIN_ID,
	N_("I'dle Mak'er"),
	VERSION,
	N_("Allows you to hand-configure how long you've been idle for"),
	N_("Allows you to hand-configure how long you've been idle for"),
	"Eric Warmenhoven &lt;eric@warmenhoven.org>",
	WEBSITE,
	NULL,
	NULL,
	NULL,
	&ui_info,
	NULL
};

static void
__init_plugin(GaimPlugin *plugin)
{
}

GAIM_INIT_PLUGIN(idle, __init_plugin, info);
