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
#ifdef USE_APPLET
#include <gnome.h>
#include <applet-widget.h>
#include "applet.h"
#endif /* USE_APPLET */
#ifdef GAIM_PLUGINS
#include <dlfcn.h>
#endif /* GAIM_PLUGINS */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <unistd.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include "prpl.h"
#include "gaim.h"
#include "pixmaps/login_icon.xpm"
#include "pixmaps/logout_icon.xpm"
#include "pixmaps/no_icon.xpm"

#include "pixmaps/away_small.xpm"
#include "pixmaps/away_big.xpm"

#include "pixmaps/add_small.xpm"
#include "pixmaps/import_small.xpm"
/*#include "pixmaps/export_small.xpm"*/
#if defined(GAIM_PLUGINS) || defined(USE_PERL)
#include "pixmaps/plugins_small.xpm"
#endif
#include "pixmaps/prefs_small.xpm"
#ifdef NO_MULTI
#include "pixmaps/search_small.xpm"
#endif
#ifdef USE_APPLET
#include "pixmaps/close_small.xpm"
#else
#include "pixmaps/exit_small.xpm"
#endif
#include "pixmaps/pounce_small.xpm"
#include "pixmaps/about_small.xpm"

#include "pixmaps/tmp_send.xpm"
#include "pixmaps/send_small.xpm"
#include "pixmaps/tb_search.xpm"
#include "pixmaps/join.xpm"
#include "pixmaps/gnome_add.xpm"
#include "pixmaps/gnome_remove.xpm"
#include "pixmaps/group.xpm"

static GtkTooltips *tips;
static GtkAccelGroup *accel;
static GtkWidget *editpane;
static GtkWidget *buddypane;
static GtkWidget *imchatbox;
static GtkWidget *edittree;
static GtkWidget *imbutton, *infobutton, *chatbutton, *awaybutton;
static GtkWidget *addbutton, *groupbutton, *rembutton;

GtkWidget *blist = NULL;
GtkWidget *bpmenu;
GtkWidget *buddies;

void BuddyTickerLogonTimeout(gpointer data);
void BuddyTickerLogoutTimeout(gpointer data);

struct buddy_show {
	GtkWidget *item;
	GtkWidget *pix;
	GtkWidget *label;
	GtkWidget *warn;
	GtkWidget *idle;
	char *name;
	char *show;
	GSList *connlist;
	guint log_timer;
	gint sound;
};

/* stuff for actual display of buddy list */
struct group_show {
	GtkWidget *item;
	GtkWidget *label;
	GtkWidget *tree;
	GSList *members;
	char *name;
};
static GSList *shows = NULL;

/* Predefine some functions */
static void new_bp_callback(GtkWidget *w, struct buddy *bs);
static struct group_show *find_group_show(char *group);
static struct buddy_show *find_buddy_show(struct group_show *gs, char *name);
static int group_number(char *group);
static int buddy_number(char *group, char *buddy);
static struct group_show *new_group_show(char *group);
static struct buddy_show *new_buddy_show(struct group_show *gs, struct buddy *buddy, char **xpm);
static void remove_buddy_show(struct group_show *gs, struct buddy_show *bs);
static struct group_show *find_gs_by_bs(struct buddy_show *b);
static void update_num_group(struct group_show *gs);

void handle_group_rename(struct group *g, char *prevname)
{
	struct group_show *gs, *new_gs;
	struct buddy_show *bs;
	struct buddy *b;
	GSList *m;
	GtkCTreeNode *c;

	c = gtk_ctree_find_by_row_data(GTK_CTREE(edittree), NULL, g);
	gtk_ctree_node_set_text(GTK_CTREE(edittree), c, 0, g->name);

	gs = find_group_show(prevname);
	if (!gs) {
		return;
	}
	new_gs = find_group_show(g->name);
	if (new_gs) {
		/* transfer everything that was in gs and is in the same gaim_conn as g
		 * over to new_gs. */
		m = gs->members;
		while (m) {
			bs = (struct buddy_show *)m->data;
			if (g_slist_index(bs->connlist, g->gc) >= 0) {
				b = find_buddy(g->gc, bs->name);
				m = g_slist_next(m);
				bs->connlist = g_slist_remove(bs->connlist, g->gc);
				if (!bs->connlist) {
					gs->members = g_slist_remove(gs->members, bs);
					if (bs->log_timer > 0)
						gtk_timeout_remove(bs->log_timer);
					bs->log_timer = 0;
					remove_buddy_show(gs, bs);
					g_free(bs->show);
					g_free(bs->name);
					g_free(bs);
				}
				if ((bs = find_buddy_show(new_gs, b->name)) == NULL) {
					if (g->gc->prpl->list_icon) {
						bs = new_buddy_show(new_gs, b,
								    (*g->gc->prpl->list_icon)(b->uc));
					} else {
						bs = new_buddy_show(new_gs, b, (char **)no_icon_xpm);
					}
				}
				bs->connlist = g_slist_append(bs->connlist, g->gc);
			} else {
				m = g_slist_next(m);
			}
		}
		if (!gs->members) {
			/* we just transferred all of the members out of this group_show,
			 * so this group_show serves no purpose now. */
			shows = g_slist_remove(shows, gs);
			gtk_tree_remove_item(GTK_TREE(buddies), gs->item);
			g_free(gs->name);
			g_free(gs);
		} else {
			update_num_group(gs);
		}
		update_num_group(new_gs);
	} else {
		/* two possible actions: if gs contains things that are only from g,
		 * just rename gs and fix the label. otherwise, move everything in g
		 * over to another group_show */
		for (m = gs->members; m != NULL; m = g_slist_next(m)) {
			bs = (struct buddy_show *)m->data;
			if (g_slist_index(bs->connlist, g->gc) < 0 || g_slist_length(bs->connlist) > 1) {
				break;
			}
		}
		if (m) {
			/* there's something from a different gaim_connection. */
			new_gs = new_group_show(g->name);
			m = gs->members;
			while (m) {
				bs = (struct buddy_show *)m->data;
				if (g_slist_index(bs->connlist, g->gc) >= 0) {
					b = find_buddy(g->gc, bs->name);
					m = g_slist_next(m);
					bs->connlist = g_slist_remove(bs->connlist, g->gc);
					if (!bs->connlist) {
						gs->members = g_slist_remove(gs->members, bs);
						if (bs->log_timer > 0)
							gtk_timeout_remove(bs->log_timer);
						bs->log_timer = 0;
						remove_buddy_show(gs, bs);
						g_free(bs->show);
						g_free(bs->name);
						g_free(bs);
					}
					if (g->gc->prpl->list_icon) {
						bs = new_buddy_show(new_gs, b,
								    (*g->gc->prpl->list_icon)(b->uc));
					} else {
						bs = new_buddy_show(new_gs, b, (char **)no_icon_xpm);
					}
					bs->connlist = g_slist_append(NULL, g->gc);
				} else {
					m = g_slist_next(m);
				}
			}
			update_num_group(gs);
			update_num_group(new_gs);
		} else {
			g_free(gs->name);
			gs->name = g_strdup(g->name);
			update_num_group(gs);
		}
	}
}

void handle_buddy_rename(struct buddy *b, char *prevname)
{
	struct buddy_show *bs;
	struct group_show *gs;
	struct group *g;
	GtkCTreeNode *c;
	char buf[256];

	c = gtk_ctree_find_by_row_data(GTK_CTREE(edittree), NULL, b);
	if (strcmp(b->show, b->name))
		g_snprintf(buf, sizeof(buf), "%s (%s)", b->name, b->show);
	else
		g_snprintf(buf, sizeof(buf), "%s", b->name);
	gtk_ctree_node_set_text(GTK_CTREE(edittree), c, 0, buf);

	gs = find_group_show(prevname);

	g = find_group_by_buddy(b->gc, b->name);
	if (!g) {
		/* shouldn't happen */
		return;
	}
	gs = find_group_show(g->name);
	if (!gs) {
		return;
	}
	bs = find_buddy_show(gs, prevname);
	if (!bs) {
		/* buddy's offline */
		return;
	}

	if (g_strcasecmp(b->name, prevname)) {
		bs->connlist = g_slist_remove(bs->connlist, b->gc);
		if (!bs->connlist) {
			gs->members = g_slist_remove(gs->members, bs);
			if (bs->log_timer > 0)
				gtk_timeout_remove(bs->log_timer);
			bs->log_timer = 0;
			remove_buddy_show(gs, bs);
			g_free(bs->show);
			g_free(bs->name);
			g_free(bs);
		}
	} else {
		gtk_label_set_text(GTK_LABEL(bs->label), b->show);
	}
}

void destroy_buddy()
{
	GSList *s = shows;
	struct group_show *g;
	GSList *m;
	struct buddy_show *b;
	while (s) {
		g = (struct group_show *)s->data;
		debug_printf("group_show still exists: %s\n", g->name);
		m = g->members;
		while (m) {
			b = (struct buddy_show *)m->data;
			debug_printf("buddy_show still exists: %s\n", b->name);
			m = g_slist_remove(m, b);
			if (b->log_timer > 0)
				gtk_timeout_remove(b->log_timer);
			b->log_timer = 0;
			gtk_tree_remove_item(GTK_TREE(g->tree), b->item);
			g_free(b->show);
			g_free(b->name);
			g_free(b);
		}
		gtk_tree_remove_item(GTK_TREE(buddies), g->item);
		s = g_slist_remove(s, g);
		g_free(g->name);
		g_free(g);
	}
	shows = NULL;

	if (blist)
		gtk_widget_destroy(blist);
	blist = NULL;
	imchatbox = NULL;
	awaymenu = NULL;
	protomenu = NULL;
}

static void adjust_pic(GtkWidget *button, const char *c, gchar **xpm)
{
	GdkPixmap *pm;
	GdkBitmap *bm;
	GtkWidget *pic;
	GtkWidget *label;

	/*if the user had opted to put pictures on the buttons */
	if (blist_options & OPT_BLIST_SHOW_BUTTON_XPM && xpm) {
		pm = gdk_pixmap_create_from_xpm_d(blist->window, &bm, NULL, xpm);
		pic = gtk_pixmap_new(pm, bm);
		gtk_widget_show(pic);
		gdk_pixmap_unref(pm);
		gdk_bitmap_unref(bm);
		label = GTK_BIN(button)->child;
		gtk_container_remove(GTK_CONTAINER(button), label);
		gtk_container_add(GTK_CONTAINER(button), pic);
	} else {
		label = gtk_label_new(c);
		gtk_widget_show(label);
		pic = GTK_BIN(button)->child;
		gtk_container_remove(GTK_CONTAINER(button), pic);
		gtk_container_add(GTK_CONTAINER(button), label);
	}

}


