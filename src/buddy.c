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
#ifdef GAIM_PLUGINS
#ifndef _WIN32
#include <dlfcn.h>
#endif
#endif /* GAIM_PLUGINS */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <ctype.h>

#ifdef _WIN32
#include <gdk/gdkwin32.h>
#else
#include <unistd.h>
#include <gdk/gdkx.h>
#endif

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include "prpl.h"
#include "sound.h"
#include "gaim.h"
#include "gtklist.h"
#include "gtkft.h"

#ifdef _WIN32
#include "win32dep.h"
#endif

static struct gaim_gtk_buddy_list *gtkblist = NULL;

/* Docklet nonsense */
static gboolean gaim_gtk_blist_obscured = FALSE;

static void gaim_gtk_blist_update(struct gaim_buddy_list *list, GaimBlistNode *node);

/***************************************************
 *              Callbacks                          *
 ***************************************************/

static void gaim_gtk_blist_destroy_cb()
{
	if (docklet_count)
		gaim_blist_set_visible(FALSE);
	else
		do_quit();
}

static void gtk_blist_menu_im_cb(GtkWidget *w, struct buddy *b)
{
       gaim_conversation_new(GAIM_CONV_IM, b->account, b->name);
}

static void gtk_blist_menu_alias_cb(GtkWidget *w, struct buddy *b)
{
       alias_dialog_bud(b);
}

static void gtk_blist_menu_bp_cb(GtkWidget *w, struct buddy *b)
{
       show_new_bp(b->name, b->account->gc, b->idle,
                               b->uc & UC_UNAVAILABLE, NULL);
}

static void gtk_blist_menu_showlog_cb(GtkWidget *w, struct buddy *b)
{
       show_log(b->name);
}

static void gtk_blist_show_systemlog_cb()
{
       show_log(NULL);
}

static void gtk_blist_button_im_cb(GtkWidget *w, GtkTreeView *tv)
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model(tv);
	GtkTreeSelection *sel = gtk_tree_view_get_selection(tv);

	if(gtk_tree_selection_get_selected(sel, &model, &iter)){
		GaimBlistNode *node;

		gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &node, -1);
		if (GAIM_BLIST_NODE_IS_BUDDY(node)) 
			gaim_conversation_new(GAIM_CONV_IM, ((struct buddy*)node)->account, ((struct buddy*)node)->name);
	}
}

static void gtk_blist_button_info_cb(GtkWidget *w, GtkTreeView *tv)
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model(tv);
	GtkTreeSelection *sel = gtk_tree_view_get_selection(tv);

	if(gtk_tree_selection_get_selected(sel, &model, &iter)){
		GaimBlistNode *node;

		gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &node, -1);
		if (GAIM_BLIST_NODE_IS_BUDDY(node)) {
			serv_get_info(((struct buddy*)node)->account->gc, ((struct buddy*)node)->name);
			return;
		}
	}
	show_info_dialog();
}

static void gtk_blist_button_chat_cb(GtkWidget *w, gpointer data)
{
	/* FIXME: someday, we can check to see if we've selected a chat node */
	join_chat();
}

static void gtk_blist_button_away_cb(GtkWidget *w, gpointer data)
{
	gtk_menu_popup(GTK_MENU(awaymenu), NULL, NULL, NULL, NULL, 1, GDK_CURRENT_TIME);
}

static void gtk_blist_row_activated_cb(GtkTreeView *tv, GtkTreePath *path, GtkTreeViewColumn *col, gpointer data) {
	GaimBlistNode *node;
	GtkTreeIter iter;
	GValue val = { 0, };
	
	gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel), &iter, path);
	
	gtk_tree_model_get_value (GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &val);
	node = g_value_get_pointer(&val);
	
	if (GAIM_BLIST_NODE_IS_BUDDY(node)) {
		gaim_conversation_new(GAIM_CONV_IM, ((struct buddy*)node)->account, ((struct buddy*)node)->name);
	} else if (GAIM_BLIST_NODE_IS_GROUP(node)) {
		if (gtk_tree_view_row_expanded(tv, path))
			gtk_tree_view_collapse_row(tv, path);
		else
			gtk_tree_view_expand_row(tv,path,FALSE);
	}
}

static void gaim_proto_menu_cb(GtkMenuItem *item, struct buddy *b)
{
	struct proto_buddy_menu *pbm = g_object_get_data(G_OBJECT(item), "gaimcallback");
	if (pbm->callback)
		pbm->callback(pbm->gc, b->name);
}

