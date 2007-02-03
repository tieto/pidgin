/*
 * Evolution integration plugin for Gaim
 *
 * Copyright (C) 2003 Christian Hammond.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */
#include "internal.h"
#include "pidgin.h"

#include "connection.h"
#include "debug.h"
#include "prefs.h"
#include "notify.h"
#include "signals.h"
#include "util.h"
#include "version.h"

#include "gtkblist.h"
#include "gtkconv.h"
#include "gtkplugin.h"
#include "gtkutils.h"

#include "gevolution.h"

#include <libedata-book/Evolution-DataServer-Addressbook.h>

#include <libebook/e-book-listener.h>
#include <libedata-book/e-data-book-factory.h>
#include <bonobo/bonobo-main.h>

#include <glib.h>

#define GEVOLUTION_PLUGIN_ID "gtk-x11-gevolution"

#define E_DATA_BOOK_FACTORY_OAF_ID \
	"OAFIID:GNOME_Evolution_DataServer_BookFactory"

enum
{
	COLUMN_AUTOADD,
	COLUMN_ICON,
	COLUMN_SCREENNAME,
	COLUMN_DATA,
	NUM_COLUMNS
};

static GaimBlistUiOps *backup_blist_ui_ops = NULL;
static GaimBlistUiOps *blist_ui_ops = NULL;
static EBook *book = NULL;
static gulong timer = 0;
static gulong book_view_tag = 0;
static EBookView *book_view = NULL;

static void
update_ims_from_contact(EContact *contact, const char *name,
						const char *prpl_id, EContactField field)
{
	GList *ims = e_contact_get(contact, field);
	GList *l, *l2;

	if (ims == NULL)
		return;

	for (l = gaim_connections_get_all(); l != NULL; l = l->next)
	{
		GaimConnection *gc = (GaimConnection *)l->data;
		GaimAccount *account = gaim_connection_get_account(gc);
		char *me;

		if (strcmp(gaim_account_get_protocol_id(account), prpl_id))
			continue;

		if (!gaim_account_get_bool(account, "gevo-autoadd", FALSE))
			continue;

		me = g_strdup(gaim_normalize(account, gaim_account_get_username(account)));
		for (l2 = ims; l2 != NULL; l2 = l2->next)
		{
			if (gaim_find_buddy(account, l2->data) != NULL ||
				!strcmp(me, gaim_normalize(account, l2->data)))
				continue;

			gevo_add_buddy(account, _("Buddies"), l2->data, name);
		}
		g_free(me);
	}

	g_list_foreach(ims, (GFunc)g_free, NULL);
	g_list_free(ims);
}

static void
update_buddies_from_contact(EContact *contact)
{
	const char *name;

	name = e_contact_get_const(contact, E_CONTACT_FULL_NAME);

	update_ims_from_contact(contact, name, "prpl-oscar",  E_CONTACT_IM_AIM);
	update_ims_from_contact(contact, name, "prpl-jabber", E_CONTACT_IM_JABBER);
	update_ims_from_contact(contact, name, "prpl-yahoo",  E_CONTACT_IM_YAHOO);
	update_ims_from_contact(contact, name, "prpl-msn",    E_CONTACT_IM_MSN);
	update_ims_from_contact(contact, name, "prpl-oscar",  E_CONTACT_IM_ICQ);
	update_ims_from_contact(contact, name, "prpl-novell", E_CONTACT_IM_GROUPWISE);
}

static void
contacts_changed_cb(EBookView *book_view, const GList *contacts)
{
	const GList *l;

	if (gaim_connections_get_all() == NULL)
		return;

	for (l = contacts; l != NULL; l = l->next)
	{
		EContact *contact = (EContact *)l->data;

		update_buddies_from_contact(contact);
	}
}

