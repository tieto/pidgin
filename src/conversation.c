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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <string.h>
#ifndef _WIN32
#include <sys/time.h>
#include <unistd.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#else
#ifdef small
#undef small
#endif
#endif /*_WIN32*/
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <gtk/gtk.h>
#ifdef USE_GTKSPELL
#include <gtkspell/gtkspell.h>
#endif
#include "gtkimhtml.h"
#include <gdk/gdkkeysyms.h>
#include "convo.h"
#include "prpl.h"

#ifdef _WIN32
#include <process.h> /* for getpid() */
#include <io.h>
#include "win32dep.h"
#endif

int state_lock = 0;

GdkPixmap *dark_icon_pm = NULL;
GdkBitmap *dark_icon_bm = NULL;

GtkWidget *all_convos = NULL;
GtkWidget *convo_notebook = NULL;
GtkWidget *convo_menubar = NULL;

char fontface[128] = { 0 };

int fontsize = 3;
extern GdkColor bgcolor;
extern GdkColor fgcolor;

void check_everything(GtkTextBuffer *buffer);
gboolean keypress_callback(GtkWidget *entry, GdkEventKey * event, struct conversation *c);
gboolean stop_rclick_callback(GtkWidget *widget, GdkEventButton *event, gpointer data);

static void update_icon(struct conversation *);
static void remove_icon(struct conversation *);

static void update_checkbox(struct conversation *);
static void remove_checkbox(struct conversation *);



/*------------------------------------------------------------------------*/
/*  Helpers                                                               */
/*------------------------------------------------------------------------*/


void gaim_setup_imhtml_smileys(GtkWidget *imhtml)
{
	/* This is ugly right now--it will get better when the themable smileys come */
	

	char *filename;
	filename = g_build_filename(DATADIR, "pixmaps", "gaim", "smileys", "default", "smile.png", NULL);
	gtk_imhtml_associate_smiley(GTK_IMHTML(imhtml), ":)", NULL, filename);
	gtk_imhtml_associate_smiley(GTK_IMHTML(imhtml), ":-)", NULL, filename);	

	filename = g_build_filename(DATADIR, "pixmaps", "gaim", "smileys", "default", "sad.png", NULL);
	gtk_imhtml_associate_smiley(GTK_IMHTML(imhtml), ":(", NULL, filename);
	gtk_imhtml_associate_smiley(GTK_IMHTML(imhtml), ":-(", NULL, filename);	
	
	filename = g_build_filename(DATADIR, "pixmaps", "gaim", "smileys", "default", "wink.png", NULL);
	gtk_imhtml_associate_smiley(GTK_IMHTML(imhtml), ";)", NULL, filename);
	gtk_imhtml_associate_smiley(GTK_IMHTML(imhtml), ";-)", NULL, filename);	
	
	filename = g_build_filename(DATADIR, "pixmaps", "gaim", "smileys", "default", "tongue.png", NULL);
	gtk_imhtml_associate_smiley(GTK_IMHTML(imhtml), ":-p", NULL, filename);
	gtk_imhtml_associate_smiley(GTK_IMHTML(imhtml), ":-P", NULL, filename);	

	filename = g_build_filename(DATADIR, "pixmaps", "gaim", "smileys", "default", "scream.png", NULL);
	gtk_imhtml_associate_smiley(GTK_IMHTML(imhtml), "=-O", NULL, filename);
	gtk_imhtml_associate_smiley(GTK_IMHTML(imhtml), "=-o", NULL, filename);			
	
	filename = g_build_filename(DATADIR, "pixmaps", "gaim", "smileys", "default", "kiss.png", NULL);
	gtk_imhtml_associate_smiley(GTK_IMHTML(imhtml), ":-*", NULL, filename);

	filename = g_build_filename(DATADIR, "pixmaps", "gaim", "smileys", "default", "yell.png", NULL);
	gtk_imhtml_associate_smiley(GTK_IMHTML(imhtml), ">:o", NULL, filename);
	gtk_imhtml_associate_smiley(GTK_IMHTML(imhtml), ">:O", NULL, filename);

	filename = g_build_filename(DATADIR, "pixmaps", "gaim", "smileys", "default", "cool.png", NULL);
	gtk_imhtml_associate_smiley(GTK_IMHTML(imhtml), "8-)", NULL, filename);
	
	filename = g_build_filename(DATADIR, "pixmaps", "gaim", "smileys", "default", "moneymouth.png", NULL);
	gtk_imhtml_associate_smiley(GTK_IMHTML(imhtml), ":-$", NULL, filename);	

	filename = g_build_filename(DATADIR, "pixmaps", "gaim", "smileys", "default", "burp.png", NULL);
	gtk_imhtml_associate_smiley(GTK_IMHTML(imhtml), ":-!", NULL, filename);		

	filename = g_build_filename(DATADIR, "pixmaps", "gaim", "smileys", "default", "embarrassed.png", NULL);
	gtk_imhtml_associate_smiley(GTK_IMHTML(imhtml), ":-[", NULL, filename);		

	filename = g_build_filename(DATADIR, "pixmaps", "gaim", "smileys", "default", "cry.png", NULL);
	gtk_imhtml_associate_smiley(GTK_IMHTML(imhtml), ":'(", NULL, filename);	

	filename = g_build_filename(DATADIR, "pixmaps", "gaim", "smileys", "default", "think.png", NULL);
	gtk_imhtml_associate_smiley(GTK_IMHTML(imhtml), ":-/", NULL, filename);
	gtk_imhtml_associate_smiley(GTK_IMHTML(imhtml), ":-\\", NULL, filename);
	
	filename = g_build_filename(DATADIR, "pixmaps", "gaim", "smileys", "default", "crossedlips.png", NULL);
	gtk_imhtml_associate_smiley(GTK_IMHTML(imhtml), ":-x", NULL, filename);
	gtk_imhtml_associate_smiley(GTK_IMHTML(imhtml), ":-X", NULL, filename); 

	filename = g_build_filename(DATADIR, "pixmaps", "gaim", "smileys", "default", "bigsmile.png", NULL);
	gtk_imhtml_associate_smiley(GTK_IMHTML(imhtml), ":-d", NULL, filename);
	gtk_imhtml_associate_smiley(GTK_IMHTML(imhtml), ":-D", NULL, filename); 	

	filename = g_build_filename(DATADIR, "pixmaps", "gaim", "smileys", "default", "angel.png", NULL);
	gtk_imhtml_associate_smiley(GTK_IMHTML(imhtml), "O:-)", NULL, filename);	


	/* "Secret" smileys */
	filename = g_build_filename(DATADIR, "pixmaps", "gaim", "smileys", "default", "luke.png", NULL);
	gtk_imhtml_associate_smiley(GTK_IMHTML(imhtml), "C:)", NULL, filename);
	gtk_imhtml_associate_smiley(GTK_IMHTML(imhtml), "C:-)", NULL, filename);
	
	filename = g_build_filename(DATADIR, "pixmaps", "gaim", "smileys", "default", "oneeye.png", NULL);
	gtk_imhtml_associate_smiley(GTK_IMHTML(imhtml), "O-)", NULL, filename);
	
	filename = g_build_filename(DATADIR, "pixmaps", "gaim", "smileys", "default", "crazy.png", NULL);
	gtk_imhtml_associate_smiley(GTK_IMHTML(imhtml), ">:)", NULL, filename);
	gtk_imhtml_associate_smiley(GTK_IMHTML(imhtml), ">:-)", NULL, filename);
	
	filename = g_build_filename(DATADIR, "pixmaps", "gaim", "smileys", "default", "mrt.png", NULL);
	gtk_imhtml_associate_smiley(GTK_IMHTML(imhtml), ":-o)))", NULL, filename);
	gtk_imhtml_associate_smiley(GTK_IMHTML(imhtml), ":-O)))", NULL, filename);
	
	filename = g_build_filename(DATADIR, "pixmaps", "gaim", "smileys", "default", "download.png", NULL);
	gtk_imhtml_associate_smiley(GTK_IMHTML(imhtml), "8-|)", NULL, filename);
	
	filename = g_build_filename(DATADIR, "pixmaps", "gaim", "smileys", "default", "farted.png", NULL);
	gtk_imhtml_associate_smiley(GTK_IMHTML(imhtml), ":-]", NULL, filename);
}

void gaim_setup_imhtml(GtkWidget *imhtml)
{
	g_return_if_fail(imhtml != NULL);
	g_return_if_fail(GTK_IS_IMHTML(imhtml));
	if (!(convo_options & OPT_CONVO_SHOW_SMILEY))
		gtk_imhtml_show_smileys(GTK_IMHTML(imhtml), FALSE);

	gtk_signal_connect(GTK_OBJECT(imhtml), "url_clicked", GTK_SIGNAL_FUNC(open_url), NULL);
	gaim_setup_imhtml_smileys(imhtml);	
}

void quiet_set(GtkWidget *tb, int state)
{
	state_lock = 1;
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(tb), state);
	state_lock = 0;
}


void set_state_lock(int i)
{
	state_lock = i;
}

void toggle_sensitive(GtkWidget *widget, GtkWidget *to_toggle)
{
	gboolean sensitivity = GTK_WIDGET_IS_SENSITIVE(GTK_WIDGET(to_toggle));

	if (sensitivity == TRUE)
		gtk_widget_set_sensitive(GTK_WIDGET(to_toggle), FALSE);
	else
		gtk_widget_set_sensitive(GTK_WIDGET(to_toggle), TRUE);

	return;
}

void set_convo_name(struct conversation *c, const char *nname)
{

	g_snprintf(c->name, sizeof(c->name), "%s", nname);

	return;
}

struct conversation *new_conversation(char *name)
{
	struct conversation *c;

	c = find_conversation(name);

	if (c != NULL)
		return c;

	c = (struct conversation *)g_new0(struct conversation, 1);
	g_snprintf(c->name, sizeof(c->name), "%s", name);

	if ((logging_options & OPT_LOG_CONVOS) || find_log_info(c->name)) {
		FILE *fd;

		fd = open_log_file(c->name, c->is_chat);
		if (fd) {
			if (!(logging_options & OPT_LOG_STRIP_HTML))
				fprintf(fd,
					"<HR><BR><H3 Align=Center> ---- New Conversation @ %s ----</H3><BR>\n",
					full_date());
			else
				fprintf(fd, " ---- New Conversation @ %s ----\n", full_date());
			fclose(fd);
		} else
			/* do we want to do something here? */ ;
	}

	if (connections)
		c->gc = (struct gaim_connection *)connections->data;
	c->history = g_string_new("");
	c->send_history = g_list_append(NULL, NULL);
	conversations = g_list_append(conversations, c);
	show_conv(c);
	update_icon(c);
	update_checkbox(c);
	plugin_event(event_new_conversation, name);
	/*gtk_imhtml_to_bottom(c->text);*/
	return c;
}


struct conversation *find_conversation(const char *name)
{
	char *cuser = g_malloc(1024);
	struct conversation *c;
	GList *cnv = conversations;

	strcpy(cuser, normalize(name));

	while (cnv) {
		c = (struct conversation *)cnv->data;
		if (!g_strcasecmp(cuser, normalize(c->name))) {
			g_free(cuser);
			return c;
		}
		cnv = cnv->next;
	}
	g_free(cuser);
	return NULL;
}

/* ---------------------------------------------------
 * Function to remove a log file entry
 * ---------------------------------------------------
 */

void rm_log(struct log_conversation *a)
{
	struct conversation *cnv = find_conversation(a->name);

	log_conversations = g_list_remove(log_conversations, a);

	save_prefs();

	if (cnv && !(im_options & OPT_IM_ONE_WINDOW))
		set_convo_title(cnv);
}

struct log_conversation *find_log_info(char *name)
{
	char *pname = g_malloc(1024);
	GList *lc = log_conversations;
	struct log_conversation *l;


	strcpy(pname, normalize(name));

	while (lc) {
		l = (struct log_conversation *)lc->data;
		if (!g_strcasecmp(pname, normalize(l->name))) {
			g_free(pname);
			return l;
		}
		lc = lc->next;
	}
	g_free(pname);
	return NULL;
}

void delete_conversation(struct conversation *c)
{
	plugin_event(event_del_conversation, c);
	conversations = g_list_remove(conversations, c);
	if (c->fg_color_dialog)
		gtk_widget_destroy(c->fg_color_dialog);
	if (c->bg_color_dialog)
		gtk_widget_destroy(c->bg_color_dialog);
	if (c->font_dialog)
		gtk_widget_destroy(c->font_dialog);
	if (c->smiley_dialog)
		gtk_widget_destroy(c->smiley_dialog);
	if (c->link_dialog)
		gtk_widget_destroy(c->link_dialog);
	if (c->log_dialog)
		gtk_widget_destroy(c->log_dialog);
	if (c->save_icon)
		gtk_widget_destroy(c->save_icon);
	c->send_history = g_list_first(c->send_history);
	while (c->send_history) {
		if (c->send_history->data)
			g_free(c->send_history->data);
		c->send_history = c->send_history->next;
	}
	g_list_free(c->send_history);
	if (c->typing_timeout)
		gtk_timeout_remove(c->typing_timeout);
	if (c->type_again_timeout)
		gtk_timeout_remove(c->type_again_timeout);
	g_string_free(c->history, TRUE);
	g_free(c);
}

void update_log_convs()
{
	GSList *C = connections;
	struct gaim_connection *g;
	GSList *bcs;
	GList *cnv = conversations;
	struct conversation *c;

	while (cnv) {
		c = (struct conversation *)cnv->data;
		if (c->log_button) {
			if (c->is_chat)
				gtk_widget_set_sensitive(GTK_WIDGET(c->log_button),
						   ((logging_options & OPT_LOG_CHATS)) ? FALSE : TRUE);
			else
				gtk_widget_set_sensitive(GTK_WIDGET(c->log_button),
							 ((logging_options & OPT_LOG_CONVOS)) ? FALSE : TRUE);
		}

		cnv = cnv->next;
	}

	while (C) {
		g = (struct gaim_connection *)C->data;
		bcs = g->buddy_chats;
		while (bcs) {
			c = (struct conversation *)bcs->data;
			if (c->log_button) {
				if (c->is_chat)
					gtk_widget_set_sensitive(GTK_WIDGET(c->log_button),
					 		   ((logging_options & OPT_LOG_CHATS)) ? FALSE :
							   TRUE);
				else
					gtk_widget_set_sensitive(GTK_WIDGET(c->log_button),
								 ((logging_options & OPT_LOG_CONVOS)) ? FALSE :
								 TRUE);
			}

			bcs = bcs->next;
		}
		C = C->next;
	}
}

void update_font_buttons()
{
	GList *cnv = conversations;
	struct conversation *c;

	while (cnv) {
		c = (struct conversation *)cnv->data;

		if (c->bold)
			gtk_widget_set_sensitive(c->bold,
						 ((font_options & OPT_FONT_BOLD)) ? FALSE : TRUE);

		if (c->italic)
			gtk_widget_set_sensitive(c->italic,
						 ((font_options & OPT_FONT_ITALIC)) ? FALSE : TRUE);

		if (c->underline)
			gtk_widget_set_sensitive(c->underline,
						 ((font_options & OPT_FONT_UNDERLINE)) ? FALSE : TRUE);

		cnv = cnv->next;
	}
}

/*
void update_transparency()
{
	GList *cnv = conversations;
	struct conversation *c;

        This func should be uncalled!

	while(cnv) {
		c = (struct conversation *)cnv->data;

		if (c->text)
			gtk_html_set_transparent(GTK_HTML(c->text),
        						  (transparent) ? TRUE : FALSE);

		cnv = cnv->next;
	}
}
*/


/*------------------------------------------------------------------------*/
/*  Callbacks                                                             */
/*------------------------------------------------------------------------*/

void toggle_loggle(GtkWidget *loggle, struct conversation *c)
{
	if (state_lock)
		return;

	if (find_log_info(c->name))
		rm_log(find_log_info(c->name));
	else if (GTK_CHECK_MENU_ITEM(c->log_button)->active)
		show_log_dialog(c);
	else
		cancel_log(NULL, c);
}

void toggle_sound(GtkWidget *widget, struct conversation *c)
{
	if (state_lock)
		return;

	c->makesound = !c->makesound;
}

static void do_save_convo(GtkObject *obj, GtkWidget *wid)
{
	struct conversation *c = gtk_object_get_user_data(obj);
	const char *filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(wid));
	FILE *f;
	if (file_is_dir(filename, wid))
		return;
	if (!((!c->is_chat && g_list_find(conversations, c)) ||
	      (c->is_chat && g_slist_find(connections, c->gc) && g_slist_find(c->gc->buddy_chats, c))))
 		filename = NULL;
	gtk_widget_destroy(wid);
	if (!filename)
		return;
	f = fopen(filename, "w+");
	if (!f)
		return;
	fprintf(f, "%s", c->history->str);
	fclose(f);
}