static gboolean gtk_blist_button_press_cb(GtkWidget *tv, GdkEventButton *event, gpointer null)
{
	GtkTreePath *path;
	GaimBlistNode *node;
	GValue val = { 0, };
	GtkTreeIter iter;
	GtkWidget *menu, *menuitem;
	GtkWidget *image;
	GList *list;
	struct prpl *prpl;

	if (event->button != 3)
		return FALSE;
	
	/* Here we figure out which node was clicked */
	if (!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(tv), event->x, event->y, &path, NULL, NULL, NULL))
		return FALSE;
	gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel), &iter, path);
	gtk_tree_model_get_value (GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &val);
	node = g_value_get_pointer(&val);
	
	if (!GAIM_BLIST_NODE_IS_BUDDY(node)) 
		return FALSE;
		
	menu = gtk_menu_new();

	/* Protocol specific options */
	prpl = find_prpl(((struct buddy*)node)->account->protocol);
	if (prpl) {
		list = prpl->buddy_menu(((struct buddy*)node)->account->gc, ((struct buddy*)node)->name);
		while (list) {
			struct proto_buddy_menu *pbm = list->data;
			menuitem = gtk_menu_item_new_with_mnemonic(pbm->label);
			g_object_set_data(G_OBJECT(menuitem), "gaimcallback", pbm);
			g_signal_connect(G_OBJECT(menuitem), "activate", G_CALLBACK(gaim_proto_menu_cb), node);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
			list = list->next;
		}
	}
	
	menuitem = gtk_image_menu_item_new_with_mnemonic("_IM");
	g_signal_connect(G_OBJECT(menuitem), "activate", G_CALLBACK(gtk_blist_menu_im_cb), node);
	image = gtk_image_new_from_stock(GAIM_STOCK_IM, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem), image);	
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	
	menuitem = gtk_image_menu_item_new_with_mnemonic("_Alias");
	g_signal_connect(G_OBJECT(menuitem), "activate", G_CALLBACK(gtk_blist_menu_alias_cb), node);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	
	menuitem = gtk_image_menu_item_new_with_mnemonic("Add Buddy _Pounce");
	g_signal_connect(G_OBJECT(menuitem), "activate", G_CALLBACK(gtk_blist_menu_bp_cb), node);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	
	menuitem = gtk_image_menu_item_new_with_mnemonic("View _Log");
	g_signal_connect(G_OBJECT(menuitem), "activate", G_CALLBACK(gtk_blist_menu_showlog_cb), node);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	gtk_widget_show_all(menu);
	
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 3, event->time);

	return FALSE;
}

static void gaim_gtk_blist_reordered_cb(GtkTreeModel *model,
					GtkTreePath *path,
					GtkTreeIter *iter,
					gint        *neworder,
					gpointer    null)
{
	debug_printf("This doesn't work because GTK is broken\n");

}

/* This is called 10 seconds after the buddy logs in.  It removes the "logged in" icon and replaces it with
 * the normal status icon */

static gboolean gaim_reset_present_icon(GaimBlistNode *b)
{
	((struct buddy*)b)->present = 1;
	gaim_gtk_blist_update(NULL, b);
	return FALSE;
}

static void gaim_gtk_blist_add_buddy_cb()
{
	GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(gtkblist->treeview));
	GtkTreeIter iter;
	GaimBlistNode *node;

	if(gtk_tree_selection_get_selected(sel, NULL, &iter)){
		gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &node, -1);
		if (GAIM_BLIST_NODE_IS_BUDDY(node)) 
			show_add_buddy(NULL, NULL, ((struct group*)node->parent)->name, NULL);
		else if (GAIM_BLIST_NODE_IS_GROUP(node))
			show_add_buddy(NULL, NULL, ((struct group*)node)->name, NULL);
	}
	else {
		show_add_buddy(NULL, NULL, NULL, NULL);
	}
}

static void gaim_gtk_blist_update_toolbar_icons (GtkWidget *widget, gpointer data) {
	if (GTK_IS_IMAGE(widget)) {
		if (blist_options & OPT_BLIST_SHOW_BUTTON_XPM)
			gtk_widget_show(widget);
		else
			gtk_widget_hide(widget);
	} else if (GTK_IS_CONTAINER(widget)) {
		gtk_container_foreach(GTK_CONTAINER(widget), gaim_gtk_blist_update_toolbar_icons, NULL);
	}
}

/***************************************************
 *            Crap                                 *
 ***************************************************/
