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
#include "../config.h"
#endif
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <gtk/gtk.h>
#include "gtkimhtml.h"
#include <gdk/gdkkeysyms.h>
#include "convo.h"
#include "gtkspell.h"
#include "prpl.h"

#include "pixmaps/underline.xpm"
#include "pixmaps/bold.xpm"
#include "pixmaps/italic.xpm"
#include "pixmaps/small.xpm"
#include "pixmaps/normal.xpm"
#include "pixmaps/big.xpm"
#include "pixmaps/fontface.xpm"
#include "pixmaps/speaker.xpm"
#include "pixmaps/smile_icon.xpm"
#include "pixmaps/wood.xpm"
#include "pixmaps/link.xpm"
#include "pixmaps/strike.xpm"
#include "pixmaps/fgcolor.xpm"
#include "pixmaps/bgcolor.xpm"

#include "pixmaps/luke03.xpm"
#include "pixmaps/oneeye.xpm"

int state_lock = 0;

GdkPixmap *dark_icon_pm = NULL;
GdkBitmap *dark_icon_bm = NULL;

char fontface[64];
int fontsize = 3;
extern GdkColor bgcolor;
extern GdkColor fgcolor;

void check_everything(GtkWidget *entry);
gboolean keypress_callback(GtkWidget *entry, GdkEventKey * event, struct conversation *c);

/*------------------------------------------------------------------------*/
/*  Helpers                                                               */
/*------------------------------------------------------------------------*/


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

struct conversation *new_conversation(char *name)
{
	struct conversation *c;

	c = find_conversation(name);

	if (c != NULL)
		return c;

	c = (struct conversation *)g_new0(struct conversation, 1);
	g_snprintf(c->name, sizeof(c->name), "%s", name);

	if ((general_options & OPT_GEN_LOG_ALL) || find_log_info(c->name)) {
		FILE *fd;

		fd = open_log_file(c->name);
		if (fd > 0) {
			if (!(general_options & OPT_GEN_STRIP_HTML))
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
	show_conv(c);
	conversations = g_list_append(conversations, c);
	plugin_event(event_new_conversation, name, 0, 0, 0);
	return c;
}


struct conversation *find_conversation(char *name)
{
	char *cuser = g_malloc(64);
	struct conversation *c;
	GList *cnv = conversations;

	strcpy(cuser, normalize(name));

	while (cnv) {
		c = (struct conversation *)cnv->data;
		if (!strcasecmp(cuser, normalize(c->name))) {
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
	char buf[128];

	log_conversations = g_list_remove(log_conversations, a);

	save_prefs();

	if (cnv) {
		if (!(general_options & OPT_GEN_LOG_ALL))
			g_snprintf(buf, sizeof(buf), CONVERSATION_TITLE, cnv->name);
		else
			g_snprintf(buf, sizeof(buf), LOG_CONVERSATION_TITLE, cnv->name);
		gtk_window_set_title(GTK_WINDOW(cnv->window), buf);
	}
}

struct log_conversation *find_log_info(char *name)
{
	char *pname = g_malloc(64);
	GList *lc = log_conversations;
	struct log_conversation *l;


	strcpy(pname, normalize(name));

	while (lc) {
		l = (struct log_conversation *)lc->data;
		if (!strcasecmp(pname, normalize(l->name))) {
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

		if (c->log_button)
			gtk_widget_set_sensitive(c->log_button,
						 ((general_options & OPT_GEN_LOG_ALL)) ? FALSE : TRUE);

		cnv = cnv->next;
	}

	while (C) {
		g = (struct gaim_connection *)C->data;
		bcs = g->buddy_chats;
		while (bcs) {
			c = (struct conversation *)bcs->data;

			if (c->log_button)
				gtk_widget_set_sensitive(c->log_button,
							 ((general_options & OPT_GEN_LOG_ALL)) ? FALSE :
							 TRUE);

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

		if (c->strike)
			gtk_widget_set_sensitive(c->strike,
						 ((font_options & OPT_FONT_STRIKE)) ? FALSE : TRUE);

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
	else if (GTK_TOGGLE_BUTTON(loggle)->active)
		show_log_dialog(c);
	else
		cancel_log(NULL, c);
}

void insert_smiley(GtkWidget *smiley, struct conversation *c)
{
	if (state_lock)
		return;
	if (GTK_TOGGLE_BUTTON(smiley)->active)
		show_smiley_dialog(c, smiley);
	else if (c->smiley_dialog)
		close_smiley_dialog(smiley, c);

	return;
}

int close_callback(GtkWidget *widget, struct conversation *c)
{
	if (c->is_chat && (widget == c->close)) {
		GtkWidget *tmp = c->window;
		debug_printf("chat clicked close button\n");
		c->window = NULL;
		gtk_widget_destroy(tmp);
		return FALSE;
	}

	debug_printf("conversation close callback\n");

	if (general_options & OPT_GEN_CHECK_SPELLING)
		gtkspell_detach(GTK_TEXT(c->entry));

	if (c->window)
		gtk_widget_destroy(c->window);
	c->window = NULL;

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
		if (c->gc)
			serv_chat_leave(c->gc, c->id);
		else {
			/* bah */
			while (c->in_room) {
				char *tmp = c->in_room->data;
				c->in_room = g_list_remove(c->in_room, tmp);
				g_free(tmp);
			}
			g_free(c);
		}
	} else {
		delete_conversation(c);
	}

	return TRUE;
}

void set_font_face(char *newfont, struct conversation *c)
{
	char *pre_fontface;
	int alloc = 1;

	pre_fontface = g_strconcat("<FONT FACE=\"", newfont, "\">", '\0');

	if (!strcmp(pre_fontface, "<FONT FACE=\"\">")) {
		g_free(pre_fontface);
		alloc--;
		pre_fontface = "<FONT FACE=\"Helvetica\">";
	}

	sprintf(c->fontface, "%s", newfont ? (newfont[0] ? newfont : "Helvetica") : "Helvetica");
	c->hasfont = 1;
	surround(c->entry, pre_fontface, "</FONT>");
	gtk_widget_grab_focus(c->entry);

	if (alloc)
		g_free(pre_fontface);
}

static gint delete_event_convo(GtkWidget *w, GdkEventAny * e, struct conversation *c)
{
	delete_conversation(c);
	return FALSE;
}

void add_callback(GtkWidget *widget, struct conversation *c)
{
	if (c->gc && find_buddy(c->gc, c->name) != NULL) {
		debug_printf(_("Removing '%s' from buddylist.\n"), c->name);
		remove_buddy(c->gc, find_group_by_buddy(c->gc, c->name), find_buddy(c->gc, c->name));
		build_edit_tree();
		update_convo_add_button(c);
	} else {
		if (c->gc)
			show_add_buddy(c->gc, c->name, NULL);
	}

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
		char *name;
		GList *i;

		i = GTK_LIST(c->list)->selection;
		if (i)
			name = (char *)gtk_object_get_user_data(GTK_OBJECT(i->data));
		else
			return;

		serv_get_info(c->gc, name);
	} else {
		serv_get_info(c->gc, c->name);
		gtk_widget_grab_focus(c->entry);
	}
}

gboolean keypress_callback(GtkWidget *entry, GdkEventKey * event, struct conversation *c)
{
	int pos;
	if (event->keyval == GDK_Escape) {
		if (general_options & OPT_GEN_ESC_CAN_CLOSE) {
			gtk_signal_emit_stop_by_name(GTK_OBJECT(entry), "key_press_event");
			close_callback(c->window, c);
		}
	} else if ((event->keyval == GDK_F2) && (general_options & OPT_GEN_F2_TOGGLES)) {
		gtk_imhtml_show_comments(GTK_IMHTML(c->text), !GTK_IMHTML(c->text)->comments);
	} else if (event->keyval == GDK_Return) {
		if ((event->state & GDK_CONTROL_MASK) && (general_options & OPT_GEN_CTL_ENTER)) {
			gtk_signal_emit_by_name(GTK_OBJECT(entry), "activate", c);
			gtk_signal_emit_stop_by_name(GTK_OBJECT(entry), "key_press_event");
		} else if (!(event->state & GDK_SHIFT_MASK) && (general_options & OPT_GEN_ENTER_SENDS)) {
			gtk_signal_emit_by_name(GTK_OBJECT(entry), "activate", c);
			gtk_signal_emit_stop_by_name(GTK_OBJECT(entry), "key_press_event");
		} else {
			gtk_signal_emit_stop_by_name(GTK_OBJECT(entry), "key_press_event");
			pos = gtk_editable_get_position(GTK_EDITABLE(entry));
			gtk_editable_insert_text(GTK_EDITABLE(entry), "\n", 1, &pos);
		}
	} else if (event->state & GDK_CONTROL_MASK) {
		if (general_options & OPT_GEN_CTL_CHARS) {
			switch (event->keyval) {
			case 'i':
				quiet_set(c->italic,
					  !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(c->italic)));
				do_italic(c->italic, c->entry);
				gtk_signal_emit_stop_by_name(GTK_OBJECT(entry), "key_press_event");
				break;
			case 'u':	/* ctl-u is GDK_Clear, which clears the line */
				quiet_set(c->underline,
					  !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON
									(c->underline)));
				do_underline(c->underline, c->entry);
				gtk_signal_emit_stop_by_name(GTK_OBJECT(entry), "key_press_event");
				break;
			case 'b':	/* ctl-b is GDK_Left, which moves backwards */
				quiet_set(c->bold,
					  !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(c->bold)));
				do_bold(c->bold, c->entry);
				gtk_signal_emit_stop_by_name(GTK_OBJECT(entry), "key_press_event");
				break;
			case 's':
				quiet_set(c->strike,
					  !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(c->strike)));
				do_strike(c->strike, c->entry);
				gtk_signal_emit_stop_by_name(GTK_OBJECT(entry), "key_press_event");
				break;
			}
		}
		if (general_options & OPT_GEN_CTL_SMILEYS) {
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
				if (GTK_EDITABLE(c->entry)->has_selection) {
					int finish = GTK_EDITABLE(c->entry)->selection_end_pos;
					gtk_editable_insert_text(GTK_EDITABLE(c->entry),
								 buf, strlen(buf), &finish);
				} else {
					pos = GTK_EDITABLE(c->entry)->current_pos;
					gtk_editable_insert_text(GTK_EDITABLE(c->entry),
								 buf, strlen(buf), &pos);
				}
				gtk_signal_emit_stop_by_name(GTK_OBJECT(entry), "key_press_event");
			}
		}
	}

