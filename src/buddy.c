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

#ifdef USE_APPLET
#include <gnome.h>
#include <applet-widget.h>
#include "gnome_applet_mgr.h"
#endif /* USE_APPLET */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include "gaim.h"
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
#include "pixmaps/permadd.xpm"
#include "pixmaps/permdel.xpm"
#include "pixmaps/away_icon.xpm"

#include "pixmaps/daemon-buddyadd.xpm"
#include "pixmaps/daemon-buddydel.xpm"
#include "pixmaps/daemon-buddychat.xpm"
#include "pixmaps/daemon-im.xpm"
#include "pixmaps/daemon-info.xpm"
#include "pixmaps/daemon-permadd.xpm"
#include "pixmaps/daemon-permdel.xpm"

static GtkTooltips *tips;
static GtkWidget *editpane;
static GtkWidget *buddypane;
static GtkWidget *permitpane;
static GtkWidget *edittree;
static GtkWidget *permtree;
static GtkWidget *imbutton, *infobutton, *chatbutton;
static GtkWidget *addbutton, *rembutton;
static GtkWidget *addpermbutton, *rempermbutton;
static GtkWidget *lagometer = NULL;
static GtkWidget *lagometer_box = NULL;

static int last_lag_us;


GtkWidget *blist = NULL;
GtkWidget *bpmenu;
GtkWidget *buddies;
int permdeny;


/* Predefine some functions */
static void new_bp_callback(GtkWidget *w, char *name);
static void log_callback(GtkWidget *w, char *name);


void destroy_buddy()
{
	if (blist)
		gtk_widget_destroy(blist);
	blist=NULL;
#ifdef USE_APPLET
	buddy_created = FALSE;
#endif
}

void update_num_groups()
{
	GList *grp = groups;
	GList *mem;
        struct buddy *b;
	struct group *g;
	int pres, total;
        char buf[BUF_LONG];

        if (!(display_options & OPT_DISP_SHOW_GRPNUM))
                return;

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

                gtk_label_set(GTK_LABEL(g->label), buf);
                grp = grp->next;
        }
        
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

void update_lagometer(int us)
{
        double pct;
        

        
        if (us != -1)
                last_lag_us = us;


        if (lagometer_box == NULL)
                return;

        
        if (!(general_options & OPT_GEN_SHOW_LAGMETER))
                gtk_widget_hide(lagometer_box);
        else
                gtk_widget_show(lagometer_box);

                
        pct = last_lag_us/100000;

	if (pct > 0)
		pct = 25 * log(pct);

        if (pct < 0)
                pct = 0;

        if (pct > 100)
                pct = 100;


        pct /= 100;


        gtk_progress_bar_update(GTK_PROGRESS_BAR(lagometer), pct);
}

static void adjust_pic(GtkWidget *button, const char *c, gchar **xpm)
{
        GdkPixmap *pm;
        GdkBitmap *bm;
        GtkWidget *pic;
        GtkWidget *label;

		/*if the user had opted to put pictures on the buttons*/
        if (display_options & OPT_DISP_SHOW_BUTTON_XPM) {
		pm = gdk_pixmap_create_from_xpm_d(blist->window, &bm,
						  NULL, xpm);
		pic = gtk_pixmap_new(pm, bm);
		gtk_widget_show(pic);
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
	        adjust_pic(addbutton, "Add", (gchar **)daemon_buddyadd_xpm);
		adjust_pic(rembutton, "Remove", (gchar **)daemon_buddydel_xpm);
		adjust_pic(chatbutton, "Chat", (gchar **)daemon_buddychat_xpm);
	        adjust_pic(imbutton, "IM", (gchar **)daemon_im_xpm);
	        adjust_pic(infobutton, "Info", (gchar **)daemon_info_xpm);
	        adjust_pic(addpermbutton, "Add", (gchar **)daemon_permadd_xpm);
	        adjust_pic(rempermbutton, "Remove", (gchar **)daemon_permdel_xpm);
	} else {
	        adjust_pic(addbutton, "Add", (gchar **)buddyadd_xpm);
		adjust_pic(rembutton, "Remove", (gchar **)buddydel_xpm);
		adjust_pic(chatbutton, "Chat", (gchar **)buddychat_xpm);
	        adjust_pic(imbutton, "IM", (gchar **)im_xpm);
	        adjust_pic(infobutton, "Info", (gchar **)info_xpm);
	        adjust_pic(addpermbutton, "Add", (gchar **)permadd_xpm);
	        adjust_pic(rempermbutton, "Remove", (gchar **)permdel_xpm);
	}
}