void save_convo(GtkWidget *save, struct conversation *c)
{
	char buf[BUF_LONG];
	GtkWidget *window = gtk_file_selection_new(_("Gaim - Save Conversation"));
	g_snprintf(buf, sizeof(buf), "%s" G_DIR_SEPARATOR_S "%s.log", gaim_home_dir(), normalize(c->name));
	gtk_file_selection_set_filename(GTK_FILE_SELECTION(window), buf);
	gtk_object_set_user_data(GTK_OBJECT(GTK_FILE_SELECTION(window)->ok_button), c);
	gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(window)->ok_button),
			   "clicked", GTK_SIGNAL_FUNC(do_save_convo), window);
	gtk_signal_connect_object(GTK_OBJECT(GTK_FILE_SELECTION(window)->cancel_button),
				  "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), (gpointer)window);
	gtk_widget_show(window);
}

static void do_insert_image(GtkObject *obj, GtkWidget *wid)
{
	struct conversation *c = gtk_object_get_user_data(obj);
	const char *name = gtk_file_selection_get_filename(GTK_FILE_SELECTION(wid));
	const char *filename;
	char *buf;
	struct stat st;
	int id = g_slist_length(c->images) + 1;
	
	if (file_is_dir(name, wid))
		return;
	if (!(!c->is_chat && g_list_find(conversations, c)))
		name = NULL;
	gtk_widget_destroy(wid);
	if (!name)
		return;

	if (stat(name, &st) != 0) {
		debug_printf("Could not stat %s\n", name);
		return;
	}
	
	filename = name;
	while (strchr(filename, '/')) 
		filename = strchr(filename, '/') + 1;
		
	buf = g_strdup_printf ("<IMG SRC=\"file://%s\" ID=\"%d\" DATASIZE=\"%d\">",
			       filename, id, (int)st.st_size);
	c->images = g_slist_append(c->images, g_strdup(name));
	gtk_text_buffer_insert_at_cursor(GTK_TEXT_BUFFER(c->entry_buffer),
					 buf, -1);
	g_free(buf);
}

void insert_image(GtkWidget *save, struct conversation *c)
{
	char buf[BUF_LONG];
	GtkWidget *window = gtk_file_selection_new(_("Gaim - Insert Image"));
	g_snprintf(buf, sizeof(buf), "%s" G_DIR_SEPARATOR_S, gaim_home_dir());
	gtk_file_selection_set_filename(GTK_FILE_SELECTION(window), buf);
	gtk_object_set_user_data(GTK_OBJECT(GTK_FILE_SELECTION(window)->ok_button), c);
	gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(window)->ok_button),
			   "clicked", GTK_SIGNAL_FUNC(do_insert_image), window);
	gtk_signal_connect_object(GTK_OBJECT(GTK_FILE_SELECTION(window)->cancel_button),
				  "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), (gpointer)window);
	gtk_widget_show(window);
}


void insert_smiley(GtkWidget *smiley, struct conversation *c)
{
	if (state_lock)
		return;

	if (GTK_TOGGLE_BUTTON(smiley)->active)
		show_smiley_dialog(c, smiley);
	else if (c->smiley_dialog)
		close_smiley_dialog(smiley, c);

	gtk_widget_grab_focus(c->entry);

	return;
}

int close_callback(GtkWidget *widget, struct conversation *c)
{
	if (c->is_chat && (widget == c->close) && !(chat_options & OPT_CHAT_ONE_WINDOW)) {
		GtkWidget *tmp = c->window;
		debug_printf("chat clicked close button\n");
		c->window = NULL;
		gtk_widget_destroy(tmp);
		return FALSE;
	}

	debug_printf("conversation close callback\n");

	if (!c->is_chat) {
		GSList *cn = connections;
		if (!(misc_options & OPT_MISC_STEALTH_TYPING))
			serv_send_typing(c->gc, c->name, NOT_TYPING);
		while (cn) {
			struct gaim_connection *gc = cn->data;
			cn = cn->next;
			if (gc->prpl->convo_closed)
				gc->prpl->convo_closed(gc, c->name);
		}
		remove_icon(c);
		remove_checkbox(c);
		if (im_options & OPT_IM_ONE_WINDOW) {
			if ((g_list_length(conversations) > 1) ||
					((convo_options & OPT_CONVO_COMBINE) &&
					 (chat_options & OPT_CHAT_ONE_WINDOW) && chats)) {
				gtk_notebook_remove_page(GTK_NOTEBOOK(convo_notebook),
							 g_list_index(conversations, c));
			} else {
				if (c->window)
					gtk_widget_destroy(c->window);
				c->window = NULL;
				all_convos = NULL;
				convo_notebook = NULL;
				if (convo_options & OPT_CONVO_COMBINE) {
					all_chats = NULL;
					chat_notebook = NULL;
				}
			}
		} else {
			if (c->window)
				gtk_widget_destroy(c->window);
			c->window = NULL;
		}
	} else {
		if (chat_options & OPT_CHAT_ONE_WINDOW) {
			if ((convo_options & OPT_CONVO_COMBINE) && (im_options & OPT_IM_ONE_WINDOW)
					&& (conversations || chats->next)) {
				gtk_notebook_remove_page(GTK_NOTEBOOK(chat_notebook),
							 g_list_index(chats, c) +
								g_list_length(conversations));
			} else if (g_list_length(chats) > 1) {
				gtk_notebook_remove_page(GTK_NOTEBOOK(chat_notebook),
							 g_list_index(chats, c));
			} else {
				if (c->window)
					gtk_widget_destroy(c->window);
				c->window = NULL;
				all_chats = NULL;
				chat_notebook = NULL;
				if (convo_options & OPT_CONVO_COMBINE) {
					all_convos = NULL;
					convo_notebook = NULL;
				}
			}
		} else {
			if (c->window)
				gtk_widget_destroy(c->window);
			c->window = NULL;
		}
	}

	if (c->fg_color_dialog)
		gtk_widget_destroy(c->fg_color_dialog);
	c->fg_color_dialog = NULL;
	if (c->bg_color_dialog)
		gtk_widget_destroy(c->bg_color_dialog);
	c->bg_color_dialog = NULL;
	if (c->font_dialog)
		gtk_widget_destroy(c->font_dialog);
	c->font_dialog = NULL;
	if (c->smiley_dialog)
		gtk_widget_destroy(c->smiley_dialog);
	c->smiley_dialog = NULL;
	if (c->link_dialog)
		gtk_widget_destroy(c->link_dialog);
	c->link_dialog = NULL;
	if (c->log_dialog)
		gtk_widget_destroy(c->log_dialog);
	c->log_dialog = NULL;

	if (c->is_chat) {
		chats = g_list_remove(chats, c);
		if (c->gc)
			serv_chat_leave(c->gc, c->id);
		else
			delete_chat(c);
	} else {
		delete_conversation(c);
	}

	return TRUE;
}

void set_font_face(char *newfont, struct conversation *c)
{
	char *pre_fontface;
	
	sprintf(c->fontface, "%s", newfont && *newfont ? newfont : DEFAULT_FONT_FACE);
	c->hasfont = 1;

	pre_fontface = g_strconcat("<FONT FACE=\"", c->fontface, "\">", NULL);
	surround(c, pre_fontface, "</FONT>");
	gtk_widget_grab_focus(c->entry);
	g_free(pre_fontface);
}

gint delete_all_convo(GtkWidget *w, GdkEventAny *e, gpointer d)
{
	if (w == all_convos) {
		while (conversations) {
			struct conversation *c = conversations->data;
			close_callback(c->close, c);
		}
	}
	if (w == all_chats) {
		while (chats) {
			struct conversation *c = chats->data;
			close_callback(c->close, c);
		}
	}
	return FALSE;
}

static gint delete_event_convo(GtkWidget *w, GdkEventAny *e, struct conversation *c)
{
	delete_conversation(c);
	return FALSE;
}

void add_callback(GtkWidget *widget, struct conversation *c)
{
	struct buddy *b = find_buddy(c->gc, c->name);
	if (b) {
		show_confirm_del(c->gc, c->name);
	} else if (c->gc)
		show_add_buddy(c->gc, c->name, NULL, NULL);

	gtk_widget_grab_focus(c->entry);
}


void block_callback(GtkWidget *widget, struct conversation *c)
{
	if (c->gc)
		show_add_perm(c->gc, c->name, FALSE);
	gtk_widget_grab_focus(c->entry);
}

void warn_callback(GtkWidget *widget, struct conversation *c)
{
	show_warn_dialog(c->gc, c->name);
	gtk_widget_grab_focus(c->entry);
}

void info_callback(GtkWidget *w, struct conversation *c)
{
	if (c->is_chat) {
		GtkTreeIter iter;
		GtkTreeModel *mod = gtk_tree_view_get_model(GTK_TREE_VIEW(c->list));
		GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(c->list));
		gchar *name;

		if (gtk_tree_selection_get_selected(sel, NULL, &iter)) {
			gtk_tree_model_get(GTK_TREE_MODEL(mod), &iter, 1, &name, -1);
		} else {
			return;
		}

		serv_get_info(c->gc, name);

	} else {
		serv_get_info(c->gc, c->name);
		gtk_widget_grab_focus(c->entry);
	}
}

static void move_next_tab(GtkNotebook *notebook, gboolean chat)
{
	int currpage = gtk_notebook_get_current_page(notebook);
	int convlen;
	GList *cnv;
	struct conversation *d = NULL;

	if ((convo_options & OPT_CONVO_COMBINE) &&
	    (im_options & OPT_IM_ONE_WINDOW) &&
	    (chat_options & OPT_CHAT_ONE_WINDOW))
		convlen = g_list_length(conversations);
	else
		convlen = 0;

	if (chat) {
		/* if chat, find next unread chat */
		cnv = g_list_nth(chats, currpage - convlen);
		while (cnv) {
			d = cnv->data;
			if (d->unseen > 0)
				break;
			cnv = cnv->next;
			d = NULL;
		}
		if (d) {
			gtk_notebook_set_page(notebook, convlen + g_list_index(chats, d));
			return;
		}
	} else {
		/* else find next unread convo */
		cnv = g_list_nth(conversations, currpage);
		while (cnv) {
			d = cnv->data;
			if (d->unseen > 0)
				break;
			cnv = cnv->next;
			d = NULL;
		}
		if (d) {
			gtk_notebook_set_page(notebook, g_list_index(conversations, d));
			return;
		}
	}

	if (convo_options & OPT_CONVO_COMBINE) {
		if (chat && (im_options & OPT_IM_ONE_WINDOW)) {
			/* if chat find next unread convo */
			cnv = conversations;
			while (cnv) {
				d = cnv->data;
				if (d->unseen > 0)
					break;
				cnv = cnv->next;
				d = NULL;
			}
			if (d) {
				gtk_notebook_set_page(notebook, g_list_index(conversations, d));
				return;
			}
		} else if (!chat && (chat_options & OPT_CHAT_ONE_WINDOW)) {
			/* else find next unread chat */
			cnv = chats;
			while (cnv) {
				d = cnv->data;
				if (d->unseen > 0)
					break;
				cnv = cnv->next;
				d = NULL;
			}
			if (d) {
				gtk_notebook_set_page(notebook, convlen + g_list_index(chats, d));
				return;
			}
		}
	}

	if (chat) {
		/* if chat find first unread chat */
		cnv = chats;
		while (cnv) {
			d = cnv->data;
			if (d->unseen > 0)
				break;
			cnv = cnv->next;
			d = NULL;
		}
		if (d) {
			gtk_notebook_set_page(notebook, convlen + g_list_index(chats, d));
			return;
		}
	} else {
		/* else find first unread convo */
		cnv = conversations;
		while (cnv) {
			d = cnv->data;
			if (d->unseen > 0)
				break;
			cnv = cnv->next;
			d = NULL;
		}
		if (d) {
			gtk_notebook_set_page(notebook, g_list_index(conversations, d));
			return;
		}
	}

	/* go to next page */
	if (currpage + 1 == g_list_length(notebook->children))
		gtk_notebook_set_page(notebook, 0);
	else
		gtk_notebook_next_page(notebook);
}

#define SEND_TYPED_TIMEOUT 5000

gboolean send_typed(gpointer data)
{
	struct conversation *c = (struct conversation*)data;
	if (c && c->gc && c->name) {
		c->type_again = 1;
		serv_send_typing(c->gc, c->name, TYPED);
		debug_printf("typed...\n");
	}
	return FALSE;
}