static GtkItemFactoryEntry blist_menu[] =
{
	/* Buddies menu */
	{ N_("/_Buddies"), NULL, NULL, 0, "<Branch>" },
	{ N_("/Buddies/_Add A Buddy..."), "<CTL>B", gaim_gtk_blist_add_buddy_cb, 0,
	  "<StockItem>", GTK_STOCK_ADD },
	{ N_("/Buddies/New _Instant Message..."), "<CTL>I", show_im_dialog, 0,
	  "<StockItem>", GAIM_STOCK_IM },
	{ N_("/Buddies/Join a _Chat..."), "<CTL>C", join_chat, 0, 
	  "<StockItem>", GAIM_STOCK_CHAT },
	{ N_("/Buddies/sep1"), NULL, NULL, 0, "<Separator>" },
	{ N_("/Buddies/Get _User Info..."), "<CTL>J", show_info_dialog, 0,
	  "<StockItem>", GAIM_STOCK_INFO },
	{ N_("/Buddies/sep2"), NULL, NULL, 0, "<Separator>" },
	{ N_("/Buddies/_Signoff"), "<CTL>D", signoff_all, 0, NULL },
	{ N_("/Buddies/_Quit"), "<CTL>Q", do_quit, 0,
	  "<StockItem>", GTK_STOCK_QUIT },

	/* Tools */ 
	{ N_("/_Tools"), NULL, NULL, 0, "<Branch>" },
	{ N_("/Tools/_Away"), NULL, NULL, 0, "<Branch>" },
	{ N_("/Tools/Buddy _Pounce"), NULL, NULL, 0, "<Branch>" },
	{ N_("/Tools/sep1"), NULL, NULL, 0, "<Separator>" },
	{ N_("/Tools/A_ccounts"), "<CTL>A", account_editor, 0, NULL },
	{ N_("/Tools/Preferences"), "<CTL>P", show_prefs, 0,
	  "<StockItem>", GTK_STOCK_PREFERENCES },
	{ N_("/Tools/_File Transfers"), NULL, gaim_show_xfer_dialog, 0,
	  "<StockItem>", GTK_STOCK_REVERT_TO_SAVED },
	{ N_("/Tools/sep2"), NULL, NULL, 0, "<Separator>" },
	{ N_("/Tools/P_rotocol Actions"), NULL, NULL, 0, "<Branch>" },
	{ N_("/Tools/Pr_ivacy"), NULL, show_privacy_options, 0, NULL },
	{ N_("/Tools/View System _Log"), NULL, gtk_blist_show_systemlog_cb, 0, NULL },

	/* Help */
	{ N_("/_Help"), NULL, NULL, 0, "<Branch>" },
	{ N_("/Help/Online _Help"), "F1", NULL, 0,
	  "<StockItem>", GTK_STOCK_HELP },
	{ N_("/Help/_Debug Window"), NULL, show_debug, 0, NULL },
	{ N_("/Help/_About"), NULL, show_about, 0, NULL },

};

/*********************************************************
 * Private Utility functions                             *
 *********************************************************/

