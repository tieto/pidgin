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
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include "gaim.h"
#include "gtkhtml.h"
#include <gdk/gdkkeysyms.h>
#include "pixmaps/underline.xpm"
#include "pixmaps/bold.xpm"
#include "pixmaps/italic.xpm"
#include "pixmaps/small.xpm"
#include "pixmaps/normal.xpm"
#include "pixmaps/big.xpm"
#include "pixmaps/speaker.xpm"
#include "pixmaps/aimicon2.xpm"
#include "pixmaps/wood.xpm"
#include "pixmaps/palette.xpm"
#include "pixmaps/link.xpm"
#include "pixmaps/strike.xpm"

int state_lock=0;

GdkPixmap *dark_icon_pm = NULL;
GdkBitmap *dark_icon_bm = NULL;


void check_everything(GtkWidget *entry);
gboolean user_keypress_callback(GtkWidget *entry, GdkEventKey *event,  struct conversation *c);


/*------------------------------------------------------------------------*/
/*  Helpers                                                               */
/*------------------------------------------------------------------------*/


void quiet_set(GtkWidget *tb, int state)
{
	state_lock=1;
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(tb), state);
	state_lock=0;
}


void set_state_lock(int i)
{
        state_lock = i;
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

		fd = open_log_file(c);
		if (!(general_options & OPT_GEN_STRIP_HTML))
			fprintf(fd, "<HR><BR><H3 Align=Center> ---- New Conversation @ %s ----</H3><BR>\n", full_date()); 
		else
			fprintf(fd, " ---- New Conversation @ %s ----\n", full_date()); 

		fclose(fd);
	}

        show_conv(c);
        conversations = g_list_append(conversations, c);
        return c;
}


struct conversation *find_conversation(char *name)
{
        char *cuser = g_malloc(64);
        struct conversation *c;
        GList *cnv = conversations;
        
        strcpy(cuser, normalize(name));

