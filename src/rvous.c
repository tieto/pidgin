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

#ifndef USE_OSCAR

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
#include <errno.h>
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

static int read_file_header(int fd, struct file_header *header) {
	char buf[257];
	int read_rv = read(fd, buf, 256);
	memcpy(&header->magic,		buf +   0,  4);
	memcpy(&header->hdrlen,		buf +   4,  2);
	memcpy(&header->hdrtype,	buf +   6,  2);
	memcpy(&header->bcookie,	buf +   8,  8);
	memcpy(&header->encrypt,	buf +  16,  2);
	memcpy(&header->compress,	buf +  18,  2);
	memcpy(&header->totfiles,	buf +  20,  2);
	memcpy(&header->filesleft,	buf +  22,  2);
	memcpy(&header->totparts,	buf +  24,  2);
	memcpy(&header->partsleft,	buf +  26,  2);
	memcpy(&header->totsize,	buf +  28,  4);
	memcpy(&header->size,		buf +  32,  4);
	memcpy(&header->modtime,	buf +  36,  4);
	memcpy(&header->checksum,	buf +  40,  4);
	memcpy(&header->rfrcsum,	buf +  44,  4);
	memcpy(&header->rfsize,		buf +  48,  4);
	memcpy(&header->cretime,	buf +  52,  4);
	memcpy(&header->rfcsum,		buf +  56,  4);
	memcpy(&header->nrecvd,		buf +  60,  4);
	memcpy(&header->recvcsum,	buf +  64,  4);
	memcpy(&header->idstring,	buf +  68, 32);
	memcpy(&header->flags,		buf + 100,  1);
	memcpy(&header->lnameoffset,	buf + 101,  1);
	memcpy(&header->lsizeoffset,	buf + 102,  1);
	memcpy(&header->dummy,		buf + 103, 69);
	memcpy(&header->macfileinfo,	buf + 172, 16);
	memcpy(&header->nencode,	buf + 188,  2);
	memcpy(&header->nlanguage,	buf + 190,  2);
	memcpy(&header->name,		buf + 192, 64);
	return read_rv;
}

static int write_file_header(int fd, struct file_header *header) {
	char buf[257];
	memcpy(buf +   0, &header->magic,	 4);
	memcpy(buf +   4, &header->hdrlen,	 2);
	memcpy(buf +   6, &header->hdrtype,	 2);
	memcpy(buf +   8, &header->bcookie,	 8);
	memcpy(buf +  16, &header->encrypt,	 2);
	memcpy(buf +  18, &header->compress,	 2);
	memcpy(buf +  20, &header->totfiles,	 2);
	memcpy(buf +  22, &header->filesleft,	 2);
	memcpy(buf +  24, &header->totparts,	 2);
	memcpy(buf +  26, &header->partsleft,	 2);
	memcpy(buf +  28, &header->totsize,	 4);
	memcpy(buf +  32, &header->size,	 4);
	memcpy(buf +  36, &header->modtime,	 4);
	memcpy(buf +  40, &header->checksum,	 4);
	memcpy(buf +  44, &header->rfrcsum,	 4);
	memcpy(buf +  48, &header->rfsize,	 4);
	memcpy(buf +  52, &header->cretime,	 4);
	memcpy(buf +  56, &header->rfcsum,	 4);
	memcpy(buf +  60, &header->nrecvd,	 4);
	memcpy(buf +  64, &header->recvcsum,	 4);
	memcpy(buf +  68, &header->idstring,	32);
	memcpy(buf + 100, &header->flags,	 1);
	memcpy(buf + 101, &header->lnameoffset,	 1);
	memcpy(buf + 102, &header->lsizeoffset,	 1);
	memcpy(buf + 103, &header->dummy,	69);
	memcpy(buf + 172, &header->macfileinfo,	16);
	memcpy(buf + 188, &header->nencode,	 2);
	memcpy(buf + 190, &header->nlanguage,	 2);
	memcpy(buf + 192, &header->name,	64);
	return write(fd, buf, 256);
}