void toggle_show_empty_groups()
{
	if (blist_options & OPT_BLIST_NO_MT_GRP) {
		/* remove any group_shows with empty members */
		GSList *s = shows;
		struct group_show *g;

		while (s) {
			g = (struct group_show *)s->data;
			if (!g_slist_length(g->members)) {
				shows = g_slist_remove(shows, g);
				s = shows;
				gtk_tree_remove_item(GTK_TREE(buddies), g->item);
				g_free(g->name);
				g_free(g);
			} else
				s = g_slist_next(s);
		}

	} else {
		/* put back all groups */
		GSList *c = connections;
		struct gaim_connection *gc;
		GSList *m;
		struct group *g;

		while (c) {
			gc = (struct gaim_connection *)c->data;
			m = gc->groups;
			while (m) {
				g = (struct group *)m->data;
				m = g_slist_next(m);
				if (!find_group_show(g->name))
					new_group_show(g->name);
			}
			c = g_slist_next(c);
		}

	}
}

void toggle_buddy_pixmaps()
{
	GSList *s = shows;
	struct group_show *g;
	GSList *m;
	struct buddy_show *b;

	while (s) {
		g = s->data;
		m = g->members;
		while (m) {
			b = m->data;
			if (blist_options & OPT_BLIST_SHOW_PIXMAPS)
				gtk_widget_show(b->pix);
			else
				gtk_widget_hide(b->pix);
			m = m->next;
		}
		s = s->next;
	}
}

static void update_num_group(struct group_show *gs)
{
	GSList *c = connections;
	struct gaim_connection *gc;
	struct group *g;
	struct buddy_show *b;
	int total = 0, on = 0;
	char buf[256];

	if (!g_slist_find(shows, gs)) {
		debug_printf("update_num_group called for unfound group_show %s\n", gs->name);
		return;
	}

	while (c) {
		gc = (struct gaim_connection *)c->data;
		g = find_group(gc, gs->name);
		if (g) {
			total += g_slist_length(g->members);
		}
		c = g_slist_next(c);
	}

	c = gs->members;
	while (c) {
		b = (struct buddy_show *)c->data;
		on += g_slist_length(b->connlist);
		c = g_slist_next(c);
	}

	if (blist_options & OPT_BLIST_SHOW_GRPNUM)
		g_snprintf(buf, sizeof buf, "%s (%d/%d)", gs->name, on, total);
	else
		g_snprintf(buf, sizeof buf, "%s", gs->name);

	gtk_label_set_text(GTK_LABEL(gs->label), buf);
}

void update_num_groups()
{
	GSList *s = shows;
	struct group_show *g;

	while (s) {
		g = (struct group_show *)s->data;
		update_num_group(g);
		s = g_slist_next(s);
	}
}

void update_button_pix()
{

	adjust_pic(addbutton, _("Add"), (gchar **)gnome_add_xpm);
	adjust_pic(groupbutton, _("Group"), (gchar **)group_xpm);
	adjust_pic(rembutton, _("Remove"), (gchar **)gnome_remove_xpm);

	if (!(blist_options & OPT_BLIST_NO_BUTTONS)) {
		adjust_pic(awaybutton, _("Away"), (gchar **)away_big_xpm);
		adjust_pic(chatbutton, _("Chat"), (gchar **)join_xpm);
		adjust_pic(imbutton, _("IM"), (gchar **)tmp_send_xpm);
		adjust_pic(infobutton, _("Info"), (gchar **)tb_search_xpm);
	}
	gtk_widget_hide(addbutton->parent);
	gtk_widget_show(addbutton->parent);
	if (!(blist_options & OPT_BLIST_NO_BUTTONS)) {
		gtk_widget_hide(chatbutton->parent);
		gtk_widget_show(chatbutton->parent);
	}
}



#ifdef USE_APPLET
gint applet_destroy_buddy(GtkWidget *widget, GdkEvent *event, gpointer *data)
{
	applet_buddy_show = FALSE;
	gtk_widget_hide(blist);
	return (TRUE);
}

#endif

void handle_click_group(GtkWidget *widget, GdkEventButton *event, struct group *g)
{
	if (event->type == GDK_2BUTTON_PRESS) {
		if (GTK_TREE_ITEM(widget)->expanded)
			gtk_tree_item_collapse(GTK_TREE_ITEM(widget));
		else
			gtk_tree_item_expand(GTK_TREE_ITEM(widget));
	} else if ((event->type == GDK_BUTTON_PRESS) && (event->button == 3)) {
	}
}

void pressed_im_bud(GtkWidget *widget, struct buddy *b)
{
	struct conversation *c;

	c = find_conversation(b->name);

	if (c != NULL) {
		gdk_window_show(c->window->window);
	} else {
		c = new_conversation(b->name);

		set_convo_gc(c, b->gc);
	}
}

void pressed_im(GtkWidget *widget, struct buddy_show *b)
{
	struct conversation *c;

	c = find_conversation(b->name);

	if (c != NULL) {
		gdk_window_show(c->window->window);
	} else {
		c = new_conversation(b->name);

		set_convo_gc(c, b->connlist->data);
	}
}

void pressed_log(GtkWidget *widget, char *name)
{
	show_log(name);
}

void show_syslog()
{
	show_log(NULL);
}

void pressed_ticker(char *buddy)
{
	struct conversation *c;

	c = find_conversation(buddy);

	if (c != NULL) {
		gdk_window_show(c->window->window);
	} else {
		c = new_conversation(buddy);
	}
}

void pressed_alias_bs(GtkWidget *widget, struct buddy_show *bs)
{
	alias_dialog_bud(find_buddy(bs->connlist->data, bs->name));
}

void pressed_alias_bud(GtkWidget *widget, struct buddy *b)
{
	alias_dialog_bud(b);
}

static void menu_click(GtkObject *obj, char *who)
{
	GList *list = gtk_object_get_user_data(obj);
	GList *first = g_list_first(list);
	struct proto_buddy_menu *pbm = list->data;
	if (pbm->callback)
		pbm->callback(pbm->gc, who);
	g_list_foreach(first, (GFunc)g_free, NULL);
	g_list_free(first);
}

void handle_click_buddy(GtkWidget *widget, GdkEventButton *event, struct buddy_show *b)
{
	if (!b->connlist)
		return;
	if (event->type == GDK_2BUTTON_PRESS && event->button == 1) {
		struct conversation *c;

		c = find_conversation(b->name);

		if (c != NULL)
			gdk_window_show(c->window->window);
		else
			c = new_conversation(b->name);

		set_convo_gc(c, b->connlist->data);
		if (im_options & OPT_IM_ONE_WINDOW)
			raise_convo_tab(c);
	} else if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
		GtkWidget *menu;
		GtkWidget *button;
		GtkWidget *menuitem;
		GtkWidget *conmenu;
		GSList *cn = b->connlist;
		struct gaim_connection *g;
		/* We're gonna make us a menu right here */

		menu = gtk_menu_new();

		button = gtk_menu_item_new_with_label(_("IM"));
		gtk_signal_connect(GTK_OBJECT(button), "activate", GTK_SIGNAL_FUNC(pressed_im), b);
		gtk_menu_append(GTK_MENU(menu), button);
		gtk_widget_show(button);

		button = gtk_menu_item_new_with_label(_("Alias"));
		gtk_signal_connect(GTK_OBJECT(button), "activate", GTK_SIGNAL_FUNC(pressed_alias_bs), b);
		gtk_menu_append(GTK_MENU(menu), button);
		gtk_widget_show(button);

		button = gtk_menu_item_new_with_label(_("Add Buddy Pounce"));
		gtk_signal_connect(GTK_OBJECT(button), "activate",
				   GTK_SIGNAL_FUNC(new_bp_callback),
				   cn ? find_buddy(cn->data, b->name) : NULL);
		gtk_menu_append(GTK_MENU(menu), button);
		gtk_widget_show(button);

		button = gtk_menu_item_new_with_label(_("View Log"));
		gtk_signal_connect(GTK_OBJECT(button), "activate",
				   GTK_SIGNAL_FUNC(pressed_log), b->name);
		gtk_menu_append(GTK_MENU(menu), button);
		gtk_widget_show(button);

		if (g_slist_length(cn) > 1) {
			while (cn) {
				g = (struct gaim_connection *)cn->data;
				if (g->prpl->buddy_menu) {
					GList *mo = (*g->prpl->buddy_menu)(g, b->name);

					menuitem = gtk_menu_item_new_with_label(g->username);
					gtk_menu_append(GTK_MENU(menu), menuitem);
					gtk_widget_show(menuitem);

					conmenu = gtk_menu_new();
					gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), conmenu);
					gtk_widget_show(conmenu);

					while (mo) {
						struct proto_buddy_menu *pbm = mo->data;
						GtkWidget *button;

						button = gtk_menu_item_new_with_label(pbm->label);
						gtk_signal_connect(GTK_OBJECT(button), "activate",
								   GTK_SIGNAL_FUNC(menu_click), b->name);
						gtk_object_set_user_data(GTK_OBJECT(button), mo);
						gtk_menu_append(GTK_MENU(conmenu), button);
						gtk_widget_show(button);

						mo = mo->next;
					}
				}
				cn = g_slist_next(cn);
			}
		} else {
			g = (struct gaim_connection *)cn->data;
			if (g->prpl->buddy_menu) {
				GList *mo = (*g->prpl->buddy_menu)(g, b->name);

				while (mo) {
					struct proto_buddy_menu *pbm = mo->data;
					GtkWidget *button;

					button = gtk_menu_item_new_with_label(pbm->label);
					gtk_signal_connect(GTK_OBJECT(button), "activate",
							   GTK_SIGNAL_FUNC(menu_click), b->name);
					gtk_object_set_user_data(GTK_OBJECT(button), mo);
					gtk_menu_append(GTK_MENU(menu), button);
					gtk_widget_show(button);

					mo = mo->next;
				}
			}
		}

		/* we send the menu widget so we can add menuitems within a plugin */
		plugin_event(event_draw_menu, menu, b->name, 0, 0);

		gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, event->time);

	} else if (event->type == GDK_3BUTTON_PRESS && event->button == 2) {
		if (!g_strcasecmp("zilding", normalize (b->name)))
			 show_ee_dialog(0);
		else if (!g_strcasecmp("robflynn", normalize (b->name)))
			 show_ee_dialog(1);
		else if (!g_strcasecmp("flynorange", normalize (b->name)))
			 show_ee_dialog(2);
		else if (!g_strcasecmp("ewarmenhoven", normalize (b->name)))
			 show_ee_dialog(3);
		else if (!g_strcasecmp("markster97", normalize (b->name)))
			 show_ee_dialog(4);

	} else {

		/* Anything for other buttons? :) */
	}
}

static void un_alias(GtkWidget *a, struct buddy *b)
{
	struct group *g = find_group_by_buddy(b->gc, b->name);
	struct group_show *gs = find_group_show(g->name);
	struct buddy_show *bs = NULL;
	GtkCTreeNode *node = gtk_ctree_find_by_row_data(GTK_CTREE(edittree), NULL, b);
	g_snprintf(b->show, sizeof(b->show), "%s", b->name);
	gtk_ctree_node_set_text(GTK_CTREE(edittree), node, 0, b->name);
	if (gs)
		bs = find_buddy_show(gs, b->name);
	if (bs)
		gtk_label_set(GTK_LABEL(bs->label), b->name);
	do_export(b->gc);
}

