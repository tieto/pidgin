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
#ifdef USE_APPLET
#include <gnome.h>
#include <applet-widget.h>
#include "gnome_applet_mgr.h"
#endif /* USE_APPLET */
#ifdef GAIM_PLUGINS
#include <dlfcn.h>
#endif /* GAIM_PLUGINS */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include "gaim.h"
#include <aim.h>
#include "pixmaps/admin_icon.xpm"
#include "pixmaps/aol_icon.xpm"
#include "pixmaps/free_icon.xpm"
#include "pixmaps/dt_icon.xpm"
#include "pixmaps/no_icon.xpm"
#include "pixmaps/login_icon.xpm"
#include "pixmaps/logout_icon.xpm"

#include "pixmaps/buddyadd.xpm"
#include "pixmaps/buddydel.xpm"
#include "pixmaps/buddychat.xpm"
#include "pixmaps/im.xpm"
#include "pixmaps/info.xpm"
#include "pixmaps/away_icon.xpm"

#include "pixmaps/daemon-buddyadd.xpm"
#include "pixmaps/daemon-buddydel.xpm"
#include "pixmaps/daemon-buddychat.xpm"
#include "pixmaps/daemon-im.xpm"
#include "pixmaps/daemon-info.xpm"

#include "pixmaps/add_small.xpm"
#include "pixmaps/import_small.xpm"
#include "pixmaps/export_small.xpm"
#ifdef GAIM_PLUGINS
#include "pixmaps/plugins_small.xpm"
#endif
#include "pixmaps/prefs_small.xpm"
#include "pixmaps/search_small.xpm"
#ifdef USE_APPLET
#include "pixmaps/close_small.xpm"
#endif
#include "pixmaps/exit_small.xpm"

static GtkTooltips *tips;
static GtkWidget *editpane;
static GtkWidget *buddypane;
static GtkWidget *imchatbox;
static GtkWidget *edittree;
static GtkWidget *imbutton, *infobutton, *chatbutton;
static GtkWidget *addbutton, *groupbutton, *rembutton;

extern int ticker_prefs;

GtkWidget *blist = NULL;
GtkWidget *bpmenu;
GtkWidget *buddies;
int permdeny;

void BuddyTickerLogonTimeout( gpointer data );
void BuddyTickerLogoutTimeout( gpointer data );

/* Predefine some functions */
static void new_bp_callback(GtkWidget *w, char *name);
static void log_callback(GtkWidget *w, char *name);


void destroy_buddy()
{
	if (blist)
		gtk_widget_destroy(blist);
	blist=NULL;
	imchatbox = NULL;
}

void update_num_groups()
{
	GList *grp = groups;
	GList *mem;
        struct buddy *b;
	struct group *g;
	int pres, total;
        char buf[BUF_LONG];

#ifndef USE_APPLET
        if (!(display_options & OPT_DISP_SHOW_GRPNUM))
                return;
#else
	int onl = 0;
	int all = 0;
#endif

        while(grp) {
		g = (struct group *)grp->data;
                mem = g->members;
                pres = 0;
                total = 0;
                while(mem) {
			b = (struct buddy *)mem->data;
                        if (b->present)
                                pres++;
                        total++;


                        mem = mem->next;
                }

                g_snprintf(buf, sizeof(buf), "%s  (%d/%d)", g->name, pres, total);

#ifdef USE_APPLET
		onl += pres;
		all += total;
		if (display_options & OPT_DISP_SHOW_GRPNUM)
#endif
                gtk_label_set(GTK_LABEL(g->label), buf);
                grp = grp->next;
        }
#ifdef USE_APPLET
	g_snprintf(buf, sizeof(buf), _("%d/%d Buddies Online"), onl, all);
	applet_set_tooltips(buf);
#endif
}

void update_show_idlepix()
{
	GList *grp = groups;
	GList *mem;
	struct group *g;
        struct buddy *b;

	while (grp) {
                g = (struct group *)grp->data;
                mem = g->members;

                while(mem) {
			b = (struct buddy *)mem->data;

                        if (display_options & OPT_DISP_SHOW_IDLETIME)
                                gtk_widget_show(b->idletime);
                        else
                                gtk_widget_hide(b->idletime);
						
                        if (display_options & OPT_DISP_SHOW_PIXMAPS)
                                gtk_widget_show(b->pix);
                        else
                                gtk_widget_hide(b->pix);
                        mem = mem->next;
                }
                grp = grp->next;
        }
}

void update_all_buddies()
{
	GList *grp = groups;
	GList *mem;
        struct buddy *b;
	struct group *g;

        while(grp) {
		g = (struct group *)grp->data;
                mem = g->members;
                while(mem) {
			b = (struct buddy *)mem->data;

                        if (b->present || !GTK_WIDGET_VISIBLE(b->item))
				set_buddy(b);
                        
                        mem = mem->next;
                }
                grp = grp->next;
        }


}