static void do_get_file(GtkWidget *w, struct file_transfer *ft)
{
	char *send = g_malloc(256);
	char *file = gtk_file_selection_get_filename(GTK_FILE_SELECTION(ft->window));
	char *buf;
	struct file_header *header = g_new0(struct file_header, 1);
	int read_rv;
	guint32 rcv;
        char *c;
	int cont = 1;
	GtkWidget *fw = NULL, *fbar = NULL, *label = NULL;
	GtkWidget *button = NULL, *pct = NULL;

	if (!(ft->f = fopen(file,"w"))) {
		buf = g_malloc(BUF_LONG);
                g_snprintf(buf, BUF_LONG / 2, "Error writing file %s", file);
		do_error_dialog(buf, "Error");
		g_free(buf);
		g_free(header);
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

	
	ft->fd = connect_address(inet_addr(ft->ip), ft->port);

	if (ft->fd <= -1) {
		g_free(header);
		fclose(ft->f);
		free_ft(ft);
		return;
	}

	read_rv = read_file_header(ft->fd, header);
	if(read_rv < 0) {
		close(ft->fd);
		fclose(ft->f);
		g_free(header);
		free_ft(ft);
		return;
	}

	sprintf(debug_buff, "header length %d\n", header->hdrlen);
	debug_print(debug_buff);

	header->hdrtype = 0x202;
	
	buf = frombase64(ft->cookie);
	memcpy(header->bcookie, buf, 8);
	g_free(buf);
	snprintf(header->idstring, 32, "Gaim");
	header->encrypt = 0; header->compress = 0;
	header->totparts = 1; header->partsleft = 1;

	sprintf(debug_buff, "sending confirmation\n");
	debug_print(debug_buff);
	write_file_header(ft->fd, header);

	buf = g_malloc(2048);
	rcv = 0;

	fw = gtk_dialog_new();
	buf = g_malloc(2048);
	snprintf(buf, 2048, "Receiving %s from %s", ft->filename, ft->user);
	label = gtk_label_new(buf);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(fw)->vbox), label, 0, 0, 5);
	gtk_widget_show(label);
	fbar = gtk_progress_bar_new();
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(fw)->action_area), fbar, 0, 0, 5);
	gtk_widget_show(fbar);
	pct = gtk_label_new("0 %");
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(fw)->action_area), pct, 0, 0, 5);
	gtk_widget_show(pct);
	button = gtk_button_new_with_label("Cancel");
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(fw)->action_area), button, 0, 0, 5);
	gtk_widget_show(button);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc)toggle, &cont);
	gtk_window_set_title(GTK_WINDOW(fw), "Gaim - File Transfer");
	gtk_widget_realize(fw);
	aol_icon(fw->window);
	gtk_widget_show(fw);

	sprintf(debug_buff, "Receiving %s from %s (%d bytes)\n", ft->filename,
			ft->user, ft->size);
	debug_print(debug_buff);

	fcntl(ft->fd, F_SETFL, O_NONBLOCK);

	while (rcv != ft->size && cont) {
		int i;
		float pcnt = ((float)rcv)/((float)ft->size);
		int remain = ft->size - rcv > 1024 ? 1024 : ft->size - rcv;
		read_rv = read(ft->fd, buf, remain);
		if(read_rv < 0) {
			if (errno == EWOULDBLOCK) {
				while(gtk_events_pending())
					gtk_main_iteration();
				continue;
			}
			gtk_widget_destroy(fw);
			fw = NULL;
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
		gtk_progress_bar_update(GTK_PROGRESS_BAR(fbar), pcnt);
		sprintf(buf, "%d / %d K (%2.0f %%)", rcv/1024, ft->size/1024, 100*pcnt);
		gtk_label_set_text(GTK_LABEL(pct), buf);
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
		g_free(header);
		return;
	}

	sprintf(debug_buff, "Download complete.\n");
	debug_print(debug_buff);

	header->hdrtype = 0x402;
	header->totparts = 0; header->partsleft = 0;
	header->flags = 0;
	header->recvcsum = header->checksum;
	header->nrecvd = header->totsize;
	write_file_header(ft->fd, header);
	close(ft->fd);

	g_free(buf);
	g_free(header);
	free_ft(ft);
}