static gboolean click_edit_tree(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	GtkCTreeNode *node;
	int *type;
	int row, column;
	GtkWidget *menu;
	GtkWidget *button;

	if (event->button != 3 || event->type != GDK_BUTTON_PRESS)
		return TRUE;

	if (!gtk_clist_get_selection_info(GTK_CLIST(edittree), event->x, event->y, &row, &column))
		return TRUE;

	node = gtk_ctree_node_nth(GTK_CTREE(edittree), row);
	type = gtk_ctree_node_get_row_data(GTK_CTREE(edittree), node);
	if (*type == EDIT_GROUP) {
		struct group *group = (struct group *)type;
		menu = gtk_menu_new();

		button = gtk_menu_item_new_with_label(_("Rename"));
		gtk_signal_connect(GTK_OBJECT(button), "activate",
				   GTK_SIGNAL_FUNC(show_rename_group), group);
		gtk_menu_append(GTK_MENU(menu), button);
		gtk_widget_show(button);

		gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, event->time);
	} else if (*type == EDIT_BUDDY) {
		struct buddy *b = (struct buddy *)type;
		menu = gtk_menu_new();

		button = gtk_menu_item_new_with_label(_("IM"));
		gtk_signal_connect(GTK_OBJECT(button), "activate", GTK_SIGNAL_FUNC(pressed_im_bud), b);
		gtk_menu_append(GTK_MENU(menu), button);
		gtk_widget_show(button);

		button = gtk_menu_item_new_with_label(_("Alias"));
		gtk_signal_connect(GTK_OBJECT(button), "activate",
				   GTK_SIGNAL_FUNC(pressed_alias_bud), b);
		gtk_menu_append(GTK_MENU(menu), button);
		gtk_widget_show(button);

		if (strcmp(b->name, b->show)) {
			button = gtk_menu_item_new_with_label(_("Un-Alias"));
			gtk_signal_connect(GTK_OBJECT(button), "activate", GTK_SIGNAL_FUNC(un_alias), b);
			gtk_menu_append(GTK_MENU(menu), button);
			gtk_widget_show(button);
		}

		button = gtk_menu_item_new_with_label(_("Rename"));
		gtk_signal_connect(GTK_OBJECT(button), "activate",
				   GTK_SIGNAL_FUNC(show_rename_buddy), b);
		gtk_menu_append(GTK_MENU(menu), button);
		gtk_widget_show(button);

		button = gtk_menu_item_new_with_label(_("Add Buddy Pounce"));
		gtk_signal_connect(GTK_OBJECT(button), "activate",
				   GTK_SIGNAL_FUNC(new_bp_callback), b);
		gtk_menu_append(GTK_MENU(menu), button);
		gtk_widget_show(button);

		button = gtk_menu_item_new_with_label(_("View Log"));
		gtk_signal_connect(GTK_OBJECT(button), "activate",
				   GTK_SIGNAL_FUNC(pressed_log), b->name);
		gtk_menu_append(GTK_MENU(menu), button);
		gtk_widget_show(button);

		gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, event->time);
	}

	return TRUE;
}


void ui_remove_buddy(struct gaim_connection *gc, struct group *rem_g, struct buddy *rem_b)
{
	struct conversation *c;
	struct group_show *gs;
	struct buddy_show *bs;

	gs = find_group_show(rem_g->name);
	if (gs) {
		bs = find_buddy_show(gs, rem_b->name);
		if (bs) {
			if (g_slist_find(bs->connlist, gc)) {
				bs->connlist = g_slist_remove(bs->connlist, gc);
				if (!g_slist_length(bs->connlist)) {
					gs->members = g_slist_remove(gs->members, bs);
					if (bs->log_timer > 0)
						gtk_timeout_remove(bs->log_timer);
					bs->log_timer = 0;
					remove_buddy_show(gs, bs);
					g_free(bs->show);
					g_free(bs->name);
					g_free(bs);
					if (!g_slist_length(gs->members) &&
					    (blist_options & OPT_BLIST_NO_MT_GRP)) {
						shows = g_slist_remove(shows, gs);
						gtk_tree_remove_item(GTK_TREE(buddies), gs->item);
						g_free(gs->name);
						g_free(gs);
					} else
						update_num_group(gs);
				} else
					update_num_group(gs);
			} else
				update_num_group(gs);
		} else
			update_num_group(gs);
	}

	c = find_conversation(rem_b->name);
	if (c)
		update_buttons_by_protocol(c);
}

void ui_remove_group(struct gaim_connection *gc, struct group *rem_g)
{
	struct group_show *gs;

	if ((gs = find_group_show(rem_g->name)) != NULL) {
		shows = g_slist_remove(shows, gs);
		gtk_tree_remove_item(GTK_TREE(buddies), gs->item);
		g_free(gs->name);
		g_free(gs);
	}
}

gboolean edit_drag_compare_func(GtkCTree *ctree, GtkCTreeNode *source_node,
				GtkCTreeNode *new_parent, GtkCTreeNode *new_sibling)
{
	int *type;

	type = (int *)gtk_ctree_node_get_row_data(GTK_CTREE(ctree), source_node);

	if (*type == EDIT_GC) {
		if (!new_parent)
			return TRUE;
	} else if (*type == EDIT_BUDDY) {
		if (new_parent) {
			type = (int *)gtk_ctree_node_get_row_data(GTK_CTREE(ctree), new_parent);
			if (*type == EDIT_GROUP)
				return TRUE;
		}
	} else {		/* group */

		if (g_slist_length(connections) > 1 && new_parent) {
			type = (int *)gtk_ctree_node_get_row_data(GTK_CTREE(ctree), new_parent);
			if (*type == EDIT_GC)
				return TRUE;
		} else if (g_slist_length(connections) == 1 && !new_parent)
			return TRUE;
	}

	return FALSE;
}


/* you really shouldn't call this function */
void redo_buddy_list()
{
	/* so here we can safely assume that we don't have to add or delete anything, we
	 * just have to go through and reorder everything. remember, nothing is going to
	 * change connections, so we can assume that we don't have to change any user
	 * data or anything. this is just a simple reordering. so calm down. */
	/* note: we only have to do this if we want to strongly enforce order; however,
	 * order doesn't particularly matter to the stability of the program. but, it's
	 * kind of nice to have */
	/* the easy way to implement this is just to go through shows and destroy all the
	 * group_shows, then go through the connections and put everything back. though,
	 * there are slight complications with that; most of them deal with timeouts and
	 * people not seeing the login icon for the full 10 seconds. butt fuck them. */
	GSList *s = shows;
	struct group_show *gs;
	GSList *m;
	struct buddy_show *bs;
	GSList *c = connections;
	struct gaim_connection *gc;
	GSList *gr;
	struct group *g;
	struct buddy *b;

	if (!blist)
		return;

	while (s) {
		gs = (struct group_show *)s->data;
		s = g_slist_remove(s, gs);
		m = gs->members;
		gtk_tree_remove_item(GTK_TREE(buddies), gs->item);
		while (m) {
			bs = (struct buddy_show *)m->data;
			m = g_slist_remove(m, bs);
			if (bs->log_timer > 0)
				gtk_timeout_remove(bs->log_timer);
			g_free(bs->show);
			g_free(bs->name);
			g_free(bs);
		}
		g_free(gs->name);
		g_free(gs);
	}
	shows = NULL;
	while (c) {
		gc = (struct gaim_connection *)c->data;
		c = c->next;
		gr = gc->groups;
		while (gr) {
			g = (struct group *)gr->data;
			gr = gr->next;
			gs = find_group_show(g->name);
			if (!gs && !(blist_options & OPT_BLIST_NO_MT_GRP))
				gs = new_group_show(g->name);
			m = g->members;
			while (m) {
				b = (struct buddy *)m->data;
				m = m->next;
				if (b->present) {
					if (!gs)
						gs = new_group_show(g->name);
					bs = find_buddy_show(gs, b->name);
					if (!bs) {
						if (gc->prpl->list_icon)
							bs = new_buddy_show(gs, b,
									    (*gc->prpl->list_icon)(b->
												   uc));
						else
							bs = new_buddy_show(gs, b, (char **)no_icon_xpm);
					}
					bs->connlist = g_slist_append(bs->connlist, gc);
				}
			}
		}
	}
}

static void edit_tree_move(GtkCTree *ctree, GtkCTreeNode *child, GtkCTreeNode *parent,
			   GtkCTreeNode *sibling, gpointer data)
{
	struct gaim_connection *gc, *pc = NULL, *sc = NULL;
	int *ctype, *ptype = NULL, *stype = NULL;

	ctype = (int *)gtk_ctree_node_get_row_data(GTK_CTREE(ctree), child);

	if (parent)
		ptype = (int *)gtk_ctree_node_get_row_data(GTK_CTREE(ctree), parent);

	if (sibling)
		stype = (int *)gtk_ctree_node_get_row_data(GTK_CTREE(ctree), sibling);

	if (*ctype == EDIT_GC) {
		/* not that it particularly matters which order the connections
		 * are in, but just for debugging sake, i guess.... */
		gc = (struct gaim_connection *)ctype;
		connections = g_slist_remove(connections, gc);
		if (sibling) {
			int pos;
			sc = (struct gaim_connection *)stype;
			pos = g_slist_index(connections, sc);
			if (pos)
				connections = g_slist_insert(connections, gc, pos);
			else
				connections = g_slist_prepend(connections, gc);
		} else
			connections = g_slist_append(connections, gc);
	} else if (*ctype == EDIT_BUDDY) {
		/* we moved a buddy. hopefully we just changed groups or positions or something.
		 * if we changed connections, we copy the buddy to the new connection. if the new
		 * connection already had the buddy in its buddy list but in a different group,
		 * we change the group that the buddy is in */
		struct group *old_g, *new_g = (struct group *)ptype;
		struct buddy *s = NULL, *buddy = (struct buddy *)ctype;
		int pos;

		if (buddy->gc != new_g->gc) {
			/* we changed connections */
			struct buddy *a;

			a = find_buddy(new_g->gc, buddy->name);

			if (a) {
				/* the buddy is in the new connection, so we'll remove it from
				 * its current group and add it to the proper group below */
				struct group *og;
				og = find_group_by_buddy(new_g->gc, buddy->name);
				og->members = g_slist_remove(og->members, a);
			} else {
				/* we don't have this buddy yet; let's add him */
				serv_add_buddy(new_g->gc, buddy->name);
			}
		}

		old_g = find_group_by_buddy(buddy->gc, buddy->name);

		if (buddy->gc == new_g->gc)
			/* this is the same connection, so we'll remove it from its old group */
			old_g->members = g_slist_remove(old_g->members, buddy);

		if (sibling) {
			s = (struct buddy *)stype;
			pos = g_slist_index(new_g->members, s);
			if (pos)
				new_g->members = g_slist_insert(new_g->members, buddy, pos);
			else
				new_g->members = g_slist_prepend(new_g->members, buddy);
		} else
			new_g->members = g_slist_append(new_g->members, buddy);

		do_export(buddy->gc);
		if (buddy->gc != new_g->gc) {
			do_export(new_g->gc);
			build_edit_tree();
		}
	} else {		/* group */

		/* move the group. if moving connections, copy the group, and each buddy in the
		 * group. if the buddy exists in the new connection, leave it where it is. */

		struct group *g, *g2, *group;
		int pos;

		pc = (struct gaim_connection *)ptype;
		group = (struct group *)ctype;

		if (g_slist_length(connections) > 1) {
			g = find_group(pc, group->name);
			if (!g)
				g = add_group(pc, group->name);

			pc->groups = g_slist_remove(pc->groups, g);

			if (sibling) {
				g2 = (struct group *)stype;
				pos = g_slist_index(pc->groups, g2);
				if (pos)
					pc->groups = g_slist_insert(pc->groups, g, pos);
				else
					pc->groups = g_slist_prepend(pc->groups, g);
			} else
				pc->groups = g_slist_append(pc->groups, g);

			if (pc != group->gc) {
				GSList *mem;
				struct buddy *b;
				g2 = group;

				mem = g2->members;
				while (mem) {
					b = (struct buddy *)mem->data;
					if (!find_buddy(pc, b->name))
						add_buddy(pc, g->name, b->name, b->show);
					mem = mem->next;
				}
			}
			do_export(pc);
		} else {
			g = group;
			gc = g->gc;

			gc->groups = g_slist_remove(gc->groups, g);

			if (sibling) {
				g2 = (struct group *)stype;
				pos = g_slist_index(gc->groups, g2);
				if (pos)
					gc->groups = g_slist_insert(gc->groups, g, pos);
				else
					gc->groups = g_slist_prepend(gc->groups, g);
			} else
				gc->groups = g_slist_append(gc->groups, g);
			do_export(gc);
		}
	}

	redo_buddy_list();
	update_num_groups();
}