	return TRUE;

}


void send_callback(GtkWidget *widget, struct conversation *c)
{
	char *buf, *buf2, *buf3;
	int limit;

	if (!c->gc)
		return;

	buf2 = gtk_editable_get_chars(GTK_EDITABLE(c->entry), 0, -1);
	/* uncomment this if you want no limit on outgoing messages.
	 * if you uncomment this, you'll probably get kicked off if
	 * you send one that's too big.
	 limit = strlen(buf2) * 2;
	 */
	limit = 7985 << 2;
	buf = g_malloc(limit);
	g_snprintf(buf, limit, "%s", buf2);
	g_free(buf2);
	gtk_editable_delete_text(GTK_EDITABLE(c->entry), 0, -1);
	if (!strlen(buf)) {
		g_free(buf);
		return;
	}

	if (general_options & OPT_GEN_SEND_LINKS)
		linkify_text(buf);

	buf2 = g_malloc(limit);

	if (c->gc->prpl->options & OPT_PROTO_HTML) {
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

		if ((font_options & OPT_FONT_SIZE) || c->hassize) {
			g_snprintf(buf2, limit, "<FONT SIZE=\"%d\">%s</FONT>", c->fontsize, buf);
			strcpy(buf, buf2);
		}

		if ((font_options & OPT_FONT_FGCOL) || c->hasfg) {
			g_snprintf(buf2, limit, "<FONT COLOR=\"#%02X%02X%02X\">%s</FONT>", c->fgcol.red,
				   c->fgcol.green, c->fgcol.blue, buf);
			strcpy(buf, buf2);
		}

		if ((font_options & OPT_FONT_BGCOL) || c->hasbg) {
			g_snprintf(buf2, limit, "<BODY BGCOLOR=\"#%02X%02X%02X\">%s</BODY>", c->bgcol.red,
				   c->bgcol.green, c->bgcol.blue, buf);
			strcpy(buf, buf2);
		}
	}

	{
		char *buffy = g_strdup(buf);
		enum gaim_event evnt = c->is_chat ? event_chat_send : event_im_send;
		int plugin_return = plugin_event(evnt, c->gc, c->name, &buffy, 0);
		if (!buffy) {
			g_free(buf2);
			g_free(buf);
			return;
		}
		if (plugin_return) {
			g_free(buffy);
			g_free(buf2);
			g_free(buf);
			return;
		}
		g_snprintf(buf, limit, "%s", buffy);
		g_free(buffy);
	}

	if (!c->is_chat) {
		buf3 = g_strdup(buf);
		write_to_conv(c, buf3, WFLAG_SEND, NULL);
		g_free(buf3);

		serv_send_im(c->gc, c->name, buf, 0);

		if (c->makesound && (sound_options & OPT_SOUND_SEND))
			play_sound(SEND);
	} else {
		serv_chat_send(c->gc, c->id, buf);

		/* no sound because we do that when we receive our message */
	}

	quiet_set(c->bold, FALSE);
	quiet_set(c->strike, FALSE);
	quiet_set(c->italic, FALSE);
	quiet_set(c->underline, FALSE);
	quiet_set(c->font, FALSE);
	quiet_set(c->fgcolorbtn, FALSE);
	quiet_set(c->bgcolorbtn, FALSE);
	quiet_set(c->link, FALSE);

	if (general_options & OPT_GEN_BACK_ON_IM) {
		if (awaymessage != NULL) {
			do_im_back();
		} else if (c->gc->away) {
			serv_set_away(c->gc, GAIM_AWAY_CUSTOM, NULL);
		}
	}

	gtk_widget_grab_focus(c->entry);
	g_free(buf2);
	g_free(buf);
}

