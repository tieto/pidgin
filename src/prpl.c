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

#include "gaim.h"
#include "gtkutils.h"
#include "gtkblist.h"
#include "prpl.h"
#include "notify.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
#include "win32dep.h"
#endif

GtkWidget *protomenu = NULL;

struct _prompt {
	GtkWidget *window;
	GtkWidget *entry;
	void (*doit)(void *, const char *);
	void (*dont)(void *);
	void *data;
};

GaimPlugin *
gaim_find_prpl(GaimProtocol type)
{
	GList *l;
	GaimPlugin *plugin;

	for (l = gaim_plugins_get_protocols(); l != NULL; l = l->next) {
		plugin = (GaimPlugin *)l->data;

		/* Just In Case (TM) */
		if (GAIM_IS_PROTOCOL_PLUGIN(plugin)) {

			if (GAIM_PLUGIN_PROTOCOL_INFO(plugin)->protocol == type)
				return plugin;
		}
	}

	return NULL;
}

static void proto_act(GtkObject *obj, struct proto_actions_menu *pam)
{
	if (pam->callback && pam->gc)
		pam->callback(pam->gc);
}

void do_proto_menu()
{
	GtkWidget *menuitem;
	GtkWidget *submenu;
	GaimPluginProtocolInfo *prpl_info = NULL;
	GList *l;
	GList *c;
	struct proto_actions_menu *pam;
	GaimConnection *gc = NULL;
	int count = 0;
	char buf[256];

	if (!protomenu)
		return;

	l = gtk_container_get_children(GTK_CONTAINER(protomenu));
	while (l) {
		menuitem = l->data;
		pam = g_object_get_data(G_OBJECT(menuitem), "proto_actions_menu");
		if (pam)
			g_free(pam);
		gtk_container_remove(GTK_CONTAINER(protomenu), GTK_WIDGET(menuitem));
		l = l->next;
	}

	for (c = gaim_connections_get_all(); c != NULL; c = c->next) {
		gc = c->data;

		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

		if (prpl_info->actions && gc->login_time)
			count++;
	}

	if (!count) {
		g_snprintf(buf, sizeof(buf), _("No actions available"));
		menuitem = gtk_menu_item_new_with_label(buf);
		gtk_menu_shell_append(GTK_MENU_SHELL(protomenu), menuitem);
		gtk_widget_show(menuitem);
		return;
	}

	if (count == 1) {
		GList *act;

		for (c = gaim_connections_get_all(); c != NULL; c = c->next) {
			gc = c->data;

			prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

			if (prpl_info->actions && gc->login_time)
				break;
		}

		act = prpl_info->actions(gc);

		while (act) {
			if (act->data) {
				struct proto_actions_menu *pam = act->data;
				menuitem = gtk_menu_item_new_with_label(pam->label);
				gtk_menu_shell_append(GTK_MENU_SHELL(protomenu), menuitem);
				g_signal_connect(G_OBJECT(menuitem), "activate",
								 G_CALLBACK(proto_act), pam);
				g_object_set_data(G_OBJECT(menuitem), "proto_actions_menu", pam);
				gtk_widget_show(menuitem);
			} else {
				gaim_separator(protomenu);
			}
			act = g_list_next(act);
		}
	} else {
		for (c = gaim_connections_get_all(); c != NULL; c = c->next) {
			GaimAccount *account;
			GList *act;
			GdkPixbuf *pixbuf, *scale;
			GtkWidget *image;

			gc = c->data;

			prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

			if (!prpl_info->actions || !gc->login_time)
				continue;

			account = gaim_connection_get_account(gc);

			g_snprintf(buf, sizeof(buf), "%s (%s)",
					   gaim_account_get_username(account),
					   gc->prpl->info->name);

			menuitem = gtk_image_menu_item_new_with_label(buf);

			pixbuf = create_prpl_icon(gc->account);
			if(pixbuf) {
				scale = gdk_pixbuf_scale_simple(pixbuf, 16, 16,
						GDK_INTERP_BILINEAR);
				image = gtk_image_new_from_pixbuf(scale);
				g_object_unref(G_OBJECT(pixbuf));
				g_object_unref(G_OBJECT(scale));
				gtk_widget_show(image);
				gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem), image);
			}

			gtk_menu_shell_append(GTK_MENU_SHELL(protomenu), menuitem);
			gtk_widget_show(menuitem);

			submenu = gtk_menu_new();
			gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);
			gtk_widget_show(submenu);

			act = prpl_info->actions(gc);

			while (act) {
				if (act->data) {
					struct proto_actions_menu *pam = act->data;
					menuitem = gtk_menu_item_new_with_label(pam->label);
					gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
					g_signal_connect(G_OBJECT(menuitem), "activate",
									 G_CALLBACK(proto_act), pam);
					g_object_set_data(G_OBJECT(menuitem), "proto_actions_menu",
							pam);
					gtk_widget_show(menuitem);
				} else {
					gaim_separator(submenu);
				}
				act = g_list_next(act);
			}
		}
	}
}