gboolean keypress_callback(GtkWidget *entry, GdkEventKey *event, struct conversation *c)
{
	if (event->keyval == GDK_Escape) {
		if (convo_options & OPT_CONVO_ESC_CAN_CLOSE) {
			gtk_signal_emit_stop_by_name(GTK_OBJECT(entry), "key_press_event");
			close_callback(c->close, c);
			c = NULL;
		}
	} else if (event->keyval == GDK_Page_Up) {
		gtk_signal_emit_stop_by_name(GTK_OBJECT(entry), "key_press_event");
		if(!(event->state & GDK_CONTROL_MASK))
			gtk_imhtml_page_up(GTK_IMHTML(c->text));
	} else if (event->keyval == GDK_Page_Down) {
		gtk_signal_emit_stop_by_name(GTK_OBJECT(entry), "key_press_event");
		if(!(event->state & GDK_CONTROL_MASK))
			gtk_imhtml_page_down(GTK_IMHTML(c->text));
	} else if ((event->keyval == GDK_F2) && (convo_options & OPT_CONVO_F2_TOGGLES)) {
		gtk_imhtml_show_comments(GTK_IMHTML(c->text), !GTK_IMHTML(c->text)->comments);
	} else if ((event->keyval == GDK_Return) || (event->keyval == GDK_KP_Enter)) {
		if ((event->state & GDK_CONTROL_MASK) && (convo_options & OPT_CONVO_CTL_ENTER)) {
			send_callback(NULL, c);
			gtk_signal_emit_stop_by_name(GTK_OBJECT(entry), "key_press_event");
			return TRUE;
		} else if (!(event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK)) && (convo_options & OPT_CONVO_ENTER_SENDS)) {
			send_callback(NULL, c);
			gtk_signal_emit_stop_by_name(GTK_OBJECT(entry), "key_press_event");
			return TRUE;
		} else {
			return FALSE;
		}
	} else if ((event->state & GDK_CONTROL_MASK) && (event->keyval == 'm')) {
		gtk_signal_emit_stop_by_name(GTK_OBJECT(entry), "key_press_event");
		gtk_text_buffer_insert_at_cursor(c->entry_buffer, "\n", 1);
	} else if (event->state & GDK_CONTROL_MASK) {
		switch (event->keyval) {
		case GDK_Up:
			if (!c->send_history)
				break;
			if (!c->send_history->prev) {
				GtkTextIter start, end;
				if (c->send_history->data)
					g_free(c->send_history->data);
				gtk_text_buffer_get_start_iter(c->entry_buffer, &start);
				gtk_text_buffer_get_end_iter(c->entry_buffer, &end);
				c->send_history->data = gtk_text_buffer_get_text(c->entry_buffer,
										 &start, &end, FALSE);
			} 
			if (c->send_history->next && c->send_history->next->data) {
				c->send_history = c->send_history->next;
				gtk_text_buffer_set_text(c->entry_buffer, c->send_history->data, -1);
			}
			
			break;
		case GDK_Down:
		  if (!c->send_history) 
				break;
			if (c->send_history->prev) {
			  c->send_history = c->send_history->prev;
				if (c->send_history->data) {
					gtk_text_buffer_set_text(c->entry_buffer, c->send_history->data, -1);
				}
			}
			break;
		}
		if (convo_options & OPT_CONVO_CTL_CHARS) {
			switch (event->keyval) {
			case 'i':
			case 'I':
				quiet_set(c->italic,
					  !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(c->italic)));
				do_italic(c->italic, c);
				gtk_signal_emit_stop_by_name(GTK_OBJECT(entry), "key_press_event");
				break;
			case 'u':	/* ctl-u is GDK_Clear, which clears the line */
			case 'U':
				quiet_set(c->underline,
					  !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON
									(c->underline)));
				do_underline(c->underline, c);
				gtk_signal_emit_stop_by_name(GTK_OBJECT(entry), "key_press_event");
				break;
			case 'b':	/* ctl-b is GDK_Left, which moves backwards */
			case 'B':
				quiet_set(c->bold,
					  !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(c->bold)));
				do_bold(c->bold, c);
				gtk_signal_emit_stop_by_name(GTK_OBJECT(entry), "key_press_event");
				break;
			case '-':
				do_small(NULL, c);
				gtk_signal_emit_stop_by_name(GTK_OBJECT(entry), "key_press_event");
				break;
			case '=':
			case '+':
				do_big(NULL, c);
				gtk_signal_emit_stop_by_name(GTK_OBJECT(entry), "key_press_event");
				break;	
			case '0':
				do_normal(NULL, c);
				gtk_signal_emit_stop_by_name(GTK_OBJECT(entry), "key_press_event");
				break;
			case 'f':
			case 'F':
				quiet_set(c->font,
					  !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(c->font)));
				toggle_font(c->font, c);
				gtk_signal_emit_stop_by_name(GTK_OBJECT(entry), "key_press_event");
				break;
			}
		}
		if (convo_options & OPT_CONVO_CTL_SMILEYS) {
			char buf[7];
			buf[0] = '\0';
			switch (event->keyval) {
			case '1':
				sprintf(buf, ":-)");
				break;
			case '2':
				sprintf(buf, ":-(");
				break;
			case '3':
				sprintf(buf, ";-)");
				break;
			case '4':
				sprintf(buf, ":-P");
				break;
			case '5':
				sprintf(buf, "=-O");
				break;
			case '6':
				sprintf(buf, ":-*");
				break;
			case '7':
				sprintf(buf, ">:o");
				break;
			case '8':
				sprintf(buf, "8-)");
				break;
			case '!':
				sprintf(buf, ":-$");
				break;
			case '@':
				sprintf(buf, ":-!");
				break;
			case '#':
				sprintf(buf, ":-[");
				break;
			case '$':
				sprintf(buf, "O:-)");
				break;
			case '%':
				sprintf(buf, ":-/");
				break;
			case '^':
				sprintf(buf, ":'(");
				break;
			case '&':
				sprintf(buf, ":-X");
				break;
			case '*':
				sprintf(buf, ":-D");
				break;
			}
			if (buf[0]) {
				gtk_text_buffer_insert_at_cursor(c->entry_buffer, buf, -1);
				gtk_signal_emit_stop_by_name(GTK_OBJECT(entry), "key_press_event");
			}
		}
		if (event->keyval == 'l') {
			gtk_imhtml_clear(GTK_IMHTML(c->text));
			g_string_free(c->history, TRUE);
			c->history = g_string_new("");
		} else if ((event->keyval == 'w') && (convo_options & OPT_CONVO_CTL_W_CLOSES)) {
			gtk_signal_emit_stop_by_name(GTK_OBJECT(entry), "key_press_event");
			close_callback(c->close, c);
			c = NULL;
			return TRUE;
		} else if (event->keyval == 'n') {
			gtk_signal_emit_stop_by_name(GTK_OBJECT(entry), "key_press_event");
			show_im_dialog();
		} else if (event->keyval == 'z') {
			gtk_signal_emit_stop_by_name(GTK_OBJECT(entry), "key_press_event");
#ifndef _WIN32
			XIconifyWindow(GDK_DISPLAY(),
				       GDK_WINDOW_XWINDOW(c->window->window),
				       ((_XPrivDisplay)GDK_DISPLAY())->default_screen);		     
#endif
		}
		

		if ((!c->is_chat && (im_options & OPT_IM_ONE_WINDOW)) ||
		    (c->is_chat && (chat_options & OPT_CHAT_ONE_WINDOW))) {
			GtkWidget *notebook = (c->is_chat ? chat_notebook : convo_notebook);
			if (event->keyval == '[') {
				gtk_notebook_prev_page(GTK_NOTEBOOK(notebook));
				gtk_signal_emit_stop_by_name(GTK_OBJECT(entry), "key_press_event");
			} else if (event->keyval == ']') {
				gtk_notebook_next_page(GTK_NOTEBOOK(notebook));
				gtk_signal_emit_stop_by_name(GTK_OBJECT(entry), "key_press_event");
			} else if (event->keyval == GDK_Tab) {
				move_next_tab(GTK_NOTEBOOK(notebook), c->is_chat);
				gtk_signal_emit_stop_by_name(GTK_OBJECT(entry), "key_press_event");
				return TRUE;
			}
		}
	} else if ((event->keyval == GDK_Tab) && c->is_chat && (chat_options & OPT_CHAT_TAB_COMPLETE)) {
	        tab_complete(c);
		gtk_signal_emit_stop_by_name(GTK_OBJECT(entry), "key_press_event");
		return TRUE;
	} else if (((!c->is_chat && (im_options & OPT_IM_ONE_WINDOW)) ||
		    (c->is_chat && (chat_options & OPT_CHAT_ONE_WINDOW))) &&
		   (event->state & GDK_MOD1_MASK) && (event->keyval > '0') && (event->keyval <= '9')) {
		GtkWidget *notebook = NULL;
		notebook = (c->is_chat ? chat_notebook : convo_notebook);
		gtk_notebook_set_page(GTK_NOTEBOOK(notebook), event->keyval - '1');
		gtk_signal_emit_stop_by_name(GTK_OBJECT(entry), "key_press_event");
	}

	return FALSE;
}

/* This guy just kills a single right click from being propagated any 
 * further.  I have no idea *why* we need this, but we do ...  It 
 * prevents right clicks on the GtkTextView in a convo dialog from
 * going all the way down to the notebook.  I suspect a bug in 
 * GtkTextView, but I'm not ready to point any fingers yet. */
gboolean stop_rclick_callback(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	if (event->button == 3 && event->type == GDK_BUTTON_PRESS) {
		/* right single click */
		g_signal_stop_emission_by_name(G_OBJECT(widget), "button_press_event");
		return TRUE;
	}
	return FALSE;
}

static void got_typing_keypress(struct conversation *c, gboolean first) {
	/* we know we got something, so we at least have to make sure we don't send
	 * TYPED any time soon */
	if(c->type_again_timeout)
		gtk_timeout_remove(c->type_again_timeout);
	c->type_again_timeout = gtk_timeout_add(SEND_TYPED_TIMEOUT, send_typed, c);

	/* we send typed if this is the first character typed, or if we're due
	 * to send another one */
	if(first || (c->type_again != 0 && time(NULL) > c->type_again)) {
		int timeout = serv_send_typing(c->gc, c->name, TYPING);
		if(timeout)
			c->type_again = time(NULL) + timeout;
		else
			c->type_again = 0;
	}
}

void delete_text_callback(GtkTextBuffer *textbuffer, GtkTextIter *start_pos, GtkTextIter *end_pos, gpointer user_data) {
	struct conversation *c = user_data;

	if(!c)
		return;

	if (misc_options & OPT_MISC_STEALTH_TYPING)
		return;

	if(gtk_text_iter_is_start(start_pos) && gtk_text_iter_is_end(end_pos)) {
		if(c->type_again_timeout)
			gtk_timeout_remove(c->type_again_timeout);
		serv_send_typing(c->gc, c->name, NOT_TYPING);
	} else {
		/* we're deleting, but not all of it, so it counts as typing */
		got_typing_keypress(c, FALSE);
	}
}

void insert_text_callback(GtkTextBuffer *textbuffer, GtkTextIter *position, gchar *new_text, gint new_text_length, gpointer user_data) {
	struct conversation *c = user_data;

	if(!c)
		return;

	if (misc_options & OPT_MISC_STEALTH_TYPING)
		return;

	got_typing_keypress(c, (gtk_text_iter_is_start(position) && gtk_text_iter_is_end(position)));
}

void send_callback(GtkWidget *widget, struct conversation *c)
{
	char *buf, *buf2;
	int limit;
	gulong length=0;
	int err = 0;
	GList *first;
	GtkTextIter start_iter, end_iter;

	if (!c->gc)
		return;

	gtk_text_buffer_get_start_iter(c->entry_buffer, &start_iter);
	gtk_text_buffer_get_end_iter(c->entry_buffer, &end_iter);
	buf2 = gtk_text_buffer_get_text(c->entry_buffer, &start_iter, &end_iter, FALSE);
	limit = 32 * 1024;	/* you shouldn't be sending more than 32k in your messages. that's a book. */
	buf = g_malloc(limit);
	g_snprintf(buf, limit, "%s", buf2);
	g_free(buf2);
	if (!strlen(buf)) {
		g_free(buf);
		return;
	}

	first = g_list_first(c->send_history);
	if (first->data)
		g_free(first->data);
	first->data = g_strdup(buf);
	c->send_history = g_list_prepend(first, NULL);
	
	buf2 = g_malloc(limit);

	if (c->gc->flags & OPT_CONN_HTML) {
		if (convo_options & OPT_CONVO_SEND_LINKS)
			linkify_text(buf);

		if (font_options & OPT_FONT_BOLD) {
			g_snprintf(buf2, limit, "<B>%s</B>", buf);
			strcpy(buf, buf2);
		}

		if (font_options & OPT_FONT_ITALIC) {
			g_snprintf(buf2, limit, "<I>%s</I>", buf);
			strcpy(buf, buf2);
		}

		if (font_options & OPT_FONT_UNDERLINE) {
			g_snprintf(buf2, limit, "<U>%s</U>", buf);
			strcpy(buf, buf2);
		}

		if (font_options & OPT_FONT_STRIKE) {
			g_snprintf(buf2, limit, "<STRIKE>%s</STRIKE>", buf);
			strcpy(buf, buf2);
		}

		if ((font_options & OPT_FONT_FACE) || c->hasfont) {
			g_snprintf(buf2, limit, "<FONT FACE=\"%s\">%s</FONT>", c->fontface, buf);
			strcpy(buf, buf2);
		}

		if (font_options & OPT_FONT_SIZE) {
			g_snprintf(buf2, limit, "<FONT SIZE=\"%d\">%s</FONT>", fontsize, buf);
			strcpy(buf, buf2);
		}

		if ((font_options & OPT_FONT_FGCOL) || c->hasfg) {
			g_snprintf(buf2, limit, "<FONT COLOR=\"#%02X%02X%02X\">%s</FONT>", c->fgcol.red,
				   c->fgcol.green, c->fgcol.blue, buf);
			strcpy(buf, buf2);
		}

		if ((font_options & OPT_FONT_BGCOL) || c->hasbg) {
			g_snprintf(buf2, limit, "<BODY BGCOLOR=\"#%02X%02X%02X\">%s</BODY>",
				   c->bgcol.red, c->bgcol.green, c->bgcol.blue, buf);
			strcpy(buf, buf2);
		}
	}

	quiet_set(c->bold, FALSE);
	quiet_set(c->italic, FALSE);
	quiet_set(c->underline, FALSE);
	quiet_set(c->font, FALSE);
	quiet_set(c->fgcolorbtn, FALSE);
	quiet_set(c->bgcolorbtn, FALSE);
	quiet_set(c->link, FALSE);
	gtk_widget_grab_focus(c->entry);

	{
		char *buffy = g_strdup(buf);
		enum gaim_event evnt = c->is_chat ? event_chat_send : event_im_send;
		int plugin_return = plugin_event(evnt, c->gc,
						 c->is_chat ? (void *)c->id : c->name,
						 &buffy);
		if (!buffy) {
			g_free(buf2);
			g_free(buf);
			return;
		}
		if (plugin_return) {
			gtk_text_buffer_set_text(c->entry_buffer, "", -1);
			g_free(buffy);
			g_free(buf2);
			g_free(buf);
			return;
		}
		g_snprintf(buf, limit, "%s", buffy);
		g_free(buffy);
	}
	
	if (!c->is_chat) {
		char *buffy;
		gboolean binary = FALSE;

		buffy = g_strdup(buf);
		plugin_event(event_im_displayed_sent, c->gc, c->name, &buffy);
		if (buffy) {
			int imflags = 0;
			if (c->check && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(c->check)))
				imflags = IM_FLAG_CHECKBOX;
			
			if (c->images) {
				int id = 0, offset = 0;
				char *bigbuf = NULL;
				GSList *tmplist = c->images;
				id = 1;
				
				while (tmplist) {
					FILE *imgfile;
					char *filename;
					struct stat st;
					char imgtag[1024];
	
					if (stat(tmplist->data, &st) != 0) {
						debug_printf("Could not stat %s\n", tmplist->data);
						tmplist = tmplist->next;
						continue;
					}
								
					/* Here we check to make sure the user still wants to send the
					 * image.  He may have deleted the <img> tag in which case we
					 * don't want to send the binary data. */
					filename = tmplist->data;
					while (strchr(filename, '/')) 
						filename = strchr(filename, '/') + 1;
					g_snprintf(imgtag, sizeof(imgtag),
						   "<IMG SRC=\"file://%s\" ID=\"%d\" DATASIZE=\"%d\">",
						   filename, id, (int)st.st_size);
				       	
					if (strstr(buffy, imgtag) == 0) {
						debug_printf("Not sending image: %s\n", tmplist->data);
						tmplist = tmplist->next;
						continue;
					}
					if (!binary) {
						length = strlen(buffy) + strlen("<BINARY></BINARY>");
						bigbuf = g_malloc(length + 1);
						g_snprintf(bigbuf, strlen(buffy) + strlen("<BINARY> ") + 1,
							   "%s<BINARY>", buffy);
						offset = strlen(buffy) + strlen("<BINARY>");
						binary = TRUE;
					}
					g_snprintf(imgtag, sizeof(imgtag),
						   "<DATA ID=\"%d\" SIZE=\"%d\">",
						   id, (int)st.st_size);
					
					length = length + strlen(imgtag) + st.st_size + strlen("</DATA>");;
					bigbuf = g_realloc(bigbuf, length + 1);
					if (!(imgfile = fopen(tmplist->data, "r"))) {
						debug_printf("Could not open %s\n", tmplist->data);
						tmplist = tmplist->next;
						continue;
					}
					g_snprintf(bigbuf + offset, strlen(imgtag) + 1, "%s", imgtag);
					offset = offset + strlen(imgtag);
					offset = offset + fread(bigbuf + offset, 1, st.st_size, imgfile);
					fclose(imgfile);
					g_snprintf(bigbuf + offset, strlen("</DATA>") + 1, "</DATA>");
					offset= offset + strlen("</DATA>");
					id++;
					tmplist = tmplist->next;
				}
				if (binary) {
					g_snprintf(bigbuf + offset, strlen("</BINARY>") + 1, "</BINARY>"); 
					err =serv_send_im(c->gc, c->name, bigbuf, length, imflags);
				} else {
					err = serv_send_im(c->gc, c->name, buffy, -1, imflags);
				}					
				if (err > 0) {
					GSList *tempy = c->images;
					while (tempy) {
						g_free(tempy->data);
						tempy = tempy->next;
					}
					g_slist_free(tempy);
					c->images = NULL;
					if (binary)
						write_to_conv(c, bigbuf, WFLAG_SEND, NULL, time(NULL), length);
					else
						write_to_conv(c, buffy, WFLAG_SEND, NULL, time(NULL), -1);
					if (c->makesound)
						play_sound(SND_SEND);
					if (im_options & OPT_IM_POPDOWN)
						gtk_widget_hide(c->window);
					
					
				}
				if (binary)
					g_free(bigbuf);
			} else {
				err =serv_send_im(c->gc, c->name, buffy, -1, imflags);
				if (err > 0) { 
					write_to_conv(c, buf, WFLAG_SEND, NULL, time(NULL), -1);
					if (c->makesound)
						play_sound(SND_SEND);
					if (im_options & OPT_IM_POPDOWN)
						gtk_widget_hide(c->window);
				}
			}
			g_free(buffy);
		}
		
	} else {
		err = serv_chat_send(c->gc, c->id, buf);

		/* no sound because we do that when we receive our message */
	}

	g_free(buf2);
	g_free(buf);

	if (err < 0) {
		if (err == -E2BIG)
			do_error_dialog(_("Unable to send message.  The message is too large"), NULL, GAIM_ERROR);
		else if (err == -ENOTCONN)
			debug_printf("Not yet connected\n");
		else
			do_error_dialog(_("Unable to send message"), NULL, GAIM_ERROR);
	} else {
		gtk_text_buffer_set_text(c->entry_buffer, "", -1);

		if ((err > 0) && (away_options & OPT_AWAY_BACK_ON_IM)) {
			if (awaymessage != NULL) {
				do_im_back();
			} else if (c->gc->away) {
				serv_set_away(c->gc, GAIM_AWAY_CUSTOM, NULL);
			}
		}
	}
}

gboolean entry_key_pressed(GtkTextBuffer *buffer)
{
	check_everything(buffer);
	return FALSE;
}