int entry_key_pressed(GtkWidget *w, GtkWidget *entry)
{
	check_everything(w);
	return TRUE;
}

/*------------------------------------------------------------------------*/
/*  HTML-type stuff                                                       */
/*------------------------------------------------------------------------*/

int count_tag(GtkWidget *entry, char *s1, char *s2)
{
	char *p1, *p2;
	int res = 0;
	char *tmp, *tmpo, h;
	tmpo = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
	h = tmpo[GTK_EDITABLE(entry)->current_pos];
	tmpo[GTK_EDITABLE(entry)->current_pos] = '\0';
	tmp = tmpo;
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
	tmpo[GTK_EDITABLE(entry)->current_pos] = h;
	g_free(tmpo);
	return res;
}


int invert_tags(GtkWidget *entry, char *s1, char *s2, int really)
{
	int start = GTK_EDITABLE(entry)->selection_start_pos;
	int finish = GTK_EDITABLE(entry)->selection_end_pos;
	char *s;

	s = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
	if (!strncasecmp(&s[start], s1, strlen(s1)) &&
	    !strncasecmp(&s[finish - strlen(s2)], s2, strlen(s2))) {
		if (really) {
			gtk_editable_delete_text(GTK_EDITABLE(entry), start, start + strlen(s1));
			gtk_editable_delete_text(GTK_EDITABLE(entry), finish - strlen(s2) - strlen(s1),
						 finish - strlen(s1));
		}
		g_free(s);
		return 1;
	}
	g_free(s);
	return 0;
}


void remove_tags(GtkWidget *entry, char *tag)
{
	char *s, *t;
	int start = GTK_EDITABLE(entry)->selection_start_pos;
	int finish = GTK_EDITABLE(entry)->selection_end_pos;
	int temp;
	s = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
	t = s;

	if (start > finish) {
		temp = start;
		start = finish;
		finish = temp;
	}

	if (strstr(tag, "<FONT SIZE=")) {
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
	} else {
		while ((t = strstr(t, tag))) {
			if (((t - s) < finish) && ((t - s) >= start)) {
				gtk_editable_delete_text(GTK_EDITABLE(entry), (t - s),
							 (t - s) + strlen(tag));
				g_free(s);
				s = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
				t = s;
			} else
				t++;
		}
	}
	g_free(s);
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
			*buffer_p++ = 'g';
			*buffer_p++ = 't';
			*buffer_p++ = ';';
		} else
			*buffer_p++ = *temp_p;
		++temp_p;
	}
	*buffer_p = '\0';

	return buffer_start;
}

void surround(GtkWidget *entry, char *pre, char *post)
{
	int temp, pos = GTK_EDITABLE(entry)->current_pos;
	int dummy;
	int start, finish;

	if (general_options & OPT_GEN_CHECK_SPELLING) {
		gtkspell_detach(GTK_TEXT(entry));
	}

	if (GTK_EDITABLE(entry)->has_selection) {
		remove_tags(entry, pre);
		remove_tags(entry, post);
		start = GTK_EDITABLE(entry)->selection_start_pos;
		finish = GTK_EDITABLE(entry)->selection_end_pos;
		if (start > finish) {
			dummy = finish;
			finish = start;
			start = dummy;
		}
		dummy = start;
		gtk_editable_insert_text(GTK_EDITABLE(entry), pre, strlen(pre), &dummy);
		dummy = finish + strlen(pre);
		gtk_editable_insert_text(GTK_EDITABLE(entry), post, strlen(post), &dummy);
		gtk_editable_select_region(GTK_EDITABLE(entry), start,
					   finish + strlen(pre) + strlen(post));
	} else {
		temp = pos;
		gtk_editable_insert_text(GTK_EDITABLE(entry), pre, strlen(pre), &pos);
		if (temp == pos) {
			dummy = pos + strlen(pre);
			gtk_editable_insert_text(GTK_EDITABLE(entry), post, strlen(post), &dummy);
			gtk_editable_set_position(GTK_EDITABLE(entry), dummy);
		} else {
			dummy = pos;
			gtk_editable_insert_text(GTK_EDITABLE(entry), post, strlen(post), &dummy);
			gtk_editable_set_position(GTK_EDITABLE(entry), pos);
		}
	}

	if (general_options & OPT_GEN_CHECK_SPELLING) {
		gtkspell_attach(GTK_TEXT(entry));
	}

	gtk_widget_grab_focus(entry);
}

void advance_past(GtkWidget *entry, char *pre, char *post)
{
	char *s, *s2;
	int pos;
	if (invert_tags(entry, pre, post, 1))
		return;
	s = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
	pos = GTK_EDITABLE(entry)->current_pos;
	debug_printf(_("Currently at %d, "), pos);
	s2 = strstr(&s[pos], post);
	if (s2) {
		pos = s2 - s + strlen(post);
	} else {
		gtk_editable_insert_text(GTK_EDITABLE(entry), post, strlen(post), &pos);
	}
	g_free(s);
	debug_printf(_("Setting position to %d\n"), pos);
	gtk_editable_set_position(GTK_EDITABLE(entry), pos);
	gtk_widget_grab_focus(entry);
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
		advance_past(c->entry, "<FONT COLOR>", "</FONT>");
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
		advance_past(c->entry, "<BODY BGCOLOR>", "</BODY>");
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
		advance_past(c->entry, "<FONT FACE>", "</FONT>");
}

void toggle_link(GtkWidget *linky, struct conversation *c)
{
	if (state_lock)
		return;
	if (GTK_TOGGLE_BUTTON(linky)->active)
		show_add_link(linky, c);
	else if (c->link_dialog)
		cancel_link(linky, c);
	else
		advance_past(c->entry, "<A HREF>", "</A>");
}

void do_strike(GtkWidget *strike, GtkWidget *entry)
{
	if (state_lock)
		return;

	if (GTK_TOGGLE_BUTTON(strike)->active)
		surround(entry, "<STRIKE>", "</STRIKE>");
	else
		advance_past(entry, "<STRIKE>", "</STRIKE>");

}

void do_bold(GtkWidget *bold, GtkWidget *entry)
{
	if (state_lock)
		return;
	if (GTK_TOGGLE_BUTTON(bold)->active)
		surround(entry, "<B>", "</B>");
	else
		advance_past(entry, "<B>", "</B>");
}

void do_underline(GtkWidget *underline, GtkWidget *entry)
{
	if (state_lock)
		return;
	if (GTK_TOGGLE_BUTTON(underline)->active)
		surround(entry, "<U>", "</U>");
	else
		advance_past(entry, "<U>", "</U>");
}