static GdkPixbuf *gaim_gtk_blist_get_status_icon(struct buddy *b) 
{
	GdkPixbuf *status = NULL;
	GdkPixbuf *scale = NULL;
	GdkPixbuf *emblem = NULL;
	gchar *filename = NULL;	
	const char *protoname = NULL;

	char *se = NULL, *sw = NULL ,*nw = NULL ,*ne = NULL;
	
	int scalesize = 30;

	struct prpl* prpl = find_prpl(b->account->protocol);
	if (prpl->list_icon)
		protoname = prpl->list_icon(b->account, b);
	if (prpl->list_emblems)
		prpl->list_emblems(b, &se, &sw, &nw, &ne);
	
	if (!(blist_options & OPT_BLIST_SHOW_ICONS)) {
		scalesize = 15;
		sw = nw = ne = NULL; /* So that only the se icon will composite */
	}


	if (b->present == 2) {
		struct gaim_gtk_blist_node *gtknode;
		/* If b->present is 2, that means this buddy has just signed on.  We use the "login" icon for the
		 * status, and we set a timeout to change it to a normal icon after 10 seconds. */
		filename = g_build_filename(DATADIR, "pixmaps", "gaim", "status", "default", "login.png", NULL);
		status = gdk_pixbuf_new_from_file(filename,NULL);
		g_free(filename);

		gtknode = GAIM_GTK_BLIST_NODE((GaimBlistNode*)b);
		gtknode->timer = g_timeout_add(10000, (GSourceFunc)gaim_reset_present_icon, b);

		/* "Hey, what's all this crap?" you ask.  Status icons will be themeable too, and 
		   then it will look up protoname from the theme */
	} else {
		char *image = g_strdup_printf("%s.png", protoname);
		filename = g_build_filename(DATADIR, "pixmaps", "gaim", "status", "default", image, NULL);
		status = gdk_pixbuf_new_from_file(filename,NULL);
		g_free(image);
		g_free(filename);

	}
	
	if (!status)
		return NULL;
	
	scale =  gdk_pixbuf_scale_simple(status, scalesize, scalesize, GDK_INTERP_BILINEAR);
	
	/* Emblems */
	
	/* Each protocol can specify up to four "emblems" to composite over the base icon.  "away", "busy", "mobile user"
	 * are all examples of states represented by emblems.  I'm not even really sure I like this yet. */
	
	/* XXX Clean this crap up, yo. */
	if (se) {
		char *image = g_strdup_printf("%s.png", se);
		filename = g_build_filename(DATADIR, "pixmaps", "gaim", "status", "default", image, NULL);
		g_free(image);
		emblem = gdk_pixbuf_new_from_file(filename,NULL);
		g_free(filename);
		if (emblem) {
			if (blist_options & OPT_BLIST_SHOW_ICONS)
				gdk_pixbuf_composite (emblem,
						      scale, 15, 15,
						      15, 15,
						      15, 15,
						      1, 1,
						      GDK_INTERP_BILINEAR,
						      255);
			else
					gdk_pixbuf_composite (emblem,
						      scale, 0, 0,
						      15, 15,
						      0, 0,
						      1, 1,
						      GDK_INTERP_BILINEAR,
						      255);
		}
	}
	if (sw) {
		char *image = g_strdup_printf("%s.png", sw);
		filename = g_build_filename(DATADIR, "pixmaps", "gaim", "status", "default", image, NULL);
		g_free(image);
		emblem = gdk_pixbuf_new_from_file(filename,NULL);
		g_free(filename);
		if (emblem) {
				gdk_pixbuf_composite (emblem,
						      scale, 0, 15,
						      15, 15,
						      0, 15,
						      1, 1,
						      GDK_INTERP_BILINEAR,
						      255);
		}
	}
	if (nw) {
		char *image = g_strdup_printf("%s.png", nw);
		filename = g_build_filename(DATADIR, "pixmaps", "gaim", "status", "default", image, NULL);
		g_free(image);
		emblem = gdk_pixbuf_new_from_file(filename,NULL);
		g_free(filename);
		if (emblem) {
			gdk_pixbuf_composite (emblem,
					      scale, 0, 0,
					      15, 15,
					      0, 0,
					      1, 1,
					      GDK_INTERP_BILINEAR,
					      255);
		}
	}
	if (ne) {
		char *image = g_strdup_printf("%s.png", ne);
		filename = g_build_filename(DATADIR, "pixmaps", "gaim", "status", "default", image, NULL);
		g_free(image);
		emblem = gdk_pixbuf_new_from_file(filename,NULL);
		g_free(filename);
		if (emblem) {
			gdk_pixbuf_composite (emblem,
					      scale, 15, 0,
					      15, 15,
					      15, 0,
					      1, 1,
					      GDK_INTERP_BILINEAR,
					      255);
		}
	}		

	
	/* Idle gray buddies affects the whole row.  This converts the status icon to greyscale. */
	if (b->idle && blist_options & OPT_BLIST_GREY_IDLERS)
		gdk_pixbuf_saturate_and_pixelate(scale, scale, 0, FALSE);
	return scale;
}

static GdkPixbuf *gaim_gtk_blist_get_buddy_icon(struct buddy *b) 
{
	/* This just opens a file from ~/.gaim/icons/screenname.  This needs to change to be more gooder. */
	char *file = g_build_filename(gaim_user_dir(), "icons", normalize(b->name), NULL);
	GdkPixbuf *buf = gdk_pixbuf_new_from_file(file, NULL);
	
	if (!(blist_options & OPT_BLIST_SHOW_ICONS))
		return NULL;
	
	if (buf) {
		if (b->idle && blist_options & OPT_BLIST_GREY_IDLERS) {
			gdk_pixbuf_saturate_and_pixelate(buf, buf, 0, FALSE);
		}
		return gdk_pixbuf_scale_simple(buf,30,30, GDK_INTERP_BILINEAR);
	}
	return NULL;
}

static gchar *gaim_gtk_blist_get_name_markup(struct buddy *b) 
{
	char *name = gaim_get_buddy_alias(b);
	char *esc = g_markup_escape_text(name, strlen(name)), *text = NULL;
	/* XXX Clean up this crap */

	int ihrs, imin;
	char *idletime = "";
	char *warning = idletime;
	time_t t;

	if (!(blist_options & OPT_BLIST_SHOW_ICONS)) {
		if (b->idle > 0 && blist_options & OPT_BLIST_GREY_IDLERS) {
			text =  g_strdup_printf("<span color='gray'>%s</span>",
						esc);
			g_free(esc);
			return text;
		} else {
			return esc;
		}
	}

	time(&t);
	ihrs = (t - b->idle) / 3600;
	imin = ((t - b->idle) / 60) % 60;

	if (b->idle) {
		if (ihrs)
			idletime = g_strdup_printf(_("Idle (%dh%02dm)"), ihrs, imin);
		else
			idletime = g_strdup_printf(_("Idle (%dm)"), imin);
	}

	if (b->evil > 0)
		warning = g_strdup_printf(_("Warned (%d%%)"), b->evil);

	if (b->idle && blist_options & OPT_BLIST_GREY_IDLERS)
		text =  g_strdup_printf("<span color='grey'>%s</span>\n<span color='gray' size='smaller'>%s %s</span>",
					esc,
					idletime, warning);
	else
		text = g_strdup_printf("%s\n<span color='gray' size='smaller'>%s %s</span>", esc, idletime, warning);

	if (idletime[0])
		g_free(idletime);
	if (warning[0])
		g_free(warning);

	return text;
}

