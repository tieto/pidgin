/*
 * gaim
 *
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h> 
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <gtk/gtk.h>
#include "gaim.h"

static void do_send_file(GtkWidget *, struct file_transfer *);
static void do_get_file (GtkWidget *, struct file_transfer *);

static void toggle(GtkWidget *w, int *m)
{
	*m = !(*m);
}

static void free_ft(struct file_transfer *ft)
{
	if (ft->window) { gtk_widget_destroy(ft->window); ft->window = NULL; }
	if (ft->filename) g_free(ft->filename);
	if (ft->user) g_free(ft->user);
	if (ft->message) g_free(ft->message);
	if (ft->ip) g_free(ft->ip);
	if (ft->cookie) g_free(ft->cookie);
	g_free(ft);
}

static void warn_callback(GtkWidget *widget, struct file_transfer *ft)
{
        show_warn_dialog(ft->user);
}

static void info_callback(GtkWidget *widget, struct file_transfer *ft)
{
        serv_get_info(ft->user);
}

static void cancel_callback(GtkWidget *widget, struct file_transfer *ft)
{
	char *send = g_malloc(256);

	if (ft->accepted) {
		g_free(send);
		return;
	}
	
	g_snprintf(send, 255, "toc_rvous_cancel %s %s %s", normalize(ft->user),
			ft->cookie, ft->UID);
	sflap_send(send, strlen(send), TYPE_DATA);
	g_free(send);
	free_ft(ft);
}

static void accept_callback(GtkWidget *widget, struct file_transfer *ft)
{
	char *buf = g_malloc(BUF_LEN);
	char *fname = g_malloc(BUF_LEN);
	char *c;

	if (!strcmp(ft->UID, FILE_SEND_UID)) {
		c = ft->filename + strlen(ft->filename);

		while (c != ft->filename) {
			if (*c == '/' || *c == '\\') {
				strcpy(fname, c+1);
				break;
			}
			c--;
		}

		if (c == ft->filename)
	                strcpy(fname, ft->filename);
	}
	
	gtk_widget_destroy(ft->window);
	ft->window = NULL;
	
	ft->window = gtk_file_selection_new("Gaim - Save As...");

	gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(ft->window));

	if (!strcmp(ft->UID, FILE_SEND_UID))
		g_snprintf(buf, BUF_LEN - 1, "%s/%s", getenv("HOME"), fname);
	else
		g_snprintf(buf, BUF_LEN - 1, "%s/", getenv("HOME"));
	gtk_file_selection_set_filename(GTK_FILE_SELECTION(ft->window), buf);

	gtk_signal_connect(GTK_OBJECT(ft->window), "destroy",
			   GTK_SIGNAL_FUNC(cancel_callback), ft);
                
	if (!strcmp(ft->UID, FILE_SEND_UID)) {
		gtk_signal_connect(GTK_OBJECT(
				GTK_FILE_SELECTION(ft->window)->ok_button),
			        "clicked", GTK_SIGNAL_FUNC(do_get_file), ft);
	} else {
		gtk_signal_connect(GTK_OBJECT(
				GTK_FILE_SELECTION(ft->window)->ok_button),
				"clicked", GTK_SIGNAL_FUNC(do_send_file), ft);
	}
	gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(ft->window)->cancel_button),
			   "clicked", GTK_SIGNAL_FUNC(cancel_callback), ft);

	gtk_widget_show(ft->window);
	
	g_free(buf);
	g_free(fname);
}

static void do_get_file(GtkWidget *w, struct file_transfer *ft)
{
	char *send = g_malloc(256);
	char *file = gtk_file_selection_get_filename(GTK_FILE_SELECTION(ft->window));
	char *buf;
	char *header;
	short hdrlen;
	int read_rv;
	char bmagic[7];
	struct sockaddr_in sin;
	guint32 rcv;
        char *c;
	int cont = 1;
	GtkWidget *fw = NULL, *fbar = NULL, *label = NULL, *button = NULL;
        
	if (!(ft->f = fopen(file,"w"))) {
		buf = g_malloc(BUF_LONG);
                g_snprintf(buf, BUF_LONG / 2, "Error writing file %s", file);
		do_error_dialog(buf, "Error");
		g_free(buf);
		ft->accepted = 0;
		accept_callback(NULL, ft);
		return;
	}

	ft->accepted = 1;
	
	gtk_widget_destroy(ft->window);
	ft->window = NULL;
	g_snprintf(send, 255, "toc_rvous_accept %s %s %s", normalize(ft->user),
			ft->cookie, ft->UID);
	sflap_send(send, strlen(send), TYPE_DATA);
	g_free(send);

	

        sin.sin_addr.s_addr = inet_addr(ft->ip);
        sin.sin_family = AF_INET;
	sin.sin_port = htons(ft->port);
	
	ft->fd = socket(AF_INET, SOCK_STREAM, 0);
	
	if (ft->fd <= -1 || connect(ft->fd, (struct sockaddr_in *)&sin, sizeof(sin))) {
		return;
	}

	rcv = 0;
	header = g_malloc(6);
	while (rcv != 6) {
		read_rv = read(ft->fd, header + rcv, 6 - rcv);
		if(read_rv < 0) {
			close(ft->fd);
			g_free(header);
			free_ft(ft);
			return;
		}
		rcv += read_rv;
		while(gtk_events_pending())
			gtk_main_iteration();
	}

	strncpy(bmagic, header, 6);
        bmagic[6] = 0;

	hdrlen = 0;
	hdrlen |= (header[4] << 8) & 0xff00;
	hdrlen |= (header[5] << 0) & 0x00ff;
	hdrlen -= 6;

	sprintf(debug_buff, "header length %d\n", hdrlen);
	debug_print(debug_buff);

	g_free(header);
	header = g_malloc(hdrlen);

	rcv = 0;

	while (rcv != hdrlen) {
		read_rv = read(ft->fd, header + rcv, hdrlen - rcv);
		if(read_rv < 0) {
			close(ft->fd);
			g_free(header);
			free_ft(ft);
			return;
		}
		rcv += read_rv;
		while(gtk_events_pending())
			gtk_main_iteration();
	}

	c = header;

	header[0] = 2; header[1] = 2;
	
	buf = frombase64(ft->cookie);
	memcpy(header + 2, buf, 8);
	g_free(buf);
	memset(header + 62, 0, 32); strcpy(header + 62, "Gaim");
	memset(header + 10, 0, 4);
	header[18] = 1; header[19] = 0;
	header[20] = 1; header[21] = 0;

	sprintf(debug_buff, "sending confirmation\n");
	debug_print(debug_buff);
	write(ft->fd, bmagic, 6);
	write(ft->fd, header, hdrlen);

	buf = g_malloc(1024);
	rcv = 0;
	
	fw = gtk_dialog_new();
	buf = g_malloc(2048);
	snprintf(buf, 2048, "Receiving %s from %s (%d bytes)", ft->filename,
			ft->user, ft->size);
	label = gtk_label_new(buf);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(fw)->vbox), label, 0, 0, 5);
	gtk_widget_show(label);
	fbar = gtk_progress_bar_new();
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(fw)->action_area), fbar, 0, 0, 5);
	gtk_widget_show(fbar);
	button = gtk_button_new_with_label("Cancel");
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(fw)->action_area), button, 0, 0, 5);
	gtk_widget_show(button);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc)toggle, &cont);
	gtk_window_set_title(GTK_WINDOW(fw), "File Transfer");
	gtk_widget_realize(fw);
	aol_icon(fw->window);
	gtk_widget_show(fw);

	sprintf(debug_buff, "Receiving %s from %s (%d bytes)\n", ft->filename,
			ft->user, ft->size);
	debug_print(debug_buff);

	while (rcv != ft->size && cont) {
		int i;
		int remain = ft->size - rcv > 1024 ? 1024 : ft->size - rcv;
		read_rv = recv(ft->fd, buf, remain, O_NONBLOCK);
		if(read_rv < 0) {
			fclose(ft->f);
			close(ft->fd);
			g_free(buf);
			g_free(header);
			free_ft(ft);
			return;
		}
		rcv += read_rv;
		for (i = 0; i < read_rv; i++)
			fprintf(ft->f, "%c", buf[i]);
		snprintf(buf, 2048, "Receiving %s from %s (%d / %d bytes)",
				header + 186, ft->user, rcv, ft->size);
		gtk_label_set_text(GTK_LABEL(label), buf);
		gtk_progress_bar_update(GTK_PROGRESS_BAR(fbar),
					(float)(rcv)/(float)(ft->size));
		while(gtk_events_pending())
			gtk_main_iteration();
	}
	fclose(ft->f);
	gtk_widget_destroy(fw);
	fw = NULL;

	if (!cont) {
		char *tmp = frombase64(ft->cookie);
		sprintf(buf, "toc_rvous_cancel %s %s %s", ft->user, tmp, ft->UID);
		sflap_send(buf, strlen(buf), TYPE_DATA);
		close(ft->fd);
		free_ft(ft);
		return;
	}

	memset(header + 18, 0, 4);
	header[94] = 0;
	header[1] = 0x04;
	memcpy(header+58, header+34, 4);
	memcpy(header+54, header+22, 4);
	write(ft->fd, bmagic, 6);
	write(ft->fd, header, hdrlen);
	close(ft->fd);

	g_free(buf);
	g_free(header);
	free_ft(ft);
}

static void do_send_file(GtkWidget *w, struct file_transfer *ft) {
	char *send = g_malloc(256);
	char *file = g_strdup(gtk_file_selection_get_filename(GTK_FILE_SELECTION(ft->window)));
	char *buf;
	char *header;
	short hdrlen;
	int read_rv;
	char bmagic[7];
	struct file_header *fhdr = g_new0(struct file_header, 1);
	struct sockaddr_in sin;
	guint32 rcv;
	char *c;
	GtkWidget *fw = NULL, *fbar = NULL, *label;
	struct stat st;

	stat(file, &st);
	if (!(ft->f = fopen(file, "r"))) {
		buf = g_malloc(BUF_LONG);
		g_snprintf(buf, BUF_LONG / 2, "Error reading file %s", file);
		do_error_dialog(buf, "Error");
		g_free(buf);
		ft->accepted = 0;
		accept_callback(NULL, ft);
		return;
	}

	ft->accepted = 1;

	gtk_widget_destroy(ft->window);
	ft->window = NULL;
	g_snprintf(send, 255, "toc_rvous_accept %s %s %s", normalize(ft->user),
			ft->cookie, ft->UID);
	sflap_send(send, strlen(send), TYPE_DATA);
	g_free(send);



	sin.sin_addr.s_addr = inet_addr(ft->ip);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(ft->port);

	ft->fd = socket(AF_INET, SOCK_STREAM, 0);

	if (ft->fd <= -1 || connect(ft->fd, (struct sockaddr_in *)&sin, sizeof(sin))) {
		return;
	}

	/* here's where we differ from do_get_file */
	/* 1. build/send header
	 * 2. receive header
	 * 3. send listing file
	 * 4. receive header
	 *
	 * then we need to wait to actually send the file.
	 */

	/* 1. build/send header */
	buf = frombase64(ft->cookie);
	sprintf(debug_buff, "Building header to send %s (cookie: %s)\n", file, buf);
	debug_print(debug_buff);
	fhdr->hdrtype = 0x1108;
	snprintf(fhdr->bcookie, 9, "%s", buf);
	g_free(buf);
	fhdr->encrypt = 0;
	fhdr->compress = 0;
	fhdr->totfiles = 1;
	fhdr->filesleft = 1;
	fhdr->totparts = 1;
	fhdr->partsleft = 1;
	fhdr->totsize = (long)st.st_size;
	fhdr->size = htonl((long)(st.st_size));
	fhdr->modtime = htonl(0);
	fhdr->checksum = htonl(0); /* FIXME? */
	fhdr->rfrcsum = 0;
	fhdr->rfsize = 0;
	fhdr->cretime = 0;
	fhdr->rfcsum = 0;
	fhdr->nrecvd = 0;
	fhdr->recvcsum = 0;
	snprintf(fhdr->idstring, 32, "Gaim");
	fhdr->flags = 0x20; /* don't ask me why */
	fhdr->lnameoffset = 0x1A;
	fhdr->lsizeoffset = 0x10;
	fhdr->dummy[0] = 0;
	fhdr->macfileinfo[0] = 0;
	fhdr->nencode = 0;
	fhdr->nlanguage = 0;
	snprintf(fhdr->name, 64, "listing.txt");
	snprintf(bmagic, 7, "OFT2\001\000");
	read_rv = write(ft->fd, bmagic, 6);
	if (read_rv <= -1) {
		sprintf(debug_buff, "Couldn't write opening header \n");
		debug_print(debug_buff);
		close(ft->fd);
		return;
	}
	read_rv = write(ft->fd, fhdr, 250);
	if (read_rv <= -1) {
		sprintf(debug_buff, "Couldn't write opening header 2\n");
		debug_print(debug_buff);
		close(ft->fd);
		return;
	}

	/* 2. receive header */
	sprintf(debug_buff, "Receiving header\n");
	debug_print(debug_buff);
	read_rv = read(ft->fd, bmagic, 6);
	if (read_rv <= -1) {
		sprintf(debug_buff, "Couldn't read header\n");
		debug_print(debug_buff);
		close(ft->fd);
		return;
	}
	read_rv = read(ft->fd, fhdr, *(short *)&bmagic[4]);
	if (read_rv <= -1) {
		sprintf(debug_buff, "Couldn't read header 2\n");
		debug_print(debug_buff);
		close(ft->fd);
		return;
	}

	/* 3. send listing file */
	sprintf(debug_buff, "Sending file\n");
	debug_print(debug_buff);
	rcv = 0;
	buf = g_malloc(2048);
	fw = gtk_dialog_new();
	snprintf(buf, 2048, "Sendin %s to %s (%ld bytes)", fhdr->name,
			ft->user, fhdr->size);
	label = gtk_label_new(buf);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(fw)->vbox), label, 0, 0, 5);
	gtk_widget_show(label);
	fbar = gtk_progress_bar_new();
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(fw)->action_area), fbar, 0, 0, 5);
	gtk_widget_show(fbar);
	gtk_window_set_title(GTK_WINDOW(fw), "File Transfer");
	gtk_widget_realize(fw);
	aol_icon(fw->window);
	gtk_widget_show(fw);

	sprintf(debug_buff, "Sending %s to %s (%d bytes)\n", fhdr->name,
			ft->user, ntohl(fhdr->size));
	debug_print(debug_buff);

	while (rcv != st.st_size) {
		int i;
		int remain = st.st_size - rcv > 1024 ? 1024 : st.st_size - rcv;
		for (i = 0; i < remain; i++)
			fscanf(ft->f, "%c", &buf[i]);
		read_rv = write(ft->fd, buf, remain);
		if (read_rv <= -1) {
			sprintf(debug_buff, "Could not send file, wrote %d\n", rcv);
			debug_print(debug_buff);
			close(ft->fd);
			gtk_widget_destroy(fw);
			return;
		}
		rcv += read_rv;
		snprintf(buf, 2048, "Sending %s to %s (%d / %ld bytes)",
				fhdr->name, ft->user, rcv, st.st_size);
		gtk_label_set_text(GTK_LABEL(label), buf);
		gtk_progress_bar_update(GTK_PROGRESS_BAR(fbar),
				(float)(rcv)/(float)(ft->size));
		while(gtk_events_pending())
			gtk_main_iteration();
	}
	gtk_widget_destroy(fw);

	/* 4. receive header */
	sprintf(debug_buff, "Receiving closing header\n");
	debug_print(debug_buff);
	read_rv = read(ft->fd, bmagic, 6);
	if (read_rv <= -1) {
		sprintf(debug_buff, "Couldn't read closing header\n");
		debug_print(debug_buff);
		close(ft->fd);
		return;
	}
	read_rv = read(ft->fd, fhdr, *(short *)&bmagic[4]);
	if (read_rv <= -1) {
		sprintf(debug_buff, "Couldn't read closing header 2\n");
		debug_print(debug_buff);
		close(ft->fd);
		return;
	}

	fclose(ft->f);
	close(ft->fd);
	g_free(fhdr);
}