void do_italic(GtkWidget *italic, GtkWidget *entry)
{
	if (state_lock)
		return;
	if (GTK_TOGGLE_BUTTON(italic)->active)
		surround(entry, "<I>", "</I>");
	else
		advance_past(entry, "<I>", "</I>");
}

/* html code to modify font sizes must all be the same length, */
/* currently set to 15 chars */

void do_small(GtkWidget *small, GtkWidget *entry)
{
	if (state_lock)
		return;
	surround(entry, "<FONT SIZE=\"1\">", "</FONT>");
}

void do_normal(GtkWidget *normal, GtkWidget *entry)
{
	if (state_lock)
		return;
	surround(entry, "<FONT SIZE=\"3\">", "</FONT>");
}

void do_big(GtkWidget *big, GtkWidget *entry)
{
	if (state_lock)
		return;
	surround(entry, "<FONT SIZE=\"5\">", "</FONT>");
}

void check_everything(GtkWidget *entry)
{
	struct conversation *c;

	c = (struct conversation *)gtk_object_get_user_data(GTK_OBJECT(entry));
	if (!c)
		return;
	if (invert_tags(entry, "<B>", "</B>", 0))
		quiet_set(c->bold, TRUE);
	else if (count_tag(entry, "<B>", "</B>"))
		quiet_set(c->bold, TRUE);
	else
		quiet_set(c->bold, FALSE);
	if (invert_tags(entry, "<I>", "</I>", 0))
		quiet_set(c->italic, TRUE);
	else if (count_tag(entry, "<I>", "</I>"))
		quiet_set(c->italic, TRUE);
	else
		quiet_set(c->italic, FALSE);

	if (invert_tags(entry, "<FONT COLOR", "</FONT>", 0))
		quiet_set(c->fgcolorbtn, TRUE);
	else if (count_tag(entry, "<FONT COLOR", "</FONT>"))
		quiet_set(c->fgcolorbtn, TRUE);
	else
		quiet_set(c->fgcolorbtn, FALSE);

	if (invert_tags(entry, "<BODY BGCOLOR", "</BODY>", 0))
		quiet_set(c->bgcolorbtn, TRUE);
	else if (count_tag(entry, "<BODY BGCOLOR", "</BODY>"))
		quiet_set(c->bgcolorbtn, TRUE);
	else
		quiet_set(c->bgcolorbtn, FALSE);

	if (invert_tags(entry, "<FONT FACE", "</FONT>", 0))
		quiet_set(c->font, TRUE);
	else if (count_tag(entry, "<FONT FACE", "</FONT>"))
		quiet_set(c->font, TRUE);
	else
		quiet_set(c->font, FALSE);

	if (invert_tags(entry, "<A HREF", "</A>", 0))
		quiet_set(c->link, TRUE);
	else if (count_tag(entry, "<A HREF", "</A>"))
		quiet_set(c->link, TRUE);
	else
		quiet_set(c->link, FALSE);

	if (invert_tags(entry, "<U>", "</U>", 0))
		quiet_set(c->underline, TRUE);
	else if (count_tag(entry, "<U>", "</U>"))
		quiet_set(c->underline, TRUE);
	else
		quiet_set(c->underline, FALSE);

	if (invert_tags(entry, "<STRIKE>", "</STRIKE>", 0))
		quiet_set(c->strike, TRUE);
	else if (count_tag(entry, "<STRIKE>", "</STRIKE>"))
		quiet_set(c->strike, TRUE);
	else
		quiet_set(c->strike, FALSE);
}


/*------------------------------------------------------------------------*/
/*  Takin care of the window..                                            */
/*------------------------------------------------------------------------*/


/* this is going to be interesting since the conversation could either be a
 * normal IM conversation or a chat window. but hopefully it won't matter */