/**********************************************************************************
 * Public API Functions                                                           *
 **********************************************************************************/
static void gaim_gtk_blist_new_list(struct gaim_buddy_list *blist)
{
	blist->ui_data = g_new0(struct gaim_gtk_buddy_list, 1);
}

static void gaim_gtk_blist_new_node(GaimBlistNode *node)
{
	node->ui_data = g_new0(struct gaim_gtk_blist_node, 1);
}

static void gaim_gtk_blist_show(struct gaim_buddy_list *list)
{
	GtkItemFactory *ift;
	GtkCellRenderer *rend;
	GtkTreeViewColumn *column;
	GtkWidget *sw;
	GtkWidget *button;
	GtkSizeGroup *sg;

	if (gtkblist) {
		gtk_widget_show(gtkblist->window);
		return;
	}

	gtkblist = GAIM_GTK_BLIST(list);

	gtkblist->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(gtkblist->window), _("Buddy List"));

	gtkblist->vbox = gtk_vbox_new(FALSE, 6);
	gtk_container_add(GTK_CONTAINER(gtkblist->window), gtkblist->vbox);

	g_signal_connect(G_OBJECT(gtkblist->window), "delete_event", G_CALLBACK(gaim_gtk_blist_destroy_cb), NULL);

	/******************************* Menu bar *************************************/
	ift = gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<GaimMain>", NULL);
	gtk_item_factory_create_items(ift, sizeof(blist_menu) / sizeof(*blist_menu),
				      blist_menu, NULL);
	gtk_box_pack_start(GTK_BOX(gtkblist->vbox), gtk_item_factory_get_widget(ift, "<GaimMain>"), FALSE, FALSE, 0);

	awaymenu = gtk_item_factory_get_widget(ift, "/Tools/Away");
	do_away_menu();

	bpmenu = gtk_item_factory_get_widget(ift, "/Tools/Buddy Pounce");
	do_bp_menu();

	protomenu = gtk_item_factory_get_widget(ift, "/Tools/Protocol Actions");
	do_proto_menu();

	/****************************** GtkTreeView **********************************/
	sw = gtk_scrolled_window_new(NULL,NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request(sw, 200, 200);

	gtkblist->treemodel = gtk_tree_store_new(BLIST_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING, 
						 G_TYPE_STRING, G_TYPE_STRING, GDK_TYPE_PIXBUF, G_TYPE_POINTER);
	/* This is broken because GTK is broken
 	g_signal_connect(G_OBJECT(gtkblist->treemodel), "row-reordered", gaim_gtk_blist_reordered_cb, NULL); */
 
	gtkblist->treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(gtkblist->treemodel));

	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(gtkblist->treeview), FALSE);

	rend = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes("Status", rend, "pixbuf", STATUS_ICON_COLUMN, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(gtkblist->treeview), column);
	
	rend = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Name", rend, "markup", NAME_COLUMN, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(gtkblist->treeview), column);
	
	rend = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Warning", rend, "text", WARNING_COLUMN, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(gtkblist->treeview), column);

	rend = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Idle", rend, "text", IDLE_COLUMN, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(gtkblist->treeview), column);

	rend = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes("Buddy Icon", rend, "pixbuf", BUDDY_ICON_COLUMN, NULL);
	g_object_set(rend, "xalign", 1.0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(gtkblist->treeview), column);
	
	g_signal_connect(G_OBJECT(gtkblist->treeview), "row-activated", G_CALLBACK(gtk_blist_row_activated_cb), NULL);
	g_signal_connect(G_OBJECT(gtkblist->treeview), "button-press-event", G_CALLBACK(gtk_blist_button_press_cb), NULL);

	gtk_box_pack_start(GTK_BOX(gtkblist->vbox), sw, TRUE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER(sw), gtkblist->treeview);
	
	/**************************** Button Box **************************************/

	sg = gtk_size_group_new(GTK_SIZE_GROUP_BOTH);
	gtkblist->bbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(gtkblist->vbox), gtkblist->bbox, FALSE, FALSE, 0);
	button = gaim_pixbuf_button_from_stock(_("IM"), GAIM_STOCK_IM, GAIM_BUTTON_VERTICAL);
	gtk_box_pack_start(GTK_BOX(gtkblist->bbox), button, FALSE, FALSE, 0);
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	gtk_size_group_add_widget(sg, button);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(gtk_blist_button_im_cb),
			 gtkblist->treeview);
	
	button = gaim_pixbuf_button_from_stock(_("Get Info"), GAIM_STOCK_INFO, GAIM_BUTTON_VERTICAL);
	gtk_box_pack_start(GTK_BOX(gtkblist->bbox), button, FALSE, FALSE, 0);
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	gtk_size_group_add_widget(sg, button);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(gtk_blist_button_info_cb),
			 gtkblist->treeview);
	
	button = gaim_pixbuf_button_from_stock(_("Chat"), GAIM_STOCK_CHAT, GAIM_BUTTON_VERTICAL);
	gtk_box_pack_start(GTK_BOX(gtkblist->bbox), button, FALSE, FALSE, 0);
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	gtk_size_group_add_widget(sg, button);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(gtk_blist_button_chat_cb), NULL);

	button = gaim_pixbuf_button_from_stock(_("Away"), GAIM_STOCK_AWAY, GAIM_BUTTON_VERTICAL);
	gtk_box_pack_start(GTK_BOX(gtkblist->bbox), button, FALSE, FALSE, 0);
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	gtk_size_group_add_widget(sg, button);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(gtk_blist_button_away_cb), NULL);

	/* OK... let's show this bad boy. */
	gaim_gtk_blist_refresh(list);
	gtk_widget_show_all(gtkblist->window);
	
	gaim_gtk_blist_update_toolbar();

}