void build_edit_tree()
{
	GtkCTreeNode *c = NULL, *p = NULL, *n;
	GSList *con = connections;
	GSList *grp;
	GSList *mem;
	struct gaim_connection *z;
	struct group *g;
	struct buddy *b;
	char *text[1];

	if (!blist)
		return;

	gtk_clist_freeze(GTK_CLIST(edittree));
	gtk_clist_clear(GTK_CLIST(edittree));


	while (con) {
		z = (struct gaim_connection *)con->data;

		if (g_slist_length(connections) > 1) {
			text[0] = z->username;

			c = gtk_ctree_insert_node(GTK_CTREE(edittree), NULL,
						  NULL, text, 5, NULL, NULL, NULL, NULL, 0, 1);

			gtk_ctree_node_set_row_data(GTK_CTREE(edittree), c, z);
		} else
			c = NULL;

		grp = z->groups;

		while (grp) {
			g = (struct group *)grp->data;

			text[0] = g->name;

			p = gtk_ctree_insert_node(GTK_CTREE(edittree), c,
						  NULL, text, 5, NULL, NULL, NULL, NULL, 0, 1);

			gtk_ctree_node_set_row_data(GTK_CTREE(edittree), p, g);

			n = NULL;

			mem = g->members;

			while (mem) {
				char buf[256];
				b = (struct buddy *)mem->data;
				if (strcmp(b->name, b->show)) {
					g_snprintf(buf, sizeof(buf), "%s (%s)", b->name, b->show);
					text[0] = buf;
				} else
					text[0] = b->name;

				n = gtk_ctree_insert_node(GTK_CTREE(edittree),
							  p, NULL, text, 5,
							  NULL, NULL, NULL, NULL, 1, 1);

				gtk_ctree_node_set_row_data(GTK_CTREE(edittree), n, b);

				mem = mem->next;

			}
			grp = g_slist_next(grp);
		}
		con = g_slist_next(con);
	}

	gtk_clist_thaw(GTK_CLIST(edittree));

}

void ui_add_buddy(struct gaim_connection *gc, struct group *g, struct buddy *b)
{
	GtkCTreeNode *p = NULL, *n;
	char *text[1];
	char buf[256];
	struct group_show *gs = find_group_show(g->name);

	b->edittype = EDIT_BUDDY;

	if (gs)
		update_num_group(gs);

	if (!blist)
		return;

	p = gtk_ctree_find_by_row_data(GTK_CTREE(edittree), NULL, g);
	if (strcmp(b->name, b->show)) {
		g_snprintf(buf, sizeof(buf), "%s (%s)", b->name, b->show);
		text[0] = buf;
	} else
		text[0] = b->name;

	n = gtk_ctree_insert_node(GTK_CTREE(edittree), p, NULL, text, 5, NULL, NULL, NULL, NULL, 1, 1);
	gtk_ctree_node_set_row_data(GTK_CTREE(edittree), n, b);
}

void ui_add_group(struct gaim_connection *gc, struct group *g)
{
	GtkCTreeNode *c = NULL, *p;
	char *text[1];

	g->edittype = EDIT_GROUP;

	if (!blist)
		return;

	c = gtk_ctree_find_by_row_data(GTK_CTREE(edittree), NULL, gc);
	text[0] = g->name;
	p = gtk_ctree_insert_node(GTK_CTREE(edittree), c, NULL, text, 5, NULL, NULL, NULL, NULL, 0, 1);
	gtk_ctree_node_set_row_data(GTK_CTREE(edittree), p, g);

	if (!(blist_options & OPT_BLIST_NO_MT_GRP) && !find_group_show(g->name))
		new_group_show(g->name);
}


static void do_del_buddy(GtkWidget *w, GtkCTree *ctree)
{
	GtkCTreeNode *node;
	struct buddy *b;
	struct group *g;
	int *type;
	GList *i;

	i = GTK_CLIST(edittree)->selection;
	if (i) {
		node = i->data;
		type = (int *)gtk_ctree_node_get_row_data(GTK_CTREE(edittree), node);

		if (*type == EDIT_BUDDY) {
			struct gaim_connection *gct;
			b = (struct buddy *)type;
			g = find_group_by_buddy(b->gc, b->name);
			gct = b->gc;
			serv_remove_buddy(b->gc, b->name);
			remove_buddy(b->gc, g, b);
			gtk_ctree_remove_node(GTK_CTREE(edittree), node);
			do_export(gct);
		} else if (*type == EDIT_GROUP) {
			struct gaim_connection *gc = ((struct group *)type)->gc;
			remove_group(gc, (struct group *)type);
			gtk_ctree_remove_node(GTK_CTREE(edittree), node);
			do_export(gc);
		}

	} else {
		/* Nothing selected. */
	}
}


void import_callback(GtkWidget *widget, void *null)
{
	show_import_dialog();
}

void do_quit()
{
#ifdef USE_APPLET
	applet = NULL;
#endif

	/* first we tell those who have requested it we're quitting */
	plugin_event(event_quit, 0, 0, 0, 0);

	signoff_all();
#ifdef GAIM_PLUGINS
	/* then we remove everyone in a mass suicide */
	remove_all_plugins();
#endif
	system_log(log_quit, NULL, NULL, OPT_LOG_BUDDY_SIGNON | OPT_LOG_MY_SIGNON);
#ifdef USE_PERL
	perl_end();
#endif

	gtk_main_quit();
}

void add_buddy_callback(GtkWidget *widget, void *dummy)
{
	char *grp = NULL;
	GtkCTreeNode *node;
	GList *i;
	struct gaim_connection *gc = NULL;
	int *type;

	i = GTK_CLIST(edittree)->selection;
	if (i) {
		node = i->data;
		type = (int *)gtk_ctree_node_get_row_data(GTK_CTREE(edittree), node);

		if (*type == EDIT_BUDDY) {
			struct buddy *b = (struct buddy *)type;
			struct group *g = find_group_by_buddy(b->gc, b->name);
			grp = g->name;
			gc = b->gc;
		} else if (*type == EDIT_GROUP) {
			struct group *g = (struct group *)type;
			grp = g->name;
			gc = g->gc;
		} else {
			gc = (struct gaim_connection *)type;
		}
	}
	show_add_buddy(gc, NULL, grp, NULL);

}

void add_group_callback(GtkWidget *widget, void *dummy)
{
	GtkCTreeNode *node;
	GList *i;
	struct gaim_connection *gc = NULL;
	int *type;

	i = GTK_CLIST(edittree)->selection;
	if (i) {
		node = i->data;
		type = (int *)gtk_ctree_node_get_row_data(GTK_CTREE(edittree), node);
		if (*type == EDIT_BUDDY)
			gc = ((struct buddy *)type)->gc;
		else if (*type == EDIT_GROUP)
			gc = ((struct group *)type)->gc;
		else
			gc = (struct gaim_connection *)type;
	}
	show_add_group(gc);
}

static void im_callback(GtkWidget *widget, GtkTree *tree)
{
	GList *i;
	struct buddy_show *b = NULL;
	struct conversation *c;
	i = GTK_TREE_SELECTION(tree);
	if (i) {
		b = gtk_object_get_user_data(GTK_OBJECT(i->data));
	}
	if (!i || !b) {
		show_im_dialog();
		return;
	}
	if (!b->name)
		return;

	c = find_conversation(b->name);
	if (c == NULL) {
		c = new_conversation(b->name);
	} else {
		gdk_window_raise(c->window->window);
	}
}


static void info_callback(GtkWidget *widget, GtkTree *tree)
{
	GList *i;
	struct buddy_show *b = NULL;
	i = GTK_TREE_SELECTION(tree);
	if (i) {
		b = gtk_object_get_user_data(GTK_OBJECT(i->data));
	}
	if (!i || !b) {
		show_info_dialog();
		return;
	}
	if (!b->name)
		return;
	if (b->connlist)
		serv_get_info(b->connlist->data, b->name);
}


void chat_callback(GtkWidget *widget, GtkTree *tree)
{
	join_chat();
}

static void away_callback(GtkWidget *widget, GtkTree *tree)
{
	GSList *awy = away_messages;
	GtkWidget *menu;
	GtkWidget *menuitem;

	if (!awy)
		return;

	menu = gtk_menu_new();

	while (awy) {
		struct away_message *a = awy->data;

		menuitem = gtk_menu_item_new_with_label(a->name);
		gtk_menu_append(GTK_MENU(menu), menuitem);
		gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
				   GTK_SIGNAL_FUNC(do_away_message), a);
		gtk_widget_show(menuitem);

		awy = awy->next;
	}

	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 1, time(NULL));
}

