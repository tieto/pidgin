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
#include "prpl.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef _WIN32
#include "win32dep.h"
#endif

#include "pixmaps/ok.xpm"
#include "pixmaps/cancel.xpm"
#include "pixmaps/tb_forward.xpm"

GSList *protocols = NULL;

GtkWidget *protomenu = NULL;
int prpl_accounts[PROTO_UNTAKEN];

struct _prompt {
	GtkWidget *window;
	GtkWidget *entry;
	void (*doit)(void *, const char *);
	void (*dont)(void *);
	void *data;
};

struct prpl *find_prpl(int prot)
{
	GSList *e = protocols;
	struct prpl *r;

	while (e) {
		r = (struct prpl *)e->data;
		if (r->protocol == prot)
			return r;
		e = e->next;
	}

	return NULL;
}

gint proto_compare(struct prpl *a, struct prpl *b)
{
	/* neg if a before b, 0 if equal, pos if a after b */
	return a->protocol - b->protocol;
}

#ifdef GAIM_PLUGINS
gboolean load_prpl(struct prpl *p)
{
	char *(*gaim_prpl_init)(struct prpl *);
	debug_printf("Loading protocol %d\n", p->protocol);

	if (!p->plug)
		return TRUE;
	
	p->plug->handle = g_module_open(p->plug->path, 0);
	if (!p->plug->handle) {
		debug_printf("%s is unloadable: %s\n", p->plug->path, g_module_error());
		return TRUE;
	}

	if (!g_module_symbol(p->plug->handle, "gaim_prpl_init", (gpointer *)&gaim_prpl_init)) {
		return TRUE;
	}

	gaim_prpl_init(p);
	return FALSE;
}
#endif

/* This is used only by static protocols */
void load_protocol(proto_init pi)
{
	struct prpl *p = g_new0(struct prpl, 1);

	if (p->protocol == PROTO_ICQ) 
		do_error_dialog(_("ICQ Protocol detected."),
				_("Gaim has loaded the ICQ plugin.  This plugin has been deprecated. "
				  "As such, it was probably not compiled from the same version of the "
				  "source as this application was, and cannot be guaranteed to work.  "
				  "It is recommended that you use the AIM/ICQ protocol to connect to ICQ"),
				GAIM_WARNING);
	pi(p);
	protocols = g_slist_insert_sorted(protocols, p, (GCompareFunc)proto_compare);
	regenerate_user_list();
}

void unload_protocol(struct prpl *p)
{
	GList *c;
	struct proto_user_opt *puo;
	if (p->name)
		g_free(p->name);
	c = p->user_opts;
	while (c) {
		puo = c->data;
		g_free(puo->label);
		g_free(puo->def);
		g_free(puo);
		c = c->next;
	}
	g_list_free(p->user_opts);
	p->user_opts = NULL;
}

STATIC_PROTO_INIT

static void des_win(GtkWidget *a, GtkWidget *b)
{
	gtk_widget_destroy(b);
}

struct doaskstruct {
	void (*yesfunc)(gpointer);
	void (*nofunc)(gpointer);
	gpointer data;
};

static void do_ask_callback(GtkDialog *d, gint resp, struct doaskstruct *doask)
{
	switch (resp) 
		{
		case GTK_RESPONSE_YES:
			if (doask->yesfunc)
				doask->yesfunc(doask->data);
			break;
		case GTK_RESPONSE_NO:
		case GTK_RESPONSE_DELETE_EVENT:
			if (doask->nofunc)
				doask->nofunc(doask->data);
			break;
		}
	g_free(doask);
	gtk_widget_destroy(GTK_WIDGET(d));
}

#define STOCK_ITEMIZE(r, l)       if (!strcmp(r,yestext))  \
                                           yestext = l;    \
                                   if (!strcmp(r,notext))  \
                                           notext = l;     

void do_ask_dialog(const char *prim, const char *sec, void *data, char *yestext, void *doit, char *notext, void *dont)
{
	GtkWidget *window;
	GtkWidget *hbox;
	GtkWidget *label;
	char labeltext[1024 * 2];
	char *filename = g_build_filename(DATADIR, "pixmaps", "gaim", "dialogs", "gaim_question.png", NULL);
	GtkWidget *img = gtk_image_new_from_file(filename);
	struct doaskstruct *doask = g_new0(struct doaskstruct, 1);

	doask->yesfunc = doit;
	doask->nofunc  = dont;
	doask->data    = data;

	g_free(filename);
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

	window = gtk_dialog_new_with_buttons("", NULL, GTK_DIALOG_MODAL, notext, GTK_RESPONSE_NO, yestext, GTK_RESPONSE_YES, NULL);
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
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, TRUE, TRUE);
	gtk_window_set_title(GTK_WINDOW(window), _("Gaim - Prompt"));
	gtk_signal_connect(GTK_OBJECT(window), "destroy", GTK_SIGNAL_FUNC(des_prompt), p);
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
	gtk_signal_connect(GTK_OBJECT(entry), "activate", GTK_SIGNAL_FUNC(act_prompt), p);
	p->entry = entry;

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	button = picture_button(window, _("Accept"), ok_xpm);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(act_prompt), p);

	button = picture_button(window, _("Cancel"), cancel_xpm);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(des_win), window);

	gtk_widget_show_all(window);
}