/*------------------------------------------------------------------------*/
/*  HTML-type stuff                                                       */
/*------------------------------------------------------------------------*/

int count_tag(GtkTextBuffer *buffer, char *s1, char *s2)
{
	char *p1, *p2;
	int res = 0;
	GtkTextIter start, end;
	char *tmp, *tmpo;

	gtk_text_buffer_get_start_iter(buffer, &start);
	gtk_text_buffer_get_iter_at_mark(buffer, &end,
					 gtk_text_buffer_get_insert(buffer));
  
	tmp = tmpo = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
	do {
		p1 = strstr(tmp, s1);
		p2 = strstr(tmp, s2);
		if (p1 && p2) {
			if (p1 < p2) {
				res = 1;
				tmp = p1 + strlen(s1);
			} else if (p2 < p1) {
				res = 0;
				tmp = p2 + strlen(s2);
			}
		} else {
			if (p1) {
				res = 1;
				tmp = p1 + strlen(s1);
			} else if (p2) {
				res = 0;
				tmp = p2 + strlen(s2);
			}
		}
	} while (p1 || p2);
	g_free(tmpo);
	return res;
}


gboolean invert_tags(GtkTextBuffer *buffer, char *s1, char *s2, gboolean really)
{
	GtkTextIter start1, start2, end1, end2;
	char *b1, *b2;

	if (gtk_text_buffer_get_selection_bounds(buffer, &start1, &end2)) {
		start2 = start1; end1 = end2;
		if (!gtk_text_iter_forward_chars(&start2, strlen(s1)))
			return FALSE;
		if (!gtk_text_iter_backward_chars(&end1, strlen(s2)))
			return FALSE;
		b1 = gtk_text_buffer_get_text(buffer, &start1, &start2, FALSE);
		b2 = gtk_text_buffer_get_text(buffer, &end1, &end2, FALSE);
		if (!g_strncasecmp(b1, s1, strlen(s1)) &&
		    !g_strncasecmp(b2, s2, strlen(s2))) {
			if (really) {
				GtkTextMark *m_end1, *m_end2;
 
				m_end1= gtk_text_buffer_create_mark(buffer, "m1", &end1, TRUE);
				m_end2= gtk_text_buffer_create_mark(buffer, "m2", &end2, TRUE);

				gtk_text_buffer_delete(buffer, &start1, &start2);
				gtk_text_buffer_get_iter_at_mark(buffer, &end1, m_end1);
				gtk_text_buffer_get_iter_at_mark(buffer, &end2, m_end2);
				gtk_text_buffer_delete(buffer, &end1, &end2);
				gtk_text_buffer_delete_mark(buffer, m_end1);
				gtk_text_buffer_delete_mark(buffer, m_end2);
			}
			 g_free(b1); g_free(b2);
			return TRUE;
		}
		g_free(b1);g_free(b2);
	}
	return FALSE;
}


void remove_tags(struct conversation *c, char *tag)
{
	GtkTextIter start, end, m_start, m_end;

	if (!gtk_text_buffer_get_selection_bounds(c->entry_buffer,
						  &start, &end))
		return;

	/* FIXMEif (strstr(tag, "<FONT SIZE=")) {
		while ((t = strstr(t, "<FONT SIZE="))) {
			if (((t - s) < finish) && ((t - s) >= start)) {
				gtk_editable_delete_text(GTK_EDITABLE(entry), (t - s),
							 (t - s) + strlen(tag));
				g_free(s);
				s = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
				t = s;
			} else
				t++;
		}
	} else*/ {
		while (gtk_text_iter_forward_search(&start, tag, 0, &m_start, &m_end, &end)) {
			gtk_text_buffer_delete(c->entry_buffer, &m_start, &m_end);
			gtk_text_buffer_get_selection_bounds(c->entry_buffer, &start, &end);
		}
	}
}

static char *html_logize(char *p)
{

	char *temp_p = p;
	char *buffer_p;
	char *buffer_start;
	int num_cr = 0;
	int char_len = 0;

	while (*temp_p != '\0') {
		char_len++;
		if ((*temp_p == '\n') || ((*temp_p == '<') && (*(temp_p + 1) == '!')))
			num_cr++;
		++temp_p;
	}

	temp_p = p;
	buffer_p = g_malloc(char_len + (4 * num_cr) + 1);
	buffer_start = buffer_p;

	while (*temp_p != '\0') {
		if (*temp_p == '\n') {
			*buffer_p++ = '<';
			*buffer_p++ = 'B';
			*buffer_p++ = 'R';
			*buffer_p++ = '>';
			*buffer_p++ = '\n';
		} else if ((*temp_p == '<') && (*(temp_p + 1) == '!')) {
			*buffer_p++ = '&';
			*buffer_p++ = 'l';
			*buffer_p++ = 't';
			*buffer_p++ = ';';
		} else
			*buffer_p++ = *temp_p;
		++temp_p;
	}
	*buffer_p = '\0';

	return buffer_start;
}

void surround(struct conversation *c, char *pre, char *post)
{
	GtkTextIter start, end;
	GtkTextMark *mark_start, *mark_end;

	if (gtk_text_buffer_get_selection_bounds(c->entry_buffer, &start, &end)) {
		remove_tags(c, pre);
		remove_tags(c, post);

		mark_start = gtk_text_buffer_create_mark(c->entry_buffer, "m1", &start, TRUE);
		mark_end = gtk_text_buffer_create_mark(c->entry_buffer, "m2", &end, FALSE);
		gtk_text_buffer_insert(c->entry_buffer, &start, pre, -1);
		gtk_text_buffer_get_selection_bounds(c->entry_buffer, &start, &end);
		gtk_text_buffer_insert(c->entry_buffer, &end, post, -1);
		gtk_text_buffer_get_iter_at_mark(c->entry_buffer, &start, mark_start);
		gtk_text_buffer_move_mark_by_name(c->entry_buffer, "selection_bound", &start);
	} else {
		gtk_text_buffer_insert(c->entry_buffer, &start, pre, -1);
		gtk_text_buffer_insert(c->entry_buffer, &start, post, -1);
		mark_start = gtk_text_buffer_get_insert(c->entry_buffer);
		gtk_text_buffer_get_iter_at_mark(c->entry_buffer, &start, mark_start);
		gtk_text_iter_backward_chars(&start, strlen(post));
		gtk_text_buffer_place_cursor(c->entry_buffer, &start);
	}

	gtk_widget_grab_focus(c->entry);
}

void advance_past(struct conversation *c, char *pre, char *post)
{
	GtkTextIter current_pos, start, end;

	if (invert_tags(c->entry_buffer, pre, post, 1))
		return;

	gtk_text_buffer_get_iter_at_mark(c->entry_buffer, &current_pos,
					 gtk_text_buffer_get_insert(c->entry_buffer));
	if (gtk_text_iter_forward_search(&current_pos, post, 0, &start, &end, NULL))
		gtk_text_buffer_place_cursor(c->entry_buffer, &end);
	else
		gtk_text_buffer_insert_at_cursor(c->entry_buffer, post, -1);

	gtk_widget_grab_focus(c->entry);
}

void toggle_fg_color(GtkWidget *color, struct conversation *c)
{
	if (state_lock)
		return;
	if (GTK_TOGGLE_BUTTON(color)->active)
		show_fgcolor_dialog(c, color);
	else if (c->fg_color_dialog)
		cancel_fgcolor(color, c);
	else
		advance_past(c, "<FONT COLOR>", "</FONT>");
}

void toggle_bg_color(GtkWidget *color, struct conversation *c)
{
	if (state_lock)
		return;
	if (GTK_TOGGLE_BUTTON(color)->active)
		show_bgcolor_dialog(c, color);
	else if (c->bg_color_dialog)
		cancel_bgcolor(color, c);
	else
		advance_past(c, "<BODY BGCOLOR>", "</BODY>");
}

void toggle_font(GtkWidget *font, struct conversation *c)
{
	if (state_lock)
		return;
	if (GTK_TOGGLE_BUTTON(font)->active)
		show_font_dialog(c, font);
	else if (c->font_dialog)
		cancel_font(font, c);
	else
		advance_past(c, "<FONT FACE>", "</FONT>");
}

void insert_link_cb(GtkWidget *w, struct conversation *c)
{
	show_add_link(c->link, c);
}

void toggle_link(GtkWidget *linky, struct conversation *c)
{
	if (state_lock)
		return;

	if (GTK_TOGGLE_BUTTON(c->link)->active)
		show_add_link(c->link, c);

	else if (c->link_dialog)
		cancel_link(c->link, c);
	else
		advance_past(c, "<A HREF>", "</A>");

	gtk_widget_grab_focus(c->entry);
}

void do_strike(GtkWidget *strike, struct conversation *c)
{
	if (state_lock)
		return;

	if (GTK_TOGGLE_BUTTON(strike)->active)
		surround(c, "<STRIKE>", "</STRIKE>");
	else
		advance_past(c, "<STRIKE>", "</STRIKE>");

	gtk_widget_grab_focus(c->entry);
}

void do_bold(GtkWidget *bold, struct conversation *c)
{
	if (state_lock)
		return;
	if (GTK_TOGGLE_BUTTON(bold)->active)
		surround(c, "<B>", "</B>");
	else
		advance_past(c, "<B>", "</B>");

	gtk_widget_grab_focus(c->entry);
}

void do_underline(GtkWidget *underline, struct conversation *c)
{
	if (state_lock)
		return;
	if (GTK_TOGGLE_BUTTON(underline)->active)
		surround(c, "<U>", "</U>");
	else
		advance_past(c, "<U>", "</U>");

	gtk_widget_grab_focus(c->entry);
}

void do_italic(GtkWidget *italic, struct conversation *c)
{
	if (state_lock)
		return;
	if (GTK_TOGGLE_BUTTON(italic)->active)
		surround(c, "<I>", "</I>");
	else
		advance_past(c, "<I>", "</I>");

	gtk_widget_grab_focus(c->entry);
}

/* html code to modify font sizes must all be the same length, */
/* currently set to 15 chars */

void do_small(GtkWidget *small, struct conversation *c)
{
	if (state_lock)
		return;

	surround(c, "<FONT SIZE=\"1\">", "</FONT>");

	gtk_widget_grab_focus(c->entry);
}

void do_normal(GtkWidget *normal, struct conversation *c)
{
	if (state_lock)
		return;

	surround(c, "<FONT SIZE=\"3\">", "</FONT>");

	gtk_widget_grab_focus(c->entry);
}

void do_big(GtkWidget *big, struct conversation *c)
{
	if (state_lock)
		return;

	surround(c, "<FONT SIZE=\"5\">", "</FONT>");

	gtk_widget_grab_focus(c->entry);
}

void check_everything(GtkTextBuffer *buffer)
{
	struct conversation *c;

	c = (struct conversation *)g_object_get_data(G_OBJECT(buffer), "user_data");
	if (!c)
		return;
	if (invert_tags(c->entry_buffer, "<B>", "</B>", 0))
		quiet_set(c->bold, TRUE);
	else if (count_tag(c->entry_buffer, "<B>", "</B>"))
		quiet_set(c->bold, TRUE);
	else
		quiet_set(c->bold, FALSE);
	if (invert_tags(c->entry_buffer, "<I>", "</I>", 0))
		quiet_set(c->italic, TRUE);
	else if (count_tag(c->entry_buffer, "<I>", "</I>"))
		quiet_set(c->italic, TRUE);
	else
		quiet_set(c->italic, FALSE);

	if (invert_tags(c->entry_buffer, "<FONT COLOR", "</FONT>", 0))
		quiet_set(c->fgcolorbtn, TRUE);
	else if (count_tag(c->entry_buffer, "<FONT COLOR", "</FONT>"))
		quiet_set(c->fgcolorbtn, TRUE);
	else
		quiet_set(c->fgcolorbtn, FALSE);

	if (invert_tags(c->entry_buffer, "<BODY BGCOLOR", "</BODY>", 0))
		quiet_set(c->bgcolorbtn, TRUE);
	else if (count_tag(c->entry_buffer, "<BODY BGCOLOR", "</BODY>"))
		quiet_set(c->bgcolorbtn, TRUE);
	else
		quiet_set(c->bgcolorbtn, FALSE);

	if (invert_tags(c->entry_buffer, "<FONT FACE", "</FONT>", 0))
		quiet_set(c->font, TRUE);
	else if (count_tag(c->entry_buffer, "<FONT FACE", "</FONT>"))
		quiet_set(c->font, TRUE);
	else
		quiet_set(c->font, FALSE);

	if (invert_tags(c->entry_buffer, "<A HREF", "</A>", 0))
		quiet_set(c->link, TRUE);
	else if (count_tag(c->entry_buffer, "<A HREF", "</A>"))
		quiet_set(c->link, TRUE);
	else
		quiet_set(c->link, FALSE);

	if (invert_tags(c->entry_buffer, "<U>", "</U>", 0))
		quiet_set(c->underline, TRUE);
	else if (count_tag(c->entry_buffer, "<U>", "</U>"))
		quiet_set(c->underline, TRUE);
	else
		quiet_set(c->underline, FALSE);
}


/*------------------------------------------------------------------------*/
/*  Takin care of the window..                                            */
/*------------------------------------------------------------------------*/

static char* nick_colors[] = {
	"#ba55d3",              /* Medium Orchid */
	"#ee82ee",              /* Violet */
	"#c715b4",              /* Medium Violet Red */
	"#ff69b4",              /* Hot Pink */
	"#ff6347",              /* Tomato */
	"#fa8c00",              /* Dark Orange */
	"#fa8072"               /* Salmon */
	"#b22222",              /* Fire Brick */
	"#f4a460",              /* Sandy Brown */
	"#cd5c5c",              /* Indian Red */
	"#bc8f8f",              /* Rosy Brown */
	"#f0e68c",              /* Khaki */
	"#bdb76b",              /* Dark Khaki */
	"#228b22",              /* Forest Green */
	"#9acd32",              /* Yellow Green */
	"#32cd32",              /* Lime Green */
	"#3cb371",              /* Medium Sea Green */
	"#2e8b57",              /* Sea Green */
	"#8fbc8f",              /* Dark Sea Green */
	"#66cdaa",              /* Medium Aquamarine */
	"#5f9ea0",              /* Cadet Blue */
	"#48d1cc",              /* Medium Turquoise */
	"#00ced1",              /* Dark Turquoise */
	"#4682b4",              /* Stell Blue */
	"#00bfff",              /* Deep Sky Blue */
	"#1690ff",              /* Dodger Blue */
	"#4169ff",              /* Royal Blue */
	"#6a5acd",              /* Slate Blue */
	"#6495ed",              /* Cornflower Blue */
	"#708090",              /* Slate gray */
	"#ffdead",              /* Navajo White */
};
#define NUM_NICK_COLORS (sizeof(nick_colors) / sizeof(char *)) 

/* this is going to be interesting since the conversation could either be a
 * normal IM conversation or a chat window. but hopefully it won't matter */