void gaim_gtk_blist_refresh(struct gaim_buddy_list *list)
{
	GaimBlistNode *group = list->root;
	GaimBlistNode *buddy;

	while (group) {
		gaim_gtk_blist_update(list, group);
		buddy = group->child;
		while (buddy) {
			gaim_gtk_blist_update(list, buddy);
			buddy = buddy->next;
		}
		group = group->next;
	}
}

static gboolean get_iter_from_node_helper(GaimBlistNode *node, GtkTreeIter *iter, GtkTreeIter *root) {
	do {
		GaimBlistNode *n;
		GtkTreeIter child;

		gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), root, NODE_COLUMN, &n, -1);
		if(n == node) {
			*iter = *root;
			return TRUE;
		}

		if(gtk_tree_model_iter_children(GTK_TREE_MODEL(gtkblist->treemodel), &child, root)) {
			if(get_iter_from_node_helper(node,iter,&child))
				return TRUE;
		}
	} while(gtk_tree_model_iter_next(GTK_TREE_MODEL(gtkblist->treemodel), root));

	return FALSE;
}

static gboolean get_iter_from_node(GaimBlistNode *node, GtkTreeIter *iter) {
	GtkTreeIter root;

	if (!gtkblist)
		return FALSE;

	if(!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(gtkblist->treemodel), &root))
		return FALSE;

	return get_iter_from_node_helper(node, iter, &root);
}

void gaim_gtk_blist_update_toolbar() {
	if (!gtkblist)
		return;

	gtk_container_foreach(GTK_CONTAINER(gtkblist->bbox), gaim_gtk_blist_update_toolbar_icons, NULL);

	if (blist_options & OPT_BLIST_NO_BUTTONS)
		gtk_widget_hide(gtkblist->bbox);
	else
		gtk_widget_show_all(gtkblist->bbox);
}

static void gaim_gtk_blist_remove(struct gaim_buddy_list *list, GaimBlistNode *node)
{
	struct gaim_gtk_blist_node *gtknode;
	GtkTreeIter iter;

	if (!node->ui_data)
		return;

	gtknode = (struct gaim_gtk_blist_node *)node->ui_data;

	if (gtknode->timer > 0) {
		g_source_remove(gtknode->timer);
		gtknode->timer = 0;
	}

	if (get_iter_from_node(node, &iter)) {
		gtk_tree_store_remove(gtkblist->treemodel, &iter);
		if(GAIM_BLIST_NODE_IS_BUDDY(node) && gaim_blist_get_group_online_count((struct group *)node->parent) == 0) {
			GtkTreeIter groupiter;
			if(get_iter_from_node(node->parent, &groupiter))
				gtk_tree_store_remove(gtkblist->treemodel, &groupiter);
		}
	}
}