void rem_bp(GtkWidget *w, struct buddy_pounce *b)
{
	buddy_pounces = g_list_remove(buddy_pounces, b);
	do_bp_menu();
	save_prefs();
}

void do_pounce(struct gaim_connection *gc, char *name, int when)
{
	char *who;

	struct buddy_pounce *b;
	struct conversation *c;
	struct aim_user *u;

	GList *bp = buddy_pounces;

	who = g_strdup(normalize (name));

	while (bp) {
		b = (struct buddy_pounce *)bp->data;
		bp = bp->next;	/* increment the list here because rem_bp can make our handle bad */

		if (!(b->options & when))
			continue;

		u = find_user(b->pouncer, b->protocol);	/* find our user */
		if (u == NULL)
			continue;

		/* check and see if we're signed on as the pouncer */
		if (u->gc != gc)
			continue;

		if (!g_strcasecmp(who, normalize (b->name))) {	/* find someone to pounce */
			if (b->options & OPT_POUNCE_POPUP) {
				c = find_conversation(name);
				if (c == NULL)
					c = new_conversation(name);

				set_convo_gc(c, u->gc);
			}
			if (b->options & OPT_POUNCE_NOTIFY) {
				char tmp[1024];

				/* I know the line below is really ugly. I only did it this way
				 * because I thought it'd be funny :-) */

				g_snprintf(tmp, sizeof(tmp), "%s has %s", name, 
					(b->options & OPT_POUNCE_SIGNON) ? "signed on" : 
					(b->options & OPT_POUNCE_UNIDLE) ? "returned from being idle" : 
					"returned from being away");

				do_error_dialog(tmp, _("Buddy Pounce"));
			}
			if (b->options & OPT_POUNCE_SEND_IM) {
				if (strlen(b->message) > 0) {
					c = find_conversation(name);

					if (c == NULL)
						c = new_conversation(name);

					set_convo_gc(c, u->gc);

					write_to_conv(c, b->message, WFLAG_SEND, NULL, time((time_t) NULL));
					serv_send_im(u->gc, name, b->message, 0);
				}
			}
			if (b->options & OPT_POUNCE_COMMAND) {
				int pid = fork();

				if (pid == 0) {
					char *args[4];
					args[0] = "sh";
					args[1] = "-c";
					args[2] = b->command;
					args[3] = NULL;
					execvp(args[0], args);
					_exit(0);
				} else if (pid > 0) {
					gtk_timeout_add(100, (GtkFunction)clean_pid, NULL);
				}
			}
			if (b->options & OPT_POUNCE_SOUND) {
				if (strlen(b->sound))
					play_file(b->sound);
				else
					play_sound(POUNCE_DEFAULT);
			}

			if (!(b->options & OPT_POUNCE_SAVE))
				rem_bp(NULL, b);

		}
	}
	g_free(who);
}

static void new_bp_callback(GtkWidget *w, struct buddy *b)
{
	if (b)
		show_new_bp(b->name, b->gc, b->idle, b->uc & UC_UNAVAILABLE);
	else
		show_new_bp(NULL, NULL, 0, 0);
}

void do_bp_menu()
{
	GtkWidget *menuitem, *mess, *messmenu;
	static GtkWidget *remmenu;
	GtkWidget *remitem;
	GtkWidget *sep;
	GList *l;
	struct buddy_pounce *b;
	GList *bp = buddy_pounces;

	l = gtk_container_children(GTK_CONTAINER(bpmenu));

	while (l) {
		gtk_widget_destroy(GTK_WIDGET(l->data));
		l = l->next;
	}

	remmenu = gtk_menu_new();

	menuitem = gtk_menu_item_new_with_label(_("New Buddy Pounce"));
	gtk_menu_append(GTK_MENU(bpmenu), menuitem);
	gtk_widget_show(menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate", GTK_SIGNAL_FUNC(new_bp_callback), NULL);


	while (bp) {

		b = (struct buddy_pounce *)bp->data;
		remitem = gtk_menu_item_new_with_label(b->name);
		gtk_menu_append(GTK_MENU(remmenu), remitem);
		gtk_widget_show(remitem);
		gtk_signal_connect(GTK_OBJECT(remitem), "activate", GTK_SIGNAL_FUNC(rem_bp), b);

		bp = bp->next;

	}

	menuitem = gtk_menu_item_new_with_label(_("Remove Buddy Pounce"));
	gtk_menu_append(GTK_MENU(bpmenu), menuitem);
	gtk_widget_show(menuitem);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), remmenu);
	gtk_widget_show(remmenu);

	sep = gtk_hseparator_new();
	menuitem = gtk_menu_item_new();
	gtk_menu_append(GTK_MENU(bpmenu), menuitem);
	gtk_container_add(GTK_CONTAINER(menuitem), sep);
	gtk_widget_set_sensitive(menuitem, FALSE);
	gtk_widget_show(menuitem);
	gtk_widget_show(sep);

	bp = buddy_pounces;

	while (bp) {

		b = (struct buddy_pounce *)bp->data;

		menuitem = gtk_menu_item_new_with_label(b->name);
		gtk_menu_append(GTK_MENU(bpmenu), menuitem);
		messmenu = gtk_menu_new();
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), messmenu);
		gtk_widget_show(menuitem);

		mess = gtk_menu_item_new_with_label(b->message);
		gtk_menu_append(GTK_MENU(messmenu), mess);
		gtk_widget_show(mess);

		bp = bp->next;

	}

}


static struct group_show *find_group_show(char *group)
{
	GSList *m = shows;
	struct group_show *g = NULL;
	char *who = g_strdup(normalize (group));

	while (m) {
		g = (struct group_show *)m->data;
		if (!g_strcasecmp(normalize (g->name), who))
			 break;
		g = NULL;
		m = m->next;
	}
	g_free(who);

	return g;
}

static struct buddy_show *find_buddy_show(struct group_show *gs, char *name)
{
	GSList *m = gs->members;
	struct buddy_show *b = NULL;
	char *who = g_strdup(normalize (name));

	while (m) {
		b = (struct buddy_show *)m->data;
		if (!g_strcasecmp(normalize (b->name), who))
			 break;
		b = NULL;
		m = m->next;
	}
	g_free(who);

	return b;
}

static int group_number(char *group)
{
	GSList *c = connections;
	struct gaim_connection *g;
	GSList *m;
	struct group *p;
	int pos = 0;

	while (c) {
		g = (struct gaim_connection *)c->data;
		m = g->groups;
		while (m) {
			p = (struct group *)m->data;
			if (!strcmp(p->name, group))
				return pos;
			if (find_group_show(p->name))
				pos++;
			m = m->next;
		}
		c = c->next;
	}
	/* um..... we'll never get here */
	return -1;
}

static int buddy_number(char *group, char *buddy)
{
	GSList *c = connections;
	struct gaim_connection *g;
	struct group *p;
	GSList *z;
	struct buddy *b;
	int pos = 0;
	char *tmp1 = g_strdup(normalize (buddy));
	struct group_show *gs = find_group_show(group);

	while (c) {
		g = (struct gaim_connection *)c->data;
		p = find_group(g, group);
		if (!p) {
			c = c->next;
			continue;
		}
		z = p->members;
		while (z) {
			b = (struct buddy *)z->data;
			if (!strcmp(tmp1, normalize (b->name))) {
				g_free(tmp1);
				return pos;
			}
			if (find_buddy_show(gs, b->name))
				pos++;
			z = z->next;
		}
		c = c->next;
	}
	/* we shouldn't ever get here */
	debug_printf("got to bad place in buddy_number\n");
	g_free(tmp1);
	return -1;
}

static struct group_show *new_group_show(char *group)
{
	struct group_show *g = g_new0(struct group_show, 1);
	int pos = group_number(group);

	g->name = g_strdup(group);

	g->item = gtk_tree_item_new();
	gtk_tree_insert(GTK_TREE(buddies), g->item, pos);
	gtk_signal_connect(GTK_OBJECT(g->item), "button_press_event",
			   GTK_SIGNAL_FUNC(handle_click_group), g);
	gtk_widget_show(g->item);

	g->label = gtk_label_new(group);
	gtk_misc_set_alignment(GTK_MISC(g->label), 0.0, 0.5);
	gtk_container_add(GTK_CONTAINER(g->item), g->label);
	gtk_widget_show(g->label);

	shows = g_slist_insert(shows, g, pos);
	update_num_groups(g);
	return g;
}

static struct buddy_show *new_buddy_show(struct group_show *gs, struct buddy *buddy, char **xpm)
{
	struct buddy_show *b = g_new0(struct buddy_show, 1);
	GtkWidget *box;
	GdkPixmap *pm;
	GdkBitmap *bm;
	int pos = buddy_number(gs->name, buddy->name);
	b->sound = 0;

	if (gs->members == NULL) {
		gs->tree = gtk_tree_new();
		gtk_tree_item_set_subtree(GTK_TREE_ITEM(gs->item), gs->tree);
		gtk_tree_item_expand(GTK_TREE_ITEM(gs->item));
		gtk_widget_show(gs->tree);
	}

	b->name = g_strdup(buddy->name);
	b->show = g_strdup(buddy->show);

	b->item = gtk_tree_item_new();
	gtk_tree_insert(GTK_TREE(gs->tree), b->item, pos);
	gtk_object_set_user_data(GTK_OBJECT(b->item), b);
	gtk_signal_connect(GTK_OBJECT(b->item), "button_press_event",
			   GTK_SIGNAL_FUNC(handle_click_buddy), b);
	gtk_widget_show(b->item);

	box = gtk_hbox_new(FALSE, 1);
	gtk_container_add(GTK_CONTAINER(b->item), box);
	gtk_widget_show(box);

	pm = gdk_pixmap_create_from_xpm_d(blist->window, &bm, NULL, xpm ? xpm : no_icon_xpm);
	b->pix = gtk_pixmap_new(pm, bm);
	gtk_box_pack_start(GTK_BOX(box), b->pix, FALSE, FALSE, 1);
	gtk_widget_show(b->pix);
	if (!(blist_options & OPT_BLIST_SHOW_PIXMAPS))
		gtk_widget_hide(b->pix);
	gdk_pixmap_unref(pm);
	gdk_bitmap_unref(bm);

	b->label = gtk_label_new(buddy->show);
	gtk_misc_set_alignment(GTK_MISC(b->label), 0.0, 0.5);
	gtk_box_pack_start(GTK_BOX(box), b->label, FALSE, FALSE, 1);
	gtk_widget_show(b->label);

	b->warn = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(box), b->warn, FALSE, FALSE, 1);
	gtk_widget_show(b->warn);

	b->idle = gtk_label_new("");
	gtk_box_pack_end(GTK_BOX(box), b->idle, FALSE, FALSE, 1);
	gtk_widget_show(b->idle);

	gs->members = g_slist_insert(gs->members, b, pos);
	update_num_group(gs);
	return b;
}