static void proto_act(GtkObject *obj, struct gaim_connection *gc)
{
	char *act = gtk_object_get_user_data(obj);
	gc->prpl->do_action(gc, act);
}

void do_proto_menu()
{
	GtkWidget *menuitem;
	GtkWidget *submenu;
	GList *l;
	GSList *c = connections;
	struct gaim_connection *gc = NULL;
	int count = 0;
	char buf[256];

	if (!protomenu)
		return;

	l = gtk_container_children(GTK_CONTAINER(protomenu));
	while (l) {
		gtk_container_remove(GTK_CONTAINER(protomenu), GTK_WIDGET(l->data));
		l = l->next;
	}

	while (c) {
		gc = c->data;
		if (gc->prpl->actions && gc->prpl->do_action)
			count++;
		c = g_slist_next(c);
	}
	c = connections;

	if (!count) {
		g_snprintf(buf, sizeof(buf), "No actions available");
		menuitem = gtk_menu_item_new_with_label(buf);
		gtk_menu_append(GTK_MENU(protomenu), menuitem);
		gtk_widget_show(menuitem);
		return;
	}

	if (count == 1) {
		GList *tmp, *act;
		while (c) {
			gc = c->data;
			if (gc->prpl->actions && gc->prpl->do_action)
				break;
			c = g_slist_next(c);
		}

		tmp = act = gc->prpl->actions();

		while (act) {
			if (act->data == NULL) {
				gaim_separator(protomenu);
				act = g_list_next(act);
				continue;
			}

			menuitem = gtk_menu_item_new_with_label(act->data);
			gtk_object_set_user_data(GTK_OBJECT(menuitem), act->data);
			gtk_menu_append(GTK_MENU(protomenu), menuitem);
			gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
					   GTK_SIGNAL_FUNC(proto_act), gc);
			gtk_widget_show(menuitem);

			act = g_list_next(act);
		}

		g_list_free(tmp);
	} else {
		while (c) {
			GList *tmp, *act;
			gc = c->data;
			if (!gc->prpl->actions || !gc->prpl->do_action) {
				c = g_slist_next(c);
				continue;
			}

			g_snprintf(buf, sizeof(buf), "%s (%s)", gc->username, gc->prpl->name);
			menuitem = gtk_menu_item_new_with_label(buf);
			gtk_menu_append(GTK_MENU(protomenu), menuitem);
			gtk_widget_show(menuitem);

			submenu = gtk_menu_new();
			gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);
			gtk_widget_show(submenu);

			tmp = act = gc->prpl->actions();

			while (act) {
				if (act->data == NULL) {
					gaim_separator(submenu);
					act = g_list_next(act);
					continue;
				}

				menuitem = gtk_menu_item_new_with_label(act->data);
				gtk_object_set_user_data(GTK_OBJECT(menuitem), act->data);
				gtk_menu_append(GTK_MENU(submenu), menuitem);
				gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
						   GTK_SIGNAL_FUNC(proto_act), gc);
				gtk_widget_show(menuitem);

				act = g_list_next(act);
			}

			g_list_free(tmp);
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
	debug_printf("removing mail notification\n");
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

	if (!(gc->user->options & OPT_USR_MAIL_CHECK))
		return;

	if (!(mn = find_mail_notify(gc))) {
		mn = g_new0(struct mail_notify, 1);
		mn->gc = gc;
		mailnots = g_slist_append(mailnots, mn);
	}

	if (count < 0) {
		if (from && subject)
			g_snprintf(buf, sizeof buf, "%s has mail from %s: %s", gc->username, from, *subject ? subject : _("No Subject"));
		else
			g_snprintf(buf, sizeof buf, "%s has new mail.", gc->username);
	} else if (count > 0) {
		g_snprintf(buf, sizeof buf, "%s has %d new message%s.",
			   gc->username, count, count == 1 ? "" : "s");
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
	gtk_window_set_policy(GTK_WINDOW(mn->email_win), FALSE, TRUE, TRUE);
	gtk_window_set_title(GTK_WINDOW(mn->email_win), _("Gaim - New Mail"));
	gtk_signal_connect(GTK_OBJECT(mn->email_win), "destroy", GTK_SIGNAL_FUNC(des_email_win), mn);
	gtk_widget_realize(mn->email_win);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(mn->email_win), vbox);
	gtk_widget_show(vbox);

	mn->email_label = gtk_label_new(buf);
	gtk_label_set_text(GTK_LABEL(mn->email_label), buf);
	gtk_label_set_line_wrap(GTK_LABEL(mn->email_label), TRUE);
	gtk_box_pack_start(GTK_BOX(vbox), mn->email_label, FALSE, TRUE, 5);
	gtk_widget_show(mn->email_label);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	close = picture_button(mn->email_win, _("Close"), cancel_xpm);
	gtk_window_set_focus(GTK_WINDOW(mn->email_win), close);
	gtk_box_pack_end(GTK_BOX(hbox), close, 0, 0, 5);
	gtk_signal_connect(GTK_OBJECT(close), "clicked", GTK_SIGNAL_FUNC(des_email_win), mn);

	if (url) {
		mn->url = g_strdup(url);
		urlbut = picture_button(mn->email_win, _("Open Mail"), tb_forward_xpm);
		gtk_box_pack_end(GTK_BOX(hbox), urlbut, 0, 0, 5);
		gtk_signal_connect(GTK_OBJECT(urlbut), "clicked", GTK_SIGNAL_FUNC(open_url), mn->url);
		gtk_signal_connect(GTK_OBJECT(urlbut), "clicked", GTK_SIGNAL_FUNC(des_email_win), mn);
	}

	gtk_widget_show(mn->email_win);
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

	return ((x->gc != y->gc) || g_strcasecmp(x->who, y->who));
}