static void do_send_file(GtkWidget *w, struct file_transfer *ft) {
	char *send = g_malloc(256);
	char *file = g_strdup(gtk_file_selection_get_filename(GTK_FILE_SELECTION(ft->window)));
	char *buf;
	int read_rv;
	struct file_header *fhdr = g_new0(struct file_header, 1);
	struct sockaddr_in sin;
	guint32 rcv;
	int cont;
	char *c;
	struct stat st;
	struct tm *fortime;
	GtkWidget *fw = NULL, *fbar = NULL, *label = NULL;
	GtkWidget *button = NULL, *pct = NULL;

	stat(file, &st);
	if (!(ft->f = fopen(file, "r"))) {
		buf = g_malloc(BUF_LONG);
		g_snprintf(buf, BUF_LONG / 2, "Error reading file %s", file);
		do_error_dialog(buf, "Error");
		g_free(buf);
		ft->accepted = 0;
		accept_callback(NULL, ft);
		free_ft(ft);
		return;
	}

	ft->accepted = 1;

	gtk_widget_destroy(ft->window);
	ft->window = NULL;
	g_snprintf(send, 255, "toc_rvous_accept %s %s %s", normalize(ft->user),
			ft->cookie, ft->UID);
	sflap_send(send, strlen(send), TYPE_DATA);
	g_free(send);



	ft->fd = connect_address(inet_addr(ft->ip), ft->port);

	if (ft->fd <= -1) {
		g_free(fhdr);
		free_ft(ft);
		return;
	}

	/* here's where we differ from do_get_file */
	/* 1. build/send header
	 * 2. receive header
	 * 3. send listing file
	 * 4. receive header
	 *
	 * then we need to wait to actually send the file.
	 *
	 * 5. either (receive header) or (remote host cancels)
	 * 6. if received header, send file
	 * 7. ? haven't gotten this far yet
	 */

	/* 1. build/send header */
	c = file + strlen(file);
	while (*(c - 1) != '/') c--;
	buf = frombase64(ft->cookie);
	sprintf(debug_buff, "Building header to send %s (cookie: %s)\n", file, buf);
	debug_print(debug_buff);
	fhdr->magic[0] = 'O'; fhdr->magic[1] = 'F';
	fhdr->magic[2] = 'T'; fhdr->magic[3] = '2';
	fhdr->hdrlen = htons(256);
	fhdr->hdrtype = htons(0x1108);
	snprintf(fhdr->bcookie, 8, "%s", buf);
	g_free(buf);
	fhdr->encrypt = 0;
	fhdr->compress = 0;
	fhdr->totfiles = htons(1);
	fhdr->filesleft = htons(1);
	fhdr->totparts = htons(1);
	fhdr->partsleft = htons(1);
	fhdr->totsize = htonl((long)st.st_size); /* combined size of all files */
	/* size = strlen("mm/dd/yyyy hh:mm sizesize 'name'\r\n") */
	fhdr->size = htonl(28 + strlen(c)); /* size of listing.txt */
	fhdr->modtime = htonl(time(NULL)); /* time since UNIX epoch */
	fhdr->checksum = htonl(0x89f70000); /* ? i don't think this matters */
	fhdr->rfrcsum = 0;
	fhdr->rfsize = 0;
	fhdr->cretime = 0;
	fhdr->rfcsum = 0;
	fhdr->nrecvd = 0;
	fhdr->recvcsum = 0;
	snprintf(fhdr->idstring, 32, "OFT_Windows ICBMFT V1.1 32");
	fhdr->flags = 0x02;		/* don't ask me why */
	fhdr->lnameoffset = 0x1A;	/* ? still no clue */
	fhdr->lsizeoffset = 0x10;	/* whatever */
	memset(fhdr->dummy, 0, 69);
	memset(fhdr->macfileinfo, 0, 16);
	fhdr->nencode = 0;
	fhdr->nlanguage = 0;
	snprintf(fhdr->name, 64, "listing.txt");
	read_rv = write_file_header(ft->fd, fhdr);
	if (read_rv <= -1) {
		sprintf(debug_buff, "Couldn't write opening header\n");
		debug_print(debug_buff);
		g_free(fhdr);
		close(ft->fd);
		free_ft(ft);
		return;
	}

	/* 2. receive header */
	sprintf(debug_buff, "Receiving header\n");
	debug_print(debug_buff);
	read_rv = read_file_header(ft->fd, fhdr);
	if (read_rv <= -1) {
		sprintf(debug_buff, "Couldn't read header\n");
		debug_print(debug_buff);
		g_free(fhdr);
		close(ft->fd);
		free_ft(ft);
		return;
	}

	/* 3. qend listing file */
	/* mm/dd/yyyy hh:mm sizesize name.ext\r\n */
	/* creation date ^ */
	sprintf(debug_buff, "Sending file\n");
	debug_print(debug_buff);
	buf = g_malloc(ft->size + 1);
	fortime = localtime(&st.st_ctime);
	snprintf(buf, ntohl(fhdr->size) + 1, "%2d/%2d/%4d %2d:%2d %8ld %s\r\n",
			fortime->tm_mon + 1, fortime->tm_mday, fortime->tm_year + 1900,
			fortime->tm_hour + 1, fortime->tm_min + 1,
			st.st_size, c);
	sprintf(debug_buff, "Sending listing.txt (%ld bytes) to %s\n",
			ntohl(fhdr->size) + 1, ft->user);
	debug_print(debug_buff);

	read_rv = write(ft->fd, buf, ntohl(fhdr->size));
	if (read_rv <= -1) {
		sprintf(debug_buff, "Could not send file, wrote %d\n", rcv);
		debug_print(debug_buff);
		g_free(buf);
		g_free(fhdr);
		close(ft->fd);
		free_ft(ft);
		return;
	}
	g_free(buf);

	/* 4. receive header */
	sprintf(debug_buff, "Receiving closing header\n");
	debug_print(debug_buff);
	read_rv = read_file_header(ft->fd, fhdr);
	if (read_rv <= -1) {
		sprintf(debug_buff, "Couldn't read closing header\n");
		debug_print(debug_buff);
		g_free(fhdr);
		close(ft->fd);
		free_ft(ft);
		return;
	}

	/* 5. wait to see if we're sending it */
	fcntl(ft->fd, F_SETFL, O_NONBLOCK);
	rcv = 0;
	while (rcv != 256) {
		int i;
		read_rv = read_file_header(ft->fd, fhdr);
		if(read_rv < 0) {
			if (errno == EWOULDBLOCK) {
				while(gtk_events_pending())
					gtk_main_iteration();
				continue;
			}
			fclose(ft->f);
			close(ft->fd);
			g_free(fhdr);
			free_ft(ft);
			return;
		}
		rcv += read_rv;
	}

	/* 6. send the file */
	fw = gtk_dialog_new();
	buf = g_malloc(2048);
	snprintf(buf, 2048, "Sending %s to %s", fhdr->name, ft->user);
	label = gtk_label_new(buf);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(fw)->vbox), label, 0, 0, 5);
	gtk_widget_show(label);
	fbar = gtk_progress_bar_new();
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(fw)->action_area), fbar, 0, 0, 5);
	gtk_widget_show(fbar);
	pct = gtk_label_new("0 %");
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(fw)->action_area), pct, 0, 0, 5);
	gtk_widget_show(pct);
	button = gtk_button_new_with_label("Cancel");
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(fw)->action_area), button, 0, 0, 5);
	gtk_widget_show(button);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc)toggle, &cont);
	gtk_window_set_title(GTK_WINDOW(fw), "Gaim - File Transfer");
	gtk_widget_realize(fw);
	aol_icon(fw->window);
	gtk_widget_show(fw);

	sprintf(debug_buff, "Sending %s to %s (%d bytes)\n", fhdr->name,
			ft->user, fhdr->totsize);
	debug_print(debug_buff);

	rcv = 0; cont = 1;
	while (rcv != fhdr->totsize && cont) {
		int i;
		float pcnt = ((float)rcv)/((float)fhdr->totsize);
		int remain = fhdr->totsize - rcv > 1024 ? 1024 : fhdr->totsize - rcv;
		for (i = 0; i < 1024; i++)
			fscanf(ft->f, "%c", &buf[i]);
		read_rv = write(ft->fd, buf, remain);
		if (read_rv < 0) {
			if (errno == EWOULDBLOCK) {
				while (gtk_events_pending())
					gtk_main_iteration();
				continue;
			}
			fclose(ft->f);
			gtk_widget_destroy(fw);
			fw = NULL;
			fclose(ft->f);
			close(ft->fd);
			g_free(buf);
			g_free(fhdr);
			free_ft(ft);
			return;
		}
		rcv += read_rv;
		gtk_progress_bar_update(GTK_PROGRESS_BAR(fbar), pcnt);
		sprintf(buf, "%d / %d K (%2.0f %%)", rcv/1024, ft->size/1024, 100*pcnt);
		gtk_label_set_text(GTK_LABEL(pct), buf);
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
		g_free(buf);
		close(ft->fd);
		free_ft(ft);
		g_free(fhdr);
		return;
	}

	sprintf(debug_buff, "Upload complete.\n");
	debug_print(debug_buff);

	/* 7. receive closing header */

	/* done */
	close(ft->fd);
	g_free(buf);
	g_free(fhdr);
	free_ft(ft);
}

void accept_file_dialog(struct file_transfer *ft)
{
        GtkWidget *accept, *info, *warn, *cancel;
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
        if (ft->message)
		strncat(buf, ft->message, sizeof(buf) - strlen(buf));
        label = gtk_label_new(buf);
        gtk_widget_show(label);
        gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 5);
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

#endif /* USE_OSCAR */