struct icon_data {
	GaimConnection *gc;
	char *who;
	void *data;
	int len;
};

static GList *icons = NULL;

static gint find_icon_data(gconstpointer a, gconstpointer b)
{
	const struct icon_data *x = a;
	const struct icon_data *y = b;

	return ((x->gc != y->gc) || gaim_utf8_strcasecmp(x->who, y->who));
}

void set_icon_data(GaimConnection *gc, const char *who, void *data, int len)
{
	GaimConversation *conv;
	struct icon_data tmp;
	GList *l;
	struct icon_data *id;
	struct buddy *b;
	/* i'm going to vent here a little bit about normalize().  normalize()
	 * uses a static buffer, so when we call functions that use normalize() from
	 * functions that use normalize(), whose parameters are the result of running
	 * normalize(), bad things happen.  To prevent some of this, we're going
	 * to make a copy of what we get from normalize(), so we know nothing else
	 * touches it, and buddy icons don't go to the wrong person.  Some day I
	 * will kill normalize(), and dance on its grave.  That will be a very happy
	 * day for everyone.
	 *                                    --ndw
	 */
	char *realwho = g_strdup(normalize(who));
	tmp.gc = gc;
	tmp.who = realwho;
	tmp.data=NULL;
	tmp.len = 0;
	l = g_list_find_custom(icons, &tmp, find_icon_data);
	id = l ? l->data : NULL;

	if (id) {
		g_free(id->data);
		if (!data) {
			icons = g_list_remove(icons, id);
			g_free(id->who);
			g_free(id);
			g_free(realwho);
			return;
		}
	} else if (data) {
		id = g_new0(struct icon_data, 1);
		icons = g_list_append(icons, id);
		id->gc = gc;
		id->who = g_strdup(realwho);
	} else {
		g_free(realwho);
		return;
	}

	gaim_debug(GAIM_DEBUG_MISC, "prpl", "Got icon for %s (length %d)\n",
			   realwho, len);

	id->data = g_memdup(data, len);
	id->len = len;

	/* Update the buddy icon for this user. */
	conv = gaim_find_conversation(realwho);

	/* XXX Buddy Icon should probalby be part of struct buddy instead of this weird global
	 * linked list stuff. */

	if ((b = gaim_find_buddy(gc->account, realwho)) != NULL) {
		char *random = g_strdup_printf("%x", g_random_int());
		char *filename = g_build_filename(gaim_user_dir(), "icons", random,
				NULL);
		char *dirname = g_build_filename(gaim_user_dir(), "icons", NULL);
		char *old_icon = gaim_buddy_get_setting(b, "buddy_icon");
		FILE *file = NULL;

		g_free(random);

		if(!g_file_test(dirname, G_FILE_TEST_IS_DIR)) {
			gaim_debug(GAIM_DEBUG_INFO, "buddy icons",
					   "Creating icon cache directory.\n");

			if(mkdir(dirname, S_IRUSR | S_IWUSR | S_IXUSR) < 0)
				gaim_debug(GAIM_DEBUG_ERROR, "buddy icons",
						   "Unable to create directory %s: %s\n",
						   dirname, strerror(errno));
		}

		g_free(dirname);

		file = fopen(filename, "wb");
		if (file) {
			fwrite(data, 1, len, file);
			fclose(file);
		}

		if(old_icon) {
			unlink(old_icon);
			g_free(old_icon);
		}

		gaim_buddy_set_setting(b, "buddy_icon", filename);
		gaim_blist_save();

		g_free(filename);

		gaim_blist_update_buddy_icon(b);
	}

	if (conv != NULL && gaim_conversation_get_gc(conv) == gc)
		gaim_gtkconv_update_buddy_icon(conv);

	g_free(realwho);
}

void remove_icon_data(GaimConnection *gc)
{
	GList *list = icons;
	struct icon_data *id;

	while (list) {
		id = list->data;
		if (id->gc == gc) {
			g_free(id->data);
			g_free(id->who);
			list = icons = g_list_remove(icons, id);
			g_free(id);
		} else
			list = list->next;
	}
}

void *get_icon_data(GaimConnection *gc, const char *who, int *len)
{
	struct icon_data tmp = { gc, normalize(who), NULL, 0 };
	GList *l = g_list_find_custom(icons, &tmp, find_icon_data);
	struct icon_data *id = l ? l->data : NULL;

	if (id) {
		*len = id->len;
		return id->data;
	}

	*len = 0;
	return NULL;
}

struct got_add {
	GaimConnection *gc;
	char *who;
	char *alias;
};

static void dont_add(struct got_add *ga)
{
	g_free(ga->who);
	if (ga->alias)
		g_free(ga->alias);
	g_free(ga);
}