void write_to_conv(struct conversation *c, char *what, int flags, char *who)
{
	char *buf = g_malloc(BUF_LONG);
	char *str;
	FILE *fd;
	char colour[10];
	int colorv = -1;
	char *clr;
	char *smiley = g_malloc(7);
	struct buddy *b;
	int gtk_font_options = 0;
	GString *logstr;

	gtk_font_options = gtk_font_options ^ GTK_IMHTML_NO_COMMENTS;

	if (display_options & OPT_DISP_IGNORE_COLOUR)
		gtk_font_options = gtk_font_options ^ GTK_IMHTML_NO_COLOURS;

	if (display_options & OPT_DISP_IGNORE_FONTS)
		gtk_font_options = gtk_font_options ^ GTK_IMHTML_NO_FONTS;

	if (display_options & OPT_DISP_IGNORE_SIZES)
		gtk_font_options = gtk_font_options ^ GTK_IMHTML_NO_SIZES;

	if (!(general_options & OPT_GEN_STRIP_HTML))
		gtk_font_options = gtk_font_options ^ GTK_IMHTML_RETURN_LOG;

	if (!who) {
		if (flags & WFLAG_SEND) {
			b = find_buddy(c->gc, c->gc->username);
			if (b)
				who = b->show;
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

	if (flags & WFLAG_SYSTEM) {

		gtk_imhtml_append_text(GTK_IMHTML(c->text), what, 0);

		gtk_imhtml_append_text(GTK_IMHTML(c->text), "<BR>", 0);

		if ((general_options & OPT_GEN_LOG_ALL) || find_log_info(c->name)) {
			char *t1;
			char nm[256];

			if (general_options & OPT_GEN_STRIP_HTML) {
				t1 = strip_html(what);
			} else {
				t1 = what;
			}
			if (c->is_chat)
				g_snprintf(nm, 256, "%s.chat", c->name);
			else
				g_snprintf(nm, 256, "%s", c->name);
			fd = open_log_file(nm);
			if (fd > 0) {
				if (general_options & OPT_GEN_STRIP_HTML) {
					fprintf(fd, "%s\n", t1);
				} else {
					fprintf(fd, "%s<BR>\n", t1);
				}
				fclose(fd);
			}
			if (general_options & OPT_GEN_STRIP_HTML) {
				g_free(t1);
			}
		}

	} else {
		char buf2[BUF_LONG];
		if ((clr = strstr(what, "<BODY BGCOLOR=\"#")) != NULL) {
			sscanf(clr + strlen("<BODY BGCOLOR=\"#"), "%x", &colorv);
		}

		if (flags & WFLAG_WHISPER) {
			/* if we're whispering, it's not an autoresponse */
			if (meify(what)) {
				str = g_malloc(64);
				g_snprintf(str, 62, "***%s", who);
				strcpy(colour, "#6C2585\0");
			} else {
				str = g_malloc(64);
				g_snprintf(str, 62, "*%s*:", who);
				strcpy(colour, "#00ff00\0");
			}
		} else {
			if (meify(what)) {
				str = g_malloc(64);
				if (flags & WFLAG_AUTO)
					g_snprintf(str, 62, "%s ***%s", AUTO_RESPONSE, who);
				else
					g_snprintf(str, 62, "***%s", who);
				strcpy(colour, "#062585\0");
			} else {
				str = g_malloc(64);
				if (flags & WFLAG_AUTO)
					g_snprintf(str, 62, "%s %s", who, AUTO_RESPONSE);
				else
					g_snprintf(str, 62, "%s:", who);
				if (flags & WFLAG_RECV)
					strcpy(colour, "#ff0000");
				else if (flags & WFLAG_SEND)
					strcpy(colour, "#0000ff");
			}
		}

		if (general_options & OPT_DISP_SHOW_TIME)
			g_snprintf(buf, BUF_LONG, "<FONT COLOR=\"%s\"><FONT SIZE=\"2\">(%s) </FONT>"
					"<B>%s</B></FONT> ", colour, date(), str);
		else
			g_snprintf(buf, BUF_LONG, "<FONT COLOR=\"%s\"><B>%s</B></FONT> ", colour, str);
		g_snprintf(buf2, BUF_LONG, "<FONT COLOR=\"%s\"><FONT SIZE=\"2\"><!--(%s) --></FONT>"
				"<B>%s</B></FONT> ", colour, date(), str);

		g_free(str);

		gtk_imhtml_append_text(GTK_IMHTML(c->text), buf2, 0);

		logstr = gtk_imhtml_append_text(GTK_IMHTML(c->text), what, gtk_font_options);

		gtk_imhtml_append_text(GTK_IMHTML(c->text), "<BR>", 0);

		if ((general_options & OPT_GEN_LOG_ALL) || find_log_info(c->name)) {
			char *t1, *t2;
			char *nm = g_malloc(256);
			if (c->is_chat)
				g_snprintf(nm, 256, "%s.chat", c->name);
			else
				g_snprintf(nm, 256, "%s", c->name);

			if (general_options & OPT_GEN_STRIP_HTML) {
				t1 = strip_html(buf);
				t2 = strip_html(what);
			} else {
				t1 = html_logize(buf);
				t2 = html_logize(what);
			}
			fd = open_log_file(nm);
			if (fd > 0) {
				if (general_options & OPT_GEN_STRIP_HTML) {
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
	}

/*        if (!GTK_WIDGET_MAPPED(c->window)) {
                
                if (dark_icon_pm == NULL)
                        dark_icon_pm = gdk_pixmap_create_from_xpm_d(c->window->window, &dark_icon_bm,
                                                                    NULL, (gchar **)aimicon2_xpm);
                gdk_window_set_icon(c->window->window, NULL, dark_icon_pm, dark_icon_bm);
	}
*/

	if ((c->is_chat && (general_options & OPT_GEN_POPUP_CHAT)) ||
	    (!c->is_chat && (general_options & OPT_GEN_POPUP_WINDOWS)))
		    gdk_window_show(c->window->window);

	g_free(smiley);
	g_free(buf);
}



GtkWidget *build_conv_toolbar(struct conversation *c)
{
	GdkPixmap *strike_i, *small_i, *normal_i, *big_i, *bold_i, *italic_i, *underline_i, *speaker_i,
	    *wood_i, *fgcolor_i, *bgcolor_i, *link_i, *font_i, *smiley_i;
	GtkWidget *strike_p, *small_p, *normal_p, *big_p, *bold_p, *italic_p, *underline_p, *speaker_p,
	    *wood_p, *fgcolor_p, *bgcolor_p, *link_p, *font_p, *smiley_p;
	GtkWidget *strike, *small, *normal, *big, *bold, *italic, *underline, *speaker, *wood,
	    *fgcolorbtn, *bgcolorbtn, *link, *font, *smiley;
	GdkBitmap *mask;
	GtkWidget *toolbar;
	GtkWidget *win;
	GtkWidget *entry;

	toolbar = gtk_toolbar_new(GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_ICONS);
	win = c->window;
	entry = c->entry;

	link_i = gdk_pixmap_create_from_xpm_d(win->window, &mask, &win->style->white, link_xpm);
	link_p = gtk_pixmap_new(link_i, mask);
	gtk_widget_show(link_p);
	gdk_bitmap_unref(mask);

	fgcolor_i = gdk_pixmap_create_from_xpm_d(win->window, &mask, &win->style->white, fgcolor_xpm);
	fgcolor_p = gtk_pixmap_new(fgcolor_i, mask);
	gtk_widget_show(fgcolor_p);
	gdk_bitmap_unref(mask);

	bgcolor_i = gdk_pixmap_create_from_xpm_d(win->window, &mask, &win->style->white, bgcolor_xpm);
	bgcolor_p = gtk_pixmap_new(bgcolor_i, mask);
	gtk_widget_show(bgcolor_p);
	gdk_bitmap_unref(mask);

	wood_i = gdk_pixmap_create_from_xpm_d(win->window, &mask, &win->style->white, wood_xpm);
	wood_p = gtk_pixmap_new(wood_i, mask);
	gtk_widget_show(wood_p);
	gdk_bitmap_unref(mask);
	speaker_i = gdk_pixmap_create_from_xpm_d(win->window, &mask, &win->style->white, speaker_xpm);
	speaker_p = gtk_pixmap_new(speaker_i, mask);
	gtk_widget_show(speaker_p);
	gdk_bitmap_unref(mask);
	c->makesound = 1;
	strike_i = gdk_pixmap_create_from_xpm_d(win->window, &mask, &win->style->white, strike_xpm);
	strike_p = gtk_pixmap_new(strike_i, mask);
	gtk_widget_show(strike_p);
	gdk_bitmap_unref(mask);
	bold_i = gdk_pixmap_create_from_xpm_d(win->window, &mask, &win->style->white, bold_xpm);
	bold_p = gtk_pixmap_new(bold_i, mask);
	gtk_widget_show(bold_p);
	gdk_bitmap_unref(mask);
	italic_i = gdk_pixmap_create_from_xpm_d(win->window, &mask, &win->style->white, italic_xpm);
	italic_p = gtk_pixmap_new(italic_i, mask);
	gtk_widget_show(italic_p);
	gdk_bitmap_unref(mask);
	underline_i = gdk_pixmap_create_from_xpm_d(win->window, &mask,
						   &win->style->white, underline_xpm);
	underline_p = gtk_pixmap_new(underline_i, mask);
	gtk_widget_show(underline_p);
	gdk_bitmap_unref(mask);
	small_i = gdk_pixmap_create_from_xpm_d(win->window, &mask, &win->style->white, small_xpm);
	small_p = gtk_pixmap_new(small_i, mask);
	gtk_widget_show(small_p);
	gdk_bitmap_unref(mask);
	normal_i = gdk_pixmap_create_from_xpm_d(win->window, &mask, &win->style->white, normal_xpm);
	normal_p = gtk_pixmap_new(normal_i, mask);
	gtk_widget_show(normal_p);
	gdk_bitmap_unref(mask);
	big_i = gdk_pixmap_create_from_xpm_d(win->window, &mask, &win->style->white, big_xpm);
	big_p = gtk_pixmap_new(big_i, mask);
	gtk_widget_show(big_p);
	gdk_bitmap_unref(mask);
	font_i = gdk_pixmap_create_from_xpm_d(win->window, &mask, &win->style->white, fontface_xpm);
	font_p = gtk_pixmap_new(font_i, mask);
	gtk_widget_show(font_p);
	gdk_bitmap_unref(mask);
	smiley_i = gdk_pixmap_create_from_xpm_d(win->window, &mask, &win->style->white, smile_icon_xpm);
	smiley_p = gtk_pixmap_new(smiley_i, mask);
	gtk_widget_show(smiley_p);
	gdk_bitmap_unref(mask);

	bold = gtk_toolbar_append_element(GTK_TOOLBAR(toolbar),
					  GTK_TOOLBAR_CHILD_TOGGLEBUTTON, NULL,
					  _("Bold"), _("Bold Text"), _("Bold"), bold_p,
					  GTK_SIGNAL_FUNC(do_bold), entry);
	italic = gtk_toolbar_append_element(GTK_TOOLBAR(toolbar),
					    GTK_TOOLBAR_CHILD_TOGGLEBUTTON,
					    NULL, _("Italics"), _("Italics Text"),
					    _("Italics"), italic_p, GTK_SIGNAL_FUNC(do_italic), entry);
	underline = gtk_toolbar_append_element(GTK_TOOLBAR(toolbar),
					       GTK_TOOLBAR_CHILD_TOGGLEBUTTON,
					       NULL, _("Underline"), _("Underline Text"),
					       _("Underline"), underline_p,
					       GTK_SIGNAL_FUNC(do_underline), entry);
	strike =
	    gtk_toolbar_append_element(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_CHILD_TOGGLEBUTTON, NULL,
				       _("Strike"), _("Strike through Text"), _("Strike"), strike_p,
				       GTK_SIGNAL_FUNC(do_strike), entry);
	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));
	small = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					_("Small"), _("Decrease font size"), _("Small"),
					small_p, GTK_SIGNAL_FUNC(do_small), entry);
	normal = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					 _("Normal"), _("Normal font size"), _("Normal"),
					 normal_p, GTK_SIGNAL_FUNC(do_normal), entry);
	big = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
				      _("Big"), _("Increase font size"), _("Big"),
				      big_p, GTK_SIGNAL_FUNC(do_big), entry);

	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));

	font = gtk_toolbar_append_element(GTK_TOOLBAR(toolbar),
					  GTK_TOOLBAR_CHILD_TOGGLEBUTTON,
					  NULL, _("Font"), _("Select Font"),
					  _("Font"), font_p, GTK_SIGNAL_FUNC(toggle_font), c);
	fgcolorbtn = gtk_toolbar_append_element(GTK_TOOLBAR(toolbar),
						GTK_TOOLBAR_CHILD_TOGGLEBUTTON,
						NULL, _("Color"), _("Text Color"),
						_("Color"), fgcolor_p, GTK_SIGNAL_FUNC(toggle_fg_color),
						c);
	bgcolorbtn =
	    gtk_toolbar_append_element(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_CHILD_TOGGLEBUTTON, NULL,
				       _("Color"), _("Background Color"), _("Color"), bgcolor_p,
				       GTK_SIGNAL_FUNC(toggle_bg_color), c);

	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));

	link = gtk_toolbar_append_element(GTK_TOOLBAR(toolbar),
					  GTK_TOOLBAR_CHILD_TOGGLEBUTTON,
					  NULL, _("Link"), _("Insert Link"),
					  _("Link"), link_p, GTK_SIGNAL_FUNC(toggle_link), c);
	smiley = gtk_toolbar_append_element(GTK_TOOLBAR(toolbar),
					    GTK_TOOLBAR_CHILD_TOGGLEBUTTON,
					    NULL, _("Smiley"), _("Insert smiley face"), _("Smiley"),
					    smiley_p, GTK_SIGNAL_FUNC(insert_smiley), c);

	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));

	wood = gtk_toolbar_append_element(GTK_TOOLBAR(toolbar),
					  GTK_TOOLBAR_CHILD_TOGGLEBUTTON,
					  NULL, _("Logging"), _("Enable logging"),
					  _("Logging"), wood_p, GTK_SIGNAL_FUNC(toggle_loggle), c);
	speaker = gtk_toolbar_append_element(GTK_TOOLBAR(toolbar),
					     GTK_TOOLBAR_CHILD_TOGGLEBUTTON,
					     NULL, _("Sound"), _("Enable sounds"),
					     _("Sound"), speaker_p, GTK_SIGNAL_FUNC(set_option),
					     &c->makesound);
	c->makesound = 0;
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(speaker), TRUE);

	state_lock = 1;
	if (find_log_info(c->name))
		 gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(wood), TRUE);
	else
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(wood), FALSE);
	state_lock = 0;

	/* use a slicker look if the user wants to */
	if (display_options & OPT_DISP_COOL_LOOK) {
		gtk_button_set_relief(GTK_BUTTON(strike), GTK_RELIEF_NONE);
		gtk_button_set_relief(GTK_BUTTON(normal), GTK_RELIEF_NONE);
		gtk_button_set_relief(GTK_BUTTON(big), GTK_RELIEF_NONE);
		gtk_button_set_relief(GTK_BUTTON(bold), GTK_RELIEF_NONE);
		gtk_button_set_relief(GTK_BUTTON(italic), GTK_RELIEF_NONE);
		gtk_button_set_relief(GTK_BUTTON(underline), GTK_RELIEF_NONE);
		gtk_button_set_relief(GTK_BUTTON(speaker), GTK_RELIEF_NONE);
		gtk_button_set_relief(GTK_BUTTON(wood), GTK_RELIEF_NONE);
		gtk_button_set_relief(GTK_BUTTON(fgcolorbtn), GTK_RELIEF_NONE);
		gtk_button_set_relief(GTK_BUTTON(bgcolorbtn), GTK_RELIEF_NONE);
		gtk_button_set_relief(GTK_BUTTON(link), GTK_RELIEF_NONE);
		gtk_button_set_relief(GTK_BUTTON(font), GTK_RELIEF_NONE);
		gtk_button_set_relief(GTK_BUTTON(small), GTK_RELIEF_NONE);
		gtk_button_set_relief(GTK_BUTTON(smiley), GTK_RELIEF_NONE);
	}

	gtk_widget_show(toolbar);

	gdk_pixmap_unref(link_i);
	gdk_pixmap_unref(fgcolor_i);
	gdk_pixmap_unref(bgcolor_i);
	gdk_pixmap_unref(wood_i);
	gdk_pixmap_unref(speaker_i);
	gdk_pixmap_unref(strike_i);
	gdk_pixmap_unref(bold_i);
	gdk_pixmap_unref(italic_i);
	gdk_pixmap_unref(underline_i);
	gdk_pixmap_unref(small_i);
	gdk_pixmap_unref(normal_i);
	gdk_pixmap_unref(big_i);
	gdk_pixmap_unref(font_i);
	gdk_pixmap_unref(smiley_i);

	c->bold = bold;
	c->strike = strike;
	c->italic = italic;
	c->underline = underline;
	c->log_button = wood;
	c->fgcolorbtn = fgcolorbtn;
	c->bgcolorbtn = bgcolorbtn;
	c->link = link;
	c->wood = wood;
	c->font = font;
	c->smiley = smiley;

	gtk_widget_set_sensitive(c->log_button, ((general_options & OPT_GEN_LOG_ALL)) ? FALSE : TRUE);

	gtk_widget_set_sensitive(c->bold, ((font_options & OPT_FONT_BOLD)) ? FALSE : TRUE);
	gtk_widget_set_sensitive(c->italic, ((font_options & OPT_FONT_ITALIC)) ? FALSE : TRUE);
	gtk_widget_set_sensitive(c->underline, ((font_options & OPT_FONT_UNDERLINE)) ? FALSE : TRUE);
	gtk_widget_set_sensitive(c->strike, ((font_options & OPT_FONT_STRIKE)) ? FALSE : TRUE);

	return toolbar;
}