void write_to_conv(struct conversation *c, char *what, int flags, char *who, time_t mtime, gint length)
{
	char buf[BUF_LONG];
	char *str;
	FILE *fd;
	char colour[10];
	struct buddy *b;
	int gtk_font_options = 0;
	GString *logstr;
	char buf2[BUF_LONG];
	char mdate[64];
	int unhighlight = 0;
	char *withfonttag;

	if (c->is_chat && (!c->gc || !g_slist_find(c->gc->buddy_chats, c)))
		return;

	if (!c->is_chat && !g_list_find(conversations, c))
		return;

	gtk_widget_show(c->window);

	if (!c->is_chat || !(c->gc->prpl->options & OPT_PROTO_UNIQUE_CHATNAME)) {
		if (!who) {
			if (flags & WFLAG_SEND) {
				b = find_buddy(c->gc, c->gc->username);
				if (b && strcmp(b->name, b->show))
					 who = b->show;
				else if (c->gc->user->alias[0])
					who = c->gc->user->alias;
				else if (c->gc->displayname[0])
					who = c->gc->displayname;
				else
					who = c->gc->username;
			} else {
				b = find_buddy(c->gc, c->name);
				if (b)
					who = b->show;
				else
					who = c->name;
			}
		} else {
			b = find_buddy(c->gc, who);
			if (b)
				who = b->show;
		}
	}

	strftime(mdate, sizeof(mdate), "%H:%M:%S", localtime(&mtime));

	gtk_font_options = gtk_font_options ^ GTK_IMHTML_NO_COMMENTS;

	if (convo_options & OPT_CONVO_IGNORE_COLOUR)
		gtk_font_options = gtk_font_options ^ GTK_IMHTML_NO_COLOURS;

	if (convo_options & OPT_CONVO_IGNORE_FONTS)
		gtk_font_options = gtk_font_options ^ GTK_IMHTML_NO_FONTS;

	if (convo_options & OPT_CONVO_IGNORE_SIZES)
		gtk_font_options = gtk_font_options ^ GTK_IMHTML_NO_SIZES;

	if (!(logging_options & OPT_LOG_STRIP_HTML))
		gtk_font_options = gtk_font_options ^ GTK_IMHTML_RETURN_LOG;

	if (flags & WFLAG_SYSTEM) {
		if (convo_options & OPT_CONVO_SHOW_TIME)
			g_snprintf(buf, BUF_LONG, "<FONT SIZE=\"2\">(%s) </FONT><B>%s</B>", mdate, what);
		else
			g_snprintf(buf, BUF_LONG, "<B>%s</B>", what);
		g_snprintf(buf2, sizeof(buf2), "<FONT SIZE=\"2\"><!--(%s) --></FONT><B>%s</B><BR>",
			   mdate, what);

		gtk_imhtml_append_text(GTK_IMHTML(c->text), buf2, -1, 0);

		if (logging_options & OPT_LOG_STRIP_HTML) {
			char *t1 = strip_html(buf);
			c->history = g_string_append(c->history, t1);
			c->history = g_string_append(c->history, "\n");
			g_free(t1);
		} else {
			c->history = g_string_append(c->history, buf);
			c->history = g_string_append(c->history, "<BR>\n");
		}

		if (!(flags & WFLAG_NOLOG) && ((c->is_chat && (logging_options & OPT_LOG_CHATS))
								               || (!c->is_chat && (logging_options & OPT_LOG_CONVOS))
															 || find_log_info(c->name))) {
			char *t1;
			char nm[256];

			if (logging_options & OPT_LOG_STRIP_HTML) {
				t1 = strip_html(buf);
			} else {
				t1 = buf;
			}
			if (c->is_chat)
				g_snprintf(nm, 256, "%s.chat", c->name);
			else
				g_snprintf(nm, 256, "%s", c->name);
			fd = open_log_file(nm, c->is_chat);
			if (fd) {
				if (logging_options & OPT_LOG_STRIP_HTML) {
					fprintf(fd, "%s\n", t1);
				} else {
					fprintf(fd, "%s<BR>\n", t1);
				}
				fclose(fd);
			}
			if (logging_options & OPT_LOG_STRIP_HTML) {
				g_free(t1);
			}
		}
	} else if (flags & WFLAG_NOLOG) {
		g_snprintf(buf, BUF_LONG, "<B><FONT COLOR=\"#777777\">%s</FONT></B><BR>", what);
		gtk_imhtml_append_text(GTK_IMHTML(c->text), buf, -1, 0);
	} else {
		if (flags & WFLAG_WHISPER) {
			/* if we're whispering, it's not an autoresponse */
			if (meify(what, length)) {
				str = g_malloc(1024);
				g_snprintf(str, 1024, "***%s", who);
				strcpy(colour, "#6C2585");
			} else {
				str = g_malloc(1024);
				g_snprintf(str, 1024, "*%s*:", who);
				strcpy(colour, "#00ff00");
			}
		} else {
			if (meify(what, length)) {
				str = g_malloc(1024);
				if (flags & WFLAG_AUTO)
					g_snprintf(str, 1024, "%s ***%s", AUTO_RESPONSE, who);
				else
					g_snprintf(str, 1024, "***%s", who);
				if (flags & WFLAG_NICK)
					strcpy(colour, "#af7f00");
				else
					strcpy(colour, "#062585");
			} else {
				str = g_malloc(1024);
				if (flags & WFLAG_AUTO)
					g_snprintf(str, 1024, "%s %s", who, AUTO_RESPONSE);
				else
					g_snprintf(str, 1024, "%s:", who);
				if (flags & WFLAG_NICK)
					strcpy(colour, "#af7f00");
				else if (flags & WFLAG_RECV) {
					if (flags & WFLAG_COLORIZE) {
						char *u = who;	
						int m = 0;
						while (*u) {
							m = m + *u;
							u++;
						}
						m = m % NUM_NICK_COLORS;
						strcpy(colour, nick_colors[m]);
					} else {
						strcpy(colour, "#a82f2f");
					}
				} else if (flags & WFLAG_SEND)
					strcpy(colour, "#6b839e");
			}
		}

		if (convo_options & OPT_CONVO_SHOW_TIME)
			g_snprintf(buf, BUF_LONG, "<FONT COLOR=\"%s\"><FONT SIZE=\"2\">(%s) </FONT>"
				   "<B>%s</B></FONT> ", colour, mdate, str);
		else
			g_snprintf(buf, BUF_LONG, "<FONT COLOR=\"%s\"><B>%s</B></FONT> ", colour, str);
		g_snprintf(buf2, BUF_LONG, "<FONT COLOR=\"%s\"><FONT SIZE=\"2\"><!--(%s) --></FONT>"
			   "<B>%s</B></FONT> ", colour, mdate, str);

		g_free(str);

		gtk_imhtml_append_text(GTK_IMHTML(c->text), buf2, -1, 0);
		
		withfonttag = g_strdup_printf("<font sml=\"%s\">%s</font>", c->gc->prpl->name, what);
		logstr = gtk_imhtml_append_text(GTK_IMHTML(c->text), withfonttag, length, gtk_font_options);

		gtk_imhtml_append_text(GTK_IMHTML(c->text), "<BR>", -1, 0);

		/* XXX this needs to be updated for the new length argument */
		if (logging_options & OPT_LOG_STRIP_HTML) {
			char *t1, *t2;
			t1 = strip_html(buf);
			t2 = strip_html(what);
			c->history = g_string_append(c->history, t1);
			c->history = g_string_append(c->history, t2);
			c->history = g_string_append(c->history, "\n");
			g_free(t1);
			g_free(t2);
		} else {
			char *t1, *t2;
			t1 = html_logize(buf);
			t2 = html_logize(what);
			c->history = g_string_append(c->history, t1);
			c->history = g_string_append(c->history, t2);
			c->history = g_string_append(c->history, "\n");
			c->history = g_string_append(c->history, logstr->str);
			c->history = g_string_append(c->history, "<BR>\n");
			g_free(t1);
			g_free(t2);
		}

		/* XXX this needs to be updated for the new length argument */
		if ((c->is_chat && (logging_options & OPT_LOG_CHATS))
				|| (!c->is_chat && (logging_options & OPT_LOG_CONVOS)) || find_log_info(c->name)) {
			char *t1, *t2;
			char *nm = g_malloc(256);
			if (c->is_chat)
				g_snprintf(nm, 256, "%s.chat", c->name);
			else
				g_snprintf(nm, 256, "%s", c->name);

			if (logging_options & OPT_LOG_STRIP_HTML) {
				t1 = strip_html(buf);
				t2 = strip_html(withfonttag);
			} else {
				t1 = html_logize(buf);
				t2 = html_logize(withfonttag);
			}
			fd = open_log_file(nm, c->is_chat);
			if (fd) {
				if (logging_options & OPT_LOG_STRIP_HTML) {
					fprintf(fd, "%s%s\n", t1, t2);
				} else {
					fprintf(fd, "%s%s%s<BR>\n", t1, t2, logstr->str);
					g_string_free(logstr, TRUE);
				}
				fclose(fd);
			}
			g_free(t1);
			g_free(t2);
			g_free(nm);
		}
		
		g_free(withfonttag);	
	}


	if (!(flags & WFLAG_NOLOG) && ((c->is_chat && (chat_options & OPT_CHAT_POPUP)) ||
								   (!c->is_chat && (im_options & OPT_IM_POPUP))))
		gdk_window_show(c->window->window);

	/* tab highlighting */
	if (c->is_chat && !(chat_options & OPT_CHAT_ONE_WINDOW)) /* if chat but not tabbed chat */
		return;
	if (!(flags & WFLAG_RECV) && !(flags & WFLAG_SYSTEM))
		return;
	if (im_options & OPT_IM_ONE_WINDOW && ((c->unseen == 2) || ((c->unseen == 1) && !(flags & WFLAG_NICK))))
		return;

	if (flags & WFLAG_RECV)
		c->typing_state = NOT_TYPING;


	if (c->is_chat) {
		int offs;
		if ((convo_options & OPT_CONVO_COMBINE) && (im_options & OPT_IM_ONE_WINDOW))
			offs = g_list_length(conversations);
		else
			offs = 0;
		if (gtk_notebook_get_current_page(GTK_NOTEBOOK(chat_notebook)) ==
				g_list_index(chats, c) + offs)
			unhighlight = 1;
	} else {
		if (im_options & OPT_IM_ONE_WINDOW)
			if (gtk_notebook_get_current_page(GTK_NOTEBOOK(convo_notebook)) ==
					g_list_index(conversations, c))
				unhighlight = 1;
	}

	if (!unhighlight && flags & WFLAG_NICK) {
		c->unseen = 2;
	} else if (!unhighlight) {
		c->unseen = 1;
	} else {
		c->unseen = 0;
	}
	update_convo_status(c);
}

void update_progress(struct conversation *c, float percent) {
            
       if (percent >= 1 && !(c->progress))
	       return;

       if (percent >= 1) {
	       gtk_widget_destroy(c->progress);
               c->progress = NULL;
               return;
       }
       
       if (!c->progress) {
               GtkBox *box = GTK_BOX(c->text->parent->parent);
               c->progress = gtk_progress_bar_new();
               gtk_box_pack_end(box, c->progress, FALSE, FALSE, 0);
               gtk_widget_set_size_request (c->progress, 1, 8);
               gtk_widget_show (c->progress);
       }
       
       if (percent < 1)
               gtk_progress_set_percentage(GTK_PROGRESS(c->progress), percent);
}

GtkWidget *build_conv_menubar(struct conversation *c)
{
	GtkWidget *hb;
	GtkWidget *menubar;
	GtkWidget *menu;
	GtkWidget *menuitem;


	hb = gtk_handle_box_new();
		
	menubar = gtk_menu_bar_new();

	gtk_container_add(GTK_CONTAINER(hb), menubar);

	menu = gtk_menu_new();

	/* The file menu */
	menuitem = gaim_new_item(NULL, _("File"));
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), menu);
	gtk_menu_bar_append(GTK_MENU_BAR(menubar), menuitem);

	gaim_new_item_from_stock(menu, _("_Save Conversation"), "gtk-save-as", GTK_SIGNAL_FUNC(save_convo), c, 0, 0, NULL); 

	gaim_new_item_from_stock(menu, _("View _History"), NULL, GTK_SIGNAL_FUNC(conv_show_log), GINT_TO_POINTER(c->name), 0, 0, NULL); 

	menuitem = gtk_menu_item_new();
	gtk_menu_append(GTK_MENU(menu), menuitem);
/*
	c->sendfile_btn = gaim_new_item_from_pixbuf(menu, _("Send File"), "send-file-small.png", NULL, NULL, 0, 0, NULL); */

	gaim_new_item_from_pixbuf(menu, _("Insert _URL"), "insert-link-small.png", GTK_SIGNAL_FUNC(insert_link_cb), c, 0, 0, NULL); 
	c->image_menubtn = gaim_new_item_from_pixbuf(menu, _("Insert _Image"), "insert-image-small.png", GTK_SIGNAL_FUNC(insert_image), c, 0, 0, NULL); 

	menuitem = gtk_menu_item_new();
	gtk_menu_append(GTK_MENU(menu), menuitem);

	gaim_new_item_from_stock(menu, _("_Close"), "gtk-close", GTK_SIGNAL_FUNC(close_callback), c, 0, 0, NULL); 

	/* The Options  menu */
	menu = gtk_menu_new();

	menuitem = gaim_new_item(NULL, _("Options"));
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), menu);
	gtk_menu_bar_append(GTK_MENU_BAR(menubar), menuitem);

	/* Logging */
	menuitem = gtk_check_menu_item_new_with_mnemonic(_("Enable _Logging"));
	c->log_button = menuitem;  /* We should save this */
	
	state_lock = 1;

	if (find_log_info(c->name))
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), TRUE);	
	else
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), FALSE);	

	state_lock = 0;

	gtk_signal_connect(GTK_OBJECT(menuitem), "toggled", GTK_SIGNAL_FUNC(toggle_loggle), c);

	/* Sounds */

	gtk_menu_append(GTK_MENU(menu), menuitem);

	menuitem = gtk_check_menu_item_new_with_mnemonic(_("Enable _Sounds"));
	c->makesound = 1;
	gtk_signal_connect(GTK_OBJECT(menuitem), "toggled", GTK_SIGNAL_FUNC(toggle_sound), c);
	state_lock = 1;
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), TRUE);
	state_lock = 0;
	gtk_menu_append(GTK_MENU(menu), menuitem);



	/* Now set the current values or something */
	gtk_widget_set_sensitive(GTK_WIDGET(c->log_button), (logging_options & OPT_LOG_CONVOS) ? FALSE : TRUE);

	gtk_widget_show_all(hb);

	return hb;

}

GtkWidget *build_conv_toolbar(struct conversation *c)
{
		GtkWidget *vbox;
		GtkWidget *hbox;
		GtkWidget *button;
		GtkWidget *sep;
		GtkSizeGroup *sg = gtk_size_group_new(GTK_SIZE_GROUP_BOTH);
		/*
	c->toolbar = toolbar;
	c->bold = bold;
	c->strike = strike;
	c->italic = italic;
	c->underline = underline;
	c->log_button = wood;
	c->viewer_button = viewer;
	c->fgcolorbtn = fgcolorbtn;
	c->bgcolorbtn = bgcolorbtn;
	c->link = link;
	c->wood = wood;
	c->font = font;
	c->smiley = smiley;
	c->imagebtn = image;
	c->speaker = speaker;
	c->speaker_p = speaker_p;
	*/

		vbox = gtk_vbox_new(FALSE, 0);
		sep = gtk_hseparator_new();
		gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 0);
		
		hbox = gtk_hbox_new(FALSE, 5);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

		/* Bold */
		button = gaim_pixbuf_toolbar_button_from_stock("gtk-bold");
		gtk_size_group_add_widget(sg, button);
		gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(do_bold), c);
		c->bold = button; /* We should remember this */

		/* Italic */
		button = gaim_pixbuf_toolbar_button_from_stock("gtk-italic");
		gtk_size_group_add_widget(sg, button);
		gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(do_italic), c);
		c->italic = button; /* We should remember this */

		/* Underline */
		button = gaim_pixbuf_toolbar_button_from_stock("gtk-underline");
		gtk_size_group_add_widget(sg, button);
		gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(do_underline), c);
		c->underline = button; /* We should remember this */

		/* Sep */
		sep = gtk_vseparator_new();
		gtk_box_pack_start(GTK_BOX(hbox), sep, FALSE, FALSE, 0);

		/* Increase font size */
		button = gaim_pixbuf_toolbar_button_from_file("text_bigger.png");
		gtk_size_group_add_widget(sg, button);
		gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(do_big), c);
		
		/* Normal Font Size */
		button = gaim_pixbuf_toolbar_button_from_file("text_normal.png");
		gtk_size_group_add_widget(sg, button);
		gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(do_normal), c);
		c->font = button; /* We should remember this */
		
		/* Decrease font size */
		button = gaim_pixbuf_toolbar_button_from_file("text_smaller.png");
		gtk_size_group_add_widget(sg, button);
		gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(do_small), c);

		/* Sep */
		sep = gtk_vseparator_new();
		gtk_box_pack_start(GTK_BOX(hbox), sep, FALSE, FALSE, 0);

		/* Font Color */
		button = gaim_pixbuf_toolbar_button_from_file("change-fgcolor-small.png");
		gtk_size_group_add_widget(sg, button);
		gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(toggle_fg_color), c);
		c->fgcolorbtn = button; /* We should remember this */

		/* Font Color */
		button = gaim_pixbuf_toolbar_button_from_file("change-bgcolor-small.png");
		gtk_size_group_add_widget(sg, button);
		gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(toggle_bg_color), c);
		c->bgcolorbtn = button; /* We should remember this */


		/* Sep */
		sep = gtk_vseparator_new();
		gtk_box_pack_start(GTK_BOX(hbox), sep, FALSE, FALSE, 0);

		/* Insert IM Image  */
		button = gaim_pixbuf_toolbar_button_from_file("insert-image-small.png");
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(insert_image), c);
		gtk_size_group_add_widget(sg, button);
		gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
		c->imagebtn = button;

		/* Insert Link  */
		button = gaim_pixbuf_toolbar_button_from_file("insert-link-small.png");
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(toggle_link), c);
		gtk_size_group_add_widget(sg, button);
		gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
		c->link = button;

		/* Insert Smiley */
		button = gaim_pixbuf_toolbar_button_from_file("insert-smiley-small.png");
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(insert_smiley), c);
		gtk_size_group_add_widget(sg, button);
		gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
		c->smiley = button;

		sep = gtk_hseparator_new();
		gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 0);

		gtk_widget_show_all(vbox);

		return vbox;
}