static void remove_buddy_show(struct group_show *gs, struct buddy_show *bs)
{
	/* the name of this function may be misleading, but don't let it fool you. the point
	 * of this is to remove bs->item from gs->tree, and make sure gs->tree still exists
	 * and is a valid tree afterwards. Otherwise, Bad Things will happen. */
	gtk_tree_remove_item(GTK_TREE(gs->tree), bs->item);
	bs->item = NULL;
}

static struct group_show *find_gs_by_bs(struct buddy_show *b)
{
	GSList *m, *n;
	struct group_show *g = NULL;
	struct buddy_show *h;

	m = shows;
	while (m) {
		g = (struct group_show *)m->data;
		n = g->members;
		while (n) {
			h = (struct buddy_show *)n->data;
			if (h == b)
				return g;
			n = n->next;
		}
		g = NULL;
		m = m->next;
	}

	return g;
}

static gint log_timeout(struct buddy_show *b)
{
	/* this part is really just a bad hack because of a bug I can't find */
	GSList *s = shows;
	while (s) {
		struct group_show *gs = s->data;
		GSList *m = gs->members;
		while (m) {
			if (b == m->data)
				break;
			m = m->next;
		}
		if (m != NULL)
			break;
		s = s->next;
	}
	if (!s)
		return 0;

	/* this is the real part. */
	if (!b->connlist) {
		struct group_show *g = find_gs_by_bs(b);
		g->members = g_slist_remove(g->members, b);
		if (blist)
			remove_buddy_show(g, b);
		else
			debug_printf("log_timeout but buddy list not available\n");
		if ((g->members == NULL) && (blist_options & OPT_BLIST_NO_MT_GRP)) {
			shows = g_slist_remove(shows, g);
			if (blist)
				gtk_tree_remove_item(GTK_TREE(buddies), g->item);
			g_free(g->name);
			g_free(g);
		}
		gtk_timeout_remove(b->log_timer);
		b->log_timer = 0;
		g_free(b->name);
		g_free(b->show);
		g_free(b);
	} else {
		/* um.... what do we have to do here? just update the pixmap? */
		GdkPixmap *pm;
		GdkBitmap *bm;
		gchar **xpm = NULL;
		struct buddy *light = find_buddy(b->connlist->data, b->name);
		if (((struct gaim_connection *)b->connlist->data)->prpl->list_icon)
			xpm =
			    (*((struct gaim_connection *)b->connlist->data)->prpl->list_icon)(light->uc);
		if (xpm == NULL)
			xpm = (char **)no_icon_xpm;
		pm = gdk_pixmap_create_from_xpm_d(blist->window, &bm, NULL, xpm);
		gtk_widget_hide(b->pix);
		gtk_pixmap_set(GTK_PIXMAP(b->pix), pm, bm);
		gtk_widget_show(b->pix);
		if (!(blist_options & OPT_BLIST_SHOW_PIXMAPS))
			gtk_widget_hide(b->pix);
		if (misc_options & OPT_MISC_BUDDY_TICKER)
			BuddyTickerSetPixmap(b->name, pm, bm);
		gdk_pixmap_unref(pm);
		gdk_bitmap_unref(bm);
		gtk_timeout_remove(b->log_timer);
		b->log_timer = 0;
		b->sound = 0;
	}
	return 0;
}

static char *caps_string(gushort caps)
{
	static char buf[256], *tmp;
	int count = 0, i = 0;
	gushort bit = 1;
	while (bit <= 0x80) {
		if (bit & caps) {
			switch (bit) {
			case 0x1:
				tmp = _("Buddy Icon");
				break;
			case 0x2:
				tmp = _("Voice");
				break;
			case 0x4:
				tmp = _("IM Image");
				break;
			case 0x8:
				tmp = _("Chat");
				break;
			case 0x10:
				tmp = _("Get File");
				break;
			case 0x20:
				tmp = _("Send File");
				break;
			case 0x40:
				tmp = _("Games");
				break;
			case 0x80:
				tmp = _("Stocks");
				break;
			default:
				tmp = NULL;
				break;
			}
			if (tmp)
				i += g_snprintf(buf + i, sizeof(buf) - i, "%s%s", (count ? ", " : ""),
						tmp);
			count++;
		}
		bit <<= 1;
	}
	return buf;
}

/* for this we're just going to assume the first connection that registered the buddy.
 * if it's not the one you were hoping for then you're shit out of luck */
static void update_idle_time(struct buddy_show *bs)
{
	/* this also updates the tooltip since that has idle time in it */
	char idlet[16], warnl[16];
	time_t t;
	int ihrs, imin;
	struct buddy *b;

	char infotip[2048];
	char warn[256];
	char caps[256];
	char *sotime = NULL, *itime;

	time(&t);
	if (!bs->connlist)
		return;
	b = find_buddy(bs->connlist->data, bs->name);
	if (!b)
		return;
	ihrs = (t - b->idle) / 3600;
	imin = ((t - b->idle) / 60) % 60;

	if (ihrs)
		g_snprintf(idlet, sizeof idlet, "(%d:%02d)", ihrs, imin);
	else
		g_snprintf(idlet, sizeof idlet, "(%d)", imin);

	gtk_widget_hide(bs->idle);
	if (b->idle)
		gtk_label_set(GTK_LABEL(bs->idle), idlet);
	else
		gtk_label_set(GTK_LABEL(bs->idle), "");
	if (blist_options & OPT_BLIST_SHOW_IDLETIME)
		gtk_widget_show(bs->idle);

	/* now we do the tooltip */
	if (b->signon) {
		char *stime = sec_to_text(t - b->signon +
					  ((struct gaim_connection *)bs->connlist->data)->
					  correction_time);
		sotime = g_strdup_printf(_("Logged in: %s\n"), stime);
		g_free(stime);
	}

	if (b->idle)
		itime = sec_to_text(t - b->idle);
	else {
		itime = g_malloc(1);
		itime[0] = 0;
	}

	if (b->evil) {
		g_snprintf(warn, sizeof warn, _("Warnings: %d%%\n"), b->evil);
		g_snprintf(warnl, sizeof warnl, "(%d%%)", b->evil);
	} else {
		warn[0] = '\0';
		warnl[0] = '\0';
	}
	gtk_widget_hide(bs->warn);
	gtk_label_set(GTK_LABEL(bs->warn), warnl);
	if (blist_options & OPT_BLIST_SHOW_WARN)
		gtk_widget_show(bs->warn);

	if (b->caps)
		g_snprintf(caps, sizeof caps, _("Capabilities: %s\n"), caps_string(b->caps));
	else
		caps[0] = '\0';

	g_snprintf(infotip, sizeof infotip, _("Alias: %s               \nScreen Name: %s\n"
					      "%s%s%s%s%s%s"),
		   b->show, b->name,
		   (b->signon ? sotime : ""), warn,
		   (b->idle ? _("Idle: ") : ""), itime, (b->idle ? "\n" : ""), caps);

	gtk_tooltips_set_tip(tips, GTK_WIDGET(bs->item), infotip, "");

	if (b->signon)
		g_free(sotime);
	g_free(itime);
}

void update_idle_times()
{
	GSList *grp = shows;
	GSList *mem;
	struct buddy_show *b;
	struct group_show *g;

	while (grp) {
		g = (struct group_show *)grp->data;
		mem = g->members;
		while (mem) {
			b = (struct buddy_show *)mem->data;
			update_idle_time(b);
			mem = mem->next;
		}
		grp = grp->next;
	}
}

void set_buddy(struct gaim_connection *gc, struct buddy *b)
{
	struct group *g = find_group_by_buddy(gc, b->name);
	struct group_show *gs;
	struct buddy_show *bs;
	GdkPixmap *pm;
	GdkBitmap *bm;
	char **xpm = NULL;

	if (!blist)
		return;

	if (b->present) {
		if ((gs = find_group_show(g->name)) == NULL)
			gs = new_group_show(g->name);
		if ((bs = find_buddy_show(gs, b->name)) == NULL)
			bs = new_buddy_show(gs, b, (char **)login_icon_xpm);
		if (!g_slist_find(bs->connlist, gc))
			bs->connlist = g_slist_append(bs->connlist, gc);
		if (b->present == 1) {
			if (bs->sound != 2)
				play_sound(BUDDY_ARRIVE);
			pm = gdk_pixmap_create_from_xpm_d(blist->window, &bm,
							  NULL, (char **)login_icon_xpm);
			gtk_widget_hide(bs->pix);
			gtk_pixmap_set(GTK_PIXMAP(bs->pix), pm, bm);
			gtk_widget_show(bs->pix);
			if (misc_options & OPT_MISC_BUDDY_TICKER) {
				BuddyTickerAddUser(b->name, pm, bm);
				gtk_timeout_add(10000, (GtkFunction)BuddyTickerLogonTimeout, b->name);
			}
			gdk_pixmap_unref(pm);
			gdk_bitmap_unref(bm);
			b->present = 2;
			if (bs->log_timer > 0)
				gtk_timeout_remove(bs->log_timer);
			bs->log_timer = gtk_timeout_add(10000, (GtkFunction)log_timeout, bs);
			update_num_group(gs);
			if ((bs->sound != 2) && (im_options & OPT_IM_LOGON)) {
				struct conversation *c = find_conversation(b->name);
				if (c) {
					char tmp[1024];
					g_snprintf(tmp, sizeof(tmp), _("%s logged in."), b->name);
					write_to_conv(c, tmp, WFLAG_SYSTEM, NULL, time((time_t) NULL));
				}
			}
			bs->sound = 2;
		} else if (bs->log_timer == 0) {
			if (gc->prpl->list_icon)
				xpm = (*gc->prpl->list_icon)(b->uc);
			if (xpm == NULL)
				xpm = (char **)no_icon_xpm;
			pm = gdk_pixmap_create_from_xpm_d(blist->window, &bm, NULL, xpm);
			gtk_widget_hide(bs->pix);
			gtk_pixmap_set(GTK_PIXMAP(bs->pix), pm, bm);
			gtk_widget_show(bs->pix);
			if (!(blist_options & OPT_BLIST_SHOW_PIXMAPS))
				gtk_widget_hide(bs->pix);
			if (misc_options & OPT_MISC_BUDDY_TICKER)
				BuddyTickerSetPixmap(b->name, pm, bm);
			gdk_pixmap_unref(pm);
			gdk_bitmap_unref(bm);
		}
		update_idle_time(bs);
	} else {
		gs = find_group_show(g->name);
		if (!gs)
			return;
		bs = find_buddy_show(gs, b->name);
		if (!bs)
			return;
		if (!bs->connlist)
			return;	/* we won't do signoff updates for
				   buddies that have already signed
				   off */
		if (bs->sound != 1)
			play_sound(BUDDY_LEAVE);

		bs->connlist = g_slist_remove(bs->connlist, gc);
		if (bs->log_timer > 0)
			gtk_timeout_remove(bs->log_timer);
		bs->log_timer = gtk_timeout_add(10000, (GtkFunction)log_timeout, bs);
		update_num_group(gs);
		pm = gdk_pixmap_create_from_xpm_d(blist->window, &bm, NULL, logout_icon_xpm);
		gtk_widget_hide(bs->pix);
		gtk_pixmap_set(GTK_PIXMAP(bs->pix), pm, bm);
		gtk_widget_show(bs->pix);
		if (misc_options & OPT_MISC_BUDDY_TICKER) {
			BuddyTickerSetPixmap(b->name, pm, bm);
			gtk_timeout_add(10000, (GtkFunction)BuddyTickerLogoutTimeout, b->name);
		}
		gdk_pixmap_unref(pm);
		gdk_bitmap_unref(bm);
		if ((bs->sound != 1) && (im_options & OPT_IM_LOGON)) {
			struct conversation *c = find_conversation(b->name);
			if (c) {
				char tmp[1024];
				g_snprintf(tmp, sizeof(tmp), _("%s logged out."), b->name);
				write_to_conv(c, tmp, WFLAG_SYSTEM, NULL, time((time_t) NULL));
			}
		}

		bs->sound = 1;
	}
}


