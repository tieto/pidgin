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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
#include "win32dep.h"
#endif

GSList *protocols = NULL;

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
	GSList *l;
	GaimPlugin *plugin;

	for (l = protocols; l != NULL; l = l->next) {
		plugin = (GaimPlugin *)l->data;

		/* Just In Case (TM) */
		if (GAIM_IS_PROTOCOL_PLUGIN(plugin)) {

			if (GAIM_PLUGIN_PROTOCOL_INFO(plugin)->protocol == type)
				return plugin;
		}
	}

	return NULL;
}

static void des_win(GtkWidget *a, GtkWidget *b)
{
	gtk_widget_destroy(b);
}

static GSList *do_ask_dialogs = NULL;

struct doaskstruct {
	GtkWidget *dialog;
	GModule *handle;
	void (*yesfunc)(gpointer);
	void (*nofunc)(gpointer);
	gpointer data;
};

void do_ask_cancel_by_handle(void *handle)
{
	GSList *d = do_ask_dialogs;

	gaim_debug(GAIM_DEBUG_MISC, "prpl",
			   "%d dialogs to search\n", g_slist_length(d));

	while (d) {
		struct doaskstruct *doask = d->data;

		d = d->next;

		if (doask->handle == handle) {
			gaim_debug(GAIM_DEBUG_MISC, "prpl",
					   "Removing dialog, %d remain\n", g_slist_length(d));
			gtk_dialog_response(GTK_DIALOG(doask->dialog), GTK_RESPONSE_NONE);
		}
	}
}

static void do_ask_callback(GtkDialog *d, gint resp, struct doaskstruct *doask)
{
	switch (resp) 
		{
		case GTK_RESPONSE_YES:
			if (doask->yesfunc)
				doask->yesfunc(doask->data);
			break;
		default:
			if (doask->nofunc)
				doask->nofunc(doask->data);
			break;
		}
	do_ask_dialogs = g_slist_remove(do_ask_dialogs, doask);
	g_free(doask);
	gtk_widget_destroy(GTK_WIDGET(d));
}

#define STOCK_ITEMIZE(r, l)       if (!strcmp(r,yestext))  \
                                           yestext = l;    \
                                   if (!strcmp(r,notext))  \
                                           notext = l;     

void do_ask_dialog(const char *prim, const char *sec, void *data, char *yestext, void *doit, char *notext, void *dont, GModule *handle, gboolean modal)
{
	GtkWidget *window;
	GtkWidget *hbox;
	GtkWidget *label;
	char labeltext[1024 * 2];
	GtkWidget *img = gtk_image_new_from_stock(GAIM_STOCK_DIALOG_QUESTION, GTK_ICON_SIZE_DIALOG);
	struct doaskstruct *doask = g_new0(struct doaskstruct, 1);

	gtk_misc_set_alignment(GTK_MISC(img), 0, 0);

	/* This is ugly.  GTK Stock items will take a button with a label "gtk-cancel" and turn it into a 
	 * Cancel button with a Cancel icon and whatnot.  We want to avoid using anything gtk in the prpls
	 * so we replace "Cancel" with "gtk-cancel" right here. */
	STOCK_ITEMIZE("Add", GTK_STOCK_ADD);
	STOCK_ITEMIZE("Apply", GTK_STOCK_APPLY);
	STOCK_ITEMIZE("Cancel", GTK_STOCK_CANCEL);
	STOCK_ITEMIZE("Close", GTK_STOCK_CLOSE);
	STOCK_ITEMIZE("Delete", GTK_STOCK_DELETE);
	STOCK_ITEMIZE("Remove", GTK_STOCK_REMOVE);
	STOCK_ITEMIZE("Yes", GTK_STOCK_YES);
	STOCK_ITEMIZE("No", GTK_STOCK_NO);

	window = gtk_dialog_new_with_buttons("", NULL, 0, notext, GTK_RESPONSE_NO, yestext, GTK_RESPONSE_YES, NULL);

	if (modal) {
		gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	}

	gtk_dialog_set_default_response (GTK_DIALOG(window), GTK_RESPONSE_YES);
	g_signal_connect(G_OBJECT(window), "response", G_CALLBACK(do_ask_callback), doask);

	gtk_container_set_border_width (GTK_CONTAINER(window), 6);
	gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
	gtk_dialog_set_has_separator(GTK_DIALOG(window), FALSE);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(window)->vbox), 12);
	gtk_container_set_border_width (GTK_CONTAINER(GTK_DIALOG(window)->vbox), 6);

	hbox = gtk_hbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(window)->vbox), hbox);
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
	
	g_snprintf(labeltext, sizeof(labeltext), "<span weight=\"bold\" size=\"larger\">%s</span>\n\n%s", prim, sec ? sec : "");
	label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(label), labeltext);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	doask->dialog  = window;
	doask->handle  = handle;
	doask->yesfunc = doit;
	doask->nofunc  = dont;
	doask->data    = data;
	do_ask_dialogs = g_slist_append(do_ask_dialogs, doask);

	gtk_widget_show_all(window);
}