static void convo_sel_send(GtkObject *m, struct gaim_connection *c)
{
	struct conversation *cnv = gtk_object_get_user_data(m);

	if (cnv->gc == c)
		return;

	cnv->gc = c;

	set_convo_title(cnv);

	update_buttons_by_protocol(cnv);

	update_icon(cnv);
	update_checkbox(cnv);
	gaim_setup_imhtml_smileys(cnv->text);
}

int set_dispstyle(int chat)
{
	int dispstyle;

	if (chat) {
		switch (chat_options & (OPT_CHAT_BUTTON_TEXT | OPT_CHAT_BUTTON_XPM)) {
		case OPT_CHAT_BUTTON_TEXT:
			dispstyle = 1;
			break;
		case OPT_CHAT_BUTTON_XPM:
			dispstyle = 0;
			break;
		default:	/* both or neither */
			dispstyle = 2;
			break;
		}
	} else {
		switch (im_options & (OPT_IM_BUTTON_TEXT | OPT_IM_BUTTON_XPM)) {
		case OPT_IM_BUTTON_TEXT:
			dispstyle = 1;
			break;
		case OPT_IM_BUTTON_XPM:
			dispstyle = 0;
			break;
		default:	/* both or neither */
			dispstyle = 2;
			break;
		}
	}
	return dispstyle;
}

void update_convo_add_button(struct conversation *c)
{
	/*int dispstyle = set_dispstyle(0);*/
	GtkWidget *parent = c->add->parent;
	gboolean rebuild = FALSE;

	if (find_buddy(c->gc, c->name)) {
		if (!gtk_object_get_user_data(GTK_OBJECT(c->add))) {
			gtk_widget_destroy(c->add);
			c->add = gaim_pixbuf_button_from_stock(_("Remove"), "gtk-remove", GAIM_BUTTON_VERTICAL);
			rebuild = TRUE;
		}
		if (c->gc) {
			if (c->gc->prpl->remove_buddy == NULL)
				gtk_widget_set_sensitive(c->add, FALSE);
			else
				gtk_widget_set_sensitive(c->add, TRUE);
		} else
			gtk_widget_set_sensitive(c->add, FALSE);
		gtk_object_set_user_data(GTK_OBJECT(c->add), c);
	} else {
		if (gtk_object_get_user_data(GTK_OBJECT(c->add))) {
			gtk_widget_destroy(c->add);
			c->add = gaim_pixbuf_button_from_stock(_("Add"), "gtk-add", GAIM_BUTTON_VERTICAL);
			rebuild = TRUE;
		}
		if (c->gc) {
			if (c->gc->prpl->add_buddy == NULL)
				gtk_widget_set_sensitive(c->add, FALSE);
			else
				gtk_widget_set_sensitive(c->add, TRUE);
		} else
			gtk_widget_set_sensitive(c->add, FALSE);
	}

	if (rebuild) {
		gtk_signal_connect(GTK_OBJECT(c->add), "clicked", GTK_SIGNAL_FUNC(add_callback), c);
		gtk_box_pack_start(GTK_BOX(parent), c->add, FALSE, FALSE, 0);
		gtk_box_reorder_child(GTK_BOX(parent), c->add, 3);
		gtk_button_set_relief(GTK_BUTTON(c->add), GTK_RELIEF_NONE);
		gtk_size_group_add_widget(c->sg, c->add);
		gtk_widget_show(c->add);
	}
}

static void create_convo_menu(struct conversation *cnv)
{
	GtkWidget *menu, *opt;
	GSList *g = connections;
	struct gaim_connection *c;
	char buf[2048];

	if (g_slist_length(g) < 2)
		gtk_widget_hide(cnv->menu->parent);
	else {
		menu = gtk_menu_new();

		while (g) {
			c = (struct gaim_connection *)g->data;
			g_snprintf(buf, sizeof buf, "%s (%s)", c->username, c->prpl->name);
			opt = gtk_menu_item_new_with_label(buf);
			gtk_object_set_user_data(GTK_OBJECT(opt), cnv);
			gtk_signal_connect(GTK_OBJECT(opt), "activate",
					   GTK_SIGNAL_FUNC(convo_sel_send), c);
			gtk_widget_show(opt);
			gtk_menu_append(GTK_MENU(menu), opt);
			g = g->next;
		}

		gtk_option_menu_remove_menu(GTK_OPTION_MENU(cnv->menu));
		gtk_option_menu_set_menu(GTK_OPTION_MENU(cnv->menu), menu);
		gtk_option_menu_set_history(GTK_OPTION_MENU(cnv->menu), 0);

		gtk_widget_show(cnv->menu);
		gtk_widget_show(cnv->menu->parent);
	}
}

void redo_convo_menus()
{
	GList *c = conversations;
	struct conversation *C;

	while (c) {
		C = (struct conversation *)c->data;
		c = c->next;

		create_convo_menu(C);

		if (g_slist_find(connections, C->gc))
			set_convo_gc(C, C->gc);
		else
			set_convo_gc(C, connections ? connections->data : NULL);
	}
}

void convo_menu_remove(struct gaim_connection *gc)
{
	GList *c = conversations;
	struct conversation *C;

	while (c) {
		C = (struct conversation *)c->data;
		c = c->next;

		remove_icon(C);
		remove_checkbox(C);
	}
}

void set_convo_gc(struct conversation *c, struct gaim_connection *gc)
{
	if (gc)
		gtk_option_menu_set_history(GTK_OPTION_MENU(c->menu), g_slist_index(connections, gc));

	if (c->gc == gc)
		return;

	c->gc = gc;

	set_convo_title(c);
	update_buttons_by_protocol(c);

	update_icon(c);
	update_checkbox(c);
	gaim_setup_imhtml_smileys(c->text);
}

void update_buttons_by_protocol(struct conversation *c)
{
	if (!c->gc) {
		if (c->info)
			gtk_widget_set_sensitive(c->info, FALSE);
		if (c->send)
			gtk_widget_set_sensitive(c->send, FALSE);
		if (c->warn)
			gtk_widget_set_sensitive(c->warn, FALSE);
		if (c->block)
			gtk_widget_set_sensitive(c->block, FALSE);
		if (c->add)
			gtk_widget_set_sensitive(c->add, FALSE);
		if (c->whisper)
			gtk_widget_set_sensitive(c->whisper, FALSE);
		if (c->invite)
			gtk_widget_set_sensitive(c->invite, FALSE);
		return;
	}

	if (c->gc->prpl->get_info == NULL && c->info)
		gtk_widget_set_sensitive(c->info, FALSE);
	else if (c->info)
		gtk_widget_set_sensitive(c->info, TRUE);
/*
	if (!c->is_chat && c->gc->prpl->file_transfer_out)
			gtk_widget_set_sensitive(c->sendfile_btn, TRUE);
	else
			gtk_widget_set_sensitive(c->sendfile_btn, FALSE);
*/	
	if (c->is_chat) {
		if (c->gc->prpl->chat_send == NULL && c->send)
			gtk_widget_set_sensitive(c->send, FALSE);
		else
			gtk_widget_set_sensitive(c->send, TRUE);
		
		gtk_widget_set_sensitive(c->imagebtn, FALSE);
		gtk_widget_set_sensitive(c->image_menubtn, FALSE);
	} else {
		if (c->gc->prpl->send_im == NULL && c->send)
			gtk_widget_set_sensitive(c->send, FALSE);
		else
			gtk_widget_set_sensitive(c->send, TRUE);
		if (c->gc->prpl->options & OPT_PROTO_IM_IMAGE) {
			if (c->imagebtn)
				gtk_widget_set_sensitive(c->imagebtn, TRUE);
			if (c->image_menubtn)
				gtk_widget_set_sensitive(c->image_menubtn, TRUE);
		}
		else {
			
			if (c->image_menubtn)
				gtk_widget_set_sensitive(c->image_menubtn, FALSE);
			if (c->imagebtn)
				gtk_widget_set_sensitive(c->imagebtn, FALSE);
		}
	}

	if (c->gc->prpl->warn == NULL && c->warn)
		gtk_widget_set_sensitive(c->warn, FALSE);
	else if (c->warn)
		gtk_widget_set_sensitive(c->warn, TRUE);

	if (c->gc->prpl->add_permit == NULL && c->block)
		gtk_widget_set_sensitive(c->block, FALSE);
	else if (c->block)
		gtk_widget_set_sensitive(c->block, TRUE);

	if (c->add)
		update_convo_add_button(c);

	if (c->whisper) {
		if (c->gc->prpl->chat_whisper == NULL)
			gtk_widget_set_sensitive(c->whisper, FALSE);
		else
			gtk_widget_set_sensitive(c->whisper, TRUE);
	}

	if (c->invite) {
		if (c->gc->prpl->chat_invite == NULL)
			gtk_widget_set_sensitive(c->invite, FALSE);
		else
			gtk_widget_set_sensitive(c->invite, TRUE);
	}
}

void convo_switch(GtkNotebook *notebook, GtkWidget *page, gint page_num, gpointer data)
{
	GtkWidget *label = NULL;
	GtkStyle *style;
	struct conversation *c;
	
	if ((convo_options & OPT_CONVO_COMBINE) &&
	    (im_options & OPT_IM_ONE_WINDOW) &&
	    (chat_options & OPT_CHAT_ONE_WINDOW)) {
		int len = g_list_length(conversations);
		if (page_num < len)
			c = g_list_nth_data(conversations, page_num);
		else
			c = g_list_nth_data(chats, page_num - len);
	} else if (GTK_WIDGET(notebook) == convo_notebook)
		c = g_list_nth_data(conversations, page_num);
	else
		c = g_list_nth_data(chats, page_num);

	if (c && c->window && c->entry)
		gtk_widget_grab_focus(c->entry);

	label = c->tab_label;

	if (!label)
		return;

	if (!GTK_WIDGET_REALIZED(label))
		return;
	style = gtk_style_new();
	gtk_style_set_font(style, gdk_font_ref(gtk_style_get_font(label->style)));
	gtk_widget_set_style(label, style);
	gtk_style_unref(style);
	if (c)
		c->unseen = 0;

	if (!c->is_chat) {
			GtkWidget *menubar;
			GtkWidget *parent = convo_notebook->parent;

			gtk_widget_freeze_child_notify(GTK_WIDGET(c->window));

			if (convo_menubar != NULL)
					gtk_widget_destroy(convo_menubar);

			menubar = build_conv_menubar(c);
			gtk_box_pack_start(GTK_BOX(parent), menubar, FALSE, TRUE, 0);
			gtk_box_reorder_child(GTK_BOX(parent), menubar, 0);
			convo_menubar = menubar;

			gtk_widget_thaw_child_notify(GTK_WIDGET(c->window));
	} else {
		gtk_widget_destroy(convo_menubar);
		convo_menubar = NULL;
	}

	update_convo_status(c);

	/*gtk_imhtml_to_bottom(c->text);*/
}

void update_convo_status(struct conversation *c) {
	if(!c)
		return;
	debug_printf("update_convo_status called for %s\n", c->name);
	if (im_options & OPT_IM_ONE_WINDOW) { /* We'll make the tab green */
		GtkStyle *style;
		GtkWidget *label = c->tab_label;
		style = gtk_style_new();
		if (!GTK_WIDGET_REALIZED(label))
			gtk_widget_realize(label);
		gtk_style_set_font(style, gdk_font_ref(gtk_style_get_font(label->style)));
		if(c->typing_state == TYPING) {
			style->fg[0].red = 0x0000;
			style->fg[0].green = 0x9999;
			style->fg[0].blue = 0x0000;
		} else if(c->typing_state == TYPED) {
			style->fg[0].red = 0xffff;
			style->fg[0].green = 0xbbbb;
			style->fg[0].blue = 0x2222;
		} else if(c->unseen == 2) {
			style->fg[0].red = 0x0000;
			style->fg[0].green = 0x0000;
			style->fg[0].blue = 0xcccc;
		} else if(c->unseen == 1) {
			style->fg[0].red = 0xcccc;
			style->fg[0].green = 0x0000;
			style->fg[0].blue = 0x0000;
		}
		gtk_widget_set_style(label, style);
			debug_printf("setting style\n");
		gtk_style_unref(style);
	} else {
		GtkWindow *win = (GtkWindow *)c->window;
		char *buf, *buf2;
		int len;
		if(strstr(win->title, _(" [TYPING]"))) {
			debug_printf("title had TYPING in it\n");
			len = strlen(win->title) - strlen(_(" [TYPING]"));
		} else if(strstr(win->title, _(" [TYPED]"))) {
			debug_printf("title had TYPED in it\n");
			len = strlen(win->title) - strlen(_(" [TYPED]"));
		} else {
			debug_printf("title was free of typing information\n");
			len = strlen(win->title);
		}
		buf = g_malloc(len+1);
		g_snprintf(buf, len+1, win->title);
		if(c->typing_state == TYPING) {
			buf2 = g_strconcat(buf,_(" [TYPING]"), NULL);
			g_free(buf);
			buf = buf2;
		} else if(c->typing_state == TYPED) {
			buf2 = g_strconcat(buf,_(" [TYPED]"), NULL);
			g_free(buf);
			buf = buf2;
		}
		gtk_window_set_title(win, buf);
		g_free(buf);
	}
}

/* This returns a boolean, so that it can timeout */
gboolean reset_typing(char *name) {
	struct conversation *c = find_conversation(name);
	if (!c) {
		g_free(name);
		return FALSE;
	}

	/* Reset the title (if necessary) */
	c->typing_state = NOT_TYPING;
	update_convo_status(c);

	g_free(name);
	c->typing_timeout = 0;
	return FALSE;
}