static void gaim_gtk_blist_update(struct gaim_buddy_list *list, GaimBlistNode *node)
{
	struct gaim_gtk_blist_node *gtknode;
	GtkTreeIter iter;
	gboolean expand = FALSE;
	gboolean new_entry = FALSE;

	if (!gtkblist)
		return;

	gtknode = GAIM_GTK_BLIST_NODE(node);


	if (!get_iter_from_node(node, &iter)) { /* This is a newly added node */
		new_entry = TRUE;
		if (GAIM_BLIST_NODE_IS_BUDDY(node)) {
			if (((struct buddy*)node)->present) {
				GtkTreeIter groupiter;
				GaimBlistNode *oldersibling;
				GtkTreeIter oldersiblingiter;

				if(node->parent && !get_iter_from_node(node->parent, &groupiter)) {
					/* This buddy's group has not yet been added.  We do that here */
					char *mark = g_strdup_printf("<span weight='bold'>%s</span>",  ((struct group*)node->parent)->name);
					oldersibling = node->parent->prev;

					/* We traverse backwards through the buddy list to find the node in the tree to insert it after */
					while (oldersibling && !get_iter_from_node(oldersibling, &oldersiblingiter))
						oldersibling = oldersibling->prev;

					/* This is where we create the node and add it. */
					gtk_tree_store_insert_after(gtkblist->treemodel, &groupiter, NULL, oldersibling ? &oldersiblingiter : NULL);
					gtk_tree_store_set(gtkblist->treemodel, &groupiter,
							   STATUS_ICON_COLUMN, gtk_widget_render_icon
							   (gtkblist->treeview,GTK_STOCK_OPEN,GTK_ICON_SIZE_SMALL_TOOLBAR,NULL),
							   NAME_COLUMN, mark,
							   NODE_COLUMN, node->parent,
							   -1);

					expand = TRUE;
				}

				oldersibling = node->prev;
				while (oldersibling && !get_iter_from_node(oldersibling, &oldersiblingiter))
					oldersibling = oldersibling->prev;

				gtk_tree_store_insert_after(gtkblist->treemodel, &iter, &groupiter, oldersibling ? &oldersiblingiter : NULL);

				if (expand) {       /* expand was set to true if this is the first element added to a group.  In such case
						     * we expand the group node */
					GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), &groupiter);
					gtk_tree_view_expand_row(GTK_TREE_VIEW(gtkblist->treeview), path, TRUE);
				}
			}
		}
	}

	if (GAIM_BLIST_NODE_IS_BUDDY(node) && ((struct buddy*)node)->present) {
		GdkPixbuf *status, *avatar;
		char *mark;
		char *warning = NULL, *idle = NULL;

		status = gaim_gtk_blist_get_status_icon((struct buddy*)node);
		avatar = gaim_gtk_blist_get_buddy_icon((struct buddy*)node);
		mark   = gaim_gtk_blist_get_name_markup((struct buddy*)node);

		if ((((struct buddy*)node)->idle > 0) &&
		    (!(blist_options & OPT_BLIST_SHOW_ICONS) && (blist_options & OPT_BLIST_SHOW_IDLETIME))) {
			time_t t;
			int ihrs, imin;
			time(&t);
			ihrs = (t - ((struct buddy *)node)->idle) / 3600;
			imin = ((t - ((struct buddy*)node)->idle) / 60) % 60;
			idle = g_strdup_printf("%d:%02d", ihrs, imin);
		}

		if ((((struct buddy*)node)->evil > 0) &&
		    (!(blist_options & OPT_BLIST_SHOW_ICONS) && (blist_options & OPT_BLIST_SHOW_WARN))) {
			warning = g_strdup_printf("%d%%", ((struct buddy*)node)->evil);
		}

		gtk_tree_store_set(gtkblist->treemodel, &iter,
				   STATUS_ICON_COLUMN, status,
				   NAME_COLUMN, mark,
				   WARNING_COLUMN, warning,
				   IDLE_COLUMN, idle,
				   BUDDY_ICON_COLUMN, avatar,
				   NODE_COLUMN, node,
				   -1);

		g_free(mark);
		if (idle)
			g_free(idle);
		if (warning)
			g_free(warning);

		if (status != NULL)
			g_object_unref(status);

		if (avatar != NULL)
			g_object_unref(avatar);

	} else if (GAIM_BLIST_NODE_IS_BUDDY(node) && !new_entry) {
		gaim_gtk_blist_remove(list, node);
	}
}

static void gaim_gtk_blist_destroy(struct gaim_buddy_list *list)
{
	gtk_widget_destroy(gtkblist->window);
}

static void gaim_gtk_blist_set_visible(struct gaim_buddy_list *list, gboolean show)
{
	if (show) {
		gtk_window_present(GTK_WINDOW(gtkblist->window));
	} else {
		if (!connections || docklet_count) {
#ifdef _WIN32
			wgaim_systray_minimize(blist);
#endif
			gtk_widget_hide(gtkblist->window);
		} else {
			gtk_window_iconify(GTK_WINDOW(gtkblist->window));
		}
	}
}