static void des_prompt(GtkWidget *w, struct _prompt *p)
{
	if (p->dont)
		(p->dont)(p->data);
	gtk_widget_destroy(p->window);
	g_free(p);
}

static void act_prompt(GtkWidget *w, struct _prompt *p)
{
	if (p->doit)
		(p->doit)(p->data, gtk_entry_get_text(GTK_ENTRY(p->entry)));
	gtk_widget_destroy(p->window);
}

void do_prompt_dialog(const char *text, const char *def, void *data, void *doit, void *dont)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *entry;
	GtkWidget *button;
	struct _prompt *p;

	p = g_new0(struct _prompt, 1);
	p->data = data;
	p->doit = doit;
	p->dont = dont;

	GAIM_DIALOG(window);
	p->window = window;
	gtk_window_set_role(GTK_WINDOW(window), "prompt");
	gtk_window_set_resizable(GTK_WINDOW(window), TRUE);
	gtk_window_set_title(GTK_WINDOW(window), _("Gaim - Prompt"));
	g_signal_connect(G_OBJECT(window), "destroy",
					 G_CALLBACK(des_prompt), p);
	gtk_widget_realize(window);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new(text);
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);

	entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(vbox), entry, FALSE, FALSE, 0);
	if (def)
		gtk_entry_set_text(GTK_ENTRY(entry), def);
	g_signal_connect(G_OBJECT(entry), "activate",
					 G_CALLBACK(act_prompt), p);
	p->entry = entry;

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	button = gaim_pixbuf_button_from_stock(_("OK"), GTK_STOCK_OK, GAIM_BUTTON_HORIZONTAL);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(act_prompt), p);

	button = gaim_pixbuf_button_from_stock(_("Cancel"), GTK_STOCK_CANCEL, GAIM_BUTTON_HORIZONTAL);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(des_win), window);

	gtk_widget_show_all(window);
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
	GSList *c = connections;
	struct proto_actions_menu *pam;
	struct gaim_connection *gc = NULL;
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

	while (c) {
		gc = c->data;

		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

		if (prpl_info->actions && gc->login_time)
			count++;

		c = g_slist_next(c);
	}
	c = connections;

	if (!count) {
		g_snprintf(buf, sizeof(buf), _("No actions available"));
		menuitem = gtk_menu_item_new_with_label(buf);
		gtk_menu_shell_append(GTK_MENU_SHELL(protomenu), menuitem);
		gtk_widget_show(menuitem);
		return;
	}

	if (count == 1) {
		GList *act;
		while (c) {
			gc = c->data;

			prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

			if (prpl_info->actions && gc->login_time)
				break;

			c = g_slist_next(c);
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
		while (c) {
			GList *act;
			GdkPixbuf *pixbuf, *scale;
			GtkWidget *image;

			gc = c->data;

			prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

			if (!prpl_info->actions || !gc->login_time) {
				c = g_slist_next(c);
				continue;
			}

			g_snprintf(buf, sizeof(buf), "%s (%s)",
					   gc->username, gc->prpl->info->name);
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
			c = g_slist_next(c);
		}
	}
}

struct mail_notify {
	struct gaim_connection *gc;
	GtkWidget *email_win;
	GtkWidget *email_label;
	char *url;
};
GSList *mailnots = NULL;

static struct mail_notify *find_mail_notify(struct gaim_connection *gc)
{
	GSList *m = mailnots;
	while (m) {
		if (((struct mail_notify *)m->data)->gc == gc)
			return m->data;
		m = m->next;
	}
	return NULL;
}

static void des_email_win(GtkWidget *w, struct mail_notify *mn)
{
	if (w != mn->email_win) {
		gtk_widget_destroy(mn->email_win);
		return;
	}

	gaim_debug(GAIM_DEBUG_INFO, "prpl", "Removing mail notification.\n");

	mailnots = g_slist_remove(mailnots, mn);
	if (mn->url)
		g_free(mn->url);
	g_free(mn);
}