void show_conv(struct conversation *c)
{
	GtkWidget *win;
	GtkWidget *cont;
	GtkWidget *text;
	GtkWidget *sw;
	GtkWidget *send;
	GtkWidget *info;
	GtkWidget *warn;
	GtkWidget *block;
	/*GtkWidget *close;*/
	GtkWidget *frame;
	GtkWidget *entry;
	GtkWidget *bbox;
	GtkWidget *vbox;
	GtkWidget *vbox2;
	GtkWidget *paned;
	GtkWidget *add;
	GtkWidget *toolbar;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *menubar;
	GtkWidget *tabby;

	c->sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	c->font_dialog = NULL;
	c->fg_color_dialog = NULL;
	c->bg_color_dialog = NULL;
	c->smiley_dialog = NULL;
	c->link_dialog = NULL;
	c->log_dialog = NULL;
	sprintf(c->fontface, "%s", fontface);
	c->hasfont = 0;
	c->bgcol = bgcolor;
	c->hasbg = 0;
	c->fgcol = fgcolor;
	c->hasfg = 0;

	if (im_options & OPT_IM_ONE_WINDOW) {
		if (!all_convos) {
			GtkWidget *testidea;
			win = all_convos = c->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
			if ((convo_options & OPT_CONVO_COMBINE) && (chat_options & OPT_CHAT_ONE_WINDOW))
				all_chats = all_convos;
			gtk_window_set_role(GTK_WINDOW(win), "conversation");
			gtk_window_set_policy(GTK_WINDOW(win), TRUE, TRUE, FALSE);
			gtk_container_border_width(GTK_CONTAINER(win), 0);
			gtk_widget_realize(win);
			gtk_window_set_title(GTK_WINDOW(win), _("Gaim - Conversations"));
			gtk_signal_connect(GTK_OBJECT(win), "delete_event",
					   GTK_SIGNAL_FUNC(delete_all_convo), NULL);

			convo_notebook = gtk_notebook_new();
			if ((convo_options & OPT_CONVO_COMBINE) && (chat_options & OPT_CHAT_ONE_WINDOW))
				chat_notebook = convo_notebook;
			if (im_options & OPT_IM_SIDE_TAB) {
				if (im_options & OPT_IM_BR_TAB) {
					gtk_notebook_set_tab_pos(GTK_NOTEBOOK(convo_notebook),
								 GTK_POS_RIGHT);
				} else {
					gtk_notebook_set_tab_pos(GTK_NOTEBOOK(convo_notebook),
								 GTK_POS_LEFT);
				}
			} else {
				if (im_options & OPT_IM_BR_TAB) {
					gtk_notebook_set_tab_pos(GTK_NOTEBOOK(convo_notebook),
								 GTK_POS_BOTTOM);
				} else {
					gtk_notebook_set_tab_pos(GTK_NOTEBOOK(convo_notebook),
								 GTK_POS_TOP);
				}
			}
			gtk_notebook_set_scrollable(GTK_NOTEBOOK(convo_notebook), TRUE);
			gtk_notebook_popup_enable(GTK_NOTEBOOK(convo_notebook));

			testidea = gtk_vbox_new(FALSE, 0);
			
			menubar = build_conv_menubar(c);
			gtk_box_pack_start(GTK_BOX(testidea), menubar, FALSE, TRUE, 0);
			gtk_box_pack_start(GTK_BOX(testidea), convo_notebook, TRUE, TRUE, 0);
			gtk_widget_show(testidea);
			gtk_widget_show(convo_notebook);
			convo_menubar = menubar;

			gtk_container_add(GTK_CONTAINER(win), testidea);
			gtk_signal_connect(GTK_OBJECT(convo_notebook), "switch-page",
					   GTK_SIGNAL_FUNC(convo_switch), NULL);
		} else
			win = c->window = all_convos;

		cont = gtk_vbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(cont), 5);
		/* this doesn't matter since we're resetting the name once we're out of the if */
/*		gtk_notebook_insert_page(GTK_NOTEBOOK(convo_notebook), cont, gtk_label_new(c->name),*/
		tabby = gtk_hbox_new(FALSE, 5);
		c->close = gtk_button_new();
		gtk_widget_set_size_request(GTK_WIDGET(c->close), 16, 16);
		gtk_container_add(GTK_CONTAINER(c->close), gtk_image_new_from_stock(GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU));
		gtk_button_set_relief(GTK_BUTTON(c->close), GTK_RELIEF_NONE);
		c->tab_label = gtk_label_new(c->name);

		gtk_signal_connect(GTK_OBJECT(c->close), "clicked", GTK_SIGNAL_FUNC(close_callback), c);

		gtk_box_pack_start(GTK_BOX(tabby), c->tab_label, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(tabby), c->close, FALSE, FALSE, 0);
		gtk_widget_show_all(tabby);
		gtk_notebook_insert_page(GTK_NOTEBOOK(convo_notebook), cont, tabby,
					 g_list_index(conversations, c));

		gtk_widget_show(cont);
	} else {
		cont = win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		c->window = win;
		gtk_object_set_user_data(GTK_OBJECT(win), c);
		gtk_window_set_role(GTK_WINDOW(win), "conversation");
		gtk_window_set_policy(GTK_WINDOW(win), TRUE, TRUE, TRUE);
		gtk_container_border_width(GTK_CONTAINER(win), 0);
		gtk_widget_realize(win);
		gtk_signal_connect(GTK_OBJECT(win), "delete_event",
				   GTK_SIGNAL_FUNC(delete_event_convo), c);
	}
	set_convo_title(c);

	paned = gtk_vpaned_new();
	gtk_paned_set_gutter_size(GTK_PANED(paned), 15);
	gtk_container_add(GTK_CONTAINER(cont), paned);
	gtk_widget_show(paned);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_paned_pack1(GTK_PANED(paned), vbox, FALSE, TRUE);
	gtk_widget_show(vbox);

	if (!(im_options & OPT_IM_ONE_WINDOW)) {
		menubar = build_conv_menubar(c);
		gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, TRUE, 0);
	}

	sw = gtk_scrolled_window_new(NULL, NULL);
	c->sw = sw;
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
	gtk_widget_set_size_request(sw, conv_size.width, conv_size.height);
	gtk_widget_show(sw);

	text = gtk_imhtml_new(NULL, NULL);
	c->text = text;
	gtk_container_add(GTK_CONTAINER(sw), text);
	if (convo_options & OPT_CONVO_SHOW_TIME)
		gtk_imhtml_show_comments(GTK_IMHTML(text), TRUE);
	gaim_setup_imhtml(text);
	gtk_widget_show(text);

	vbox2 = gtk_vbox_new(FALSE, 5);
	gtk_paned_pack2(GTK_PANED(paned), vbox2, FALSE, FALSE);
	gtk_widget_show(vbox2);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new(_("Send message as: "));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	c->menu = gtk_option_menu_new();
	gtk_box_pack_start(GTK_BOX(hbox), c->menu, FALSE, FALSE, 5);
	gtk_widget_show(c->menu);

	create_convo_menu(c);

	c->lbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox2), c->lbox, FALSE, FALSE, 0);
	gtk_widget_show(c->lbox);

	toolbar = build_conv_toolbar(c);
	gtk_box_pack_start(GTK_BOX(vbox2), toolbar, FALSE, FALSE, 0);

	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(vbox2), frame, TRUE, TRUE, 0);
	gtk_widget_show(frame);
	
	c->entry_buffer = gtk_text_buffer_new(NULL);
	g_object_set_data(G_OBJECT(c->entry_buffer), "user_data", c);
	entry = gtk_text_view_new_with_buffer(c->entry_buffer);
	c->entry = entry;

	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(c->entry), GTK_WRAP_WORD);

	gtk_widget_set_size_request(entry, conv_size.width - 20, MAX(conv_size.entry_height, 25));
			
	g_signal_connect_swapped(G_OBJECT(c->entry), "key_press_event",
				 G_CALLBACK(entry_key_pressed), c->entry_buffer);
	g_signal_connect(G_OBJECT(c->entry), "key_press_event", G_CALLBACK(keypress_callback), c);
	g_signal_connect_after(G_OBJECT(c->entry), "button_press_event", 
			       G_CALLBACK(stop_rclick_callback), NULL);
	g_signal_connect(G_OBJECT(c->entry_buffer), "insert_text",
			   G_CALLBACK(insert_text_callback), c);
	g_signal_connect(G_OBJECT(c->entry_buffer), "delete_range",
			   G_CALLBACK(delete_text_callback), c);

#ifdef USE_GTKSPELL
	if (convo_options & OPT_CONVO_CHECK_SPELLING)
		gtkspell_new_attach(GTK_TEXT_VIEW(c->entry), NULL, NULL);
#endif

	gtk_container_add(GTK_CONTAINER(frame), GTK_WIDGET(entry));
	gtk_widget_show(entry);

	c->bbox = bbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox2), bbox, FALSE, FALSE, 0);
	gtk_widget_show(bbox);

/* I'm leaving this here just incase we want to bring this back. I'd rather not have the close
 * button any more.  If we do, though, it needs to be on the left side.  I might bring it back and put
 * it on that side.  */

/*	
	close = picture_button2(win, _("Close"), cancel_xpm, dispstyle);
	c->close = close;
	gtk_object_set_user_data(GTK_OBJECT(close), c);
	gtk_signal_connect(GTK_OBJECT(close), "clicked", GTK_SIGNAL_FUNC(close_callback), c);
	gtk_box_pack_end(GTK_BOX(bbox), close, dispstyle, dispstyle, 0);
	gtk_widget_show(close);
	
	c->sep1 = gtk_vseparator_new();
	gtk_box_pack_end(GTK_BOX(bbox), c->sep1, dispstyle, dispstyle, 0);
	gtk_widget_show(c->sep1);
*/	

	/* Put the send button on the right */
	send = gaim_pixbuf_button_from_stock(_("Send"), "gtk-convert", GAIM_BUTTON_VERTICAL);
	c->send = send;
	gtk_signal_connect(GTK_OBJECT(send), "clicked", GTK_SIGNAL_FUNC(send_callback), c);
	gtk_box_pack_end(GTK_BOX(bbox), send, FALSE, FALSE, 0);
	gtk_widget_show(send);

	c->sep2 = gtk_vseparator_new();
	gtk_box_pack_end(GTK_BOX(bbox), c->sep2, FALSE, TRUE, 0);
	gtk_widget_show(c->sep2);
	
	/* And put the other buttons on the left */

	if (c->gc && find_buddy(c->gc, c->name) != NULL) {
		add = gaim_pixbuf_button_from_stock(_("Remove"), "gtk-remove", GAIM_BUTTON_VERTICAL);
		gtk_object_set_user_data(GTK_OBJECT(add), c);
	} else
		add = gaim_pixbuf_button_from_stock(_("Add"), "gtk-add", GAIM_BUTTON_VERTICAL);

	c->add = add;
	gtk_signal_connect(GTK_OBJECT(add), "clicked", GTK_SIGNAL_FUNC(add_callback), c);
	gtk_box_pack_start(GTK_BOX(bbox), add, FALSE, FALSE, 0);
	gtk_widget_show(add);

	warn = gaim_pixbuf_button_from_stock(_("Warn"), "gtk-dialog-warning", GAIM_BUTTON_VERTICAL);
	c->warn = warn;
	gtk_signal_connect(GTK_OBJECT(warn), "clicked", GTK_SIGNAL_FUNC(warn_callback), c);
	gtk_box_pack_start(GTK_BOX(bbox), warn, FALSE, FALSE, 0);
	gtk_widget_show(warn);

	info = gaim_pixbuf_button_from_stock(_("Info"), "gtk-find", GAIM_BUTTON_VERTICAL);
	c->info = info;
	gtk_signal_connect(GTK_OBJECT(info), "clicked", GTK_SIGNAL_FUNC(info_callback), c);
	gtk_box_pack_start(GTK_BOX(bbox), info, FALSE, FALSE, 0);
	gtk_widget_show(info);


	block = gaim_pixbuf_button_from_stock(_("Block"), "gtk-stop", GAIM_BUTTON_VERTICAL);
	c->block = block;
	gtk_signal_connect(GTK_OBJECT(block), "clicked", GTK_SIGNAL_FUNC(block_callback), c);
	gtk_box_pack_start(GTK_BOX(bbox), block, FALSE, FALSE, 0);
	gtk_widget_show(block);

	/* I don't know if these should have borders.  They look kind of dumb
	 * with borders.  */
	gtk_button_set_relief(GTK_BUTTON(info), GTK_RELIEF_NONE);
	gtk_button_set_relief(GTK_BUTTON(add), GTK_RELIEF_NONE);
	gtk_button_set_relief(GTK_BUTTON(warn), GTK_RELIEF_NONE);
	gtk_button_set_relief(GTK_BUTTON(send), GTK_RELIEF_NONE);
	gtk_button_set_relief(GTK_BUTTON(block), GTK_RELIEF_NONE);

	gtk_size_group_add_widget(c->sg, info);
	gtk_size_group_add_widget(c->sg, add);
	gtk_size_group_add_widget(c->sg, warn);
	gtk_size_group_add_widget(c->sg, send);
	gtk_size_group_add_widget(c->sg, block);

	gtk_box_reorder_child(GTK_BOX(bbox), c->warn, 1);
	gtk_box_reorder_child(GTK_BOX(bbox), c->block, 2);
	gtk_box_reorder_child(GTK_BOX(bbox), c->add, 3);
	gtk_box_reorder_child(GTK_BOX(bbox), c->info, 4);


	update_buttons_by_protocol(c);

	gtk_widget_show(win);

	if (!(im_options & OPT_IM_ONE_WINDOW)
			|| ((gtk_notebook_get_current_page(GTK_NOTEBOOK(convo_notebook)) == 0)
				&& (c == g_list_nth_data(conversations, 0))))
		gtk_widget_grab_focus(c->entry);
}


void toggle_spellchk()
{
#ifdef USE_GTKSPELL
	GList *cnv = conversations;
	GSList *cht;
	struct conversation *c;
	GSList *con = connections;
	struct gaim_connection *gc;
	GtkSpell *spell;
	
	while (cnv) {
		c = (struct conversation *)cnv->data;
		if (convo_options & OPT_CONVO_CHECK_SPELLING) {
			gtkspell_new_attach(GTK_TEXT_VIEW(c->entry), NULL, NULL);
		} else {
			spell = gtkspell_get_from_text_view(GTK_TEXT_VIEW(c->entry));
			gtkspell_detach(spell);
		}
		cnv = cnv->next;
	}

	while (con) {
		gc = (struct gaim_connection *)con->data;
		cht = gc->buddy_chats;
		while (cht) {
			c = (struct conversation *)cht->data;
			if (convo_options & OPT_CONVO_CHECK_SPELLING) {
				gtkspell_new_attach(GTK_TEXT_VIEW(c->entry), NULL, NULL);
			} else {
				spell = gtkspell_get_from_text_view(GTK_TEXT_VIEW(c->entry));
				gtkspell_detach(spell);
			}
			cht = cht->next;
		}
		con = con->next;
	}
#endif
}

void toggle_timestamps()
{
	GList *cnv = conversations;
	GSList *cht;
	struct conversation *c;
	GSList *con = connections;
	struct gaim_connection *gc;

	while (cnv) {
		c = (struct conversation *)cnv->data;
		if (convo_options & OPT_CONVO_SHOW_TIME)
			gtk_imhtml_show_comments(GTK_IMHTML(c->text), TRUE);
		else
			gtk_imhtml_show_comments(GTK_IMHTML(c->text), FALSE);
		cnv = cnv->next;
	}

	while (con) {
		gc = (struct gaim_connection *)con->data;
		cht = gc->buddy_chats;
		while (cht) {
			c = (struct conversation *)cht->data;
			if (convo_options & OPT_CONVO_SHOW_TIME)
				gtk_imhtml_show_comments(GTK_IMHTML(c->text), TRUE);
			else
				gtk_imhtml_show_comments(GTK_IMHTML(c->text), FALSE);
			cht = cht->next;
		}
		con = con->next;
	}
}

void toggle_smileys()
{
	GList *cnv = conversations;
	GSList *cht;
	struct conversation *c;
	GSList *con = connections;
	struct gaim_connection *gc;

	while (cnv) {
		c = (struct conversation *)cnv->data;
		if (convo_options & OPT_CONVO_SHOW_SMILEY)
			gtk_imhtml_show_smileys(GTK_IMHTML(c->text), TRUE);
		else
			gtk_imhtml_show_smileys(GTK_IMHTML(c->text), FALSE);
		cnv = cnv->next;
	}

	while (con) {
		gc = (struct gaim_connection *)con->data;
		cht = gc->buddy_chats;
		while (cht) {
			c = (struct conversation *)cht->data;
			if (convo_options & OPT_CONVO_SHOW_SMILEY)
				gtk_imhtml_show_smileys(GTK_IMHTML(c->text), TRUE);
			else
				gtk_imhtml_show_smileys(GTK_IMHTML(c->text), FALSE);
			cht = cht->next;
		}
		con = con->next;
	}
}

void im_tabize()
{
	/* evil, evil i tell you! evil! */
	if (im_options & OPT_IM_ONE_WINDOW) {
		GList *x = conversations;
		if ((convo_options & OPT_CONVO_COMBINE) && (chat_options & OPT_CHAT_ONE_WINDOW)) {
			all_convos = all_chats;
			convo_notebook = chat_notebook;
		}
		while (x) {
			struct conversation *c = x->data;
			GtkWidget *imhtml, *win;

			imhtml = c->text;
			win = c->window;
			remove_icon(c);
			remove_checkbox(c);
			show_conv(c);
			gtk_widget_destroy(c->text);
			gtk_widget_reparent(imhtml, c->sw);
			c->text = imhtml;
			gtk_widget_destroy(win);
			update_icon(c);
			update_checkbox(c);
			set_convo_title(c);

			x = x->next;
		}
	} else {
		GList *x, *m;
		x = m = conversations;
		conversations = NULL;
		while (x) {
			struct conversation *c = x->data;
			GtkWidget *imhtml;

			imhtml = c->text;
			remove_icon(c);
			remove_checkbox(c);
			show_conv(c);
			gtk_container_remove(GTK_CONTAINER(c->sw), c->text);
			gtk_widget_reparent(imhtml, c->sw);
			c->text = imhtml;
			update_icon(c);
			update_checkbox(c);
			set_convo_title(c);

			x = x->next;
		}
		conversations = m;
		if ((convo_options & OPT_CONVO_COMBINE) && (chat_options & OPT_CHAT_ONE_WINDOW)) {
			if (chats) {
				struct conversation *c;
				while (m) {
					gtk_notebook_remove_page(GTK_NOTEBOOK(chat_notebook), 0);
					m = m->next;
				}
				c = chats->data;
				gtk_widget_grab_focus(c->entry);
			} else {
				if (all_convos)
					gtk_widget_destroy(all_convos);
				all_chats = NULL;
				chat_notebook = NULL;
			}
		} else if (all_convos)
			gtk_widget_destroy(all_convos);
		all_convos = NULL;
		convo_notebook = NULL;
	}
}