#ifdef USE_APPLET
gint applet_destroy_buddy( GtkWidget *widget, GdkEvent *event,gpointer *data ) {
	set_applet_draw_closed();
	gnome_buddy_hide();
	return (TRUE);
}

void gnome_buddy_show(){
	gtk_widget_show( blist );
}

void gnome_buddy_hide(){
	gtk_widget_hide( blist );
}

void gnome_buddy_set_pos( gint x, gint y ){
	if (general_options & OPT_GEN_NEAR_APPLET)
		gtk_widget_set_uposition ( blist, x, y );
	else if (general_options & OPT_GEN_SAVED_WINDOWS)
		gtk_widget_set_uposition(blist, blist_pos.x - blist_pos.xoff, blist_pos.y - blist_pos.yoff);
}

GtkRequisition gnome_buddy_get_dimentions(){
	if (general_options & OPT_GEN_SAVED_WINDOWS) {
		GtkRequisition r;
		r.width = blist_pos.width;
		r.height = blist_pos.height;
		return r;
	} else {
		return blist->requisition;
	}
}

#endif


extern enum gaim_user_states MRI_user_status;
void signoff()
{
	GList *mem;

#ifdef GAIM_PLUGINS
	GList *c = callbacks;
	struct gaim_callback *g;
	void (*function)(void *);
	while (c) {
		g = (struct gaim_callback *)c->data;
		if (g->event == event_signoff && g->function != NULL) {
			function = g->function;
			(*function)(g->data);
		}
		c = c->next;
	}
#endif

        while(groups) {
		mem = ((struct group *)groups->data)->members;
		while(mem) {
			g_free(mem->data);
                        mem = g_list_remove(mem, mem->data);
		}
		g_free(groups->data);
                groups = g_list_remove(groups, groups->data);
	}

	serv_close();
        destroy_all_dialogs();
        destroy_buddy();
        hide_login_progress("");
#ifdef USE_APPLET
	MRI_user_status = offline;
        set_applet_draw_closed();
        applet_widget_unregister_callback(APPLET_WIDGET(applet),"signoff");
	remove_applet_away();
        applet_widget_register_callback(APPLET_WIDGET(applet),
                "signon",
                _("Signon"),
                applet_show_login,
                NULL);
#else
        show_login();
#endif /* USE_APPLET */
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

void pressed_info(GtkWidget *widget, struct buddy *b)
{
        serv_get_info(b->name);

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

		button = gtk_menu_item_new_with_label("IM");
		gtk_signal_connect(GTK_OBJECT(button), "activate",
				   GTK_SIGNAL_FUNC(pressed_im), b);
		gtk_menu_append(GTK_MENU(menu), button);
		gtk_widget_show(button);

		button = gtk_menu_item_new_with_label("Info");
		gtk_signal_connect(GTK_OBJECT(button), "activate",
				   GTK_SIGNAL_FUNC(pressed_info), b);
		gtk_menu_append(GTK_MENU(menu), button);
		gtk_widget_show(button);

		button = gtk_menu_item_new_with_label("Dir Info");
		gtk_signal_connect(GTK_OBJECT(button), "activate",
				   GTK_SIGNAL_FUNC(pressed_dir_info), b);
		gtk_menu_append(GTK_MENU(menu), button);
		gtk_widget_show(button);

#ifdef USE_OSCAR /* FIXME : someday maybe TOC can do this too */
		button = gtk_menu_item_new_with_label("Away Msg");
		gtk_signal_connect(GTK_OBJECT(button), "activate",
				   GTK_SIGNAL_FUNC(pressed_away_msg), b);
		gtk_menu_append(GTK_MENU(menu), button);
		gtk_widget_show(button);
#endif

		button = gtk_menu_item_new_with_label("Toggle Logging");
		gtk_signal_connect(GTK_OBJECT(button), "activate",
				   GTK_SIGNAL_FUNC(log_callback), b->name);
		gtk_menu_append(GTK_MENU(menu), button);
		gtk_widget_show(button);

		button = gtk_menu_item_new_with_label("Add Buddy Pounce");
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



void build_permit_tree()
{
	GtkWidget *ti;
        GtkWidget *sub;
        GList *plist = permit;
        GList *dlist = deny;

        gtk_tree_clear_items(GTK_TREE(permtree), 0, -1);

        ti = gtk_tree_item_new_with_label("Permit");
        sub = gtk_tree_new();
        gtk_widget_show(ti);
        gtk_widget_show(sub);
        gtk_tree_prepend(GTK_TREE(permtree), ti);
        gtk_tree_item_set_subtree(GTK_TREE_ITEM(ti), sub);
        gtk_tree_item_expand(GTK_TREE_ITEM(ti));
        
        while(plist) {
                ti = gtk_tree_item_new_with_label((char *)plist->data);
                gtk_widget_show(ti);
                gtk_tree_prepend(GTK_TREE(sub), ti);
                plist = plist->next;
        }


        ti = gtk_tree_item_new_with_label("Deny");
        sub = gtk_tree_new();
        gtk_widget_show(ti);
        gtk_widget_show(sub);
        gtk_tree_prepend(GTK_TREE(permtree), ti);
        gtk_tree_item_set_subtree(GTK_TREE_ITEM(ti), sub);
        gtk_tree_item_expand(GTK_TREE_ITEM(ti));
        
        while(dlist) {
                ti = gtk_tree_item_new_with_label((char *)dlist->data);
                gtk_widget_show(ti);
                gtk_tree_prepend(GTK_TREE(sub), ti);
                dlist = dlist->next;
        }

	
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

struct buddy *add_buddy(char *group, char *buddy)
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
        g->members = g_list_append(g->members, b);


        if (blist == NULL)
                return b;
        
	box = gtk_hbox_new(FALSE, 1);
	pm = gdk_pixmap_create_from_xpm_d(blist->window, &bm,
					  NULL, (gchar **)login_icon_xpm);
        b->pix = gtk_pixmap_new(pm, bm);

        b->idle = 0;
			
	gtk_widget_show(b->pix);

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

static void do_del_perm(GtkWidget *w, GtkTree *permtree)
{
	GtkLabel *label, *plabel;
	GtkWidget *item, *pitem;
	char *c, *d;
	GList *i;
	
        GList *plist;
        GList *dlist;
	int level;

        plist = permit;
        dlist = deny;
        
	i = GTK_TREE_SELECTION(permtree);
	if (i) {
		item = GTK_WIDGET(i->data);
		gtk_tree_unselect_child(GTK_TREE(permtree), item);
		label = GTK_LABEL(GTK_BIN(item)->child);
		gtk_label_get(label, &c);
		level = GTK_TREE(item->parent)->level;
		if (level > 0) {
			pitem = GTK_WIDGET(GTK_TREE(item->parent)->tree_owner);
			plabel = GTK_LABEL(GTK_BIN(pitem)->child);
			gtk_label_get(plabel, &d);
                        if (!strcasecmp(d, "Permit")) {
                                while(plist) {
                                        if (!strcasecmp((char *)(plist->data), c)) {
                                                permit = g_list_remove(permit, plist->data);
                                                break;
                                        }

                                        plist = plist->next;
                                }

                        } else {
                                while(dlist) {
                                        if (!strcasecmp((char *)(dlist->data), c)) {
                                                deny = g_list_remove(deny, dlist->data);
                                                
                                                break;
                                        }
                                        dlist = dlist->next;
                                }

                        }

                        
                } else {
                        /* Can't delete groups here! :) */
                        return;
                }
                serv_set_permit_deny();
		gtk_tree_clear_items(GTK_TREE(permtree), 0, -1);
                build_permit_tree();
                serv_save_config();
	}
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

void add_perm_callback(GtkWidget *widget, void *dummy)
{
        show_add_perm(NULL);
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

                        	write_to_conv(c, b->message, WFLAG_SEND);

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
		show_log_dialog(name);
		if (c) {
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
	
	menuitem = gtk_menu_item_new_with_label("New Buddy Pounce");
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
	
	menuitem = gtk_menu_item_new_with_label("Remove Buddy Pounce");
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


void set_buddy(struct buddy *b)
{
	char infotip[256];
        char idlet[16];
        char warn[256];
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
			g_snprintf(warn, sizeof(warn), "Warnings: %d%%\n", b->evil);

		} else
			warn[0] = '\0';
		
                i = g_snprintf(infotip, sizeof(infotip), "Name: %s                \nLogged in: %s\n%s%s%s", b->name, sotime, warn, ((b->idle) ? "Idle: " : ""),  itime);
		
		gtk_tooltips_set_tip(tips, GTK_WIDGET(b->item), infotip, "");

                g_free(sotime);
                g_free(itime);

                

		/* this check should also depend on whether they left,
		 * and signed on again before they got erased */
                if (!GTK_WIDGET_VISIBLE(b->item) || b->present == 1) {
#ifdef GAIM_PLUGINS
			GList *c = callbacks;
			struct gaim_callback *g;
			void (*function)(char *, void *);
			while (c) {
				g = (struct gaim_callback *)c->data;
				if (g->event == event_buddy_signon &&
						g->function != NULL) {
					function = g->function;
					(*function)(b->name, g->data);
				}
				c = c->next;
			}
#endif
			
			play_sound(BUDDY_ARRIVE);
			b->present = 2;

			who = g_malloc(sizeof(b->name) + 10);
			strcpy(who, b->name);
			gtk_label_set(GTK_LABEL(b->label), who);
			g_free(who);

			pm = gdk_pixmap_create_from_xpm_d(blist->window, &bm,
				NULL, (gchar **)login_icon_xpm);
			gtk_widget_hide(b->pix);
			gtk_pixmap_set(GTK_PIXMAP(b->pix), pm, bm);
                        if (display_options & OPT_DISP_SHOW_PIXMAPS)
				gtk_widget_show(b->pix);

			if (display_options & OPT_DISP_SHOW_LOGON) {
				struct conversation *c = find_conversation(b->name);
				if (c) {
					char tmp[1024];

					
					g_snprintf(tmp, sizeof(tmp), "<HR><B>%s logged in%s%s.</B><BR><HR>", b->name,
                                                   ((display_options & OPT_DISP_SHOW_TIME) ? " @ " : ""),
                                                   ((display_options & OPT_DISP_SHOW_TIME) ? date() : ""));


					write_to_conv(c, tmp, WFLAG_SYSTEM);

				}
			}

			
			gtk_widget_show(b->item);
			gtk_widget_show(b->label);
                        b->log_timer = gtk_timeout_add(10000, (GtkFunction) log_timeout, b->name);
                        update_num_groups();
                        update_show_idlepix();
                        setup_buddy_chats();
			return;
                }


                
                if (!b->log_timer) {
                        gtk_widget_hide(b->pix);
                        if (b->uc & UC_UNAVAILABLE) {
#ifdef GAIM_PLUGINS
				GList *c = callbacks;
				struct gaim_callback *g;
				void (*function)(char *, void *);
				while (c) {
					g = (struct gaim_callback *)c->data;
					if (g->event == event_buddy_away &&
							g->function != NULL) {
						function = g->function;
						(*function)(b->name, g->data);
					}
					c = c->next;
				}
#endif
                                pm = gdk_pixmap_create_from_xpm_d(blist->window, &bm,
                                                                  NULL, (gchar **)away_icon_xpm);
                                gtk_pixmap_set(GTK_PIXMAP(b->pix), pm, bm);
                        } else if (b->uc & UC_AOL) {
                                pm = gdk_pixmap_create_from_xpm_d(blist->window, &bm,
                                                                  NULL, (gchar **)aol_icon_xpm);
                                gtk_pixmap_set(GTK_PIXMAP(b->pix), pm, bm);
                        } else if (b->uc & UC_NORMAL) {
                                pm = gdk_pixmap_create_from_xpm_d(blist->window, &bm,
                                                                  NULL, (gchar **)free_icon_xpm);
                                gtk_pixmap_set(GTK_PIXMAP(b->pix), pm, bm);
                        } else if (b->uc & UC_ADMIN) {
                                pm = gdk_pixmap_create_from_xpm_d(blist->window, &bm,
                                                                  NULL, (gchar **)admin_icon_xpm);
                                gtk_pixmap_set(GTK_PIXMAP(b->pix), pm, bm);
                        } else if (b->uc & UC_UNCONFIRMED) {
                                pm = gdk_pixmap_create_from_xpm_d(blist->window, &bm,
                                                                  NULL, (gchar **)dt_icon_xpm);
                                gtk_pixmap_set(GTK_PIXMAP(b->pix), pm, bm);
                        } else {
                                pm = gdk_pixmap_create_from_xpm_d(blist->window, &bm,
                                                                  NULL, (gchar **)no_icon_xpm);
                                gtk_pixmap_set(GTK_PIXMAP(b->pix), pm, bm);
                        }
                        if (display_options & OPT_DISP_SHOW_PIXMAPS)
                                gtk_widget_show(b->pix);
                }
	


	} else {
		if (GTK_WIDGET_VISIBLE(b->item)) {
#ifdef GAIM_PLUGINS
			GList *c = callbacks;
			struct gaim_callback *g;
			void (*function)(char *, void *);
			while (c) {
				g = (struct gaim_callback *)c->data;
				if (g->event == event_buddy_signoff &&
						g->function != NULL) {
					function = g->function;
					(*function)(b->name, g->data);
				}
				c = c->next;
			}
#endif
			play_sound(BUDDY_LEAVE);
			pm = gdk_pixmap_create_from_xpm_d(blist->window, &bm,
				NULL, (gchar **)logout_icon_xpm);
			gtk_widget_hide(b->pix);
			gtk_pixmap_set(GTK_PIXMAP(b->pix), pm, bm);
                        if (display_options & OPT_DISP_SHOW_PIXMAPS)
				gtk_widget_show(b->pix);
			if (display_options & OPT_DISP_SHOW_LOGON) {
				struct conversation *c = find_conversation(b->name);
				if (c) {
					char tmp[1024];

					
					g_snprintf(tmp, sizeof(tmp), "<HR><B>%s logged out%s%s.</B><BR><HR>", b->name,
                                                   ((display_options & OPT_DISP_SHOW_TIME) ? " @ " : ""),
                                                   ((display_options & OPT_DISP_SHOW_TIME) ? date() : ""));


					write_to_conv(c, tmp, WFLAG_SYSTEM);

				}
			}
                        b->log_timer = gtk_timeout_add(10000, (GtkFunction)log_timeout, b->name);
                        update_num_groups();
                        update_show_idlepix();
		}
        }
        setup_buddy_chats();
}


static void set_permit(GtkWidget *w, int *data)
{
	permdeny = (int)data;
/*	printf("BLAH BLAH %d %d", permdeny, (int) data); */
	/* We don't save this 'at home', it's on the server.
         * So, we gotta resend the config to the server. */
        serv_save_config();
#ifdef USE_OSCAR
	/* we do this here because we can :) */
	serv_set_permit_deny();
#endif
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





void show_buddy_list()
{
	
	/* Build the buddy list, based on *config */
        
	GtkWidget *sw;
	GtkWidget *menu;
	GtkWidget *findmenu;
	GtkWidget *setmenu;
	GtkWidget *menubar;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *menuitem;
        GtkWidget *notebook;
        GtkWidget *label;
        GtkWidget *bbox;
        GtkWidget *permopt;
        GtkWidget *tbox;
        GtkWidget *xbox;
        GtkWidget *pbox;


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


	menuitem = gaim_new_item(NULL, "File", NULL);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), menu);
	gtk_menu_bar_append(GTK_MENU_BAR(menubar), menuitem);

	gaim_new_item(menu, "Add A Buddy", GTK_SIGNAL_FUNC(add_buddy_callback));
        gaim_seperator(menu);
        gaim_new_item(menu, "Import Buddy List", GTK_SIGNAL_FUNC(import_callback));
        gaim_new_item(menu, "Export Buddy List", GTK_SIGNAL_FUNC(export_callback));
	if (!(general_options & OPT_GEN_REGISTERED))
	{
        	gaim_seperator(menu);
		gaim_new_item(menu, "Register", GTK_SIGNAL_FUNC(gaimreg_callback));
	}
	gaim_seperator(menu);
	gaim_new_item(menu, "Signoff", GTK_SIGNAL_FUNC(signoff));

#ifndef USE_APPLET
	gaim_new_item(menu, "Quit", GTK_SIGNAL_FUNC(do_quit));
#else
	gaim_new_item(menu, "Close", GTK_SIGNAL_FUNC(applet_destroy_buddy));
#endif

	menu = gtk_menu_new();

	menuitem = gaim_new_item(NULL, "Tools", NULL);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), menu);
	gtk_menu_bar_append(GTK_MENU_BAR(menubar), menuitem);

	awaymenu = gtk_menu_new();
	menuitem = gaim_new_item(menu, "Away", NULL);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), awaymenu);
        do_away_menu();

        bpmenu = gtk_menu_new();
        menuitem = gaim_new_item(menu, "Buddy Pounce", NULL);
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), bpmenu);
        do_bp_menu();

        gaim_seperator(menu);

	findmenu = gtk_menu_new();
	gtk_widget_show(findmenu);
	menuitem = gaim_new_item(menu, "Search for Buddy", NULL);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), findmenu);
	gtk_widget_show(menuitem);
	menuitem = gtk_menu_item_new_with_label("by Email");
	gtk_menu_append(GTK_MENU(findmenu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate", GTK_SIGNAL_FUNC(show_find_email), NULL);
	gtk_widget_show(menuitem);
	menuitem = gtk_menu_item_new_with_label("by Dir Info");
	gtk_menu_append(GTK_MENU(findmenu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate", GTK_SIGNAL_FUNC(show_find_info), NULL);
 	gtk_widget_show(menuitem);

	setmenu = gtk_menu_new();
	gtk_widget_show(setmenu);
	menuitem = gaim_new_item(menu, "Settings", NULL);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), setmenu);
	gtk_widget_show(menuitem);
	menuitem = gtk_menu_item_new_with_label("User Info");
	gtk_menu_append(GTK_MENU(setmenu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate", GTK_SIGNAL_FUNC(show_set_info), NULL);
	gtk_widget_show(menuitem);
	menuitem = gtk_menu_item_new_with_label("Directory Info");
	gtk_menu_append(GTK_MENU(setmenu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate", GTK_SIGNAL_FUNC(show_set_dir), NULL);	
	gtk_widget_show(menuitem);
	menuitem = gtk_menu_item_new_with_label("Change Password");
	gtk_menu_append(GTK_MENU(setmenu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate", GTK_SIGNAL_FUNC(show_change_passwd), NULL);
	gtk_widget_show(menuitem);
	gaim_seperator(menu);

        gaim_new_item(menu, "Preferences", GTK_SIGNAL_FUNC(show_prefs));

#ifdef GAIM_PLUGINS
        gaim_new_item(menu, "Plugins", GTK_SIGNAL_FUNC(show_plugins));
#endif

	menu = gtk_menu_new();

	menuitem = gaim_new_item(NULL, "Help", NULL);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), menu);
	gtk_menu_item_right_justify(GTK_MENU_ITEM(menuitem));
	gtk_menu_bar_append(GTK_MENU_BAR(menubar), menuitem);
	
	gaim_new_item(menu, "About", show_about);

        gtk_widget_show(menubar);

        lagometer_box = gtk_hbox_new(FALSE, 0);

        
        lagometer = gtk_progress_bar_new();
        gtk_widget_show(lagometer);

        label = gtk_label_new("Lag-O-Meter: ");
        gtk_widget_show(label);
        
        gtk_box_pack_start(GTK_BOX(lagometer_box), label, FALSE, FALSE, 5);
        gtk_box_pack_start(GTK_BOX(lagometer_box), lagometer, TRUE, TRUE, 5);

        gtk_widget_set_usize(lagometer, 5, 5);
        

        if ((general_options & OPT_GEN_SHOW_LAGMETER))
                gtk_widget_show(lagometer_box);

        
	vbox       = gtk_vbox_new(FALSE, 10);
        
        notebook = gtk_notebook_new();

 
        

        /* Do buddy list stuff */

        buddypane = gtk_vbox_new(FALSE, 0);
        
        imbutton   = gtk_button_new_with_label("IM");
	infobutton = gtk_button_new_with_label("Info");
	chatbutton = gtk_button_new_with_label("Chat");

	hbox       = gtk_hbox_new(TRUE, 10);

	buddies    = gtk_tree_new();
	sw         = gtk_scrolled_window_new(NULL, NULL);
	



        
	tips = gtk_tooltips_new();
	gtk_object_set_data(GTK_OBJECT(blist), "Buddy List", tips);
	
 
	
	/* Now the buddy list */
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw),buddies);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
				       GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
	gtk_widget_set_usize(sw,200,200);
	gtk_widget_show(buddies);
	gtk_widget_show(sw);

	/* Put the buttons in the hbox */
	gtk_widget_show(imbutton);
	gtk_widget_show(chatbutton);
	gtk_widget_show(infobutton);

	gtk_box_pack_start(GTK_BOX(hbox), imbutton, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), infobutton, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), chatbutton, TRUE, TRUE, 0);
        gtk_container_border_width(GTK_CONTAINER(hbox), 10);


	gtk_tooltips_set_tip(tips,infobutton, "Information on selected Buddy", "Penguin");
	gtk_tooltips_set_tip(tips,imbutton, "Send Instant Message", "Penguin");
	gtk_tooltips_set_tip(tips,chatbutton, "Start/join a Buddy Chat", "Penguin");

        gtk_box_pack_start(GTK_BOX(buddypane), sw, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(buddypane), hbox, FALSE, FALSE, 0);

        gtk_widget_show(hbox);
        gtk_widget_show(buddypane);



        /* Swing the edit buddy */
        editpane = gtk_vbox_new(FALSE, 0);

        
       	addbutton = gtk_button_new_with_label("Add");
       	rembutton = gtk_button_new_with_label("Remove");
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
       	gtk_box_pack_start(GTK_BOX(bbox), addbutton, TRUE, TRUE, 10);
       	gtk_box_pack_start(GTK_BOX(bbox), rembutton, TRUE, TRUE, 10);

	gtk_tooltips_set_tip(tips, addbutton, "Add a new Buddy", "Penguin");
	gtk_tooltips_set_tip(tips, rembutton, "Remove selected Buddy", "Penguin");

       	/* And the boxes in the box */
       	gtk_box_pack_start(GTK_BOX(editpane), tbox, TRUE, TRUE, 5);
       	gtk_box_pack_start(GTK_BOX(editpane), bbox, FALSE, FALSE, 5);

	/* Handle closes right */

	

       	/* Finish up */
       	gtk_widget_show(addbutton);
       	gtk_widget_show(rembutton);
       	gtk_widget_show(edittree);
       	gtk_widget_show(tbox);
       	gtk_widget_show(bbox);
	gtk_widget_show(editpane);


	/* Permit/Deny */

	permitpane = gtk_vbox_new(FALSE, 0);

        permopt = gtk_radio_button_new_with_label(NULL, "Allow anyone");
        gtk_box_pack_start(GTK_BOX(permitpane), permopt, FALSE, FALSE, 0);
        gtk_signal_connect(GTK_OBJECT(permopt), "clicked", GTK_SIGNAL_FUNC(set_permit), (void *)1);
	gtk_widget_show(permopt);
	if (permdeny == 1)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(permopt), TRUE);

        permopt = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(permopt)), "Permit some");
        gtk_box_pack_start(GTK_BOX(permitpane), permopt, FALSE, FALSE, 0);
        gtk_signal_connect(GTK_OBJECT(permopt), "clicked", GTK_SIGNAL_FUNC(set_permit), (void *)3);
	gtk_widget_show(permopt);
	if (permdeny == 3)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(permopt), TRUE);


        permopt = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(permopt)), "Deny some");
        gtk_box_pack_start(GTK_BOX(permitpane), permopt, FALSE, FALSE, 0);
        gtk_signal_connect(GTK_OBJECT(permopt), "clicked", GTK_SIGNAL_FUNC(set_permit), (void *)4);
        gtk_widget_show(permopt);
	if (permdeny == 4)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(permopt), TRUE);



        addpermbutton = gtk_button_new_with_label("Add");
        rempermbutton = gtk_button_new_with_label("Remove");
        
       	permtree = gtk_tree_new();
	build_permit_tree();
       	pbox = gtk_hbox_new(TRUE, 10);
       	xbox = gtk_scrolled_window_new(NULL, NULL);
       	/* Put the buttons in the box */
       	gtk_box_pack_start(GTK_BOX(pbox), addpermbutton, TRUE, TRUE, 10);
        gtk_box_pack_start(GTK_BOX(pbox), rempermbutton, TRUE, TRUE, 10);
        

	gtk_tooltips_set_tip(tips, addpermbutton, "Add buddy to permit/deny", "Penguin");
	gtk_tooltips_set_tip(tips, rempermbutton, "Remove buddy from permit/deny", "Penguin");
       	/* And the boxes in the box */
       	gtk_box_pack_start(GTK_BOX(permitpane), xbox, TRUE, TRUE, 5);
       	gtk_box_pack_start(GTK_BOX(permitpane), pbox, FALSE, FALSE, 5);

	/* Handle closes right */

	

       	/* Finish up */
       	gtk_widget_show(addpermbutton);
        gtk_widget_show(rempermbutton);
       	gtk_widget_show(permtree);
       	gtk_widget_show(xbox);
       	gtk_widget_show(pbox);
	gtk_widget_show(permitpane);



        label = gtk_label_new("Online");
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), buddypane, label);
        label = gtk_label_new("Edit Buddies");
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), editpane, label);
	label = gtk_label_new("Permit");
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), permitpane, label);

        gtk_widget_show_all(notebook);

	/* Pack things in the vbox */
        gtk_widget_show(vbox);


        gtk_widget_show(notebook);

        /* Enable buttons */
	
	gtk_signal_connect(GTK_OBJECT(imbutton), "clicked", GTK_SIGNAL_FUNC(show_im_dialog), buddies);
	gtk_signal_connect(GTK_OBJECT(infobutton), "clicked", GTK_SIGNAL_FUNC(info_callback), buddies);
	gtk_signal_connect(GTK_OBJECT(chatbutton), "clicked", GTK_SIGNAL_FUNC(chat_callback), buddies);
       	gtk_signal_connect(GTK_OBJECT(rembutton), "clicked", GTK_SIGNAL_FUNC(do_del_buddy), edittree);
       	gtk_signal_connect(GTK_OBJECT(addbutton), "clicked", GTK_SIGNAL_FUNC(add_buddy_callback), NULL);
        gtk_signal_connect(GTK_OBJECT(addpermbutton), "clicked", GTK_SIGNAL_FUNC(add_perm_callback), NULL);
        gtk_signal_connect(GTK_OBJECT(rempermbutton), "clicked", GTK_SIGNAL_FUNC(do_del_perm), permtree);
        gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(vbox), lagometer_box, FALSE, TRUE, 0);
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


        /* The permit tree */
       	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(xbox), permtree);
       	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(xbox),
                                       GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);

        gtk_window_set_title(GTK_WINDOW(blist), "Gaim - Buddy List");

        if (general_options & OPT_GEN_SAVED_WINDOWS) {
                if (blist_pos.width != 0) { /* Sanity check! */
                        gtk_widget_set_uposition(blist, blist_pos.x - blist_pos.xoff, blist_pos.y - blist_pos.yoff);
                        gtk_widget_set_usize(blist, blist_pos.width, blist_pos.height);
                }
        }
}

void refresh_buddy_window()
{
        setup_buddy_chats();
 
        build_edit_tree();
        build_permit_tree();
        
        update_button_pix();
        gtk_widget_show(blist);


}