static void adjust_pic(GtkWidget *button, const char *c, gchar **xpm)
{
        GdkPixmap *pm;
        GdkBitmap *bm;
        GtkWidget *pic;
        GtkWidget *label;

		/*if the user had opted to put pictures on the buttons*/
        if (display_options & OPT_DISP_SHOW_BUTTON_XPM && xpm) {
		pm = gdk_pixmap_create_from_xpm_d(blist->window, &bm,
						  NULL, xpm);
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


void update_button_pix()
{

	if (display_options & OPT_DISP_DEVIL_PIXMAPS) {
	        adjust_pic(addbutton, _("Add"), (gchar **)daemon_buddyadd_xpm);
	        adjust_pic(groupbutton, _("Group"), NULL);
		adjust_pic(rembutton, _("Remove"), (gchar **)daemon_buddydel_xpm);
		if (!(display_options & OPT_DISP_NO_BUTTONS)) {
			adjust_pic(chatbutton, _("Chat"), (gchar **)daemon_buddychat_xpm);
		        adjust_pic(imbutton, _("IM"), (gchar **)daemon_im_xpm);
		        adjust_pic(infobutton, _("Info"), (gchar **)daemon_info_xpm);
		}
/*	        adjust_pic(addpermbutton, _("Add"), (gchar **)daemon_permadd_xpm);
	        adjust_pic(rempermbutton, _("Remove"), (gchar **)daemon_permdel_xpm);
*/	} else {
	        adjust_pic(addbutton, _("Add"), (gchar **)buddyadd_xpm);
	        adjust_pic(groupbutton, _("Group"), NULL);
		adjust_pic(rembutton, _("Remove"), (gchar **)buddydel_xpm);
		if (!(display_options & OPT_DISP_NO_BUTTONS)) {
			adjust_pic(chatbutton, _("Chat"), (gchar **)buddychat_xpm);
		        adjust_pic(imbutton, _("IM"), (gchar **)im_xpm);
		        adjust_pic(infobutton, _("Info"), (gchar **)info_xpm);
		}
/*	        adjust_pic(addpermbutton, _("Add"), (gchar **)permadd_xpm);
	        adjust_pic(rempermbutton, _("Remove"), (gchar **)permdel_xpm);
*/	}
	gtk_widget_hide(addbutton->parent);
	gtk_widget_show(addbutton->parent);
	if (!(display_options & OPT_DISP_NO_BUTTONS)) {
		gtk_widget_hide(chatbutton->parent);
		gtk_widget_show(chatbutton->parent);
	}
/*	gtk_widget_hide(addpermbutton->parent);
	gtk_widget_show(addpermbutton->parent);
*/}



#ifdef USE_APPLET
gint applet_destroy_buddy( GtkWidget *widget, GdkEvent *event,gpointer *data ) {
	applet_buddy_show = FALSE;
	gtk_widget_hide(blist);
	return (TRUE);
}

#endif


void signoff()
{
	GList *mem;

	plugin_event(event_signoff, 0, 0, 0);

        while(groups) {
		mem = ((struct group *)groups->data)->members;
		while(mem) {
			g_free(mem->data);
                        mem = g_list_remove(mem, mem->data);
		}
		g_free(groups->data);
                groups = g_list_remove(groups, groups->data);
	}

	sprintf(debug_buff, "date: %s\n", full_date());
	debug_print(debug_buff);
	serv_close();
        destroy_all_dialogs();
        destroy_buddy();
        hide_login_progress("");
#ifdef USE_APPLET
	set_user_state(offline);
	applet_buddy_show = FALSE;
        applet_widget_unregister_callback(APPLET_WIDGET(applet),"signoff");
	remove_applet_away();
        applet_widget_register_callback(APPLET_WIDGET(applet),
                "signon",
                _("Signon"),
                applet_do_signon,
                NULL);
#else
        show_login();
#endif /* USE_APPLET */
	if ( ticker_prefs & OPT_DISP_SHOW_BUDDYTICKER )
		BuddyTickerSignoff();
}

void handle_click_group(GtkWidget *widget, GdkEventButton *event, gpointer func_data)
{
	if (event->type == GDK_2BUTTON_PRESS) {
		if (GTK_TREE_ITEM(widget)->expanded)
			gtk_tree_item_collapse(GTK_TREE_ITEM(widget));
		else
			gtk_tree_item_expand(GTK_TREE_ITEM(widget));
	} else {
	}
}

void pressed_im(GtkWidget *widget, struct buddy *b)
{
	struct conversation *c;

	c = find_conversation(b->name);

	if (c != NULL) {
		gdk_window_show(c->window->window);
	} else {
		c = new_conversation(b->name);
	}
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

void pressed_info(GtkWidget *widget, struct buddy *b)
{
        serv_get_info(b->name);
}

void pressed_alias(GtkWidget *widget, struct buddy *b)
{
	alias_dialog(b);
}

void pressed_dir_info(GtkWidget *widget, struct buddy *b)
{
        serv_get_dir(b->name);

}

void pressed_away_msg(GtkWidget *widget, struct buddy *b)
{
        serv_get_away_msg(b->name);

}

void handle_click_buddy(GtkWidget *widget, GdkEventButton *event, struct buddy *b)
{
        if (event->type == GDK_2BUTTON_PRESS && event->button == 1) {
                struct conversation *c;

                c = find_conversation(b->name);

                if (c != NULL) {
                        gdk_window_show(c->window->window);
                } else {
                        c = new_conversation(b->name);
                }
	} else if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
                GtkWidget *menu, *button;
		/* We're gonna make us a menu right here */

		menu = gtk_menu_new();

		button = gtk_menu_item_new_with_label(_("IM"));
		gtk_signal_connect(GTK_OBJECT(button), "activate",
				   GTK_SIGNAL_FUNC(pressed_im), b);
		gtk_menu_append(GTK_MENU(menu), button);
		gtk_widget_show(button);

		button = gtk_menu_item_new_with_label(_("Info"));
		gtk_signal_connect(GTK_OBJECT(button), "activate",
				   GTK_SIGNAL_FUNC(pressed_info), b);
		gtk_menu_append(GTK_MENU(menu), button);
		gtk_widget_show(button);

		button = gtk_menu_item_new_with_label(_("Alias"));
		gtk_signal_connect(GTK_OBJECT(button), "activate",
				   GTK_SIGNAL_FUNC(pressed_alias), b);
		gtk_menu_append(GTK_MENU(menu), button);
		gtk_widget_show(button);

	if (!USE_OSCAR) {
		button = gtk_menu_item_new_with_label(_("Dir Info"));
		gtk_signal_connect(GTK_OBJECT(button), "activate",
				   GTK_SIGNAL_FUNC(pressed_dir_info), b);
		gtk_menu_append(GTK_MENU(menu), button);
		gtk_widget_show(button);
	} else {

		button = gtk_menu_item_new_with_label(_("Direct IM"));
		gtk_signal_connect(GTK_OBJECT(button), "activate",
				   GTK_SIGNAL_FUNC(serv_do_imimage), b->name);
		gtk_menu_append(GTK_MENU(menu), button);
		gtk_widget_show(button);

		button = gtk_menu_item_new_with_label(_("Away Msg"));
		gtk_signal_connect(GTK_OBJECT(button), "activate",
				   GTK_SIGNAL_FUNC(pressed_away_msg), b);
		gtk_menu_append(GTK_MENU(menu), button);
		gtk_widget_show(button);
	}

		button = gtk_menu_item_new_with_label(_("Toggle Logging"));
		gtk_signal_connect(GTK_OBJECT(button), "activate",
				   GTK_SIGNAL_FUNC(log_callback), b->name);
		gtk_menu_append(GTK_MENU(menu), button);
		gtk_widget_show(button);

		button = gtk_menu_item_new_with_label(_("Add Buddy Pounce"));
		gtk_signal_connect(GTK_OBJECT(button), "activate",
				   GTK_SIGNAL_FUNC(new_bp_callback), b->name);
		gtk_menu_append(GTK_MENU(menu), button);
		gtk_widget_show(button);

                
		
		gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
			       event->button, event->time);

	} else if (event->type == GDK_3BUTTON_PRESS && event->button == 2) {
		if (!strcasecmp("zilding", normalize(b->name)))
			show_ee_dialog(0);
		else if (!strcasecmp("robflynn", normalize(b->name)))
			show_ee_dialog(1);
		else if (!strcasecmp("flynorange", normalize(b->name)))
			show_ee_dialog(2);
		else if (!strcasecmp("ewarmenhoven", normalize(b->name)))
			show_ee_dialog(3);

	} else {
		
                /* Anything for other buttons? :) */
	}
}



void remove_buddy(struct group *rem_g, struct buddy *rem_b)
{
	GList *grp;
	GList *mem;
	
	struct group *delg;
	struct buddy *delb;

	grp = g_list_find(groups, rem_g);
        delg = (struct group *)grp->data;
        mem = delg->members;
	
        mem = g_list_find(mem, rem_b);
        delb = (struct buddy *)mem->data;
	
	gtk_tree_remove_items(GTK_TREE(delg->tree), g_list_append(NULL, delb->item));
        delg->members = g_list_remove(delg->members, delb);
        serv_remove_buddy(delb->name);
        g_free(delb);

        serv_save_config();

	// flush buddy list to cache

	do_export( (GtkWidget *) NULL, 0 );
        
	update_num_groups();


}

void remove_group(struct group *rem_g)
{
	GList *grp;
	GList *mem;
	
	struct group *delg;
	struct buddy *delb;

	grp = g_list_find(groups, rem_g);
        delg = (struct group *)grp->data;
        mem = delg->members;

	while(delg->members) {
		delb = (struct buddy *)delg->members->data;
		gtk_tree_remove_items(GTK_TREE(delg->tree), g_list_append(NULL, delb->item));
                delg->members = g_list_remove(delg->members, delb);
                serv_remove_buddy(delb->name);
                g_free(delb);
	}


	gtk_tree_remove_items(GTK_TREE(buddies), g_list_append(NULL, delg->item));
	groups = g_list_remove(groups, delg);
	g_free(delg);

        serv_save_config();

        // flush buddy list to cache

        do_export( (GtkWidget *) NULL, 0 );
}





gboolean edit_drag_compare_func (GtkCTree *ctree, GtkCTreeNode *source_node,
				 GtkCTreeNode *new_parent, GtkCTreeNode *new_sibling)
{
        gboolean leaf;

	gtk_ctree_get_node_info (ctree, source_node, NULL,
				 NULL, NULL, NULL, NULL, NULL, &leaf, NULL);

	
	if (leaf) {
		if (!new_parent)
			return FALSE;
	} else {
		
		if (new_parent)
			return FALSE;
		
	}

	return TRUE;
}



static void edit_tree_move (GtkCTree *ctree, GtkCTreeNode *child, GtkCTreeNode *parent,
                 GtkCTreeNode *sibling, gpointer data)
{
	char *source;
	char *target1;
        char *target2;
	
	gtk_ctree_get_node_info (ctree, child, &source,
				 NULL, NULL, NULL, NULL, NULL, NULL, NULL);
	if (parent)
		gtk_ctree_get_node_info (ctree, parent, &target1,
					 NULL, NULL, NULL, NULL, NULL, NULL, NULL);
	if (sibling)
		gtk_ctree_get_node_info (ctree, sibling, &target2,
					 NULL, NULL, NULL, NULL, NULL, NULL, NULL);

	
	if (!parent) {
		GList *grps, *buds;
		struct group *g, *g2;
		GList *tmp;
		int pos;
		struct buddy *b;
		/* Okay we've moved group order... */

		g = find_group(source);

                gtk_widget_ref(g->tree);

		buds = g->members;
		while(buds) {
			b = (struct buddy *)buds->data;
			gtk_widget_ref(b->item);
			gtk_widget_ref(b->label);
			gtk_widget_ref(b->idletime);
			gtk_widget_ref(b->pix);
			buds = buds->next;
		}

		

		
		pos = g_list_index(GTK_TREE(buddies)->children, g->item);

                tmp = g_list_append(NULL, g->item);
		gtk_tree_remove_items(GTK_TREE(buddies), tmp);
		g_list_free(tmp);

                groups = g_list_remove(groups, g);

                g->item = gtk_tree_item_new_with_label(g->name);
                gtk_widget_show(g->item);

		if (sibling) {
			g2 = find_group(target2);
                        pos = g_list_index(groups, g2);
                        if (pos == 0) {
				groups = g_list_prepend(groups, g);
				gtk_tree_prepend(GTK_TREE(buddies), g->item);
			} else {
				groups = g_list_insert(groups, g, pos);
				gtk_tree_insert(GTK_TREE(buddies), g->item, pos);
			}
         
		} else {
			groups = g_list_append(groups, g);
			gtk_tree_append(GTK_TREE(buddies), g->item);

		}

		gtk_tree_item_set_subtree (GTK_TREE_ITEM(g->item), g->tree);
                gtk_tree_item_expand (GTK_TREE_ITEM(g->item));
                gtk_signal_connect(GTK_OBJECT(g->item), "button_press_event",
                                   GTK_SIGNAL_FUNC(handle_click_group),
                                   NULL);
                gtk_object_set_user_data(GTK_OBJECT(g->item), NULL);

                gtk_widget_unref(g->tree);

                update_num_groups();


		buds = g->members;

		while(buds) {
			b = (struct buddy *)buds->data;
			set_buddy(b);
			buds = buds->next;
		}

		grps = groups;
		while(grps) {
			g = (struct group *)grps->data;
			grps = grps->next;
		}

        } else {
                struct group *new_g, *old_g;
                struct buddy *b, *s;
                GtkWidget *gtree;
                GtkWidget *owner;
                GList *temp;
                int pos;

                b = find_buddy(source);
                new_g = find_group(target1);
                old_g = find_group_by_buddy(source);
                gtree = old_g->tree;
                if (sibling)
                        s = find_buddy(target2);
                else
                        s = NULL;

                old_g->members = g_list_remove(old_g->members, b);

                gtk_widget_ref(b->item);
                gtk_widget_ref(b->label);
                gtk_widget_ref(b->pix);
                gtk_widget_ref(b->idletime);
                gtk_widget_ref(gtree);

                owner = GTK_TREE(gtree)->tree_owner;
                
                temp = g_list_append(NULL, b->item);
                gtk_tree_remove_items(GTK_TREE(old_g->tree), temp);
                g_list_free(temp);

                if (gtree->parent == NULL){
                        gtk_tree_item_set_subtree (GTK_TREE_ITEM(owner), gtree);
                        gtk_tree_item_expand (GTK_TREE_ITEM(owner));
                }
                
                if (!sibling) {
                        gtk_tree_append(GTK_TREE(new_g->tree), b->item);
                        new_g->members = g_list_append(new_g->members, b);
                } else {
                        pos = g_list_index(new_g->members, s);
                        if (pos != 0) {
                                new_g->members = g_list_insert(new_g->members, b, pos);
                                gtk_tree_insert(GTK_TREE(new_g->tree), b->item, pos);
                        } else {
                                new_g->members = g_list_prepend(new_g->members, b);
                                gtk_tree_prepend(GTK_TREE(new_g->tree), b->item);

                        }
                }

                gtk_widget_unref(b->item);
                gtk_widget_unref(b->label);
                gtk_widget_unref(b->pix);
                gtk_widget_unref(b->idletime);
                gtk_widget_unref(gtree);

                gtk_ctree_expand(ctree, parent);
                
                update_num_groups();
                update_show_idlepix();
                set_buddy(b);
                




        }

        serv_save_config();

        // flush buddy list to cache

        do_export( (GtkWidget *) NULL, 0 );
}



void build_edit_tree()
{
        GtkCTreeNode *p = NULL, *n;
	GList *grp = groups;
	GList *mem;
	struct group *g;
	struct buddy *b;
	char *text[1];

	gtk_clist_freeze(GTK_CLIST(edittree));
	gtk_clist_clear(GTK_CLIST(edittree));
	
        
	while(grp) {
		g = (struct group *)grp->data;

                text[0] = g->name;
		
		p = gtk_ctree_insert_node(GTK_CTREE(edittree), NULL,
					     NULL, text, 5, NULL, NULL,
					     NULL, NULL, 0, 1);

		n = NULL;
		
		mem = g->members;

                while(mem) {
			b = (struct buddy *)mem->data;

			text[0] = b->name;

			n = gtk_ctree_insert_node(GTK_CTREE(edittree),
						  p, NULL, text, 5,
						  NULL, NULL,
						  NULL, NULL, 1, 1);

			mem = mem->next;
                        
 		}
		grp = grp->next;
	}

	gtk_clist_thaw(GTK_CLIST(edittree));
	
}

struct buddy *add_buddy(char *group, char *buddy, char *show)
{
	struct buddy *b;
	struct group *g;
	GdkPixmap *pm;
	GdkBitmap *bm;
	GtkWidget *box;


	if ((b = find_buddy(buddy)) != NULL)
                return b;

	g = find_group(group);

	if (g == NULL)
		g = add_group(group);
	
        b = (struct buddy *)g_new0(struct buddy, 1);
        
	if (!b)
		return NULL;

	b->present = 0;
        b->item = gtk_tree_item_new();

	g_snprintf(b->name, sizeof(b->name), "%s", buddy);
	g_snprintf(b->show, sizeof(b->show), "%s", show ? (show[0] ? show : buddy) : buddy);
		
        g->members = g_list_append(g->members, b);


        if (blist == NULL)
                return b;
        
	box = gtk_hbox_new(FALSE, 1);
	pm = gdk_pixmap_create_from_xpm_d(blist->window, &bm,
					  NULL, (gchar **)login_icon_xpm);
        b->pix = gtk_pixmap_new(pm, bm);

        b->idle = 0;
	b->caps = 0;
			
	gtk_widget_show(b->pix);
	gdk_pixmap_unref(pm);
	gdk_bitmap_unref(bm);

	b->label = gtk_label_new(buddy);
	gtk_misc_set_alignment(GTK_MISC(b->label), 0.0, 0.5);

	b->idletime = gtk_label_new("");

	gtk_tree_append(GTK_TREE(g->tree),b->item);
	gtk_container_add(GTK_CONTAINER(b->item), box);

	gtk_box_pack_start(GTK_BOX(box), b->pix, FALSE, FALSE, 1);
	gtk_box_pack_start(GTK_BOX(box), b->label, TRUE, TRUE, 1);
	gtk_box_pack_start(GTK_BOX(box), b->idletime, FALSE, FALSE, 1);

	gtk_widget_show(b->label);
	gtk_widget_show(box);

	gtk_object_set_user_data(GTK_OBJECT(b->item), b);

	gtk_signal_connect(GTK_OBJECT(b->item), "button_press_event",
			   GTK_SIGNAL_FUNC(handle_click_buddy), b);

	return b;
}


struct group *add_group(char *group)
{
	struct group *g = find_group(group);
	if (g)
		return g;
	g = (struct group *)g_new0(struct group, 1);
	if (!g)
		return NULL;

	strncpy(g->name, group, sizeof(g->name));
        groups = g_list_append(groups, g);

        if (blist == NULL)
                return g;
        
        g->item = gtk_tree_item_new();
        g->label = gtk_label_new(g->name);
        gtk_misc_set_alignment(GTK_MISC(g->label), 0.0, 0.5);
        gtk_widget_show(g->label);
        gtk_container_add(GTK_CONTAINER(g->item), g->label);
	g->tree = gtk_tree_new();
	gtk_widget_show(g->item);
	gtk_widget_show(g->tree);
	gtk_tree_append(GTK_TREE(buddies), g->item);
	gtk_tree_item_set_subtree(GTK_TREE_ITEM(g->item), g->tree);
	gtk_tree_item_expand(GTK_TREE_ITEM(g->item));
	gtk_signal_connect(GTK_OBJECT(g->item), "button_press_event",
			   GTK_SIGNAL_FUNC(handle_click_group),
			   NULL);
        gtk_object_set_user_data(GTK_OBJECT(g->item), NULL);
	g->members = NULL;
	
 
	build_edit_tree();
	
	return g;
	
}


static void do_del_buddy(GtkWidget *w, GtkCTree *ctree)
{
	GtkCTreeNode *node;
        char *bud, *grp;
	struct buddy *b;
	struct group *g;
	GList *i;
	
	i = GTK_CLIST(edittree)->selection;
	if (i) {
		node = i->data;

		if (GTK_CTREE_ROW(node)->is_leaf) {
                        gtk_ctree_get_node_info (GTK_CTREE(edittree), node, &bud,
						 NULL, NULL, NULL, NULL, NULL, NULL, NULL);

			b = find_buddy(bud);
			g = find_group_by_buddy(bud);
			remove_buddy(g, b);
		} else {
			gtk_ctree_get_node_info (ctree, node, &grp,
						 NULL, NULL, NULL, NULL, NULL, NULL, NULL);
			g = find_group(grp);
                        remove_group(g);
                }
                
                build_edit_tree();
                serv_save_config();

        	// flush buddy list to cache

        	do_export( (GtkWidget *) NULL, 0 );

        } else {
                /* Nothing selected. */
        }
        update_num_groups();
}


void gaimreg_callback(GtkWidget *widget)
{
	show_register_dialog(); 
}

void import_callback(GtkWidget *widget, void *null)
{
        show_import_dialog();
}

void export_callback(GtkWidget *widget, void *null)
{
        show_export_dialog();
}



void do_quit()
{
#ifdef GAIM_PLUGINS
	GList *c;
	struct gaim_plugin *p;
	void (*gaim_plugin_remove)();
	char *error;

	/* first we tell those who have requested it we're quitting */
	plugin_event(event_quit, 0, 0, 0);

	/* then we remove everyone in a mass suicide */
	c = plugins;
	while (c) {
		p = (struct gaim_plugin *)c->data;
		gaim_plugin_remove = dlsym(p->handle, "gaim_plugin_remove");
		if ((error = (char *)dlerror()) == NULL)
			(*gaim_plugin_remove)();
		/* we don't need to worry about removing callbacks since
		 * there won't be any more chance to call them back :) */
		dlclose(p->handle);
		g_free(p->filename); /* why do i bother? */
		g_free(p);
		c = c->next;
	}
#endif
#ifdef USE_PERL
	perl_end();
#endif

	exit(0);
}

void add_buddy_callback(GtkWidget *widget, void *dummy)
{
	char *grp = NULL;
	GtkCTreeNode *node;
	GList *i;

	i = GTK_CLIST(edittree)->selection;
	if (i) {
		node = i->data;

		if (GTK_CTREE_ROW(node)->is_leaf) {
			node = GTK_CTREE_ROW(node)->parent;
		}

		gtk_ctree_get_node_info (GTK_CTREE(edittree), node, &grp,
					 NULL, NULL, NULL, NULL, NULL, NULL, NULL);
	}
	show_add_buddy(NULL, grp);

}

void add_group_callback(GtkWidget *widget, void *dummy)
{
	show_add_group();
}

static void info_callback(GtkWidget *widget, GtkTree *tree)
{
	GList *i;
	struct buddy *b = NULL;
	i = GTK_TREE_SELECTION(tree);
	if (i) {
		b = gtk_object_get_user_data(GTK_OBJECT(i->data));
        } else {
		return;
        }
	if (!b->name)
		return;
        serv_get_info(b->name);
}


void chat_callback(GtkWidget *widget, GtkTree *tree)
{
	join_chat();
}

struct group *find_group(char *group)
{
	struct group *g;
        GList *grp = groups;
	char *grpname = g_malloc(strlen(group) + 1);

	strcpy(grpname, normalize(group));
	while (grp) {
		g = (struct group *)grp->data;
		if (!strcasecmp(normalize(g->name), grpname)) {
				g_free(grpname);
				return g;
		}
		grp = grp->next;
	}

	g_free(grpname);
	return NULL;	
	
}


struct group *find_group_by_buddy(char *who)
{
	struct group *g;
	struct buddy *b;
	GList *grp = groups;
	GList *mem;
        char *whoname = g_malloc(strlen(who) + 1);

	strcpy(whoname, normalize(who));
	
	while(grp) {
		g = (struct group *)grp->data;

		mem = g->members;
		while(mem) {
			b = (struct buddy *)mem->data;
			if (!strcasecmp(normalize(b->name), whoname)) {
				g_free(whoname);
				return g;
			}
			mem = mem->next;
		}
		grp = grp->next;
	}
	g_free(whoname);
	return NULL;
}


struct buddy *find_buddy(char *who)
{
	struct group *g;
	struct buddy *b;
	GList *grp = groups;
	GList *mem;
        char *whoname = g_malloc(strlen(who) + 1);

	strcpy(whoname, normalize(who));
	
	while(grp) {
		g = (struct group *)grp->data;

		mem = g->members;
		while(mem) {
			b = (struct buddy *)mem->data;
			if (!strcasecmp(normalize(b->name), whoname)) {
				g_free(whoname);
				return b;
			}
			mem = mem->next;
		}
		grp = grp->next;
	}
	g_free(whoname);
	return NULL;
}


void rem_bp(GtkWidget *w, struct buddy_pounce *b)
{
	buddy_pounces = g_list_remove(buddy_pounces, b);
	do_bp_menu();
	save_prefs();
}

void do_pounce(char *name)
{
        char *who;
        
        struct buddy_pounce *b;
	struct conversation *c;

	GList *bp = buddy_pounces;
        
	who = g_strdup(normalize(name));

	while(bp) {
		b = (struct buddy_pounce *)bp->data;;
		bp = bp->next; /* increment the list here because rem_bp can make our handle bad */

                if (!strcasecmp(who, normalize(b->name))) {
			if (b->popup == 1)
			{
				c = find_conversation(name);
				if (c == NULL)
					c = new_conversation(name);
			}
			if (b->sendim == 1)
			{
                        	c = find_conversation(name);
                        	if (c == NULL)
                                	c = new_conversation(name);

                        	write_to_conv(c, b->message, WFLAG_SEND, NULL);

                        	escape_text(b->message);

                                serv_send_im(name, b->message, 0);
			}
                        
                        rem_bp(NULL, b);
                        
                }
        }
        g_free(who);
}

static void new_bp_callback(GtkWidget *w, char *name)
{
        show_new_bp(name);
}

static void log_callback(GtkWidget *w, char *name)
{
	struct conversation *c = find_conversation(name);

	if (find_log_info(name))
	{
		if (c) { 
			set_state_lock(1);
			gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(c->log_button), FALSE);
			set_state_lock(0);
		}
		rm_log(find_log_info(name));
	}
	else
	{
		if (c) {
			show_log_dialog(c);
			set_state_lock(1);
			gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(c->log_button), TRUE);
			set_state_lock(0);
		}
	}
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
	
	while(l) {
		gtk_widget_destroy(GTK_WIDGET(l->data));
		l = l->next;
	}

	remmenu = gtk_menu_new();
	
	menuitem = gtk_menu_item_new_with_label(_("New Buddy Pounce"));
	gtk_menu_append(GTK_MENU(bpmenu), menuitem);
	gtk_widget_show(menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate", GTK_SIGNAL_FUNC(new_bp_callback), NULL);


	while(bp) {

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

	bp = buddy_pounces;;
	
	while(bp) {

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


gint log_timeout(char *name)
{
	struct buddy *b;
	struct group *g;
	
	b = find_buddy(name);

	if(!b)
                return FALSE;

        b->log_timer = 0;
			
	if (!b->present) {
		gtk_widget_hide(b->item);
		g = find_group_by_buddy(name);
		if (GTK_TREE_ITEM(g->item)->expanded) {
			gtk_tree_item_collapse(GTK_TREE_ITEM(g->item));
			gtk_tree_item_expand(GTK_TREE_ITEM(g->item));
		}
	} else
		set_buddy(b);
	
	return FALSE;
}


static char *caps_string(u_short caps)
{
	static char buf[BUF_LEN];
	int count = 0, i = 0;
	u_short bit = 1;
	while (bit <= AIM_CAPS_SENDFILE) {
		if (bit & caps) {
			switch (bit) {
			case AIM_CAPS_BUDDYICON:
				i += g_snprintf(buf + i, sizeof(buf) - i, _("%sBuddy Icon"), count ? ", " : "");
				break;
			case AIM_CAPS_VOICE:
				i += g_snprintf(buf + i, sizeof(buf) - i, _("%sVoice"), count ? ", " : "");
				break;
			case AIM_CAPS_IMIMAGE:
				i += g_snprintf(buf + i, sizeof(buf) - i, _("%sIM Image"), count ? ", " : "");
				break;
			case AIM_CAPS_CHAT:
				i += g_snprintf(buf + i, sizeof(buf) - i, _("%sChat"), count ? ", " : "");
				break;
			case AIM_CAPS_GETFILE:
				i += g_snprintf(buf + i, sizeof(buf) - i, _("%sGet File"), count ? ", " : "");
				break;
			case AIM_CAPS_SENDFILE:
				i += g_snprintf(buf + i, sizeof(buf) - i, _("%sSend File"), count ? ", " : "");
				break;
			}
			count++;
		}
		bit <<= 1;
	}
	return buf;
}


void set_buddy(struct buddy *b)
{
	char infotip[256];
        char idlet[16];
        char warn[256];
	char caps[256];
	char *who;
        int i;
        int ihrs, imin;
        time_t t;
	GdkPixmap *pm;
        GdkBitmap *bm;
        char *itime, *sotime;
	
        if (b->present) {
                time(&t);

                ihrs = (t - b->idle) / 3600;
                imin = ((t - b->idle) / 60) % 60;

                if (ihrs)
                        g_snprintf(idlet, sizeof(idlet), "(%d:%02d)", ihrs, imin);
                else
                        g_snprintf(idlet, sizeof(idlet), "(%02d)", imin);
                
                gtk_widget_hide(b->idletime);
                
		if (b->idle)
			gtk_label_set(GTK_LABEL(b->idletime), idlet);
		else
			gtk_label_set(GTK_LABEL(b->idletime), "");
                if (display_options & OPT_DISP_SHOW_IDLETIME)
                        gtk_widget_show(b->idletime);


                sotime = sec_to_text(t - b->signon + correction_time);
                if (b->idle) {
                        itime = sec_to_text(t - b->idle);
                } else {
                        itime = g_malloc(1);
                        itime[0] = 0;
                }
		
		if (b->evil) {
			g_snprintf(warn, sizeof(warn), _("Warnings: %d%%\n"), b->evil);

		} else
			warn[0] = '\0';
		
		if (b->caps) {
			g_snprintf(caps, sizeof(caps), _("Capabilities: %s\n"), caps_string(b->caps));
		} else
			caps[0] = '\0';
		
                i = g_snprintf(infotip, sizeof(infotip), _("Name: %s                \nLogged in: %s\n%s%s%s%s%s"), b->show, sotime, warn, ((b->idle) ? _("Idle: ") : ""),  itime, ((b->idle) ? "\n" : ""), caps);

		gtk_tooltips_set_tip(tips, GTK_WIDGET(b->item), infotip, "");

                g_free(sotime);
                g_free(itime);

                

		/* this check should also depend on whether they left,
		 * and signed on again before they got erased */
                if (!GTK_WIDGET_VISIBLE(b->item) || b->present == 1) {
			plugin_event(event_buddy_signon, b->name, 0, 0);
			
			play_sound(BUDDY_ARRIVE);
			b->present = 2;

			who = g_malloc(sizeof(b->show) + 10);
			strcpy(who, b->show);
			gtk_label_set(GTK_LABEL(b->label), who);
			g_free(who);

			pm = gdk_pixmap_create_from_xpm_d(blist->window, &bm,
				NULL, (gchar **)login_icon_xpm);
			gtk_widget_hide(b->pix);
			gtk_pixmap_set(GTK_PIXMAP(b->pix), pm, bm);
                        if (display_options & OPT_DISP_SHOW_PIXMAPS)
				gtk_widget_show(b->pix);
			gdk_pixmap_unref(pm);
			gdk_bitmap_unref(bm);

			pm = gdk_pixmap_create_from_xpm_d(blist->window, &bm,
				NULL, (gchar **)login_icon_xpm);

        		if ( ticker_prefs & OPT_DISP_SHOW_BUDDYTICKER )
				BuddyTickerAddUser( b->name, pm, bm );	
			gdk_pixmap_unref(pm);
			gdk_bitmap_unref(bm);

			if (display_options & OPT_DISP_SHOW_LOGON) {
				struct conversation *c = find_conversation(b->name);
				if (c) {
					char tmp[1024];

					
					g_snprintf(tmp, sizeof(tmp), _("<HR><B>%s logged in%s%s.</B><BR><HR>"), b->name,
                                                   ((display_options & OPT_DISP_SHOW_TIME) ? " @ " : ""),
                                                   ((display_options & OPT_DISP_SHOW_TIME) ? date() : ""));


					write_to_conv(c, tmp, WFLAG_SYSTEM, NULL);

				}
			}

			
			gtk_widget_show(b->item);
			gtk_widget_show(b->label);
                        b->log_timer = gtk_timeout_add(10000, (GtkFunction) log_timeout, b->name);
        		if ( ticker_prefs & OPT_DISP_SHOW_BUDDYTICKER )
                        	gtk_timeout_add(10000, (GtkFunction) BuddyTickerLogonTimeout, b->name);
                        update_num_groups();
                        update_show_idlepix();
                        setup_buddy_chats();
			return;
                }


                
                if (!b->log_timer) {
                        gtk_widget_hide(b->pix);
                        if (b->uc & UC_UNAVAILABLE) {
                                pm = gdk_pixmap_create_from_xpm_d(blist->window, &bm,
                                                                  NULL, (gchar **)away_icon_xpm);
                                gtk_pixmap_set(GTK_PIXMAP(b->pix), pm, bm);
				gdk_pixmap_unref(pm);
				gdk_bitmap_unref(bm);
        			if ( ticker_prefs & OPT_DISP_SHOW_BUDDYTICKER )
				{
					pm = gdk_pixmap_create_from_xpm_d(blist->window, &bm,
                                                                  NULL, (gchar **)away_icon_xpm);
					BuddyTickerSetPixmap(b->name, pm, bm);
					gdk_pixmap_unref(pm);
					gdk_bitmap_unref(bm);
				}
                        } else if (b->uc & UC_AOL) {
                                pm = gdk_pixmap_create_from_xpm_d(blist->window, &bm,
                                                                  NULL, (gchar **)aol_icon_xpm);
                                gtk_pixmap_set(GTK_PIXMAP(b->pix), pm, bm);
				gdk_pixmap_unref(pm);
				gdk_bitmap_unref(bm);
        			if ( ticker_prefs & OPT_DISP_SHOW_BUDDYTICKER )
				{
					pm = gdk_pixmap_create_from_xpm_d(blist->window, &bm,
                                                                  NULL, (gchar **)aol_icon_xpm);
					BuddyTickerSetPixmap(b->name, pm, bm);
					gdk_pixmap_unref(pm);
					gdk_bitmap_unref(bm);
				}
                        } else if (b->uc & UC_NORMAL) {
                                pm = gdk_pixmap_create_from_xpm_d(blist->window, &bm,
                                                                  NULL, (gchar **)free_icon_xpm);
                                gtk_pixmap_set(GTK_PIXMAP(b->pix), pm, bm);
				gdk_pixmap_unref(pm);
				gdk_bitmap_unref(bm);
        			if ( ticker_prefs & OPT_DISP_SHOW_BUDDYTICKER )
				{
					pm = gdk_pixmap_create_from_xpm_d(blist->window, &bm,
                                                                  NULL, (gchar **)free_icon_xpm);
					BuddyTickerSetPixmap(b->name, pm, bm);
					gdk_pixmap_unref(pm);
					gdk_bitmap_unref(bm);
				}
                        } else if (b->uc & UC_ADMIN) {
                                pm = gdk_pixmap_create_from_xpm_d(blist->window, &bm,
                                                                  NULL, (gchar **)admin_icon_xpm);
                                gtk_pixmap_set(GTK_PIXMAP(b->pix), pm, bm);
				gdk_pixmap_unref(pm);
				gdk_bitmap_unref(bm);
        			if ( ticker_prefs & OPT_DISP_SHOW_BUDDYTICKER )
				{
					pm = gdk_pixmap_create_from_xpm_d(blist->window, &bm,
                                                                  NULL, (gchar **)admin_icon_xpm);
					BuddyTickerSetPixmap(b->name, pm, bm);
					gdk_pixmap_unref(pm);
					gdk_bitmap_unref(bm);
				}
                        } else if (b->uc & UC_UNCONFIRMED) {
                                pm = gdk_pixmap_create_from_xpm_d(blist->window, &bm,
                                                                  NULL, (gchar **)dt_icon_xpm);
                                gtk_pixmap_set(GTK_PIXMAP(b->pix), pm, bm);
				gdk_pixmap_unref(pm);
				gdk_bitmap_unref(bm);
        			if ( ticker_prefs & OPT_DISP_SHOW_BUDDYTICKER )
				{
					pm = gdk_pixmap_create_from_xpm_d(blist->window, &bm,
                                                                  NULL, (gchar **)dt_icon_xpm);
					BuddyTickerSetPixmap(b->name, pm, bm);
					gdk_pixmap_unref(pm);
					gdk_bitmap_unref(bm);
				}
                        } else {
                                pm = gdk_pixmap_create_from_xpm_d(blist->window, &bm,
                                                                  NULL, (gchar **)no_icon_xpm);
                                gtk_pixmap_set(GTK_PIXMAP(b->pix), pm, bm);
				gdk_pixmap_unref(pm);
				gdk_bitmap_unref(bm);
        			if ( ticker_prefs & OPT_DISP_SHOW_BUDDYTICKER )
				{
					pm = gdk_pixmap_create_from_xpm_d(blist->window, &bm,
                                                                  NULL, (gchar **)no_icon_xpm);
					BuddyTickerSetPixmap(b->name, pm, bm);
					gdk_pixmap_unref(pm);
					gdk_bitmap_unref(bm);
				}
                        }
                        if (display_options & OPT_DISP_SHOW_PIXMAPS)
                                gtk_widget_show(b->pix);
                }
	


	} else {
		if (GTK_WIDGET_VISIBLE(b->item)) {
			plugin_event(event_buddy_signoff, b->name, 0, 0);
			play_sound(BUDDY_LEAVE);
			pm = gdk_pixmap_create_from_xpm_d(blist->window, &bm,
				NULL, (gchar **)logout_icon_xpm);
			gtk_widget_hide(b->pix);
			gtk_pixmap_set(GTK_PIXMAP(b->pix), pm, bm);
                        if (display_options & OPT_DISP_SHOW_PIXMAPS)
				gtk_widget_show(b->pix);
			gdk_pixmap_unref(pm);
			gdk_bitmap_unref(bm);
			pm = gdk_pixmap_create_from_xpm_d(blist->window, &bm,
				NULL, (gchar **)logout_icon_xpm);
        		if ( ticker_prefs & OPT_DISP_SHOW_BUDDYTICKER )
				BuddyTickerSetPixmap( b->name, pm, bm );
			gdk_pixmap_unref(pm);
			gdk_bitmap_unref(bm);
			if (display_options & OPT_DISP_SHOW_LOGON) {
				struct conversation *c = find_conversation(b->name);
				if (c) {
					char tmp[1024];

					
					g_snprintf(tmp, sizeof(tmp), _("<HR><B>%s logged out%s%s.</B><BR><HR>"), b->name,
                                                   ((display_options & OPT_DISP_SHOW_TIME) ? " @ " : ""),
                                                   ((display_options & OPT_DISP_SHOW_TIME) ? date() : ""));


					write_to_conv(c, tmp, WFLAG_SYSTEM, NULL);

				}
			}
                        b->log_timer = gtk_timeout_add(10000, (GtkFunction)log_timeout, b->name);
        		if ( ticker_prefs & OPT_DISP_SHOW_BUDDYTICKER )
                        	gtk_timeout_add(10000, (GtkFunction)BuddyTickerLogoutTimeout, b->name);
                        update_num_groups();
                        update_show_idlepix();
		}
        }
        setup_buddy_chats();
}


static void move_blist_window(GtkWidget *w, GdkEventConfigure *e, void *dummy)
{
        int x, y, width, height;
        int save = 0;
        gdk_window_get_position(blist->window, &x, &y);
        gdk_window_get_size(blist->window, &width, &height);

        if(e->send_event) { /* Is a position event */
                if (blist_pos.x != x || blist_pos.y != y)
                        save = 1;
                blist_pos.x = x;
                blist_pos.y = y;
        } else { /* Is a size event */
                if (blist_pos.xoff != x || blist_pos.yoff != y ||
                   blist_pos.width != width || blist_pos.width != width)
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

void gaim_seperator(GtkWidget *menu)
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

GtkWidget *gaim_new_item(GtkWidget *menu, const char *str, GtkSignalFunc sf)
{
	GtkWidget *menuitem;
	menuitem = gtk_menu_item_new_with_label(str);
        if (menu)
		gtk_menu_append(GTK_MENU(menu), menuitem);
	gtk_widget_show(menuitem);
	if (sf)
		gtk_signal_connect(GTK_OBJECT(menuitem), "activate", sf, NULL);
	return menuitem;
}

GtkWidget *gaim_new_item_with_pixmap(GtkWidget *menu, const char *str, char **xpm, GtkSignalFunc sf)
{
	GtkWidget *menuitem;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *pixmap;
	GdkPixmap *pm;
	GdkBitmap *mask;

	menuitem = gtk_menu_item_new();
	gtk_widget_show(menuitem);

	/* Create our container */
	hbox = gtk_hbox_new(FALSE, 2);

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
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);

	/* And finally, pack our box within our menu item */

	gtk_container_add(GTK_CONTAINER(menuitem), hbox);
	gtk_widget_show(hbox);
	
        if (menu)
		gtk_menu_append(GTK_MENU(menu), menuitem);

	if (sf)
		gtk_signal_connect(GTK_OBJECT(menuitem), "activate", sf, NULL);
	return menuitem;
}



void build_imchat_box(gboolean on)
{
	if (on) {
		if (imchatbox) return;

		imbutton   = gtk_button_new_with_label(_("IM"));
		infobutton = gtk_button_new_with_label(_("Info"));
		chatbutton = gtk_button_new_with_label(_("Chat"));

		imchatbox  = gtk_hbox_new(TRUE, 10);

		if (display_options & OPT_DISP_COOL_LOOK)
		{
			gtk_button_set_relief(GTK_BUTTON(imbutton), GTK_RELIEF_NONE);
			gtk_button_set_relief(GTK_BUTTON(infobutton), GTK_RELIEF_NONE);
			gtk_button_set_relief(GTK_BUTTON(chatbutton), GTK_RELIEF_NONE);
		}

		/* Put the buttons in the hbox */
		gtk_widget_show(imbutton);
		gtk_widget_show(chatbutton);
		gtk_widget_show(infobutton);

		gtk_box_pack_start(GTK_BOX(imchatbox), imbutton, TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(imchatbox), infobutton, TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(imchatbox), chatbutton, TRUE, TRUE, 0);
		gtk_container_border_width(GTK_CONTAINER(imchatbox), 10);

		gtk_signal_connect(GTK_OBJECT(imbutton), "clicked", GTK_SIGNAL_FUNC(show_im_dialog), buddies);
		gtk_signal_connect(GTK_OBJECT(infobutton), "clicked", GTK_SIGNAL_FUNC(info_callback), buddies);
		gtk_signal_connect(GTK_OBJECT(chatbutton), "clicked", GTK_SIGNAL_FUNC(chat_callback), buddies);

		gtk_tooltips_set_tip(tips,infobutton, _("Information on selected Buddy"), "Penguin");
		gtk_tooltips_set_tip(tips,imbutton, _("Send Instant Message"), "Penguin");
		gtk_tooltips_set_tip(tips,chatbutton, _("Start/join a Buddy Chat"), "Penguin");

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
	GtkWidget *findmenu;
#ifdef USE_PERL
	GtkWidget *perlmenu;
#endif
	GtkWidget *setmenu;
	GtkWidget *menubar;
	GtkWidget *vbox;
	GtkWidget *menuitem;
        GtkWidget *notebook;
        GtkWidget *label;
        GtkWidget *bbox;
        GtkWidget *tbox;


#ifdef USE_APPLET
        blist = gtk_window_new(GTK_WINDOW_DIALOG);
#else
        blist = gtk_window_new(GTK_WINDOW_TOPLEVEL);
#endif
        
	gtk_widget_realize(blist);
        aol_icon(blist->window);
        
        gtk_window_set_policy(GTK_WINDOW(blist), TRUE, TRUE, TRUE);
        
	menubar = gtk_menu_bar_new();
	
	menu = gtk_menu_new();


	menuitem = gaim_new_item(NULL, _("File"), NULL);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), menu);
	gtk_menu_bar_append(GTK_MENU_BAR(menubar), menuitem);

	// gaim_new_item(menu, _("Add A Buddy"), GTK_SIGNAL_FUNC(add_buddy_callback));
	gaim_new_item_with_pixmap(menu, _("Add A Buddy"), add_small_xpm, GTK_SIGNAL_FUNC(add_buddy_callback));
	gaim_new_item_with_pixmap(menu, _("Join A Chat"), add_small_xpm, GTK_SIGNAL_FUNC(chat_callback));
        gaim_seperator(menu);
        gaim_new_item_with_pixmap(menu, _("Import Buddy List"), import_small_xpm, GTK_SIGNAL_FUNC(import_callback));
        gaim_new_item_with_pixmap(menu, _("Export Buddy List"), export_small_xpm,GTK_SIGNAL_FUNC(export_callback));
	if (!(general_options & OPT_GEN_REGISTERED))
	{
        	gaim_seperator(menu);
		gaim_new_item_with_pixmap(menu, _("Register"), add_small_xpm, GTK_SIGNAL_FUNC(gaimreg_callback));
	}
	gaim_seperator(menu);
	gaim_new_item_with_pixmap(menu, _("Signoff"), logout_icon_xpm, GTK_SIGNAL_FUNC(signoff));

#ifndef USE_APPLET
	gaim_new_item_with_pixmap(menu, _("Quit"), exit_small_xpm, GTK_SIGNAL_FUNC(do_quit));
#else
	gaim_new_item_with_pixmap(menu, _("Close"), close_small_xpm, GTK_SIGNAL_FUNC(applet_destroy_buddy));
#endif

	menu = gtk_menu_new();

	menuitem = gaim_new_item(NULL, _("Tools"), NULL);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), menu);
	gtk_menu_bar_append(GTK_MENU_BAR(menubar), menuitem);

	awaymenu = gtk_menu_new();
	menuitem = gaim_new_item(menu, _("Away"), NULL);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), awaymenu);
        do_away_menu();

        bpmenu = gtk_menu_new();
        menuitem = gaim_new_item(menu, _("Buddy Pounce"), NULL);
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), bpmenu);
        do_bp_menu();

        gaim_seperator(menu);

	findmenu = gtk_menu_new();
	gtk_widget_show(findmenu);
	menuitem = gaim_new_item_with_pixmap(menu, _("Search for Buddy"), search_small_xpm, NULL);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), findmenu);
	gtk_widget_show(menuitem);
	menuitem = gtk_menu_item_new_with_label(_("by Email"));
	gtk_menu_append(GTK_MENU(findmenu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate", GTK_SIGNAL_FUNC(show_find_email), NULL);
	gtk_widget_show(menuitem);
	menuitem = gtk_menu_item_new_with_label(_("by Dir Info"));
	gtk_menu_append(GTK_MENU(findmenu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate", GTK_SIGNAL_FUNC(show_find_info), NULL);
 	gtk_widget_show(menuitem);

	setmenu = gtk_menu_new();
	gtk_widget_show(setmenu);
	menuitem = gaim_new_item(menu, _("Settings"), NULL);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), setmenu);
	gtk_widget_show(menuitem);
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
	gaim_seperator(menu);

        gaim_new_item_with_pixmap(menu, _("Preferences"), prefs_small_xpm, GTK_SIGNAL_FUNC(show_prefs));

#ifdef GAIM_PLUGINS
        gaim_new_item_with_pixmap(menu, _("Plugins"), plugins_small_xpm, GTK_SIGNAL_FUNC(show_plugins));
#endif
#ifdef USE_PERL
	perlmenu = gtk_menu_new();
	gtk_widget_show(perlmenu);
	menuitem = gaim_new_item(menu, _("Perl"), NULL);
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

	menuitem = gaim_new_item(NULL, _("Help"), NULL);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), menu);
	gtk_menu_item_right_justify(GTK_MENU_ITEM(menuitem));
	gtk_menu_bar_append(GTK_MENU_BAR(menubar), menuitem);
	
	gaim_new_item(menu, _("About"), show_about);

        gtk_widget_show(menubar);

	vbox       = gtk_vbox_new(FALSE, 10);
        
        notebook = gtk_notebook_new();

 
        

        /* Do buddy list stuff */

        buddypane = gtk_vbox_new(FALSE, 0);
        
	buddies    = gtk_tree_new();
	sw         = gtk_scrolled_window_new(NULL, NULL);
	
	tips = gtk_tooltips_new();
	gtk_object_set_data(GTK_OBJECT(blist), _("Buddy List"), tips);
	
	/* Now the buddy list */
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw),buddies);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
				       GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
	gtk_widget_set_usize(sw,200,200);
	gtk_widget_show(buddies);
	gtk_widget_show(sw);

        gtk_box_pack_start(GTK_BOX(buddypane), sw, TRUE, TRUE, 0);
        gtk_widget_show(buddypane);

	if (!(display_options & OPT_DISP_NO_BUTTONS))
		build_imchat_box(TRUE);


        /* Swing the edit buddy */
        editpane = gtk_vbox_new(FALSE, 0);

        
       	addbutton = gtk_button_new_with_label(_("Add"));
       	groupbutton = gtk_button_new_with_label(_("Group"));
       	rembutton = gtk_button_new_with_label(_("Remove"));
	
	if (display_options & OPT_DISP_COOL_LOOK)
	{
		gtk_button_set_relief(GTK_BUTTON(addbutton), GTK_RELIEF_NONE);
		gtk_button_set_relief(GTK_BUTTON(groupbutton), GTK_RELIEF_NONE);
		gtk_button_set_relief(GTK_BUTTON(rembutton), GTK_RELIEF_NONE);
	}
	
	edittree = gtk_ctree_new(1, 0);
	gtk_ctree_set_line_style (GTK_CTREE(edittree), GTK_CTREE_LINES_SOLID);
        gtk_ctree_set_expander_style(GTK_CTREE(edittree), GTK_CTREE_EXPANDER_SQUARE);
	gtk_clist_set_reorderable(GTK_CLIST(edittree), TRUE);

	gtk_ctree_set_drag_compare_func (GTK_CTREE(edittree),
                                      (GtkCTreeCompareDragFunc)edit_drag_compare_func);

	
	gtk_signal_connect_after (GTK_OBJECT (edittree), "tree_move",
				  GTK_SIGNAL_FUNC (edit_tree_move), NULL);

	
	bbox = gtk_hbox_new(TRUE, 10);
       	tbox = gtk_scrolled_window_new(NULL, NULL);
       	/* Put the buttons in the box */
       	gtk_box_pack_start(GTK_BOX(bbox), addbutton, TRUE, TRUE, 0);
       	gtk_box_pack_start(GTK_BOX(bbox), groupbutton, TRUE, TRUE, 0);
       	gtk_box_pack_start(GTK_BOX(bbox), rembutton, TRUE, TRUE, 0);

	gtk_tooltips_set_tip(tips, addbutton, _("Add a new Buddy"), "Penguin");
	gtk_tooltips_set_tip(tips, groupbutton, _("Add a new Group"), "Penguin");
	gtk_tooltips_set_tip(tips, rembutton, _("Remove selected Buddy"), "Penguin");

       	/* And the boxes in the box */
       	gtk_box_pack_start(GTK_BOX(editpane), tbox, TRUE, TRUE, 5);
       	gtk_box_pack_start(GTK_BOX(editpane), bbox, FALSE, FALSE, 5);

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
       	gtk_signal_connect(GTK_OBJECT(groupbutton), "clicked", GTK_SIGNAL_FUNC(add_group_callback), NULL);
        gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);

        gtk_container_add(GTK_CONTAINER(blist), vbox);

#ifndef USE_APPLET
        gtk_signal_connect(GTK_OBJECT(blist), "delete_event", GTK_SIGNAL_FUNC(do_quit), blist);
#else
	gtk_signal_connect(GTK_OBJECT(blist), "delete_event", GTK_SIGNAL_FUNC(applet_destroy_buddy), NULL);
#endif

        gtk_signal_connect(GTK_OBJECT(blist), "configure_event", GTK_SIGNAL_FUNC(move_blist_window), NULL);



        /* The edit tree */
        gtk_container_add(GTK_CONTAINER(tbox), edittree);
       	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(tbox),
                                       GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);


        gtk_window_set_title(GTK_WINDOW(blist), _("Gaim - Buddy List"));

        if (general_options & OPT_GEN_SAVED_WINDOWS) {
                if (blist_pos.width != 0) { /* Sanity check! */
                        gtk_widget_set_uposition(blist, blist_pos.x - blist_pos.xoff, blist_pos.y - blist_pos.yoff);
                        gtk_widget_set_usize(blist, blist_pos.width, blist_pos.height);
                }
        }
}

void refresh_buddy_window()
{
        build_edit_tree();
	build_permit_tree();
        
        update_button_pix();
        gtk_widget_show(blist);
}