void convo_tabize()
{
	GList *x, *m;
	GtkWidget *tmp;

	if (!chats && !conversations)
		return;

	if (convo_options & OPT_CONVO_COMBINE) {
		if (!chats) {
			all_chats = all_convos;
			chat_notebook = convo_notebook;
			return;
		} else if (!conversations) {
			all_convos = all_chats;
			convo_notebook = chat_notebook;
			return;
		}
	} else {
		if (!chats) {
			all_chats = NULL;
			chat_notebook = NULL;
			return;
		} else if (!conversations) {
			all_convos = NULL;
			convo_notebook = NULL;
			return;
		}
	}

	tmp = all_convos;
	if (convo_options & OPT_CONVO_COMBINE) {
		all_convos = all_chats;
		convo_notebook = chat_notebook;
	} else {
		all_convos = NULL;
		convo_notebook = NULL;
	}
	x = m = conversations;
	while (x) {
		struct conversation *c = x->data;
		GtkWidget *imhtml;

		imhtml = c->text;
		remove_icon(c);
		remove_checkbox(c);
		show_conv(c);
		gtk_container_remove(GTK_CONTAINER(c->sw), c->text);
		gtk_widget_reparent(imhtml, c->sw);
		c->text = imhtml;
		update_icon(c);
		update_checkbox(c);

		x = x->next;
	}

	conversations = m;
	if (convo_options & OPT_CONVO_COMBINE) {
		if (tmp)
			gtk_widget_destroy(tmp);
	} else {
		while (m) {
			gtk_notebook_remove_page(GTK_NOTEBOOK(chat_notebook), 0);
			m = m->next;
		}
	}
	m = conversations;
	while (m) {
		set_convo_title(m->data);
		m = m->next;
	}
}

void set_convo_title(struct conversation *c)
{
	struct buddy *b;
	char *text;
	int index;
	GtkNotebook *nb;

	if ((im_options & OPT_IM_ALIAS_TAB) && c->gc && ((b = find_buddy(c->gc, c->name)) != NULL))
		text = b->show;
	else
		text = c->name;

	if (im_options & OPT_IM_ONE_WINDOW) {
		nb = GTK_NOTEBOOK(convo_notebook);
		index = g_list_index(conversations, c);
		gtk_label_set_text(GTK_LABEL(c->tab_label), text);
	} else {
		char buf[256];
		if ((find_log_info(c->name)) || (c->is_chat && (logging_options & OPT_LOG_CHATS))
				|| (!c->is_chat && (logging_options & OPT_LOG_CONVOS)))
			g_snprintf(buf, sizeof(buf), LOG_CONVERSATION_TITLE, text);
		else
			g_snprintf(buf, sizeof(buf), CONVERSATION_TITLE, text);
		gtk_window_set_title(GTK_WINDOW(c->window), buf);
	}
}

void set_convo_titles()
{
	GList *c = conversations;
	while (c) {
		set_convo_title(c->data);
		c = c->next;
	}
}

void raise_convo_tab(struct conversation *c)
{
	gtk_notebook_set_page(GTK_NOTEBOOK(convo_notebook), g_list_index(conversations, c));
	gdk_window_show(c->window->window);
}

void update_im_tabs()
{
	if (!convo_notebook || !all_convos)
		return;
	if (im_options & OPT_IM_SIDE_TAB) {
		if (im_options & OPT_IM_BR_TAB) {
			gtk_notebook_set_tab_pos(GTK_NOTEBOOK(convo_notebook), GTK_POS_RIGHT);
		} else {
			gtk_notebook_set_tab_pos(GTK_NOTEBOOK(convo_notebook), GTK_POS_LEFT);
		}
	} else {
		if (im_options & OPT_IM_BR_TAB) {
			gtk_notebook_set_tab_pos(GTK_NOTEBOOK(convo_notebook), GTK_POS_BOTTOM);
		} else {
			gtk_notebook_set_tab_pos(GTK_NOTEBOOK(convo_notebook), GTK_POS_TOP);
		}
	}
}

void update_chat_tabs()
{
	if (!chat_notebook || !all_chats)
		return;
	if (chat_options & OPT_CHAT_SIDE_TAB) {
		if (chat_options & OPT_CHAT_BR_TAB) {
			gtk_notebook_set_tab_pos(GTK_NOTEBOOK(chat_notebook), GTK_POS_RIGHT);
		} else {
			gtk_notebook_set_tab_pos(GTK_NOTEBOOK(chat_notebook), GTK_POS_LEFT);
		}
	} else {
		if (chat_options & OPT_CHAT_BR_TAB) {
			gtk_notebook_set_tab_pos(GTK_NOTEBOOK(chat_notebook), GTK_POS_BOTTOM);
		} else {
			gtk_notebook_set_tab_pos(GTK_NOTEBOOK(chat_notebook), GTK_POS_TOP);
		}
	}
}

void update_convo_color(gboolean fg)
{
	GList *c = conversations;
	struct conversation *b;

	while (c) {
		b = c->data;
		c = c->next;
		if (fg) {
			if (b->hasfg)
				continue;
			b->fgcol = fgcolor;
		} else {
			if (b->hasbg)
				continue;
			b->bgcol = bgcolor;
		}
	}
}

void update_convo_font()
{
	GList *c = conversations;
	struct conversation *b;

	while (c) {
		b = c->data;
		c = c->next;
		if (b->hasfont)
			continue;
		sprintf(b->fontface, "%s", fontface);
	}
}

#include <gdk-pixbuf/gdk-pixbuf.h>

#define SCALE(x) ((gdk_pixbuf_animation_get_width(x) <= 48 && gdk_pixbuf_animation_get_height(x) <= 48) \
		 ? 48 : 50)

static gboolean redraw_icon(gpointer data)
{
	struct conversation *c = data;

	GdkPixbuf *buf;
	GdkPixbuf *scale;
	GdkPixmap *pm;
	GdkBitmap *bm;
	gint delay;

	if (!g_list_find(conversations, c)) {
		debug_printf("I think this is a bug.\n");
		return FALSE;
	}

	gdk_pixbuf_animation_iter_advance(c->iter, NULL);
	buf = gdk_pixbuf_animation_iter_get_pixbuf(c->iter);
	scale = gdk_pixbuf_scale_simple(buf,
					MAX(gdk_pixbuf_get_width(buf) * SCALE(c->anim) /
					    gdk_pixbuf_animation_get_width(c->anim), 1),
					MAX(gdk_pixbuf_get_height(buf) * SCALE(c->anim) /
					    gdk_pixbuf_animation_get_height(c->anim), 1),
					GDK_INTERP_NEAREST);
	gdk_pixbuf_render_pixmap_and_mask(scale, &pm, &bm, 100);
	gdk_pixbuf_unref(scale);
	gtk_pixmap_set(GTK_PIXMAP(c->icon), pm, bm);
	gdk_pixmap_unref(pm);
	gtk_widget_queue_draw(c->icon);
	if (bm)
		gdk_bitmap_unref(bm);
	delay = gdk_pixbuf_animation_iter_get_delay_time(c->iter) / 10;

	c->icon_timer = gtk_timeout_add(delay * 10, redraw_icon, c);
	return FALSE;
}

static void stop_anim(GtkObject *obj, struct conversation *c)
{
	if (c->icon_timer)
		gtk_timeout_remove(c->icon_timer);
	c->icon_timer = 0;
}

static void start_anim(GtkObject *obj, struct conversation *c)
{
	int delay;
	delay = gdk_pixbuf_animation_iter_get_delay_time(c->iter) / 10;
	if (c->anim)
	    c->icon_timer = gtk_timeout_add(delay * 10, redraw_icon, c);
}

static int des_save_icon(GtkObject *obj, GdkEvent *e, struct conversation *c)
{
	gtk_widget_destroy(c->save_icon);
	c->save_icon = NULL;
	return TRUE;
}

static void do_save_icon(GtkObject *obj, struct conversation *c)
{
	FILE *file;
	const char *f = gtk_file_selection_get_filename(GTK_FILE_SELECTION(c->save_icon));
	if (file_is_dir(f, c->save_icon))
		return;

	file = fopen(f, "w");
	if (file) {
		int len;
		void *data = get_icon_data(c->gc, normalize(c->name), &len);
		if (data)
			fwrite(data, 1, len, file);
		fclose(file);
	} else {
		do_error_dialog("Can't save icon file to disk", strerror(errno), GAIM_ERROR);
	}

	gtk_widget_destroy(c->save_icon);
	c->save_icon = NULL;
}

static void cancel_save_icon(GtkObject *obj, struct conversation *c)
{
	gtk_widget_destroy(c->save_icon);
	c->save_icon = NULL;
}

static void save_icon(GtkObject *obj, struct conversation *c)
{
	char buf[BUF_LEN];

	if (c->save_icon) {
		gdk_window_raise(c->save_icon->window);
		return;
	}

	c->save_icon = gtk_file_selection_new(_("Gaim - Save Icon"));
	gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(c->save_icon));
	g_snprintf(buf, BUF_LEN - 1, "%s" G_DIR_SEPARATOR_S "%s.icon", gaim_home_dir(), c->name);
	gtk_file_selection_set_filename(GTK_FILE_SELECTION(c->save_icon), buf);
	gtk_signal_connect(GTK_OBJECT(c->save_icon), "delete_event",
			   GTK_SIGNAL_FUNC(des_save_icon), c);
	gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(c->save_icon)->ok_button), "clicked",
			   GTK_SIGNAL_FUNC(do_save_icon), c);
	gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(c->save_icon)->cancel_button), "clicked",
			   GTK_SIGNAL_FUNC(cancel_save_icon), c);

	gtk_widget_show(c->save_icon);
}

static gboolean icon_menu(GtkObject *obj, GdkEventButton *e, struct conversation *c)
{
	static GtkWidget *menu = NULL;
	GtkWidget *button;

	if (e->button != 3)
		return FALSE;
	if (e->type != GDK_BUTTON_PRESS)
		return FALSE;

	/*
	 * If a menu already exists, destroy it before creating a new one,
	 * thus freeing-up the memory it occupied.
	 */
	if(menu)
		gtk_widget_destroy(menu);

	menu = gtk_menu_new();

	if (c->icon_timer) {
		button = gtk_menu_item_new_with_label(_("Disable Animation"));
		gtk_signal_connect(GTK_OBJECT(button), "activate", GTK_SIGNAL_FUNC(stop_anim), c);
		gtk_menu_append(GTK_MENU(menu), button);
		gtk_widget_show(button);
	}
	 else if (c->anim && !(gdk_pixbuf_animation_is_static_image(c->anim))) 
	{
		button = gtk_menu_item_new_with_label(_("Enable Animation"));
		gtk_signal_connect(GTK_OBJECT(button), "activate", GTK_SIGNAL_FUNC(start_anim), c);
		gtk_menu_append(GTK_MENU(menu), button);
		gtk_widget_show(button);
	}

	button = gtk_menu_item_new_with_label(_("Hide Icon"));
	gtk_signal_connect_object(GTK_OBJECT(button), "activate",
				  GTK_SIGNAL_FUNC(remove_icon), (void *)c);
	gtk_menu_append(GTK_MENU(menu), button);
	gtk_widget_show(button);

	button = gtk_menu_item_new_with_label(_("Save Icon As..."));
	gtk_signal_connect(GTK_OBJECT(button), "activate", GTK_SIGNAL_FUNC(save_icon), c);
	gtk_menu_append(GTK_MENU(menu), button);
	gtk_widget_show(button);

	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, e->button, e->time);

	return TRUE;
}

void remove_icon(struct conversation *c)
{
	if (c->icon)
		gtk_container_remove(GTK_CONTAINER(c->bbox), c->icon->parent->parent);
	c->icon = NULL;
	if (c->anim)
		gdk_pixbuf_animation_unref(c->anim);
	c->anim = NULL;
	if (c->icon_timer)
		gtk_timeout_remove(c->icon_timer);
	c->icon_timer = 0;
	if(c->iter)
		g_object_unref(G_OBJECT(c->iter));
}

void update_icon(struct conversation *c)
{
	char filename[256];
	FILE *file;
	GError *err = NULL;

	void *data;
	int len, delay;

	GdkPixbuf *buf;

	GtkWidget *event;
	GtkWidget *frame;
	GdkPixbuf *scale;
	GdkPixmap *pm;
	GdkBitmap *bm;
	int sf = 0;

	if (!c)
		return;

	remove_icon(c);

	if (im_options & OPT_IM_HIDE_ICONS)
		return;

	if (!c->gc)
		return;

	data = get_icon_data(c->gc, normalize(c->name), &len);
	if (!data)
		return;

	/* this is such an evil hack, i don't know why i'm even considering it.
	 * we'll do it differently when gdk-pixbuf-loader isn't leaky anymore. */
	g_snprintf(filename, sizeof(filename), "%s" G_DIR_SEPARATOR_S "gaimicon-%s.%d", g_get_tmp_dir(), c->name, getpid());
	file = fopen(filename, "w");
	if (!file)
		return;
	fwrite(data, 1, len, file);
	fclose(file);

	c->anim = gdk_pixbuf_animation_new_from_file(filename, &err);
	if (err) {
		debug_printf("Buddy icon error: %s\n", err->message);
		g_error_free(err);
	}
	/* make sure we remove the file as soon as possible */
	unlink(filename);

	if (!c->anim)
		return;

	if (gdk_pixbuf_animation_is_static_image(c->anim)) {
		c->iter = NULL;
		delay = 0;
		buf = gdk_pixbuf_animation_get_static_image(c->anim);
	} else {
		c->iter = gdk_pixbuf_animation_get_iter(c->anim, NULL);
		buf = gdk_pixbuf_animation_iter_get_pixbuf(c->iter);
		delay = gdk_pixbuf_animation_iter_get_delay_time(c->iter);
		delay = delay / 10;
	}
		sf = SCALE(c->anim);
		scale = gdk_pixbuf_scale_simple(buf,
						MAX(gdk_pixbuf_get_width(buf) * sf /
						    gdk_pixbuf_animation_get_width(c->anim), 1),
						MAX(gdk_pixbuf_get_height(buf) * sf /
						    gdk_pixbuf_animation_get_height(c->anim), 1),
						GDK_INTERP_NEAREST);
	
	if (delay)
		c->icon_timer = gtk_timeout_add(delay * 10, redraw_icon, c);
	

	gdk_pixbuf_render_pixmap_and_mask(scale, &pm, &bm, 100);
	gdk_pixbuf_unref(scale);

	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), bm ? GTK_SHADOW_NONE : GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(c->bbox), frame, FALSE, FALSE, 5);
	gtk_box_reorder_child(GTK_BOX(c->bbox), frame, 0);
	gtk_widget_show(frame);

	event = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(frame), event);
	gtk_signal_connect(GTK_OBJECT(event), "button-press-event", GTK_SIGNAL_FUNC(icon_menu), c);
	gtk_widget_show(event);

	c->icon = gtk_pixmap_new(pm, bm);
	gtk_widget_set_size_request(c->icon, sf, sf);
	gtk_container_add(GTK_CONTAINER(event), c->icon);
	gtk_widget_show(c->icon);
	if(im_options & OPT_IM_NO_ANIMATION)
		stop_anim(NULL, c);		
	gdk_pixmap_unref(pm);
	if (bm)
		gdk_bitmap_unref(bm);
}

void got_new_icon(struct gaim_connection *gc, char *who)
{
	struct conversation *c = find_conversation(who);
	if (c && (c->gc == gc))
		update_icon(c);
}

void set_hide_icons()
{
	GList *c = conversations;
	while (c) {
		update_icon(c->data);
		c = c->next;
	}
}

void set_anim()
{
	GList *c = conversations;
	if (im_options & OPT_IM_HIDE_ICONS)
		return;
	while (c) {
		if(im_options & OPT_IM_NO_ANIMATION)
			stop_anim(NULL, c->data);
		else 
			start_anim(NULL, c->data);
		c = c->next;
	}
}

static void remove_checkbox(struct conversation *c)
{
	if (c->check)
		gtk_container_remove(GTK_CONTAINER(c->lbox), c->check);
	c->check = NULL;
}

static void update_checkbox(struct conversation *c)
{
	if (!c)
		return;

	remove_checkbox(c);

	if (!c->gc)
		return;

	if (!c->gc->checkbox)
		return;

	c->check = gtk_check_button_new_with_label(c->gc->checkbox);
	gtk_box_pack_start(GTK_BOX(c->lbox), c->check, FALSE, FALSE, 5);
	gtk_widget_show(c->check);
}