static void
request_add_buddy(GaimAccount *account, const char *username,
				  const char *group, const char *alias)
{
	if (book == NULL)
	{
		backup_blist_ui_ops->request_add_buddy(account, username, group,
											   alias);
	}
	else
	{
		gevo_add_buddy_dialog_show(account, username, group, alias);
	}
}

static void
got_book_view_cb(EBook *book, EBookStatus status, EBookView *view,
				 gpointer user_data)
{
	book_view_tag = 0;

	if (status != E_BOOK_ERROR_OK)
	{
		gaim_debug_error("evolution", "Unable to retrieve book view! :(\n");

		return;
	}

	book_view = view;

	g_object_ref(book_view);

	g_signal_connect(G_OBJECT(book_view), "contacts_changed",
					 G_CALLBACK(contacts_changed_cb), book);

	g_signal_connect(G_OBJECT(book_view), "contacts_added",
					 G_CALLBACK(contacts_changed_cb), book);

	e_book_view_start(view);
}

static void
signed_on_cb(GaimConnection *gc)
{
	EBookQuery *query;
	gboolean status;
	GList *contacts;
	GList *l;

	if (book == NULL)
		return;

	query = e_book_query_any_field_contains("");

	status = e_book_get_contacts(book, query, &contacts, NULL);

	e_book_query_unref(query);

	if (!status)
		return;

	for (l = contacts; l != NULL; l = l->next)
	{
		EContact *contact = E_CONTACT(l->data);

		update_buddies_from_contact(contact);

		g_object_unref(contact);
	}

	g_list_free(contacts);
}

static void
menu_item_activate_cb(GaimBlistNode *node, gpointer user_data)
{
	GaimBuddy *buddy = (GaimBuddy *)node;
	gevo_associate_buddy_dialog_new(buddy);
}

static void
menu_item_send_mail_activate_cb(GaimBlistNode *node, gpointer user_data)
{
	GaimBuddy *buddy = (GaimBuddy *)node;
	char *mail = NULL;

	mail = gevo_get_email_for_buddy(buddy);

	if (mail != NULL)
	{
		char *app = g_find_program_in_path("evolution");
		if (app != NULL)
		{
			char *command_line = g_strdup_printf("%s mailto:%s", app, mail);
			g_free(app);
			g_free(mail);

			g_spawn_command_line_async(command_line, NULL);
			g_free(command_line);
		}
		else
		{
			gaim_notify_error(NULL, NULL, _("Unable to send e-mail"),
							  _("The evolution executable was not found in the PATH."));
		}
	}
	else
	{
		gaim_notify_error(NULL, NULL, _("Unable to send e-mail"),
						  _("An e-mail address was not found for this buddy."));
	}
}

static void
blist_node_extended_menu_cb(GaimBlistNode *node, GList **menu)
{
	GaimMenuAction *act;
	GaimBuddy *buddy;
	GaimAccount *account;
	EContact *contact;
	char *mail;

	if (!GAIM_BLIST_NODE_IS_BUDDY(node))
		return;

	buddy = (GaimBuddy *)node;
	account = gaim_buddy_get_account(buddy);

	if (!gevo_prpl_is_supported(account, buddy))
		return;

	contact = gevo_search_buddy_in_contacts(buddy, NULL);

	if (contact == NULL)
	{
		act = gaim_menu_action_new(_("Add to Address Book"),
		                           GAIM_CALLBACK(menu_item_activate_cb),
		                           NULL, NULL);
		*menu = g_list_append(*menu, act);
	}
	else
		g_object_unref(contact);

	mail = gevo_get_email_for_buddy(buddy);

	if (mail != NULL)
	{
		act = gaim_menu_action_new(_("Send E-Mail"),
			GAIM_CALLBACK(menu_item_send_mail_activate_cb), NULL, NULL);
		*menu = g_list_append(*menu, act);
		g_free(mail);
	}
}