        while(cnv) {
                c = (struct conversation *)cnv->data;
                if(!strcasecmp(cuser, normalize(c->name))) {
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

        while(lc) {
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

void delete_conversation(struct conversation *cnv)
{
        conversations = g_list_remove(conversations, cnv);
        g_free(cnv);
}

void update_log_convs()
{
	GList *cnv = conversations;
	struct conversation *c;

	while(cnv) {
		c = (struct conversation *)cnv->data;

		if (c->log_button)
			gtk_widget_set_sensitive(c->log_button, ((general_options & OPT_GEN_LOG_ALL)) ? FALSE : TRUE);

		cnv = cnv->next;
	}
}

void update_font_buttons()
{
	GList *cnv = conversations;
	struct conversation *c;

	while (cnv) {
		c = (struct conversation *)cnv->data;

		if (c->bold)
			gtk_widget_set_sensitive(c->bold, ((font_options & OPT_FONT_BOLD)) ? FALSE : TRUE);

                if (c->italic)
                        gtk_widget_set_sensitive(c->italic, ((font_options & OPT_FONT_ITALIC)) ? FALSE : TRUE);

                if (c->underline)
                        gtk_widget_set_sensitive(c->underline, ((font_options & OPT_FONT_UNDERLINE)) ? FALSE : TRUE);

                if (c->strike)
                        gtk_widget_set_sensitive(c->strike, ((font_options & OPT_FONT_STRIKE)) ? FALSE : TRUE);

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

void toggle_loggle(GtkWidget *w, struct conversation *p)
{
        if (state_lock)
                return;
        
        if (find_log_info(p->name))
                rm_log(find_log_info(p->name));
        else 
		show_log_dialog(p->name);
}


static int close_callback(GtkWidget *widget, struct conversation *c)
{
        gtk_widget_destroy(c->window);
        delete_conversation(c);
        return TRUE;
}

static gint delete_event_convo(GtkWidget *w, GdkEventAny *e, struct conversation *c)
{
	delete_conversation(c);
	return FALSE;
}
	

static void color_callback(GtkWidget *widget, struct conversation *c)
{
	/* show_color_dialog(c); */
	gtk_widget_grab_focus(c->entry);
}

static void add_callback(GtkWidget *widget, struct conversation *c)
{
	if (find_buddy(c->name) != NULL) {
		sprintf(debug_buff,"Removing '%s' from buddylist.\n", c->name);
		debug_print(debug_buff);
		remove_buddy(find_group_by_buddy(c->name), find_buddy(c->name));
		build_edit_tree();
		gtk_label_set_text(GTK_LABEL(GTK_BIN(c->add_button)->child), "Add");
	}
	else
	{
        	show_add_buddy(c->name, NULL);
	}

	gtk_widget_grab_focus(c->entry);
}


static void block_callback(GtkWidget *widget, struct conversation *c)
{
        show_add_perm(c->name);
	gtk_widget_grab_focus(c->entry);
}

static void warn_callback(GtkWidget *widget, struct conversation *c)
{
        show_warn_dialog(c->name);
	gtk_widget_grab_focus(c->entry);
}

static void info_callback(GtkWidget *widget, struct conversation *c)
{
        serv_get_info(c->name);
	gtk_widget_grab_focus(c->entry);
}

gboolean user_keypress_callback(GtkWidget *entry, GdkEventKey *event,  struct conversation *c)
{
  int pos;
  if(event->keyval==GDK_Return) {
    if(!(event->state & GDK_SHIFT_MASK)){
      gtk_signal_emit_by_name(GTK_OBJECT(entry), "activate", c);
      //to stop the putting in of the enter character
      gtk_signal_emit_stop_by_name(GTK_OBJECT(entry), "key_press_event");
    } else {
      gtk_signal_emit_stop_by_name(GTK_OBJECT(entry), "key_press_event");
      pos=gtk_editable_get_position(GTK_EDITABLE(entry));
      gtk_editable_insert_text(GTK_EDITABLE(entry), "\n", 1, &pos);
    }
  }

  return TRUE;

}


static void send_callback(GtkWidget *widget, struct conversation *c)
{
        char buf[BUF_LEN*4];
	char *buf2;
	gchar *buf4;
        int hdrlen;

	buf4 = gtk_editable_get_chars(GTK_EDITABLE(c->entry), 0, -1);
	g_snprintf(buf, BUF_LONG, "%s", buf4);
	g_free(buf4);

        if (!strlen(buf)) {
                return;
        }

        if (general_options & OPT_GEN_SEND_LINKS) {
                linkify_text(buf);
        }
	
        /* Let us determine how long the message CAN be.
         * toc_send_im is 11 chars long + 2 quotes.
	 * + 2 spaces + 6 for the header + 2 for good
	 * measure = 23 bytes + the length of normalize c->name */

	buf2 = g_malloc(BUF_LONG);
	
        hdrlen = 23 + strlen(normalize(c->name));

/*	printf("%d %d %d\n", strlen(buf), hdrlen, BUF_LONG);*/

        if (font_options & OPT_FONT_BOLD) {
                g_snprintf(buf2, BUF_LONG, "<B>%s</B>", buf);
                strcpy(buf, buf2);
        }

        if (font_options & OPT_FONT_ITALIC) {
                g_snprintf(buf2, BUF_LONG, "<I>%s</I>", buf);
                strcpy(buf, buf2);
        }

        if (font_options & OPT_FONT_UNDERLINE) {
                g_snprintf(buf2, BUF_LONG, "<U>%s</U>", buf);
                strcpy(buf, buf2);
        }

        if (font_options & OPT_FONT_STRIKE) {
                g_snprintf(buf2, BUF_LONG, "<STRIKE>%s</STRIKE>", buf);
                strcpy(buf, buf2);
        }
        
	write_to_conv(c, buf, WFLAG_SEND);

	gtk_editable_delete_text(GTK_EDITABLE(c->entry), 0, -1);

	escape_text(buf);
	if (escape_message(buf) > MSG_LEN - hdrlen) {
		do_error_dialog("Message too long, some data truncated.", "Error");
	}

        serv_send_im(c->name, buf, 0);
        
	quiet_set(c->bold, FALSE);
	quiet_set(c->strike, FALSE);
	quiet_set(c->italic, FALSE);
	quiet_set(c->underline, FALSE);
	quiet_set(c->palette, FALSE);
        quiet_set(c->link, FALSE);
        
	if (c->makesound && (sound_options & OPT_SOUND_SEND))
		play_sound(SEND);

	if (awaymessage != NULL) {
		do_im_back();
	}


	gtk_widget_grab_focus(c->entry);

        g_free(buf2);

}

static int
entry_key_pressed(GtkWidget *w, GtkWidget *entry)
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
	int res=0;
	char *tmp, *tmpo, h;
	tmpo = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1); 
	h = tmpo[GTK_EDITABLE(entry)->current_pos];
	tmpo[GTK_EDITABLE(entry)->current_pos]='\0';
	tmp=tmpo;
	do {
		p1 = strstr(tmp, s1);
		p2 = strstr(tmp, s2);
		if (p1 && p2) {
			if (p1 < p2) {
				res=1;
				tmp = p1 +strlen(s1);
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
	tmpo[GTK_EDITABLE(entry)->current_pos]=h;
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
			gtk_editable_delete_text(GTK_EDITABLE(entry), finish - strlen(s2) - strlen(s1), finish - strlen(s1));
		}
		g_free(s);
		return 1;
	}
	g_free(s);
	return 0;
}


void remove_tags(GtkWidget *entry, char *tag)
{
	char *s;
	char *t;
	int start = GTK_EDITABLE(entry)->selection_start_pos;
	int finish = GTK_EDITABLE(entry)->selection_end_pos;
	s = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1); 
	t = s;
	while((t = strstr(t, tag))) {
		if (((t-s) < finish) && ((t-s) >= start)) { 
			gtk_editable_delete_text(GTK_EDITABLE(entry), (t-s), (t-s) + strlen(tag));
		}
		else t++;
	}
	g_free(s);
}

void surround(GtkWidget *entry, char *pre, char *post)
{
	int pos = GTK_EDITABLE(entry)->current_pos;
	int dummy;
	int start, finish;
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
		gtk_editable_select_region(GTK_EDITABLE(entry), start, finish + strlen(pre) + strlen(post));
	} else {
		gtk_editable_insert_text(GTK_EDITABLE(entry), pre, strlen(pre), &pos);
		dummy = pos;
		gtk_editable_insert_text(GTK_EDITABLE(entry), post, strlen(post), &dummy);
		gtk_editable_set_position(GTK_EDITABLE(entry), pos);
	}
	gtk_widget_grab_focus(entry);
}

static void advance_past(GtkWidget *entry, char *pre, char *post)
{
	char *s, *s2;
	int pos;
	if (invert_tags(entry, pre, post, 1))
		return;
	s = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
	pos = GTK_EDITABLE(entry)->current_pos;
	sprintf(debug_buff,"Currently at %d\n",pos);
	debug_print(debug_buff);
	s2= strstr(&s[pos], post);
	if (s2)
		pos = s2 - s + strlen(post);
	else
		pos=-1;
	sprintf(debug_buff,"Setting position to %d\n",pos);
	debug_print(debug_buff);
	gtk_editable_set_position(GTK_EDITABLE(entry), pos);
	gtk_widget_grab_focus(entry);
}

static void toggle_color(GtkWidget *color, GtkWidget *entry)
{
        if (state_lock)
                return;
        if (GTK_TOGGLE_BUTTON(color)->active)
		show_color_dialog(entry, color);
        else
                advance_past(entry, "<FONT COLOR>", "</FONT>");
}

static void do_link(GtkWidget *linky, GtkWidget *entry)
{
	if (state_lock)
		return;
	if (GTK_TOGGLE_BUTTON(linky)->active)
		show_add_link(entry, linky);
	else
		advance_past(entry, "<A HREF>", "</A>"	);
}

static void do_strike(GtkWidget *strike, GtkWidget *entry)
{
	if (state_lock)
		return;
	if (GTK_TOGGLE_BUTTON(strike)->active)
		surround(entry, "<STRIKE>","</STRIKE>");
	else
		advance_past(entry, "<STRIKE>", "</STRIKE>");
}

static void do_bold(GtkWidget *bold, GtkWidget *entry)
{
	if (state_lock)
		return;
	if (GTK_TOGGLE_BUTTON(bold)->active)
		surround(entry, "<B>","</B>");
	else
		advance_past(entry, "<B>", "</B>");
}

static void do_underline(GtkWidget *underline, GtkWidget *entry)
{
	if (state_lock)
		return;
	if (GTK_TOGGLE_BUTTON(underline)->active)
		surround(entry, "<U>","</U>");
	else
		advance_past(entry, "<U>", "</U>");
}

static void do_italic(GtkWidget *italic, GtkWidget *entry)
{
	if (state_lock)
		return;
	if (GTK_TOGGLE_BUTTON(italic)->active)
		surround(entry, "<I>","</I>");
	else
		advance_past(entry, "<I>", "</I>");
}

static void do_small(GtkWidget *small, GtkWidget *entry)
{
	if (state_lock)
		return;
	surround(entry, "<FONT SIZE=\"+1\">","</FONT>");
}

static void do_normal(GtkWidget *normal, GtkWidget *entry)
{
	if (state_lock)
                 return;
	surround(entry, "<FONT SIZE=\"+3\">","</FONT>");
}

static void do_big(GtkWidget *big, GtkWidget *entry)
{
	if (state_lock)
		return;
	surround(entry, "<FONT SIZE=\"+5\">","</FONT>");
}

void check_everything(GtkWidget *entry)
{
	struct conversation *c;
	c = (struct conversation *)gtk_object_get_user_data(GTK_OBJECT(entry));
	if (!c) return;
	if (invert_tags(entry, "<B>", "</B>", 0))
		quiet_set(c->bold, TRUE);
	else if (count_tag(entry, "<B>", "</B>")) 
		quiet_set(c->bold, TRUE);
	else
		quiet_set(c->bold,FALSE);
	if (invert_tags(entry, "<I>", "</I>", 0))
		quiet_set(c->italic, TRUE);
	else if (count_tag(entry, "<I>", "</I>"))  
		quiet_set(c->italic, TRUE);
	else
		quiet_set(c->italic, FALSE);
     
	if (invert_tags(entry, "<FONT COLOR", "</FONT>", 0))
                quiet_set(c->palette, TRUE);
        else if (count_tag(entry, "<FONT COLOR", "</FONT>"))
                quiet_set(c->palette, TRUE);
        else
                quiet_set(c->palette, FALSE);

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


void write_to_conv(struct conversation *c, char *what, int flags)
{
	char *buf = g_malloc(BUF_LONG);
        char *buf2 = g_malloc(BUF_LONG);
        char *who = NULL;
        FILE *fd;
        char colour[10];

        if (flags & WFLAG_SYSTEM) {

                gtk_html_freeze(GTK_HTML(c->text));
        
                gtk_html_append_text(GTK_HTML(c->text), what, 0);

                gtk_html_append_text(GTK_HTML(c->text), "<BR>", 0);

                gtk_html_thaw(GTK_HTML(c->text));


                if ((general_options & OPT_GEN_LOG_ALL) || find_log_info(c->name)) {
                        char *t1;

                        if (general_options & OPT_GEN_STRIP_HTML) {
                                t1 = strip_html(what);
                        } else {
                                t1 = what;
                        }
                        fd = open_log_file(c);
                        fprintf(fd, "%s\n", t1);
                        fclose(fd);
                        if (general_options & OPT_GEN_STRIP_HTML) {
                                g_free(t1);
                        }
                }
                
        } else {

                if (flags & WFLAG_RECV) {
                        strcpy(colour, "#ff0000");
                        who = c->name;
                } else if (flags & WFLAG_SEND) {
                        strcpy(colour, "#0000ff");
                        who = current_user->username;
                }

                if (flags & WFLAG_AUTO && flags & WFLAG_SEND)
                        sprintf(buf2, " %s", AUTO_RESPONSE);
                else
                        buf2[0]=0; /* sprintf(buf2, ""); */

                if (display_options & OPT_DISP_SHOW_TIME)
                        g_snprintf(buf, BUF_LONG, "<FONT COLOR=\"%s\"><B>%s %s:%s</B></FONT> ", colour, date(), who, buf2);
                else
                        g_snprintf(buf, BUF_LONG, "<FONT COLOR=\"%s\"><B>%s:%s</B></FONT> ", colour, who, buf2);

                gtk_html_freeze(GTK_HTML(c->text));

                gtk_html_append_text(GTK_HTML(c->text), buf, 0);
                gtk_html_append_text(GTK_HTML(c->text), what, (display_options & OPT_DISP_IGNORE_COLOUR) ? HTML_OPTION_NO_COLOURS : 0);

                gtk_html_append_text(GTK_HTML(c->text), "<BR>", 0);


                gtk_html_thaw(GTK_HTML(c->text));

                if ((general_options & OPT_GEN_LOG_ALL) || find_log_info(c->name)) {
                        char *t1, *t2;

                        if (general_options & OPT_GEN_STRIP_HTML) {
                                t1 = strip_html(buf);
                                t2 = strip_html(what);
                        } else {
                                t1 = buf;
                                t2 = what;
                        }
                        fd = open_log_file(c);
                        fprintf(fd, "%s%s\n", t1, t2);
                        fclose(fd);
                        if (general_options & OPT_GEN_STRIP_HTML) {
                                g_free(t1);
                                g_free(t2);
                        }
                }
        }

/*        if (!GTK_WIDGET_MAPPED(c->window)) {
                
                if (dark_icon_pm == NULL)
                        dark_icon_pm = gdk_pixmap_create_from_xpm_d(c->window->window, &dark_icon_bm,
                                                                    NULL, (gchar **)aimicon2_xpm);
                gdk_window_set_icon(c->window->window, NULL, dark_icon_pm, dark_icon_bm);
	}
*/

        if (general_options & OPT_GEN_POPUP_WINDOWS)
                gdk_window_raise(c->window->window);

        
	g_free(buf);
        g_free(buf2);
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
	GtkWidget *color;
	GtkWidget *close;
	GtkWidget *entry;
	GtkWidget *toolbar;
	GtkWidget *bbox;
        GtkWidget *vbox;
        GtkWidget *add;
        GdkPixmap *strike_i, *small_i, *normal_i, *big_i, *bold_i, *italic_i, *underline_i, *speaker_i, *wood_i, *palette_i, *link_i;
        GtkWidget *strike_p, *small_p, *normal_p, *big_p, *bold_p, *italic_p, *underline_p, *speaker_p, *wood_p, *palette_p, *link_p;
        GtkWidget *strike, *small, *normal, *big, *bold, *italic, *underline, *speaker, *wood, *palette, *link;
        GdkBitmap *mask;
	
	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_policy(GTK_WINDOW(win), TRUE, TRUE, TRUE);

        gtk_widget_realize(win);
        aol_icon(win->window);
 
        
        c->window = win;
        
	send = gtk_button_new_with_label("Send");
	info = gtk_button_new_with_label("Info");
	warn = gtk_button_new_with_label("Warn");
	color = gtk_button_new_with_label("Color");
	close = gtk_button_new_with_label("Close");
	if (find_buddy(c->name) != NULL) {
       		add = gtk_button_new_with_label("Remove");
	}
	else {
		add = gtk_button_new_with_label("Add");
	}
        block = gtk_button_new_with_label("Block");


        bbox = gtk_hbox_new(TRUE, 0);
	vbox = gtk_vbox_new(FALSE, 0);

	entry = gtk_text_new(NULL, NULL);
	gtk_text_set_editable(GTK_TEXT(entry), TRUE);
	gtk_text_set_word_wrap(GTK_TEXT(entry), TRUE);
        
	/* Toolbar */
	toolbar = gtk_toolbar_new(GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_ICONS);

	link_i = gdk_pixmap_create_from_xpm_d(win->window, &mask,
	     &win->style->white, link_xpm );
	link_p = gtk_pixmap_new(link_i, mask);
	gtk_widget_show(link_p);

	palette_i = gdk_pixmap_create_from_xpm_d (win->window, &mask,
             &win->style->white, palette_xpm );
	palette_p = gtk_pixmap_new(palette_i, mask);
	gtk_widget_show(palette_p);

	wood_i = gdk_pixmap_create_from_xpm_d ( win->window, &mask, 
	     &win->style->white, wood_xpm );
	wood_p = gtk_pixmap_new(wood_i, mask);
	gtk_widget_show(wood_p);
	speaker_i = gdk_pixmap_create_from_xpm_d ( win->window, &mask,
             &win->style->white, speaker_xpm );
	speaker_p = gtk_pixmap_new(speaker_i, mask);
	gtk_widget_show(speaker_p);
	c->makesound=1;
	strike_i = gdk_pixmap_create_from_xpm_d ( win->window, &mask,
	     &win->style->white, strike_xpm );
	strike_p = gtk_pixmap_new(strike_i, mask);
	gtk_widget_show(strike_p);
	bold_i = gdk_pixmap_create_from_xpm_d ( win->window, &mask,
             &win->style->white, bold_xpm );
	bold_p = gtk_pixmap_new(bold_i, mask);
	gtk_widget_show(bold_p);
	italic_i = gdk_pixmap_create_from_xpm_d ( win->window, &mask,
             &win->style->white, italic_xpm );
	italic_p = gtk_pixmap_new(italic_i, mask);
	gtk_widget_show(italic_p);
	underline_i = gdk_pixmap_create_from_xpm_d ( win->window, &mask,
	     &win->style->white, underline_xpm );
	underline_p = gtk_pixmap_new(underline_i, mask);
	gtk_widget_show(underline_p);
	small_i = gdk_pixmap_create_from_xpm_d ( win->window, &mask,
             &win->style->white, small_xpm );
	small_p = gtk_pixmap_new(small_i, mask);
	gtk_widget_show(small_p);
	normal_i = gdk_pixmap_create_from_xpm_d ( win->window, &mask,
             &win->style->white, normal_xpm );
	normal_p = gtk_pixmap_new(normal_i, mask);
	gtk_widget_show(normal_p);
	big_i = gdk_pixmap_create_from_xpm_d ( win->window, &mask,
             &win->style->white, big_xpm );
	big_p = gtk_pixmap_new(big_i, mask);
	gtk_widget_show(big_p);


	bold = gtk_toolbar_append_element(GTK_TOOLBAR(toolbar),
	                                  GTK_TOOLBAR_CHILD_TOGGLEBUTTON, NULL,
					  "Bold", "Bold Text", "Bold", bold_p,
					  GTK_SIGNAL_FUNC(do_bold), entry);
	italic = gtk_toolbar_append_element(GTK_TOOLBAR(toolbar), 
		                            GTK_TOOLBAR_CHILD_TOGGLEBUTTON,
					    NULL, "Italics", "Italics Text",
					    "Italics", italic_p, GTK_SIGNAL_FUNC(do_italic), entry);
	underline = gtk_toolbar_append_element(GTK_TOOLBAR(toolbar),
					    GTK_TOOLBAR_CHILD_TOGGLEBUTTON,
					    NULL, "Underline", "Underline Text",
					    "Underline", underline_p, GTK_SIGNAL_FUNC(do_underline), entry);
	strike = gtk_toolbar_append_element(GTK_TOOLBAR(toolbar),
					    GTK_TOOLBAR_CHILD_TOGGLEBUTTON,
					    NULL, "Strike", "Strike through Text",
					    "Strike", strike_p, GTK_SIGNAL_FUNC(do_strike), entry);
	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));
	small = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar), "Small", "Decrease font size", "Small", small_p, GTK_SIGNAL_FUNC(do_small), entry);
	normal = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar), "Normal", "Normal font size", "Normal", normal_p, GTK_SIGNAL_FUNC(do_normal), entry);
	big = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar), "Big", "Increase font size", "Big", big_p, GTK_SIGNAL_FUNC(do_big), entry);
	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));
	link = gtk_toolbar_append_element(GTK_TOOLBAR(toolbar),
                                            GTK_TOOLBAR_CHILD_TOGGLEBUTTON,                                                 NULL, "Link", "Insert Link",
                                            "Link", link_p, GTK_SIGNAL_FUNC(do_link), entry);                 
	palette = gtk_toolbar_append_element(GTK_TOOLBAR(toolbar),
					    GTK_TOOLBAR_CHILD_TOGGLEBUTTON,
					    NULL, "Color", "Text Color",
				 	    "Color", palette_p, GTK_SIGNAL_FUNC(toggle_color), entry); 
	wood = gtk_toolbar_append_element(GTK_TOOLBAR(toolbar),
					    GTK_TOOLBAR_CHILD_TOGGLEBUTTON,
					    NULL, "Logging", "Enable logging",
                                          "Logging", wood_p, GTK_SIGNAL_FUNC(toggle_loggle), c);
        speaker = gtk_toolbar_append_element(GTK_TOOLBAR(toolbar),
		                            GTK_TOOLBAR_CHILD_TOGGLEBUTTON,
					    NULL, "Sound", "Enable sounds",
					    "Sound", speaker_p, GTK_SIGNAL_FUNC(set_option), &c->makesound);
	c->makesound=0;
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(speaker), TRUE);

        state_lock = 1;
	if (find_log_info(c->name))
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(wood), TRUE);
	else
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(wood), FALSE);
        state_lock = 0;
        
	gtk_widget_show(toolbar);
	
	c->entry = entry;
	c->bold = bold;
	c->strike = strike;
        c->italic = italic;
	c->underline = underline;
        c->log_button = wood;
	c->palette = palette;
	c->link = link;  
        c->add_button = add;

        gtk_widget_set_sensitive(c->log_button, ((general_options & OPT_GEN_LOG_ALL)) ? FALSE : TRUE);
        
	gtk_widget_set_sensitive(c->bold, ((font_options & OPT_FONT_BOLD)) ? FALSE : TRUE);
	gtk_widget_set_sensitive(c->italic, ((font_options & OPT_FONT_ITALIC)) ? FALSE : TRUE);
	gtk_widget_set_sensitive(c->underline, ((font_options & OPT_FONT_UNDERLINE)) ? FALSE : TRUE);
	gtk_widget_set_sensitive(c->strike, ((font_options & OPT_FONT_STRIKE)) ? FALSE : TRUE);
        
	gtk_object_set_user_data(GTK_OBJECT(entry), c);

	

	gtk_signal_connect(GTK_OBJECT(entry), "activate", GTK_SIGNAL_FUNC(send_callback),c);

        /* Text box */

        
        text = gtk_html_new(NULL, NULL);

	gtk_html_set_editable(GTK_HTML(text), FALSE);