void connection_has_mail(struct gaim_connection *gc, int count, const char *from, const char *subject, const char *url)
{
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *urlbut;
	GtkWidget *close;

	struct mail_notify *mn;
	char buf[2048];

	if (!(gc->account->options & OPT_ACCT_MAIL_CHECK))
		return;

	if (!(mn = find_mail_notify(gc))) {
		mn = g_new0(struct mail_notify, 1);
		mn->gc = gc;
		mailnots = g_slist_append(mailnots, mn);
	}

	if (count < 0) {
		if (from && subject)
			g_snprintf(buf, sizeof buf, _("%s has mail from %s: %s"), gc->username, from, *subject ? subject : _("No Subject"));
		else
			g_snprintf(buf, sizeof buf, _("%s has new mail."), gc->username);
	} else if (count > 0) {
		g_snprintf(buf, sizeof buf, 
			   ngettext("%s has %d new message.","%s has %d new messages.",count), gc->username, count);
	} else if (mn->email_win) {
		gtk_widget_destroy(mn->email_win);
		return;
	} else
		return;

	if (mn->email_win) {
		gtk_label_set_text(GTK_LABEL(mn->email_label), buf);
		return;
	}


	GAIM_DIALOG(mn->email_win);
	gtk_window_set_role(GTK_WINDOW(mn->email_win), "mail");
	gtk_window_set_resizable(GTK_WINDOW(mn->email_win), TRUE);
	gtk_window_set_title(GTK_WINDOW(mn->email_win), _("Gaim - New Mail"));
	g_signal_connect(G_OBJECT(mn->email_win), "destroy",
					 G_CALLBACK(des_email_win), mn);
	gtk_widget_realize(mn->email_win);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(mn->email_win), vbox);

	mn->email_label = gtk_label_new(buf);
	gtk_label_set_text(GTK_LABEL(mn->email_label), buf);
	gtk_label_set_line_wrap(GTK_LABEL(mn->email_label), TRUE);
	gtk_box_pack_start(GTK_BOX(vbox), mn->email_label, FALSE, TRUE, 5);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	if (url) {
		mn->url = g_strdup(url);
		urlbut = gaim_pixbuf_button_from_stock(_("Open Mail"), GTK_STOCK_JUMP_TO, GAIM_BUTTON_HORIZONTAL);
		gtk_box_pack_end(GTK_BOX(hbox), urlbut, 0, 0, 5);
		g_signal_connect(G_OBJECT(urlbut), "clicked",
						 G_CALLBACK(open_url), mn->url);
		g_signal_connect(G_OBJECT(urlbut), "clicked",
						 G_CALLBACK(des_email_win), mn);
	}

	close = gaim_pixbuf_button_from_stock(_("Close"), GTK_STOCK_CLOSE, GAIM_BUTTON_HORIZONTAL);
	gtk_window_set_focus(GTK_WINDOW(mn->email_win), close);
	gtk_box_pack_end(GTK_BOX(hbox), close, 0, 0, 5);
	g_signal_connect(G_OBJECT(close), "clicked",
					 G_CALLBACK(des_email_win), mn);

	gtk_widget_show_all(mn->email_win);
}

struct icon_data {
	struct gaim_connection *gc;
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

void set_icon_data(struct gaim_connection *gc, const char *who, void *data, int len)
{
	struct gaim_conversation *conv;
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

void remove_icon_data(struct gaim_connection *gc)
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

void *get_icon_data(struct gaim_connection *gc, const char *who, int *len)
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
	struct gaim_connection *gc;
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
	if (g_slist_find(connections, ga->gc))
		show_add_buddy(ga->gc, ga->who, NULL, ga->alias);
	dont_add(ga);
}

void show_got_added(struct gaim_connection *gc, const char *id,
		    const char *who, const char *alias, const char *msg)
{
	char buf[BUF_LONG];
	struct got_add *ga = g_new0(struct got_add, 1);
	struct buddy *b = gaim_find_buddy(gc->account, who);

	ga->gc = gc;
	ga->who = g_strdup(who);
	ga->alias = alias ? g_strdup(alias) : NULL;


	g_snprintf(buf, sizeof(buf), _("%s%s%s%s has made %s his or her buddy%s%s%s"),
		   who,
		   alias ? " (" : "",
		   alias ? alias : "",
		   alias ? ")" : "",
		   id ? id : gc->displayname[0] ? gc->displayname : gc->username,
		   msg ? ": " : ".",
		   msg ? msg : "",
		   b ? "" : _("\n\nDo you wish to add him or her to your buddy list?"));
	if (b)
		do_error_dialog(_("Gaim - Information"), buf, GAIM_INFO);
	else
		do_ask_dialog(_("Gaim - Confirm"), buf, ga, _("Add"), do_add, _("Cancel"), dont_add, NULL, FALSE);
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
	GSList *P = protocols;

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