static void convo_sel_send(GtkObject * m, struct gaim_connection *c)
{
	struct conversation *cnv = gtk_object_get_user_data(m);
	cnv->gc = c;

	update_buttons_by_protocol(cnv);
}

void update_convo_add_button(struct conversation *c)
{
	int dispstyle = set_dispstyle(0);
	GtkWidget *parent = c->add->parent;
	gtk_widget_destroy(c->add);

	if (find_buddy(c->gc, c->name)) {
		c->add = picture_button2(c->window, _("Remove"), gnome_remove_xpm, dispstyle);

		if (c->gc) 
		{
			if (c->gc->prpl->remove_buddy == NULL)
				gtk_widget_set_sensitive(c->add, FALSE);
			else
				gtk_widget_set_sensitive(c->add, TRUE);
		}
	} else {
		c->add = picture_button2(c->window, _("Add"), gnome_add_xpm, dispstyle);
		if (c->gc) 
		{
			if (c->gc->prpl->add_buddy == NULL)
				gtk_widget_set_sensitive(c->add, FALSE);
			else
				gtk_widget_set_sensitive(c->add, TRUE);
		}
	}

	if (!c->gc)
		gtk_widget_set_sensitive(c->add, FALSE);

	gtk_signal_connect(GTK_OBJECT(c->add), "clicked", GTK_SIGNAL_FUNC(add_callback), c);
	gtk_box_pack_end(GTK_BOX(parent), c->add, dispstyle, dispstyle, 0);
	gtk_box_reorder_child(GTK_BOX(parent), c->add, 2);
	gtk_widget_show(c->add);
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
			g_snprintf(buf, sizeof buf, "%s (%s)", c->username, (*c->prpl->name)());
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
		create_convo_menu(C);

		if (connections)
			C->gc = (struct gaim_connection *)connections->data;
		else
			C->gc = NULL;

		update_buttons_by_protocol(C);

		c = c->next;
	}
}