/* TODO: Something in here leaks 1 reference to a bonobo object! */
static gboolean
load_timeout(gpointer data)
{
	GaimPlugin *plugin = (GaimPlugin *)data;
	EBookQuery *query;

	timer = 0;

	/* Maybe this is it? */
	if (!gevo_load_addressbook(NULL, &book, NULL))
		return FALSE;

	query = e_book_query_any_field_contains("");

	/* Is it this? */
	book_view_tag = e_book_async_get_book_view(book, query, NULL, -1,
											   got_book_view_cb, NULL);

	e_book_query_unref(query);

	gaim_signal_connect(gaim_blist_get_handle(), "blist-node-extended-menu",
						plugin, GAIM_CALLBACK(blist_node_extended_menu_cb), NULL);

	return FALSE;
}

static gboolean
plugin_load(GaimPlugin *plugin)
{
	bonobo_activate();

	backup_blist_ui_ops = gaim_blist_get_ui_ops();

	blist_ui_ops = g_memdup(backup_blist_ui_ops, sizeof(GaimBlistUiOps));
	blist_ui_ops->request_add_buddy = request_add_buddy;

	gaim_blist_set_ui_ops(blist_ui_ops);

	gaim_signal_connect(gaim_connections_get_handle(), "signed-on",
						plugin, GAIM_CALLBACK(signed_on_cb), NULL);

	timer = g_timeout_add(1, load_timeout, plugin);

	return TRUE;
}

static gboolean
plugin_unload(GaimPlugin *plugin)
{
	gaim_blist_set_ui_ops(backup_blist_ui_ops);

	g_free(blist_ui_ops);

	backup_blist_ui_ops = NULL;
	blist_ui_ops = NULL;

	if (book_view != NULL)
	{
		e_book_view_stop(book_view);
		g_object_unref(book_view);
		book_view = NULL;
	}

	if (book != NULL)
	{
		g_object_unref(book);
		book = NULL;
	}

	return TRUE;
}

static void
plugin_destroy(GaimPlugin *plugin)
{
	bonobo_debug_shutdown();
}

static void
autoadd_toggled_cb(GtkCellRendererToggle *renderer, gchar *path_str,
				   gpointer data)
{
	GaimAccount *account;
	GtkTreeModel *model = (GtkTreeModel *)data;
	GtkTreeIter iter;
	gboolean autoadd;

	gtk_tree_model_get_iter_from_string(model, &iter, path_str);
	gtk_tree_model_get(model, &iter,
					   COLUMN_DATA, &account,
					   COLUMN_AUTOADD, &autoadd,
					   -1);

	gaim_account_set_bool(account, "gevo-autoadd", !autoadd);

	gtk_list_store_set(GTK_LIST_STORE(model), &iter,
					   COLUMN_AUTOADD, !autoadd,
					   -1);
}

