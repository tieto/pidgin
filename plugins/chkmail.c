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
#include "gaim.h"

char username[] = "";
char password[] = "";
char mailhost[] = "";
int mailport = 110;

static void *handle = NULL;
extern GtkWidget *blist;
GtkWidget *maily;
GtkWidget *vbox2;
GtkWidget *yo;

GList *tmp;
int lastnum = 0;
int orig = 0;
int mytimer;

void update_mail();

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
                if (!strncmp(recv, "-ERR", strlen("-ERR"))) { step = 4; break; 
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

void gaim_plugin_init(void *h) {
	handle = h;
	tmp = gtk_container_children(GTK_CONTAINER(blist));

	maily = gtk_label_new("You have no new email");
	vbox2 = (GtkWidget *)tmp->data;

	yo = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(yo), GTK_SHADOW_IN );
	gtk_widget_show(yo);

	gtk_box_pack_start(GTK_BOX(vbox2), yo, FALSE, FALSE, 5);
	gtk_box_reorder_child(GTK_BOX(vbox2), yo, 2);
	gtk_container_add(GTK_CONTAINER(yo), maily);

	gtk_widget_show(maily);

	orig = num_msgs();
	lastnum = orig;

	mytimer = gtk_timeout_add(30000, (GtkFunction)update_mail, NULL);
}

void update_mail () {
	int newnum;
	gchar *buf;

	gtk_timeout_remove(mytimer);

	newnum = num_msgs();

	buf = g_malloc(BUF_LONG);

	if ( (newnum >= lastnum) && (newnum > 0)) {
		g_snprintf(buf, BUF_LONG, "You have %d new e-mail(s)", newnum - orig);
	} else {
		g_snprintf(buf, BUF_LONG, "You have no new email");
	}

	gtk_widget_destroy(maily);
	maily = gtk_label_new(buf);
	g_free(buf);
        
	gtk_container_add(GTK_CONTAINER(yo), maily);

       	gtk_widget_show(maily);

	if (newnum < lastnum) {
		orig = 0;
	}

	lastnum = newnum;
	mytimer = gtk_timeout_add(30000, (GtkFunction)update_mail, NULL);
}


void gaim_plugin_remove() {
	handle = NULL;
	gtk_widget_hide(maily);
	gtk_widget_destroy(yo);
	gtk_timeout_remove(mytimer);
}

char *name() {
	return "Check Mail";
}

char *description() {
	return "Check email every X seconds.\n";
}