static void do_add(struct got_add *ga)
{
	if (g_list_find(gaim_connections_get_all(), ga->gc))
		show_add_buddy(ga->gc, ga->who, NULL, ga->alias);
	dont_add(ga);
}

void show_got_added(GaimConnection *gc, const char *id,
		    const char *who, const char *alias, const char *msg)
{
	GaimAccount *account;
	char buf[BUF_LONG];
	struct got_add *ga;
	struct buddy *b;

	account = gaim_connection_get_account(gc);
	b = gaim_find_buddy(gc->account, who);

	ga = g_new0(struct got_add, 1);
	ga->gc    = gc;
	ga->who   = g_strdup(who);
	ga->alias = (alias ? g_strdup(alias) : NULL);


	g_snprintf(buf, sizeof(buf), _("%s%s%s%s has made %s his or her buddy%s%s%s"),
		   who,
		   alias ? " (" : "",
		   alias ? alias : "",
		   alias ? ")" : "",
		   (id
			? id
			: (gaim_connection_get_display_name(gc)
			   ? gaim_connection_get_display_name(gc)
			   : gaim_account_get_username(account))),
		   msg ? ": " : ".",
		   msg ? msg : "",
		   b ? "" : _("\n\nDo you wish to add him or her to your buddy list?"));

	if (b) {
		gaim_notify_info(NULL, NULL, _("Gaim - Information"), buf);
	}
	else
		gaim_request_action(NULL, NULL, _("Add buddy to your list?"), buf,
							0, ga, 2,
							_("Add"), G_CALLBACK(do_add),
							_("Cancel"), G_CALLBACK(dont_add));
}

static GtkWidget *regdlg = NULL;
static GtkWidget *reg_list = NULL;
static GtkWidget *reg_area = NULL;
static GtkWidget *reg_reg = NULL;

static void delete_regdlg()
{
	GtkWidget *tmp = regdlg;
	regdlg = NULL;
	if (tmp)
		gtk_widget_destroy(tmp);
}

static void reset_reg_dlg()
{
	GList *P = gaim_plugins_get_protocols();

	if (!regdlg)
		return;

	while (GTK_BOX(reg_list)->children)
		gtk_container_remove(GTK_CONTAINER(reg_list),
				     ((GtkBoxChild *)GTK_BOX(reg_list)->children->data)->widget);

	while (GTK_BOX(reg_area)->children)
		gtk_container_remove(GTK_CONTAINER(reg_area),
				     ((GtkBoxChild *)GTK_BOX(reg_area)->children->data)->widget);

	while (P) {
		GaimPlugin *p = P->data;

		if (GAIM_PLUGIN_PROTOCOL_INFO(p)->register_user)
			break;

		P = P->next;
	}

	if (!P) {
		GtkWidget *no = gtk_label_new(_("You do not currently have any protocols available"
						" that are able to register new accounts."));
		gtk_box_pack_start(GTK_BOX(reg_area), no, FALSE, FALSE, 5);
		gtk_widget_show(no);

		gtk_widget_set_sensitive(reg_reg, FALSE);

		return;
	}

	gtk_widget_set_sensitive(reg_reg, TRUE);

	while (P) {	/* we can safely ignore all the previous ones */
		GaimPlugin *p = P->data;
		P = P->next;

		if (GAIM_PLUGIN_PROTOCOL_INFO(p)->register_user)
			continue;

		/* do stuff */
	}
}

void register_dialog()
{
	/* this is just one big hack */
	GtkWidget *vbox;
	GtkWidget *frame;
	GtkWidget *hbox;
	GtkWidget *close;

	if (regdlg) {
		gdk_window_raise(regdlg->window);
		return;
	}

	regdlg = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(regdlg), _("Gaim - Registration"));
	gtk_window_set_role(GTK_WINDOW(regdlg), "register");
	gtk_widget_realize(regdlg);
	g_signal_connect(G_OBJECT(regdlg), "destroy",
					 G_CALLBACK(delete_regdlg), NULL);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(regdlg), vbox);

	reg_list = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), reg_list, FALSE, FALSE, 5);

	frame = gtk_frame_new(_("Registration Information"));
	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 5);

	reg_area = gtk_hbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), reg_area);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

	close = gaim_pixbuf_button_from_stock(_("Close"), GTK_STOCK_CLOSE, GAIM_BUTTON_HORIZONTAL);
	gtk_box_pack_end(GTK_BOX(hbox), close, FALSE, FALSE, 5);
	g_signal_connect(G_OBJECT(close), "clicked",
					 G_CALLBACK(delete_regdlg), NULL);

	reg_reg = gaim_pixbuf_button_from_stock(_("Register"), GTK_STOCK_JUMP_TO, GAIM_BUTTON_HORIZONTAL);
	gtk_box_pack_end(GTK_BOX(hbox), reg_reg, FALSE, FALSE, 5);

	/* fuck me */
	reset_reg_dlg();

	gtk_widget_show_all(regdlg);
}