void gaim_gtk_blist_docklet_toggle() {
	/* Useful for the docklet plugin and also for the win32 tray icon*/
	/* This is called when one of those is clicked--it will show/hide the 
	   buddy list/login window--depending on which is active */
	if (connections && gtkblist) {
		if (GTK_WIDGET_VISIBLE(gtkblist->window)) {
			gaim_blist_set_visible(GAIM_WINDOW_ICONIFIED(gtkblist->window) || gaim_gtk_blist_obscured);
		} else {
#if _WIN32
			wgaim_systray_maximize(blist);
#endif
			gaim_blist_set_visible(TRUE);
		}
	} else if (connections) {
		/* we're logging in or something... do nothing */
		debug_printf("docklet_toggle called with connections but no blist!\n");
	} else {
		if (GTK_WIDGET_VISIBLE(mainwindow)) {
			if (GAIM_WINDOW_ICONIFIED(mainwindow)) {
				gtk_window_present(GTK_WINDOW(mainwindow));
			} else {
#if _WIN32
				wgaim_systray_minimize(mainwindow);
#endif
				gtk_widget_hide(mainwindow);
			}
		} else {
#if _WIN32
			wgaim_systray_maximize(mainwindow);
#endif
			gtk_window_present(GTK_WINDOW(mainwindow));
		}
	}
}

void gaim_gtk_blist_docklet_add()
{
	docklet_count++;
}

void gaim_gtk_blist_docklet_remove()
{
	docklet_count--;
	if (!docklet_count) {
		if (connections) {
			gaim_blist_set_visible(TRUE);
		} else {
			gtk_window_present(GTK_WINDOW(gtkblist->window));
		}
	}
}

static struct gaim_blist_ui_ops blist_ui_ops =
{
	gaim_gtk_blist_new_list,
	gaim_gtk_blist_new_node,
	gaim_gtk_blist_show,
	gaim_gtk_blist_update,
	gaim_gtk_blist_remove,
	gaim_gtk_blist_destroy,
	gaim_gtk_blist_set_visible
};


struct gaim_blist_ui_ops *gaim_get_gtk_blist_ui_ops()
{
	return &blist_ui_ops;
}



/*********************************************************************
 * Public utility functions                                          *
 *********************************************************************/

GdkPixbuf *
create_prpl_icon(struct gaim_account *account)
{
	struct prpl *prpl = find_prpl(account->protocol);
	GdkPixbuf *status = NULL;
	char *filename = NULL;
	const char *protoname = prpl->list_icon(account, NULL);
	/* "Hey, what's all this crap?" you ask.  Status icons will be themeable too, and 
	   then it will look up protoname from the theme */
	if (!strcmp(protoname, "aim")) {
		filename = g_build_filename(DATADIR, "pixmaps", "gaim", "status", "default", "aim.png", NULL);
		status = gdk_pixbuf_new_from_file(filename,NULL);
		g_free(filename);
	} else if (!strcmp(protoname, "yahoo")) {
		filename = g_build_filename(DATADIR, "pixmaps", "gaim", "status", "default", "yahoo.png", NULL);
		status = gdk_pixbuf_new_from_file(filename,NULL);
		g_free(filename);
	} else if (!strcmp(protoname, "msn")) {
		filename = g_build_filename(DATADIR, "pixmaps", "gaim", "status", "default", "msn.png", NULL);
		status = gdk_pixbuf_new_from_file(filename,NULL);
		g_free(filename);
	} else if (!strcmp(protoname, "jabber")) {
		filename = g_build_filename(DATADIR, "pixmaps", "gaim", "status", "default", "jabber.png", NULL);
		status = gdk_pixbuf_new_from_file(filename,NULL);
		g_free(filename);
	} else if (!strcmp(protoname, "icq")) {
		filename = g_build_filename(DATADIR, "pixmaps", "gaim", "status", "default", "icq.png", NULL);
		status = gdk_pixbuf_new_from_file(filename,NULL);
		g_free(filename);
	} else if (!strcmp(protoname, "gadu-gadu")) {
		filename = g_build_filename(DATADIR, "pixmaps", "gaim", "status", "default", "gadugadu.png", NULL);
		status = gdk_pixbuf_new_from_file(filename,NULL);
		g_free(filename);
	} else if (!strcmp(protoname, "napster")) {
		filename = g_build_filename(DATADIR, "pixmaps", "gaim", "status", "default", "napster.png", NULL);
		status = gdk_pixbuf_new_from_file(filename,NULL);
		g_free(filename);
	} else if (!strcmp(protoname, "irc")) {
		filename = g_build_filename(DATADIR, "pixmaps", "gaim", "status", "default", "irc.png", NULL);
		status = gdk_pixbuf_new_from_file(filename,NULL);
		g_free(filename);
	}
	return status;
}