void update_buttons_by_protocol(struct conversation *c)
{
	if (!c->gc)
	{
		gtk_widget_set_sensitive(c->info, FALSE);
		gtk_widget_set_sensitive(c->send, FALSE);
		gtk_widget_set_sensitive(c->warn, FALSE);
		gtk_widget_set_sensitive(c->block, FALSE);
		gtk_widget_set_sensitive(c->add, FALSE);

		return;
	}
	
	if (c->gc->prpl->set_info == NULL && c->info)
		gtk_widget_set_sensitive(c->info, FALSE);
	else if (c->info)
		gtk_widget_set_sensitive(c->info, TRUE);

	if (c->gc->prpl->send_im == NULL && c->send)
		gtk_widget_set_sensitive(c->send, FALSE);
	else
		gtk_widget_set_sensitive(c->send, TRUE);

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

	if (c->whisper) 
	{
		if (c->gc->prpl->chat_whisper == NULL)
			gtk_widget_set_sensitive(c->whisper, FALSE);
		else
			gtk_widget_set_sensitive(c->whisper, TRUE);
	}

	if (c->invite) 
	{
		if (c->gc->prpl->chat_invite == NULL)
			gtk_widget_set_sensitive(c->invite, FALSE);
		else
			gtk_widget_set_sensitive(c->invite, TRUE);
	}
}


