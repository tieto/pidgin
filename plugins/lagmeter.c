#define GAIM_PLUGINS
#include "gaim.h"
#include "prpl.h"

#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#define MY_LAG_STRING "ZYXCHECKLAGXYZ"

GModule *handle;
GtkWidget *lagbox = NULL;
GtkWidget *my_lagometer;
struct timeval my_lag_tv;
guint check_timeout = 0;
guint delay = 10;
static GtkWidget *confdlg = NULL;
struct gaim_connection *my_gc = NULL;

static void avail_now(struct gaim_connection *, void *);

static void update_lag(int us) {
	double pct;

	if (lagbox == NULL) {
		/* guess we better build it then :P */
		GtkWidget *label = gtk_label_new("Lag-O-Meter: ");
		GList *tmp = gtk_container_children(GTK_CONTAINER(blist));
		GtkWidget *vbox2 = (GtkWidget *)tmp->data;
		lagbox = gtk_hbox_new(FALSE, 0);
		my_lagometer = gtk_progress_bar_new();

		gtk_box_pack_start(GTK_BOX(lagbox), label, FALSE, FALSE, 5);
		gtk_box_pack_start(GTK_BOX(lagbox), my_lagometer, TRUE, TRUE, 5);
		gtk_widget_set_usize(my_lagometer, 5, 5);

		gtk_widget_show(label);
		gtk_widget_show(my_lagometer);

		gtk_box_pack_start(GTK_BOX(vbox2), lagbox, FALSE, TRUE, 0);
		gtk_box_reorder_child(GTK_BOX(vbox2), lagbox, 1);
		gtk_widget_show(lagbox);
	}

	pct = us/100000;
	if (pct > 0)
		pct = 25 * log(pct);
	if (pct < 0)
		pct = 0;
	if (pct > 100)
		pct = 100;
	pct /= 100;

	gtk_progress_bar_update(GTK_PROGRESS_BAR(my_lagometer), pct);
}

static void check_lag(struct gaim_connection *gc, char **who, char **message, void *m) {
	char *name;
	if (gc != my_gc)
		return;

	name = g_strdup(normalize(*who));
	if (!strcasecmp(normalize(gc->username), name) &&
	    (*message != NULL) &&
	    !strcmp(*message, MY_LAG_STRING)) {
		struct timeval tv;
		int ms;

		gettimeofday(&tv, NULL);

		ms = 1000000 * (tv.tv_sec - my_lag_tv.tv_sec);

		ms += tv.tv_usec - my_lag_tv.tv_usec;

		update_lag(ms);
		*message = NULL;
	}
	g_free(name);
}

static gint send_lag() {
	gettimeofday(&my_lag_tv, NULL);
	if (g_slist_find(connections, my_gc)) {
		char *m = g_strdup(MY_LAG_STRING);
		if (my_gc->protocol == PROTO_JABBER) {
			if (!strchr(my_gc->username, '@')) {
				char *buf = g_strconcat(my_gc->username, "@jabber.org/GAIM", NULL);
				serv_send_im(my_gc, buf, m, 1);
				g_free(buf);
			} else if (!strchr(my_gc->username, '/')) {
				char *buf = g_strconcat(my_gc->username, "@jabber.org/GAIM", NULL);
				serv_send_im(my_gc, buf, m, 1);
				g_free(buf);
			} else
				serv_send_im(my_gc, my_gc->username, m, 1);
		} else
			serv_send_im(my_gc, my_gc->username, m, 1);
		g_free(m);
		return TRUE;
	} else {
		debug_printf("LAGMETER: send_lag called for connection that no longer exists\n");
		check_timeout = 0;
		return FALSE;
	}
}

static void got_signoff(struct gaim_connection *gc, void *m) {
	if (gc != my_gc)
		return;

	if (check_timeout > 0)
		gtk_timeout_remove(check_timeout);
	check_timeout = 0;

	if (confdlg)
		gtk_widget_destroy(confdlg);
	confdlg = NULL;

	if (lagbox)
		gtk_widget_destroy(lagbox);
	lagbox = NULL;

	if (g_slist_length(connections) > 1) {
		if (connections->data == my_gc) {
			my_gc = NULL;
			avail_now(connections->next->data, NULL);
		} else {
			my_gc = NULL;
			avail_now(connections->data, NULL);
		}
	} else {
		my_gc = NULL;
	}
}

static void avail_now(struct gaim_connection *gc, void *m) {
	if (my_gc)
		return;
	update_lag(0);
	check_timeout = gtk_timeout_add(1000 * delay, (GtkFunction)send_lag, NULL);
	my_gc = gc;
}

void gaim_plugin_remove() {
	if (check_timeout > 0)
		gtk_timeout_remove(check_timeout);
	check_timeout = 0;
	if (confdlg)
		gtk_widget_destroy(confdlg);
	if (lagbox)
		gtk_widget_destroy(lagbox);

	confdlg = NULL;
	lagbox = NULL;
	my_gc = NULL;
}

char *gaim_plugin_init(GModule *h) {
	handle = h;

	confdlg = NULL;
	lagbox = NULL;
	my_gc = NULL;

	gaim_signal_connect(handle, event_signon, avail_now, NULL);
	gaim_signal_connect(handle, event_im_recv, check_lag, NULL);
	gaim_signal_connect(handle, event_signoff, got_signoff, NULL);

	if (connections)
		avail_now(connections->data, NULL);

	return NULL;
}

static void adjust_timeout(GtkWidget *button, GtkWidget *spinner) {
	delay = CLAMP(gtk_spin_button_get_value_as_int(
				GTK_SPIN_BUTTON(spinner)), 0, 3600);
	debug_printf("LAGMETER: new updates: %d\n", delay);
	if (check_timeout > 0)
		gtk_timeout_remove(check_timeout);
	check_timeout = 0;
	if (my_gc)
		check_timeout = gtk_timeout_add(1000 * delay, (GtkFunction)send_lag, NULL);
	gtk_widget_destroy(confdlg);
	confdlg = NULL;
}

void gaim_plugin_config() {
	GtkWidget *label;
	GtkAdjustment *adj;
	GtkWidget *spinner;
	GtkWidget *button;
	GtkWidget *box;

	if (confdlg) {
		gtk_widget_show_all(confdlg);
		return;
	}

	confdlg = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_window_set_title(GTK_WINDOW(confdlg), "Gaim Lag Delay");

	box = gtk_hbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(box), 5);
	gtk_container_add(GTK_CONTAINER(confdlg), box);

	label = gtk_label_new("Delay between updates: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, TRUE, 0);

	adj = (GtkAdjustment *)gtk_adjustment_new(delay, 0, 3600, 1, 0, 0);
	spinner = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 0, 0);
	gtk_box_pack_start(GTK_BOX(box), spinner, TRUE, TRUE, 0);

	button = gtk_button_new_with_label("OK");
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
			   (GtkSignalFunc)adjust_timeout, spinner);
	gtk_box_pack_start(GTK_BOX(box), button, FALSE, TRUE, 0);

	gtk_widget_show_all(confdlg);
}

char *name() {
	return "Lag-O-Meter, Pluggified";
}

char *description() {
	return "Your old familiar Lag-O-Meter, in a brand new form.";
}