static GtkWidget *
get_config_frame(GaimPlugin *plugin)
{
	GtkWidget *ret;
	GtkWidget *vbox;
	GtkWidget *label;
	GtkWidget *sw;
	GtkWidget *treeview;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GdkPixbuf *pixbuf;
	GtkListStore *model;
	GList *l;

	/* Outside container */
	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width(GTK_CONTAINER(ret), 12);

	/* Configuration frame */
	vbox = pidgin_make_frame(ret, _("Evolution Integration Configuration"));

	/* Label */
	label = gtk_label_new(_("Select all accounts that buddies should be "
							"auto-added to."));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	/* Scrolled window */
	sw = gtk_scrolled_window_new(0, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
								   GTK_POLICY_AUTOMATIC,
								   GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw),
										GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
	gtk_widget_set_size_request(sw, 300, 300);
	gtk_widget_show(sw);

	/* Create the list model for the treeview. */
	model = gtk_list_store_new(NUM_COLUMNS,
							   G_TYPE_BOOLEAN, GDK_TYPE_PIXBUF,
							   G_TYPE_STRING, G_TYPE_POINTER);

	/* Setup the treeview */
	treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(model));
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeview), TRUE);
	gtk_container_add(GTK_CONTAINER(sw), treeview);
	gtk_widget_show(treeview);

	/* Setup the column */
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Account"));
	gtk_tree_view_insert_column(GTK_TREE_VIEW(treeview), column, -1);

	/* Checkbox */
	renderer = gtk_cell_renderer_toggle_new();

	g_signal_connect(G_OBJECT(renderer), "toggled",
					 G_CALLBACK(autoadd_toggled_cb), model);

	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_add_attribute(column, renderer,
									   "active", COLUMN_AUTOADD);

	/* Icon */
	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_add_attribute(column, renderer,
									   "pixbuf", COLUMN_ICON);

	/* Screenname */
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer,
									   "text", COLUMN_SCREENNAME);


	/* Populate */
	for (l = gaim_accounts_get_all(); l != NULL; l = l->next)
	{
		GaimAccount *account = (GaimAccount *)l->data;
		GtkTreeIter iter;

		gaim_debug_info("evolution", "Adding account\n");

		gtk_list_store_append(model, &iter);

		pixbuf = pidgin_create_prpl_icon(account, 0.5);
		if ((pixbuf != NULL) && (!gaim_account_is_connected(account)))
			gdk_pixbuf_saturate_and_pixelate(pixbuf, pixbuf, 0.0, FALSE);

		gtk_list_store_set(model, &iter,
						   COLUMN_AUTOADD,
						   gaim_account_get_bool(account, "gevo-autoadd",
												 FALSE),
						   COLUMN_ICON, pixbuf,
						   COLUMN_SCREENNAME,
						   gaim_account_get_username(account),
						   COLUMN_DATA, account,
						   -1);

		if (pixbuf != NULL)
			g_object_unref(G_OBJECT(pixbuf));
	}

	gtk_widget_show_all(ret);

	return ret;
}

static PidginPluginUiInfo ui_info =
{
	get_config_frame,	/**< get_config_frame */
	0			/**< page_num */
};

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_STANDARD,                             /**< type           */
	PIDGIN_PLUGIN_TYPE,                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	GEVOLUTION_PLUGIN_ID,                             /**< id             */
	N_("Evolution Integration"),                      /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Provides integration with Evolution."),
	                                                  /**  description    */
	N_("Provides integration with Evolution."),
	"Christian Hammond <chipx86@chipx86.com>",        /**< author         */
	GAIM_WEBSITE,                                     /**< homepage       */

	plugin_load,                                      /**< load           */
	plugin_unload,                                    /**< unload         */
	plugin_destroy,                                   /**< destroy        */

	&ui_info,                                         /**< ui_info        */
	NULL,                                             /**< extra_info     */
	NULL,
	NULL
};

static void
init_plugin(GaimPlugin *plugin)
{
	/* TODO: Change to core-remote when possible. */
	/* info.dependencies = g_list_append(info.dependencies, "gtk-remote"); */

	/*
	 * I'm going to rant a little bit here...
	 *
	 * For some reason, when we init bonobo from inside a plugin, it will
	 * segfault when destroyed. The backtraces are within gmodule somewhere.
	 * There's not much I can do, and I'm not sure where the bug lies.
	 * However, plugins are only destroyed when Gaim is shutting down. After
	 * destroying the plugins, gaim ends, and anything else is of course
	 * freed. That includes this, if we make the module resident, which
	 * prevents us from being able to actually unload it.
	 *
	 * So, in conclusion, this is an evil hack, but it doesn't harm anything
	 * and it works.
	 */
	g_module_make_resident(plugin->handle);

	if (!bonobo_init_full(NULL, NULL, bonobo_activation_orb_get(),
						  CORBA_OBJECT_NIL, CORBA_OBJECT_NIL))
	{
		gaim_debug_error("evolution", "Unable to initialize bonobo.\n");
	}
}

GAIM_INIT_PLUGIN(gevolution, init_plugin, info)