void accept_file_dialog(struct file_transfer *ft)
{
        GtkWidget *accept, *info, *warn, *cancel;
        GtkWidget *text = NULL, *sw;
        GtkWidget *label;
        GtkWidget *vbox, *bbox;
        char buf[1024];

        
        ft->window = gtk_window_new(GTK_WINDOW_DIALOG);

        accept = gtk_button_new_with_label("Accept");
        info = gtk_button_new_with_label("Info");
        warn = gtk_button_new_with_label("Warn");
        cancel = gtk_button_new_with_label("Cancel");

        bbox = gtk_hbox_new(TRUE, 10);
        vbox = gtk_vbox_new(FALSE, 5);

        gtk_widget_show(accept);
        gtk_widget_show(info);
        gtk_widget_show(warn);
        gtk_widget_show(cancel);

        gtk_box_pack_start(GTK_BOX(bbox), accept, TRUE, TRUE, 10);
        gtk_box_pack_start(GTK_BOX(bbox), info, TRUE, TRUE, 10);
        gtk_box_pack_start(GTK_BOX(bbox), warn, TRUE, TRUE, 10);
        gtk_box_pack_start(GTK_BOX(bbox), cancel, TRUE, TRUE, 10);

	if (!strcmp(ft->UID, FILE_SEND_UID)) {
	        g_snprintf(buf, sizeof(buf), "%s requests you to accept the file: %s (%d bytes)",
				ft->user, ft->filename, ft->size);
	} else {
		g_snprintf(buf, sizeof(buf), "%s requests you to send them a file",
				ft->user);
	}
        label = gtk_label_new(buf);
        gtk_widget_show(label);
        gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 5);
        
        if (ft->message) {
		/* we'll do this later
                text = gaim_new_layout();
                sw = gtk_scrolled_window_new (NULL, NULL);
                gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                                GTK_POLICY_NEVER,
                                                GTK_POLICY_AUTOMATIC);
                gtk_widget_show(sw);
                gtk_container_add(GTK_CONTAINER(sw), text);
                gtk_widget_show(text);

                gtk_layout_set_size(GTK_LAYOUT(text), 250, 100);
                GTK_LAYOUT (text)->vadjustment->step_increment = 10.0;
                gtk_widget_set_usize(sw, 250, 100);

                gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 10);
		*/
        }
        gtk_box_pack_start(GTK_BOX(vbox), bbox, TRUE, TRUE, 5);

        gtk_window_set_title(GTK_WINDOW(ft->window), "Gaim - File Transfer?");
        gtk_window_set_focus(GTK_WINDOW(ft->window), accept);
        gtk_container_add(GTK_CONTAINER(ft->window), vbox);
        gtk_container_border_width(GTK_CONTAINER(ft->window), 10);
        gtk_widget_show(vbox);
        gtk_widget_show(bbox);
        gtk_widget_realize(ft->window);
        aol_icon(ft->window->window);

        gtk_widget_show(ft->window);


	gtk_signal_connect(GTK_OBJECT(accept), "clicked",
			   GTK_SIGNAL_FUNC(accept_callback), ft);
	gtk_signal_connect(GTK_OBJECT(cancel), "clicked",
			   GTK_SIGNAL_FUNC(cancel_callback), ft);
	gtk_signal_connect(GTK_OBJECT(warn), "clicked",
			   GTK_SIGNAL_FUNC(warn_callback), ft);
	gtk_signal_connect(GTK_OBJECT(info), "clicked",
			   GTK_SIGNAL_FUNC(info_callback), ft);


	if (ft->message) {
		/* we'll do this later
		while(gtk_events_pending())
			gtk_main_iteration();
		html_print(text, ft->message);
		*/
	}
}
