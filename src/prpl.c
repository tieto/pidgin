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

#include "pixmaps/ok.xpm"
#include "pixmaps/cancel.xpm"
#include "pixmaps/tb_forward.xpm"

GSList *protocols = NULL;

GtkWidget *protomenu = NULL;

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

static gint proto_compare(struct prpl *a, struct prpl *b)
{
	/* neg if a before b, 0 if equal, pos if a after b */
	return a->protocol - b->protocol;
}

void load_protocol(proto_init pi, int size)
{
	struct prpl *p;
	struct prpl *old;
	if (size != sizeof(struct prpl)) {
		do_error_dialog(_("Incompatible protocol detected."),
				_("You have attempted to load a protocol which was not compiled"
				  " from the same version of the source as this application was."
				  " Unfortunately, because it is not the same version I cannot"
				  " safely tell you which one it was. Needless to say, it was not"
				  " successfully loaded."), GAIM_ERROR);
		return;
	}
	
	p = g_new0(struct prpl, 1);
	pi(p);
	if ((old = find_prpl(p->protocol)) != NULL)
		unload_protocol(old);

	if (p->protocol == PROTO_ICQ) 
		do_error_dialog(_("Libicq.so detected."),
				_("Gaim has loaded the ICQ plugin.  This plugin has been deprecated. "
				  "As such, it was probably not compiled from the same version of the "
				  "source as this application was, and cannot be guaranteed to work.  "
				  "It is reccomended that you use the AIM/ICQ protocol to connect to ICQ"),
				GAIM_WARNING);


	protocols = g_slist_insert_sorted(protocols, p, (GCompareFunc)proto_compare);
	regenerate_user_list();
}

void unload_protocol(struct prpl *p)
{
	GSList *c = connections;
	struct gaim_connection *g;
	while (c) {
		g = (struct gaim_connection *)c->data;
		if (g->prpl == p) {
			char buf[256];
			g_snprintf(buf, sizeof buf, _("%s was using %s, which got removed."
						      " %s is now offline."), g->username,
				   p->name(), g->username);
			do_error_dialog(buf, NULL, GAIM_ERROR);
			signoff(g);
			c = connections;
		} else
			c = c->next;
	}
	protocols = g_slist_remove(protocols, p);
	g_free(p);
	regenerate_user_list();
}

STATIC_PROTO_INIT

static void des_win(GtkWidget *a, GtkWidget *b)
{
	gtk_widget_destroy(b);
}

static void rem_win(GtkWidget *a, GtkWidget *b)
{
	void *d = gtk_object_get_user_data(GTK_OBJECT(a));
	if (d)
		gtk_signal_disconnect_by_data(GTK_OBJECT(b), d);
	gtk_widget_destroy(b);
}

void do_ask_dialog(const char *text, void *data, void *doit, void *dont)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *label;
	GtkWidget *hbox;
	GtkWidget *button;

	GAIM_DIALOG(window);
	gtk_window_set_wmclass(GTK_WINDOW(window), "accept", "Gaim");
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, TRUE, TRUE);
	gtk_window_set_title(GTK_WINDOW(window), _("Accept?"));
	gtk_widget_realize(window);
	if (dont)
		gtk_signal_connect(GTK_OBJECT(window), "destroy", GTK_SIGNAL_FUNC(dont), data);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	label = gtk_label_new(text);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	button = picture_button(window, _("Cancel"), cancel_xpm);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(des_win), window);

	button = picture_button(window, _("Accept"), ok_xpm);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	if (dont)
		gtk_object_set_user_data(GTK_OBJECT(button), data);
	if (doit)
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(doit), data);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(rem_win), window);

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
	gtk_window_set_wmclass(GTK_WINDOW(window), "prompt", "Gaim");
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

	button = picture_button(window, _("Cancel"), cancel_xpm);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(des_win), window);

	button = picture_button(window, _("Accept"), ok_xpm);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(act_prompt), p);

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

			g_snprintf(buf, sizeof(buf), "%s (%s)", gc->username, gc->prpl->name());
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

	if (count < 0 && from && subject) {
		g_snprintf(buf, sizeof buf, "%s has mail from %s: %s", gc->username, from, subject);
	} else if (count) {
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
	gtk_window_set_wmclass(GTK_WINDOW(mn->email_win), "mail", "Gaim");
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
	gtk_box_pack_start(GTK_BOX(vbox), mn->email_label, 0, 0, 5);
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

static void dont_add(gpointer x, struct got_add *ga)
{
	g_free(ga->who);
	if (ga->alias)
		g_free(ga->alias);
	g_free(ga);
}

static void do_add(gpointer x, struct got_add *ga)
{
	if (g_slist_find(connections, ga->gc))
		show_add_buddy(ga->gc, ga->who, NULL, ga->alias);
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
		do_ask_dialog(buf, ga, do_add, dont_add);
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
	gtk_window_set_wmclass(GTK_WINDOW(regdlg), "register", "Gaim");
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

GSList *add_smiley(GSList *list, char *key, char **xpm, int show) 
{
	struct _prpl_smiley *smiley;

 	smiley = (struct _prpl_smiley *)g_new0(struct _prpl_smiley, 1);
	smiley->key = g_strdup(key);
	smiley->xpm = xpm;
	smiley->show = show;
	list = g_slist_append(list, smiley);

	return list;
}