static void move_blist_window(GtkWidget *w, GdkEventConfigure *e, void *dummy)
{
	int x, y, width, height;
	int save = 0;
	gdk_window_get_position(blist->window, &x, &y);
	gdk_window_get_size(blist->window, &width, &height);

	if (e->send_event) {	/* Is a position event */
		if (blist_pos.x != x || blist_pos.y != y)
			save = 1;
		blist_pos.x = x;
		blist_pos.y = y;
	} else {		/* Is a size event */
		if (blist_pos.xoff != x || blist_pos.yoff != y || blist_pos.width != width)
			save = 1;

		blist_pos.width = width;
		blist_pos.height = height;
		blist_pos.xoff = x;
		blist_pos.yoff = y;
	}

	if (save)
		save_prefs();

}


/*******************************************************************
 *
 * Helper funs for making the menu
 *
 *******************************************************************/

void gaim_separator(GtkWidget *menu)
{
	GtkWidget *sep, *menuitem;
	sep = gtk_hseparator_new();
	menuitem = gtk_menu_item_new();
	gtk_menu_append(GTK_MENU(menu), menuitem);
	gtk_container_add(GTK_CONTAINER(menuitem), sep);
	gtk_widget_set_sensitive(menuitem, FALSE);
	gtk_widget_show(menuitem);
	gtk_widget_show(sep);
}

GtkWidget *gaim_new_item(GtkWidget *menu, const char *str)
{
	GtkWidget *menuitem;
	GtkWidget *label;

	menuitem = gtk_menu_item_new();
	if (menu)
		gtk_menu_append(GTK_MENU(menu), menuitem);
	gtk_widget_show(menuitem);

	label = gtk_label_new(str);
	gtk_label_set_pattern(GTK_LABEL(label), "_");
	gtk_container_add(GTK_CONTAINER(menuitem), label);
	gtk_widget_show(label);

	gtk_widget_add_accelerator(menuitem, "activate-item", accel, str[0],
				   GDK_MOD1_MASK, GTK_ACCEL_LOCKED);
	gtk_widget_lock_accelerators(menuitem);

	return menuitem;
}

GtkWidget *gaim_new_item_with_pixmap(GtkWidget *menu, const char *str, char **xpm, GtkSignalFunc sf,
				     guint accel_key, guint accel_mods, char *mod)
{
	GtkWidget *menuitem;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *pixmap;
	GdkPixmap *pm;
	GdkBitmap *mask;

	menuitem = gtk_menu_item_new();
	if (menu)
		gtk_menu_append(GTK_MENU(menu), menuitem);
	if (sf)
		/* passing 1 is necessary so if we sign off closing the account editor doesn't exit */
		gtk_signal_connect(GTK_OBJECT(menuitem), "activate", sf, (void *)1);
	gtk_widget_show(menuitem);

	/* Create our container */
	hbox = gtk_hbox_new(FALSE, 2);
	gtk_container_add(GTK_CONTAINER(menuitem), hbox);
	gtk_widget_show(hbox);

	/* Create our pixmap and pack it */
	gtk_widget_realize(menu->parent);
	pm = gdk_pixmap_create_from_xpm_d(menu->parent->window, &mask, NULL, xpm);
	pixmap = gtk_pixmap_new(pm, mask);
	gtk_widget_show(pixmap);
	gdk_pixmap_unref(pm);
	gdk_bitmap_unref(mask);
	gtk_box_pack_start(GTK_BOX(hbox), pixmap, FALSE, FALSE, 2);

	/* Create our label and pack it */
	label = gtk_label_new(str);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
	gtk_widget_show(label);

	if (mod) {
		label = gtk_label_new(mod);
		gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 2);
		gtk_widget_show(label);
	}

	if (accel_key) {
		gtk_widget_add_accelerator(menuitem, "activate", accel, accel_key,
					   accel_mods, GTK_ACCEL_LOCKED);
		gtk_widget_lock_accelerators(menuitem);
	}

	return menuitem;
}



void build_imchat_box(gboolean on)
{
	if (on) {
		if (imchatbox)
			return;

		imbutton = gtk_button_new_with_label(_("IM"));
		infobutton = gtk_button_new_with_label(_("Info"));
		chatbutton = gtk_button_new_with_label(_("Chat"));
		awaybutton = gtk_button_new_with_label(_("Away"));

		imchatbox = gtk_hbox_new(TRUE, 10);

		if (misc_options & OPT_MISC_COOL_LOOK) {
			gtk_button_set_relief(GTK_BUTTON(imbutton), GTK_RELIEF_NONE);
			gtk_button_set_relief(GTK_BUTTON(infobutton), GTK_RELIEF_NONE);
			gtk_button_set_relief(GTK_BUTTON(chatbutton), GTK_RELIEF_NONE);
			gtk_button_set_relief(GTK_BUTTON(awaybutton), GTK_RELIEF_NONE);
		}

		/* Put the buttons in the hbox */
		gtk_widget_show(imbutton);
		gtk_widget_show(infobutton);
		gtk_widget_show(chatbutton);
		gtk_widget_show(awaybutton);

		gtk_box_pack_start(GTK_BOX(imchatbox), imbutton, TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(imchatbox), infobutton, TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(imchatbox), chatbutton, TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(imchatbox), awaybutton, TRUE, TRUE, 0);
		gtk_container_border_width(GTK_CONTAINER(imchatbox), 5);

		gtk_signal_connect(GTK_OBJECT(imbutton), "clicked", GTK_SIGNAL_FUNC(im_callback),
				   buddies);
		gtk_signal_connect(GTK_OBJECT(infobutton), "clicked", GTK_SIGNAL_FUNC(info_callback),
				   buddies);
		gtk_signal_connect(GTK_OBJECT(chatbutton), "clicked", GTK_SIGNAL_FUNC(chat_callback),
				   buddies);
		gtk_signal_connect(GTK_OBJECT(awaybutton), "clicked", GTK_SIGNAL_FUNC(away_callback),
				   buddies);

		gtk_tooltips_set_tip(tips, infobutton, _("Information on selected Buddy"), "Penguin");
		gtk_tooltips_set_tip(tips, imbutton, _("Send Instant Message"), "Penguin");
		gtk_tooltips_set_tip(tips, chatbutton, _("Start/join a Buddy Chat"), "Penguin");
		gtk_tooltips_set_tip(tips, awaybutton, _("Activate Away Message"), "Penguin");

		gtk_box_pack_start(GTK_BOX(buddypane), imchatbox, FALSE, FALSE, 0);

		gtk_widget_show(imchatbox);
	} else {
		if (imchatbox)
			gtk_widget_destroy(imchatbox);
		imchatbox = NULL;
	}
}



void show_buddy_list()
{

	/* Build the buddy list, based on *config */

	GtkWidget *sw;
	GtkWidget *menu;
#ifdef USE_PERL
	GtkWidget *perlmenu;
#endif
#ifdef NO_MULTI
	GtkWidget *setmenu;
	GtkWidget *findmenu;
#endif
	GtkWidget *menubar;
	GtkWidget *vbox;
	GtkWidget *menuitem;
	GtkWidget *notebook;
	GtkWidget *label;
	GtkWidget *bbox;
	GtkWidget *tbox;

	if (blist) {
		gtk_widget_show(blist);
		return;
	}

#ifdef USE_APPLET
	blist = gtk_window_new(GTK_WINDOW_DIALOG);
#else
	blist = gtk_window_new(GTK_WINDOW_TOPLEVEL);
#endif

	gtk_window_set_wmclass(GTK_WINDOW(blist), "buddy_list", "Gaim");

	gtk_widget_realize(blist);
	aol_icon(blist->window);

	gtk_window_set_policy(GTK_WINDOW(blist), TRUE, TRUE, TRUE);

	accel = gtk_accel_group_new();
	gtk_accel_group_attach(accel, GTK_OBJECT(blist));

	menubar = gtk_menu_bar_new();

	menu = gtk_menu_new();
	gtk_menu_set_accel_group(GTK_MENU(menu), accel);


	menuitem = gaim_new_item(NULL, _("File"));
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), menu);
	gtk_menu_bar_append(GTK_MENU_BAR(menubar), menuitem);

	gaim_new_item_with_pixmap(menu, _("Add A Buddy"), add_small_xpm,
				  GTK_SIGNAL_FUNC(add_buddy_callback), 'b', GDK_CONTROL_MASK, "Ctl+B");
	gaim_new_item_with_pixmap(menu, _("Join A Chat"), pounce_small_xpm,
				  GTK_SIGNAL_FUNC(chat_callback), 'c', GDK_CONTROL_MASK, "Ctl+C");
	gaim_new_item_with_pixmap(menu, _("New Instant Message"), send_small_xpm,
				  GTK_SIGNAL_FUNC(show_im_dialog), 'i', GDK_CONTROL_MASK, "Ctl+I");

	gaim_separator(menu);

	gaim_new_item_with_pixmap(menu, _("Import Buddy List"), import_small_xpm,
				  GTK_SIGNAL_FUNC(import_callback), 0, 0, 0);
	/*gaim_new_item_with_pixmap(menu, _("Export Buddy List"), export_small_xpm,
	   GTK_SIGNAL_FUNC(show_export_dialog), 0, 0, 0); */
	gaim_separator(menu);
	gaim_new_item_with_pixmap(menu, _("Signoff"), logout_icon_xpm,
				  GTK_SIGNAL_FUNC(signoff_all), 'd', GDK_CONTROL_MASK, "Ctl+D");

#ifndef USE_APPLET
	gaim_new_item_with_pixmap(menu, _("Quit"), exit_small_xpm,
				  GTK_SIGNAL_FUNC(do_quit), 'q', GDK_CONTROL_MASK, "Ctl+Q");