void show_conv(struct conversation *c)
{
	GtkWidget *win;
	char buf[256];
	GtkWidget *text;
	GtkWidget *sw;
	GtkWidget *send;
	GtkWidget *info;
	GtkWidget *warn;
	GtkWidget *block;
	GtkWidget *close;
	GtkWidget *entry;
	GtkWidget *bbox;
	GtkWidget *vbox;
	GtkWidget *vbox2;
	GtkWidget *paned;
	GtkWidget *add;
	GtkWidget *toolbar;
	GtkWidget *hbox;
	GtkWidget *label;
	int dispstyle = set_dispstyle(0);

	c->font_dialog = NULL;
	c->fg_color_dialog = NULL;
	c->bg_color_dialog = NULL;
	c->smiley_dialog = NULL;
	c->link_dialog = NULL;
	c->log_dialog = NULL;
	sprintf(c->fontface, "%s", fontface);
	c->hasfont = 0;
	c->fontsize = fontsize;
	c->hassize = 0;
	c->bgcol = bgcolor;
	c->hasbg = 0;
	c->fgcol = fgcolor;
	c->hasfg = 0;

	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	c->window = win;
	gtk_object_set_user_data(GTK_OBJECT(win), c);
	gtk_window_set_wmclass(GTK_WINDOW(win), "conversation", "Gaim");
	gtk_window_set_policy(GTK_WINDOW(win), TRUE, TRUE, TRUE);
	gtk_container_border_width(GTK_CONTAINER(win), 10);
	gtk_widget_realize(win);
	aol_icon(win->window);
	if ((find_log_info(c->name)) || ((general_options & OPT_GEN_LOG_ALL)))
		 g_snprintf(buf, sizeof(buf), LOG_CONVERSATION_TITLE, c->name);
	else
		g_snprintf(buf, sizeof(buf), CONVERSATION_TITLE, c->name);
	gtk_window_set_title(GTK_WINDOW(win), buf);
	gtk_signal_connect(GTK_OBJECT(win), "delete_event", GTK_SIGNAL_FUNC(delete_event_convo), c);

	paned = gtk_vpaned_new();
	gtk_paned_set_gutter_size(GTK_PANED(paned), 15);
	gtk_container_add(GTK_CONTAINER(win), paned);
	gtk_widget_show(paned);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_paned_pack1(GTK_PANED(paned), vbox, FALSE, TRUE);
	gtk_widget_show(vbox);

	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
	gtk_widget_set_usize(sw, 320, 175);
	gtk_widget_show(sw);

	text = gtk_imhtml_new(NULL, NULL);
	c->text = text;
	gtk_container_add(GTK_CONTAINER(sw), text);
	GTK_LAYOUT(text)->hadjustment->step_increment = 10.0;
	GTK_LAYOUT(text)->vadjustment->step_increment = 10.0;
	if (!(display_options & OPT_DISP_SHOW_SMILEY))
		gtk_imhtml_show_smileys(GTK_IMHTML(text), FALSE);
	if (display_options & OPT_DISP_SHOW_TIME)
		gtk_imhtml_show_comments(GTK_IMHTML(text), TRUE);
	gtk_signal_connect(GTK_OBJECT(text), "url_clicked", GTK_SIGNAL_FUNC(open_url_nw), NULL);
	gtk_imhtml_associate_smiley(GTK_IMHTML(text), "C:)", luke03_xpm);
	gtk_imhtml_associate_smiley(GTK_IMHTML(text), "C:-)", luke03_xpm);
	gtk_imhtml_associate_smiley(GTK_IMHTML(text), "O-)", oneeye_xpm);
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

	entry = gtk_text_new(NULL, NULL);
	c->entry = entry;

	toolbar = build_conv_toolbar(c);
	gtk_box_pack_start(GTK_BOX(vbox2), toolbar, FALSE, FALSE, 0);

	gtk_object_set_user_data(GTK_OBJECT(entry), c);
	gtk_text_set_editable(GTK_TEXT(entry), TRUE);
	gtk_text_set_word_wrap(GTK_TEXT(entry), TRUE);
	if (display_options & OPT_DISP_CONV_BIG_ENTRY)
		gtk_widget_set_usize(entry, 300, 50);
	else
		gtk_widget_set_usize(entry, 300, 25);
	gtk_window_set_focus(GTK_WINDOW(win), entry);
	gtk_signal_connect(GTK_OBJECT(entry), "activate", GTK_SIGNAL_FUNC(send_callback), c);
	gtk_signal_connect(GTK_OBJECT(entry), "key_press_event", GTK_SIGNAL_FUNC(keypress_callback), c);
	gtk_signal_connect(GTK_OBJECT(entry), "key_press_event", GTK_SIGNAL_FUNC(entry_key_pressed),
			   entry);
	if (general_options & OPT_GEN_CHECK_SPELLING)
		gtkspell_attach(GTK_TEXT(c->entry));
	gtk_box_pack_start(GTK_BOX(vbox2), entry, TRUE, TRUE, 0);
	gtk_widget_show(entry);

	bbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox2), bbox, FALSE, FALSE, 0);
	gtk_widget_show(bbox);

	close = picture_button2(win, _("Close"), cancel_xpm, dispstyle);
	c->close = close;
	gtk_object_set_user_data(GTK_OBJECT(close), c);
	gtk_signal_connect(GTK_OBJECT(close), "clicked", GTK_SIGNAL_FUNC(close_callback), c);
	gtk_box_pack_end(GTK_BOX(bbox), close, dispstyle, dispstyle, 0);
	gtk_widget_show(close);

	c->sep1 = gtk_vseparator_new();
	gtk_box_pack_end(GTK_BOX(bbox), c->sep1, dispstyle, dispstyle, 0);
	gtk_widget_show(c->sep1);

	if (c->gc && find_buddy(c->gc, c->name) != NULL)
		 add = picture_button2(win, _("Remove"), gnome_remove_xpm, dispstyle);
	else
		add = picture_button2(win, _("Add"), gnome_add_xpm, dispstyle);
	c->add = add;
	gtk_signal_connect(GTK_OBJECT(add), "clicked", GTK_SIGNAL_FUNC(add_callback), c);
	gtk_box_pack_end(GTK_BOX(bbox), add, dispstyle, dispstyle, 0);
	gtk_widget_show(add);

	block = picture_button2(win, _("Block"), block_xpm, dispstyle);
	c->block = block;
	gtk_signal_connect(GTK_OBJECT(block), "clicked", GTK_SIGNAL_FUNC(block_callback), c);
	gtk_box_pack_end(GTK_BOX(bbox), block, dispstyle, dispstyle, 0);
	gtk_widget_show(block);

	warn = picture_button2(win, _("Warn"), warn_xpm, dispstyle);
	c->warn = warn;
	gtk_signal_connect(GTK_OBJECT(warn), "clicked", GTK_SIGNAL_FUNC(warn_callback), c);
	gtk_box_pack_end(GTK_BOX(bbox), warn, dispstyle, dispstyle, 0);
	gtk_widget_show(warn);

	info = picture_button2(win, _("Info"), tb_search_xpm, dispstyle);
	c->info = info;

	gtk_signal_connect(GTK_OBJECT(info), "clicked", GTK_SIGNAL_FUNC(info_callback), c);
	gtk_box_pack_end(GTK_BOX(bbox), info, dispstyle, dispstyle, 0);
	gtk_widget_show(info);

	c->sep2 = gtk_vseparator_new();
	gtk_box_pack_end(GTK_BOX(bbox), c->sep2, dispstyle, dispstyle, 0);
	gtk_widget_show(c->sep2);

	send = picture_button2(win, _("Send"), tmp_send_xpm, dispstyle);
	c->send = send;
	gtk_signal_connect(GTK_OBJECT(send), "clicked", GTK_SIGNAL_FUNC(send_callback), c);
	gtk_box_pack_end(GTK_BOX(bbox), send, dispstyle, dispstyle, 0);
	gtk_widget_show(send);

	update_buttons_by_protocol(c);

	gtk_widget_show(win);
}


void toggle_spellchk()
{
	GList *cnv = conversations;
	GSList *cht;
	struct conversation *c;
	GSList *con = connections;
	struct gaim_connection *gc;

	while (cnv) {
		c = (struct conversation *)cnv->data;
		if (general_options & OPT_GEN_CHECK_SPELLING)
			gtkspell_attach(GTK_TEXT(c->entry));
		else
			gtkspell_detach(GTK_TEXT(c->entry));
		cnv = cnv->next;
	}

	while (con) {
		gc = (struct gaim_connection *)con->data;
		cht = gc->buddy_chats;
		while (cht) {
			c = (struct conversation *)cht->data;
			if (general_options & OPT_GEN_CHECK_SPELLING)
				gtkspell_attach(GTK_TEXT(c->entry));
			else
				gtkspell_detach(GTK_TEXT(c->entry));
			cht = cht->next;
		}
		con = con->next;
	}
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
		if (display_options & OPT_DISP_SHOW_TIME)
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
			if (display_options & OPT_DISP_SHOW_TIME)
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
		if (display_options & OPT_DISP_SHOW_SMILEY)
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
			if (display_options & OPT_DISP_SHOW_SMILEY)
				gtk_imhtml_show_smileys(GTK_IMHTML(c->text), TRUE);
			else
				gtk_imhtml_show_smileys(GTK_IMHTML(c->text), FALSE);
			cht = cht->next;
		}
		con = con->next;
	}
}
