/* This is some funky code.  It is still being developed by Rob Flynn - rob@linuxpimps.com
 * I recommend not using this code right now. :)
*/

#define GAIM_PLUGINS

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>

#include "internal.h"
#include "gtkgaim.h"

char username[] = "";
char password[] = "";
char mailhost[] = "";
int mailport = 110;
int state = 0;

static void *handle = NULL;
extern GtkWidget *buddies;

int lastnum = 0;
int orig = 0;
int mytimer;

void update_mail();
void check_mail();

int num_msgs()
{
	struct in_addr *sin;
	char recv[1024];
	char command[256];
	int fd;
	int num = 0;
	int step = 0;
	int len;

	sin = (struct in_addr *)get_address(mailhost);
	fd = connect_address(sin->s_addr, mailport);
	while ((len = read(fd, recv, 1023))>0) {
		recv[len] = 0;
		if (!strncmp(recv, "-ERR", strlen("-ERR"))) {
			step = 4;
			break;
		} else if (!strncmp(recv, "+OK", strlen("+OK"))) {
			if (step == 3) {
				if (sscanf(recv, "+OK %d %d\n", &num, &step) != 2)
					break;
				g_snprintf(command, sizeof(command), "QUIT\n");
				write(fd, command, strlen(command));
				close(fd);
				return num;
			}

			if (step == 0) {
				g_snprintf(command, sizeof(command), "USER %s\n", username);
				write(fd, command, strlen(command));
				step = 1;
			} else if (step == 1) {
				g_snprintf(command, sizeof(command), "PASS %s\n", password);
				write(fd, command, strlen(command));
				step = 2;
			} else if (step == 2) {
				g_snprintf(command, sizeof(command), "STAT\n");
				write(fd, command, strlen(command));
				step = 3;
			}
		}
	}
	close(fd);

	return 0;
}

void destroy_mail_list()
{
	GList *list;
	GtkWidget *w;

	list = GTK_TREE(buddies)->children;
	while (list) {
		w = (GtkWidget *)list->data;
		if (!strcmp(GTK_LABEL(GTK_BIN(w)->child)->label, _("Mail Server"))) {
			gtk_tree_remove_items(GTK_TREE(buddies), list);
			list = GTK_TREE(buddies)->children;
			if (!list)
				break;
		}
		list = list->next;
	}
}

void setup_mail_list()
{
	GList *list;
	GtkWidget *w;
	GtkWidget *item;
	GtkWidget *tree;
	gchar *buf;

	list = GTK_TREE(buddies)->children;

	while (list) {
		w = (GtkWidget *)list->data;
		if (!strcmp(GTK_LABEL(GTK_BIN(w)->child)->label, _("Mail Server"))) {
			gtk_tree_remove_items(GTK_TREE(buddies), list);
			list = GTK_TREE(buddies)->children;
			if (!list)
				break;
		}
		list = list->next;
	}

	item = gtk_tree_item_new_with_label(_("Mail Server"));
	tree = gtk_tree_new();
	gtk_widget_show(item);
	gtk_widget_show(tree);
	gtk_tree_append(GTK_TREE(buddies), item);
	gtk_tree_item_set_subtree(GTK_TREE_ITEM(item), tree);
	gtk_tree_item_expand(GTK_TREE_ITEM(item));

	/* XXX - This needs to use ngettext() */
	buf = g_strdup_printf(_("%s (%d new/%d total)"), mailhost, lastnum - orig, lastnum);
	item = gtk_tree_item_new_with_label(buf);
	g_free(buf);

	gtk_tree_append(GTK_TREE(tree), item);
	gtk_widget_show(item);
}

void gaim_plugin_init(void *h) {
	handle = h;

	orig = num_msgs();
	lastnum = orig;

	gaim_signal_connect(handle, event_blist_update, setup_mail_list, NULL);
	setup_mail_list();

	mytimer = g_timeout_add(30000, check_mail, NULL);
}

void check_mail() {
	pthread_t mail_thread;
	pthread_attr_t attr;

	if (state == 0) {
		state = 1;
		pthread_attr_init(&attr);
		pthread_create(&mail_thread, &attr, (void *)&update_mail, NULL);
	}
}

void update_mail () {
	int newnum;

	g_source_remove(mytimer);

	newnum = num_msgs();

	if ((newnum >= lastnum) && (newnum > 0)) {
		newnum = newnum - lastnum;
	} else {
		newnum = 0;
	}

	if (newnum < lastnum) {
		orig = lastnum;
	}

	lastnum = newnum;
	mytimer = g_timeout_add(30000, check_mail, NULL);
	setup_mail_list();
	state = 0;
}

void gaim_plugin_remove() {
	g_source_remove(mytimer);
	while (state == 1) { }
	destroy_mail_list();
	handle = NULL;
}

char *name() {
	return _("Check Mail");
}

char *description() {
	return _("Check email every X seconds.\n");
}