#else
	gaim_new_item_with_pixmap(menu, _("Close"), close_small_xpm,
				  GTK_SIGNAL_FUNC(applet_destroy_buddy), 'x', GDK_CONTROL_MASK, "Ctl+X");
#endif

	menu = gtk_menu_new();

	menuitem = gaim_new_item(NULL, _("Tools"));
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), menu);
	gtk_menu_bar_append(GTK_MENU_BAR(menubar), menuitem);

	awaymenu = gtk_menu_new();
	menuitem = gaim_new_item_with_pixmap(menu, _("Away"), away_small_xpm, NULL, 0, 0, 0);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), awaymenu);
	do_away_menu();

	bpmenu = gtk_menu_new();
	menuitem = gaim_new_item_with_pixmap(menu, _("Buddy Pounce"), pounce_small_xpm, NULL, 0, 0, 0);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), bpmenu);
	do_bp_menu();

	gaim_separator(menu);

#ifdef NO_MULTI
	findmenu = gtk_menu_new();
	gtk_widget_show(findmenu);
	menuitem = gaim_new_item_with_pixmap(menu, _("Search for Buddy"), search_small_xpm,
					     NULL, 0, 0, 0);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), findmenu);
	gtk_widget_show(menuitem);
	menuitem = gtk_menu_item_new_with_label(_("by Email"));
	gtk_menu_append(GTK_MENU(findmenu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate", GTK_SIGNAL_FUNC(show_find_email), NULL);
	gtk_widget_show(menuitem);
	menuitem = gtk_menu_item_new_with_label(_("by Dir Info"));
	gtk_menu_append(GTK_MENU(findmenu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate", GTK_SIGNAL_FUNC(show_find_info),
			   connections->data);
	gtk_widget_show(menuitem);

	setmenu = gtk_menu_new();
	gtk_widget_show(setmenu);
	menuitem = gaim_new_item_with_pixmap(menu, _("Settings"), prefs_small_xpm, NULL, 0, 0, 0);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), setmenu);
	menuitem = gtk_menu_item_new_with_label(_("User Info"));
	gtk_menu_append(GTK_MENU(setmenu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate", GTK_SIGNAL_FUNC(show_set_info), NULL);
	gtk_widget_show(menuitem);
	menuitem = gtk_menu_item_new_with_label(_("Directory Info"));
	gtk_menu_append(GTK_MENU(setmenu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate", GTK_SIGNAL_FUNC(show_set_dir), NULL);
	gtk_widget_show(menuitem);
	menuitem = gtk_menu_item_new_with_label(_("Change Password"));
	gtk_menu_append(GTK_MENU(setmenu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate", GTK_SIGNAL_FUNC(show_change_passwd), NULL);
	gtk_widget_show(menuitem);
#else
	gaim_new_item_with_pixmap(menu, _("Accounts"), add_small_xpm,
				  GTK_SIGNAL_FUNC(account_editor), 'a', GDK_CONTROL_MASK, "Ctl+A");

	protomenu = gtk_menu_new();
	menuitem =
	    gaim_new_item_with_pixmap(menu, _("Protocol Actions"), prefs_small_xpm, NULL, 0, 0, 0);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), protomenu);
	do_proto_menu();
#endif

	gaim_new_item_with_pixmap(menu, _("Preferences"), prefs_small_xpm,
				  GTK_SIGNAL_FUNC(show_prefs), 'p', GDK_CONTROL_MASK, "Ctl+P");
	gaim_new_item_with_pixmap(menu, _("View System Log"), prefs_small_xpm,
				  GTK_SIGNAL_FUNC(show_syslog), 0, 0, 0);

	gaim_separator(menu);

#ifdef GAIM_PLUGINS
	gaim_new_item_with_pixmap(menu, _("Plugins"), plugins_small_xpm, GTK_SIGNAL_FUNC(show_plugins),
				  0, 0, 0);
#endif
#ifdef USE_PERL
	perlmenu = gtk_menu_new();
	gtk_widget_show(perlmenu);
	menuitem = gaim_new_item_with_pixmap(menu, _("Perl"), plugins_small_xpm, NULL, 0, 0, 0);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), perlmenu);
	gtk_widget_show(menuitem);
	menuitem = gtk_menu_item_new_with_label(_("Load Script"));
	gtk_menu_append(GTK_MENU(perlmenu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate", GTK_SIGNAL_FUNC(load_perl_script), NULL);
	gtk_widget_show(menuitem);
	menuitem = gtk_menu_item_new_with_label(_("Unload All Scripts"));
	gtk_menu_append(GTK_MENU(perlmenu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate", GTK_SIGNAL_FUNC(unload_perl_scripts), NULL);
	gtk_widget_show(menuitem);
	menuitem = gtk_menu_item_new_with_label(_("List Scripts"));
	gtk_menu_append(GTK_MENU(perlmenu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate", GTK_SIGNAL_FUNC(list_perl_scripts), NULL);
	gtk_widget_show(menuitem);
#endif

	menu = gtk_menu_new();

	menuitem = gaim_new_item(NULL, _("Help"));
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), menu);
	gtk_menu_item_right_justify(GTK_MENU_ITEM(menuitem));
	gtk_menu_bar_append(GTK_MENU_BAR(menubar), menuitem);

	gaim_new_item_with_pixmap(menu, _("About Gaim"), about_small_xpm, show_about, GDK_F1, 0, NULL);

	gtk_widget_show(menubar);

	vbox = gtk_vbox_new(FALSE, 0);

	notebook = gtk_notebook_new();




	/* Do buddy list stuff */
	/* FIXME: spacing on both panes is ad hoc */
	buddypane = gtk_vbox_new(FALSE, 1);

	buddies = gtk_tree_new();
	sw = gtk_scrolled_window_new(NULL, NULL);

	tips = gtk_tooltips_new();
	gtk_object_set_data(GTK_OBJECT(blist), _("Buddy List"), tips);

	/* Now the buddy list */
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw), buddies);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_usize(sw, 200, 200);
	gtk_widget_show(buddies);
	gtk_widget_show(sw);

	gtk_box_pack_start(GTK_BOX(buddypane), sw, TRUE, TRUE, 0);
	gtk_widget_show(buddypane);

	if (!(blist_options & OPT_BLIST_NO_BUTTONS))
		build_imchat_box(TRUE);


	/* Swing the edit buddy */
	editpane = gtk_vbox_new(FALSE, 1);

	addbutton = gtk_button_new_with_label(_("Add"));
	groupbutton = gtk_button_new_with_label(_("Group"));
	rembutton = gtk_button_new_with_label(_("Remove"));

	if (misc_options & OPT_MISC_COOL_LOOK) {
		gtk_button_set_relief(GTK_BUTTON(addbutton), GTK_RELIEF_NONE);
		gtk_button_set_relief(GTK_BUTTON(groupbutton), GTK_RELIEF_NONE);
		gtk_button_set_relief(GTK_BUTTON(rembutton), GTK_RELIEF_NONE);
	}

	edittree = gtk_ctree_new(1, 0);
	gtk_ctree_set_line_style(GTK_CTREE(edittree), GTK_CTREE_LINES_SOLID);
	gtk_ctree_set_expander_style(GTK_CTREE(edittree), GTK_CTREE_EXPANDER_SQUARE);
	gtk_clist_set_reorderable(GTK_CLIST(edittree), TRUE);
	gtk_signal_connect(GTK_OBJECT(edittree), "button_press_event",
			   GTK_SIGNAL_FUNC(click_edit_tree), NULL);

	gtk_ctree_set_drag_compare_func(GTK_CTREE(edittree),
					(GtkCTreeCompareDragFunc) edit_drag_compare_func);


	gtk_signal_connect_after(GTK_OBJECT(edittree), "tree_move",
				 GTK_SIGNAL_FUNC(edit_tree_move), NULL);


	bbox = gtk_hbox_new(TRUE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(bbox), 5);
	tbox = gtk_scrolled_window_new(NULL, NULL);
	/* Put the buttons in the box */
	gtk_box_pack_start(GTK_BOX(bbox), addbutton, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(bbox), groupbutton, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(bbox), rembutton, TRUE, TRUE, 0);

	gtk_tooltips_set_tip(tips, addbutton, _("Add a new Buddy"), "Penguin");
	gtk_tooltips_set_tip(tips, groupbutton, _("Add a new Group"), "Penguin");
	gtk_tooltips_set_tip(tips, rembutton, _("Remove selected Buddy/Group"), "Penguin");

	/* And the boxes in the box */
	gtk_box_pack_start(GTK_BOX(editpane), tbox, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(editpane), bbox, FALSE, FALSE, 0);

	/* Handle closes right */



	/* Finish up */
	gtk_widget_show(addbutton);
	gtk_widget_show(groupbutton);
	gtk_widget_show(rembutton);
	gtk_widget_show(edittree);
	gtk_widget_show(tbox);
	gtk_widget_show(bbox);
	gtk_widget_show(editpane);



	update_button_pix();



	label = gtk_label_new(_("Online"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), buddypane, label);
	label = gtk_label_new(_("Edit Buddies"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), editpane, label);

	gtk_widget_show_all(notebook);

	/* Pack things in the vbox */
	gtk_widget_show(vbox);


	gtk_widget_show(notebook);

	/* Enable buttons */

	gtk_signal_connect(GTK_OBJECT(rembutton), "clicked", GTK_SIGNAL_FUNC(do_del_buddy), edittree);
	gtk_signal_connect(GTK_OBJECT(addbutton), "clicked", GTK_SIGNAL_FUNC(add_buddy_callback), NULL);
	gtk_signal_connect(GTK_OBJECT(groupbutton), "clicked", GTK_SIGNAL_FUNC(add_group_callback),
			   NULL);
	gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);

	gtk_container_add(GTK_CONTAINER(blist), vbox);

#ifndef USE_APPLET
	gtk_signal_connect(GTK_OBJECT(blist), "delete_event", GTK_SIGNAL_FUNC(do_quit), blist);
#else
	gtk_signal_connect(GTK_OBJECT(blist), "delete_event", GTK_SIGNAL_FUNC(applet_destroy_buddy),
			   NULL);
#endif

	gtk_signal_connect(GTK_OBJECT(blist), "configure_event", GTK_SIGNAL_FUNC(move_blist_window),
			   NULL);



	/* The edit tree */
	gtk_container_add(GTK_CONTAINER(tbox), edittree);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(tbox),
				       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);


	gtk_window_set_title(GTK_WINDOW(blist), _("Gaim - Buddy List"));

	if (blist_options & OPT_BLIST_SAVED_WINDOWS) {
		if (blist_pos.width != 0) {	/* Sanity check! */
			gtk_widget_set_uposition(blist, blist_pos.x - blist_pos.xoff,
						 blist_pos.y - blist_pos.yoff);
			gtk_widget_set_usize(blist, blist_pos.width, blist_pos.height);
		}
	}
}

void refresh_buddy_window()
{
	build_edit_tree();

	update_button_pix();
	gtk_widget_show(blist);
}