/*	gtk_html_set_transparent(GTK_HTML(text), (transparent) ? TRUE : FALSE);*/
        c->text = text;

        sw = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                        GTK_POLICY_NEVER,
                                        GTK_POLICY_ALWAYS);
        gtk_widget_show(sw);
        gtk_container_add(GTK_CONTAINER(sw), text);
        gtk_widget_show(text);
        



        GTK_HTML (text)->hadj->step_increment = 10.0;
        GTK_HTML (text)->vadj->step_increment = 10.0;
        gtk_widget_set_usize(sw, 320, 150);

        

	/* Ready and pack buttons */
	gtk_object_set_user_data(GTK_OBJECT(win), c);
	gtk_object_set_user_data(GTK_OBJECT(close), c);
	gtk_signal_connect(GTK_OBJECT(close), "clicked", GTK_SIGNAL_FUNC(close_callback), c);
	gtk_signal_connect(GTK_OBJECT(send), "clicked", GTK_SIGNAL_FUNC(send_callback), c);
	gtk_signal_connect(GTK_OBJECT(add), "clicked", GTK_SIGNAL_FUNC(add_callback), c);
        gtk_signal_connect(GTK_OBJECT(info), "clicked", GTK_SIGNAL_FUNC(info_callback), c);
        gtk_signal_connect(GTK_OBJECT(warn), "clicked", GTK_SIGNAL_FUNC(warn_callback), c);
        gtk_signal_connect(GTK_OBJECT(block), "clicked", GTK_SIGNAL_FUNC(block_callback), c);
	gtk_signal_connect(GTK_OBJECT(color), "clicked", GTK_SIGNAL_FUNC(color_callback), c);
       
	gtk_signal_connect(GTK_OBJECT(entry), "key_press_event", GTK_SIGNAL_FUNC(user_keypress_callback), c);
	gtk_widget_set_usize(entry, 300, 70);

	gtk_box_pack_start(GTK_BOX(bbox), send, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(bbox), info, TRUE, TRUE, 5);
        gtk_box_pack_start(GTK_BOX(bbox), warn, TRUE, TRUE, 5);
        gtk_box_pack_start(GTK_BOX(bbox), block, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(bbox), color, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(bbox), add, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(bbox), close, TRUE, TRUE, 5);
	
	/* pack and fill the rest */

	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 5);
        gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), entry, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 5);

	


	gtk_widget_show(send);
	gtk_widget_show(info);
	gtk_widget_show(warn);
	/* gtk_widget_show(color); */
	gtk_widget_show(close);	
        gtk_widget_show(add);
        gtk_widget_show(block);
	gtk_widget_show(bbox);
	gtk_widget_show(vbox);
	gtk_widget_show(entry);
	gtk_widget_show(text);
	
	
	gtk_container_add(GTK_CONTAINER(win), vbox);
        gtk_container_border_width(GTK_CONTAINER(win), 10);

	if ((find_log_info(c->name)) || ((general_options & OPT_GEN_LOG_ALL)))
		g_snprintf(buf, sizeof(buf), LOG_CONVERSATION_TITLE, c->name);
	else
		g_snprintf(buf, sizeof(buf), CONVERSATION_TITLE, c->name);
	gtk_window_set_title(GTK_WINDOW(win), buf);
	gtk_window_set_focus(GTK_WINDOW(win),entry);

	gtk_signal_connect(GTK_OBJECT(win), "delete_event", GTK_SIGNAL_FUNC(delete_event_convo), c);
	gtk_signal_connect(GTK_OBJECT(entry), "key_press_event", GTK_SIGNAL_FUNC(entry_key_pressed), entry);

	gtk_widget_show(win);
	
}


