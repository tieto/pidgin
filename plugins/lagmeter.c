/* KNOWN BUGS:
 * 	if you are also using notify.so, it will open a new window to yourself.
 * 	it will not, however, write anything in that window. this is a problem
 * 	with notify.c. maybe one day i'll modify notify.c so that these two
 * 	plugins are more compatible. we'll see.
 *
 * This lagometer has a tendency to not at all show the same lag that the
 * built-in lagometer shows. My guess as to why this is (because they use the
 * exact same code) is because it sends the string more often. That's why I
 * included the configuration option to set the delay between updates.
 *
 * You can load this plugin even when you're not signed on, even though it
 * modifies the buddy list. This is because it checks to see that the buddy
 * list is actually there. In every case that I've been able to think of so
 * far, it does the right thing (tm).
 */

#define GAIM_PLUGINS
#include "gaim.h"

#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <math.h>

#define MY_LAG_STRING "ZYXCHECKLAGXYZ"

void *handle;
GtkWidget *lagbox;
GtkWidget *my_lagometer;
struct timeval my_lag_tv;
int check_timeout = -1;
guint delay = 10;
static GtkWidget *confdlg;

void update_lag(int us) {
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

void check_lag(char **who, char **message, void *m) {
	char *name = g_strdup(normalize(*who));
	if (!strcasecmp(normalize(current_user->username), name) &&
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

void send_lag() {
	gettimeofday(&my_lag_tv, NULL);
	serv_send_im(current_user->username, MY_LAG_STRING, 1);
}

void gaim_plugin_remove() {
	if (check_timeout != -1)
		gtk_timeout_remove(check_timeout);
	if (confdlg)
		gtk_widget_destroy(confdlg);
	confdlg = NULL;
	gtk_widget_destroy(lagbox);
}

void avail_now(void *m) {
	update_lag(0);
	gaim_signal_connect(handle, event_im_recv, check_lag, NULL);
	gaim_signal_connect(handle, event_signoff, gaim_plugin_remove, NULL);
	check_timeout = gtk_timeout_add(1000 * delay, (GtkFunction)send_lag, NULL);
}

void gaim_plugin_init(void *h) {
	handle = h;

	if (!blist)
		gaim_signal_connect(handle, event_signon, avail_now, NULL);
	else
		avail_now(NULL);
}

void adjust_timeout(GtkWidget *button, GtkWidget *spinner) {
	delay = CLAMP(gtk_spin_button_get_value_as_int(
				GTK_SPIN_BUTTON(spinner)), 0, 3600);
	sprintf(debug_buff, "new updates: %d\n", delay);
	debug_print(debug_buff);
	if (check_timeout >= 0)
		gtk_timeout_remove(check_timeout);
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
		gtk_widget_show(confdlg);
		return;
	}

	confdlg = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_window_set_title(GTK_WINDOW(confdlg), "Gaim Lag Delay");

	box = gtk_hbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(box), 5);
	gtk_container_add(GTK_CONTAINER(confdlg), box);
	gtk_widget_show(box);

	label = gtk_label_new("Delay between updates: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, TRUE, 0);
	gtk_widget_show(label);

	adj = (GtkAdjustment *)gtk_adjustment_new(delay, 0, 3600, 1, 0, 0);
	spinner = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 0, 0);
	gtk_box_pack_start(GTK_BOX(box), spinner, TRUE, TRUE, 0);
	gtk_widget_show(spinner);

	button = gtk_button_new_with_label("OK");
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
			   (GtkSignalFunc)adjust_timeout, spinner);
	gtk_box_pack_start(GTK_BOX(box), button, FALSE, TRUE, 0);
	gtk_widget_show(button);

	gtk_widget_show(confdlg);
}

char *name() {
	return "Lag-O-Meter, Pluggified";
}

char *description() {
	return "Your old familiar Lag-O-Meter, in a brand new form.";
}