void set_icon_data(struct gaim_connection *gc, char *who, void *data, int len)
{
	struct icon_data tmp;
	GList *l;
	struct icon_data *id;
	tmp.gc = gc;
	tmp.who = normalize(who);
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
			return;
		}
	} else if (data) {
		id = g_new0(struct icon_data, 1);
		icons = g_list_append(icons, id);
		id->gc = gc;
		id->who = g_strdup(normalize(who));
	} else {
		return;
	}

	debug_printf("Got icon for %s (length %d)\n", who, len);

	id->data = g_memdup(data, len);
	id->len = len;

	got_new_icon(gc, who);
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

void *get_icon_data(struct gaim_connection *gc, char *who, int *len)
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
		   find_buddy(gc, ga->who) ? "" : _("\n\nDo you wish to add him or her to your buddy list?"));
	if (find_buddy(gc, ga->who))
		do_error_dialog(buf, NULL, GAIM_INFO);
	else
		do_ask_dialog(buf, NULL, ga, _("Add"), do_add, _("Cancel"), dont_add);
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
		struct prpl *p = P->data;
		if (p->register_user)
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
		struct prpl *p = P->data;
		P = P->next;

		if (!p->register_user)
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
	gtk_signal_connect(GTK_OBJECT(regdlg), "destroy", GTK_SIGNAL_FUNC(delete_regdlg), NULL);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(regdlg), vbox);
	gtk_widget_show(vbox);

	reg_list = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), reg_list, FALSE, FALSE, 5);
	gtk_widget_show(reg_list);

	frame = gtk_frame_new(_("Registration Information"));
	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 5);
	gtk_widget_show(frame);

	reg_area = gtk_hbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), reg_area);
	gtk_widget_show(reg_area);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	close = picture_button(regdlg, _("Close"), cancel_xpm);
	gtk_box_pack_end(GTK_BOX(hbox), close, FALSE, FALSE, 5);
	gtk_signal_connect(GTK_OBJECT(close), "clicked", GTK_SIGNAL_FUNC(delete_regdlg), NULL);
	gtk_widget_show(close);

	reg_reg = picture_button(regdlg, _("Register"), ok_xpm);
	gtk_box_pack_end(GTK_BOX(hbox), reg_reg, FALSE, FALSE, 5);
	gtk_widget_show(reg_reg);

	/* fuck me */
	reset_reg_dlg();

	gtk_widget_show(regdlg);
}

static gboolean delayed_unload(void *handle) {
	g_module_close(handle);
	return FALSE;
}

gboolean ref_protocol(struct prpl *p) {
#ifdef GAIM_PLUGINS
	if(p->plug) { /* This protocol is a plugin */
		prpl_accounts[p->protocol]++;
		debug_printf("Protocol %s now in use by %d connections.\n", p->name, prpl_accounts[p->protocol]);
		if(!p->plug->handle) { /*But the protocol isn't yet loaded */
			unload_protocol(p);
			if (load_prpl(p))
				return FALSE;
		}
	}
#endif /* GAIM_PLUGINS */
	return TRUE;
}

void unref_protocol(struct prpl *p) {
#ifdef GAIM_PLUGINS
	if(p->plug) { /* This protocol is a plugin */
		prpl_accounts[p->protocol]--;
		debug_printf("Protocol %s now in use by %d connections.\n", p->name, prpl_accounts[p->protocol]);
		if(prpl_accounts[p->protocol] == 0) { /* No longer needed */
			debug_printf("Throwing out %s protocol plugin\n", p->name);
			g_timeout_add(0, delayed_unload, p->plug->handle);
			p->plug->handle = NULL;
		}
	}
#endif /* GAIM_PLUGINS */
}

