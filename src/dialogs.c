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
 */
#include "gtkinternal.h"

#include "debug.h"
#include "log.h"
#include "multi.h"
#include "notify.h"
#include "prefs.h"
#include "prpl.h"
#include "request.h"
#include "status.h"
#include "util.h"

#include "gtkblist.h"
#include "gtkconv.h"
#include "gtkimhtml.h"
#include "gtkprefs.h"
#include "gtkutils.h"
#include "stock.h"

#include "ui.h"

#ifdef USE_GTKSPELL
# include <gtkspell/gtkspell.h>
#endif

/* XXX */
#include "gaim.h"

#ifdef _WIN32
# include "wspell.h"
#endif

static GtkWidget *imdialog = NULL;	/*I only want ONE of these :) */
static GList *dialogwindows = NULL;
static GtkWidget *importdialog;
static GaimConnection *importgc;
static GtkWidget *icondlg;
static GtkWidget *rename_dialog = NULL;
static GtkWidget *fontseld = NULL;


struct confirm_del {
	GtkWidget *window;
	GtkWidget *label;
	GtkWidget *ok;
	GtkWidget *cancel;
	char name[1024];
	GaimConnection *gc;
};

struct create_away {
	GtkWidget *window;
	GtkWidget *entry;
	GtkWidget *text;
	struct away_message *mess;
};

struct warning {
	GtkWidget *window;
	GtkWidget *anon;
	char *who;
	GaimConnection *gc;
};

struct findbyemail {
	GtkWidget *window;
	GtkWidget *emailentry;
	GaimConnection *gc;
};

struct findbyinfo {
	GaimConnection *gc;
	GtkWidget *window;
	GtkWidget *firstentry;
	GtkWidget *middleentry;
	GtkWidget *lastentry;
	GtkWidget *maidenentry;
	GtkWidget *cityentry;
	GtkWidget *stateentry;
	GtkWidget *countryentry;
};

struct info_dlg {
	GaimConnection *gc;
	char *who;
	GtkWidget *window;
	GtkWidget *text;
};

struct getuserinfo {
	GtkWidget *window;
	GtkWidget *entry;
	GtkWidget *account;
	GaimConnection *gc;
};

typedef struct
{
	char *username;
	gboolean block;
	GaimConnection *gc;

} GaimGtkBlockData;

struct set_info_dlg {
	GtkWidget *window;
	GtkWidget *menu;
	GaimAccount *account;
	GtkWidget *text;
	GtkWidget *save;
	GtkWidget *cancel;
};

struct set_icon_dlg {
	GtkWidget *window;
	GaimAccount *account;
	GtkWidget *ok;
	GtkWidget *cancel;
	GtkWidget *entry;
};

struct set_dir_dlg {
	GaimConnection *gc;
	GtkWidget *window;
	GtkWidget *first;
	GtkWidget *middle;
	GtkWidget *last;
	GtkWidget *maiden;
	GtkWidget *city;
	GtkWidget *state;
	GtkWidget *country;
	GtkWidget *web;
	GtkWidget *cancel;
	GtkWidget *save;
};

struct linkdlg {
	GtkWidget *ok;
	GtkWidget *cancel;
	GtkWidget *window;
	GtkWidget *url;
	GtkWidget *text;
	GtkWidget *toggle;
	GtkWidget *entry;
	GaimConversation *c;
};

struct passwddlg {
	GtkWidget *window;
	GtkWidget *ok;
	GtkWidget *cancel;
	GtkWidget *original;
	GtkWidget *new1;
	GtkWidget *new2;
	GaimConnection *gc;
};

struct view_log {
	long offset;
	int options;
	char *name;
	GtkWidget *bbox;
	GtkWidget *window;
	GtkWidget *layout;
	void *clear_handle;
};

/* Wrapper to get all the text from a GtkTextView */
gchar* gtk_text_view_get_text(GtkTextView *text, gboolean include_hidden_chars)
{
	GtkTextBuffer *buffer;
	GtkTextIter start, end;

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
	gtk_text_buffer_get_start_iter(buffer, &start);
	gtk_text_buffer_get_end_iter(buffer, &end);

	return gtk_text_buffer_get_text(buffer, &start, &end, include_hidden_chars);
}

/*------------------------------------------------------------------------*/
/*  Destroys                                                              */
/*------------------------------------------------------------------------*/

static gint delete_event_dialog(GtkWidget *w, GdkEventAny *e, GaimConversation *c)
{
	GaimGtkConversation *gtkconv;
	gchar *object_data;

	object_data = g_object_get_data(G_OBJECT(w), "dialog_type");

	gtkconv = GAIM_GTK_CONVERSATION(c);

	if (GTK_IS_COLOR_SELECTION_DIALOG(w)) {
		if (w == gtkconv->dialogs.fg_color) {
			gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON(gtkconv->toolbar.fgcolor), FALSE);
			gtkconv->dialogs.fg_color = NULL;
		} else {
			gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON(gtkconv->toolbar.bgcolor), FALSE);
			gtkconv->dialogs.bg_color = NULL;
		}
	} else if (GTK_IS_FONT_SELECTION_DIALOG(w)) {
		gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON(gtkconv->toolbar.font), FALSE);
		gtkconv->dialogs.font = NULL;
	} else if (!g_ascii_strcasecmp(object_data, "smiley dialog")) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtkconv->toolbar.smiley),
									FALSE);
		gtkconv->dialogs.smiley = NULL;
	} else if (!g_ascii_strcasecmp(object_data, "log dialog")) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gtkconv->toolbar.log),
									   FALSE);
		gtkconv->dialogs.log = NULL;
	}

	dialogwindows = g_list_remove(dialogwindows, w);
	gtk_widget_destroy(w);

	return FALSE;
}

static void destroy_dialog(GtkWidget *w, GtkWidget *w2)
{
	GtkWidget *dest;

	if (!GTK_IS_WIDGET(w2))
		dest = w;
	else
		dest = w2;

	if (dest == imdialog)
		imdialog = NULL;
	else if (dest == importdialog) {
		importdialog = NULL;
		importgc = NULL;
	}
	else if (dest == icondlg)
		icondlg = NULL;
	else if (dest == rename_dialog)
		rename_dialog = NULL;

	dialogwindows = g_list_remove(dialogwindows, dest);
	gtk_widget_destroy(dest);
}


void destroy_all_dialogs()
{
	while (dialogwindows)
		destroy_dialog(NULL, dialogwindows->data);

	if (awaymessage)
		do_im_back(NULL, NULL);

	if (imdialog) {
		destroy_dialog(NULL, imdialog);
		imdialog = NULL;
	}

	if (importdialog) {
		destroy_dialog(NULL, importdialog);
		importdialog = NULL;
	}

	if (icondlg) {
		destroy_dialog(NULL, icondlg);
		icondlg = NULL;
	}
}

static void do_warn(GtkWidget *widget, gint resp, struct warning *w)
{
	if (resp == GTK_RESPONSE_OK)
		serv_warn(w->gc, w->who, (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->anon))) ? 1 : 0);

	destroy_dialog(NULL, w->window);
	g_free(w);
}

void show_warn_dialog(GaimConnection *gc, char *who)
{
	char *labeltext;
	GtkWidget *hbox, *vbox;
	GtkWidget *label;
	GtkWidget *img = gtk_image_new_from_stock(GAIM_STOCK_DIALOG_WARNING, GTK_ICON_SIZE_DIALOG);
	GaimConversation *c = gaim_find_conversation_with_account(who, gc->account);

	struct warning *w = g_new0(struct warning, 1);
	w->who = who;
	w->gc = gc;

	gtk_misc_set_alignment(GTK_MISC(img), 0, 0);

	w->window = gtk_dialog_new_with_buttons(_("Warn User"),
			GTK_WINDOW(GAIM_GTK_WINDOW(c->window)->window), 0,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			_("_Warn"), GTK_RESPONSE_OK, NULL);
	gtk_dialog_set_default_response (GTK_DIALOG(w->window), GTK_RESPONSE_OK);
	g_signal_connect(G_OBJECT(w->window), "response", G_CALLBACK(do_warn), w);

	gtk_container_set_border_width (GTK_CONTAINER(w->window), 6);
	gtk_window_set_resizable(GTK_WINDOW(w->window), FALSE);
	gtk_dialog_set_has_separator(GTK_DIALOG(w->window), FALSE);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(w->window)->vbox), 12);
	gtk_container_set_border_width (GTK_CONTAINER(GTK_DIALOG(w->window)->vbox), 6);

	hbox = gtk_hbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(w->window)->vbox), hbox);
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(hbox), vbox);
	labeltext = g_strdup_printf(_("<span weight=\"bold\" size=\"larger\">Warn %s?</span>\n\n"
				      "This will increase %s's warning level and he or she will be subject to harsher rate limiting.\n"), who, who);
	label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(label), labeltext);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	g_free(labeltext);

	w->anon = gtk_check_button_new_with_mnemonic(_("Warn _anonymously?"));
	gtk_box_pack_start(GTK_BOX(vbox), w->anon, FALSE, FALSE, 0);

	hbox = gtk_hbox_new(FALSE, 6);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);
	img = gtk_image_new_from_stock(GTK_STOCK_DIALOG_INFO, GTK_ICON_SIZE_MENU);
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
	labeltext = _("<b>Anonymous warnings are less severe.</b>");
	/* labeltext = _("Anonymous warnings are less severe."); */
	label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(label), labeltext);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	dialogwindows = g_list_prepend(dialogwindows, w->window);
	gtk_widget_show_all(w->window);
}

static void
do_remove_chat(GaimChat *chat)
{
	gaim_blist_remove_chat(chat);
	gaim_blist_save();
}

static void
do_remove_buddy(GaimBuddy *b)
{
	GaimGroup *g;
	GaimConversation *c;
	gchar *name;
	GaimAccount *account;

	if (!b)
		return;

	g = gaim_find_buddys_group(b);
	name = g_strdup(b->name); /* b->name is a crasher after remove_buddy */
	account = b->account;

	gaim_debug(GAIM_DEBUG_INFO, "blist",
			   "Removing '%s' from buddy list.\n", b->name);
	serv_remove_buddy(b->account->gc, name, g->name);
	gaim_blist_remove_buddy(b);
	gaim_blist_save();

	c = gaim_find_conversation_with_account(name, account);

	if (c != NULL)
		gaim_conversation_update(c, GAIM_CONV_UPDATE_REMOVE);

	g_free(name);
}

static void do_remove_contact(GaimContact *c)
{
	GaimBlistNode *bnode, *cnode;
	GaimGroup *g;

	if(!c)
		return;

	cnode = (GaimBlistNode *)c;
	g = (GaimGroup*)cnode->parent;
	for(bnode = cnode->child; bnode; bnode = bnode->next) {
		GaimBuddy *b = (GaimBuddy*)bnode;
		if(b->account->gc)
			serv_remove_buddy(b->account->gc, b->name, g->name);
	}
	gaim_blist_remove_contact(c);
}

void do_remove_group(GaimGroup *g)
{
	GaimBlistNode *cnode, *bnode;

	cnode = ((GaimBlistNode*)g)->child;

	while(cnode) {
		if(GAIM_BLIST_NODE_IS_CONTACT(cnode)) {
			bnode = cnode->child;
			cnode = cnode->next;
			while(bnode) {
				GaimBuddy *b;
				if(GAIM_BLIST_NODE_IS_BUDDY(bnode)) {
					GaimConversation *c;
                                        b = (GaimBuddy*)bnode;
					bnode = bnode->next;
					c = gaim_find_conversation_with_account(b->name, b->account);
					if(gaim_account_is_connected(b->account)) {
						serv_remove_buddy(b->account->gc, b->name, g->name);
						gaim_blist_remove_buddy(b);
						if(c)
							gaim_conversation_update(c,
									GAIM_CONV_UPDATE_REMOVE);
					}
				} else {
					bnode = bnode->next;
				}
			}
		} else if(GAIM_BLIST_NODE_IS_CHAT(cnode)) {
			GaimChat *chat = (GaimChat *)cnode;
			cnode = cnode->next;
			if(gaim_account_is_connected(chat->account))
				gaim_blist_remove_chat(chat);
		} else {
			cnode = cnode->next;
		}
	}

	gaim_blist_remove_group(g);
	gaim_blist_save();
}

void show_confirm_del(GaimBuddy *b)
{
	char *text;
	if (!b)
		return;

	text = g_strdup_printf(_("You are about to remove %s from your buddy list.  Do you want to continue?"), b->name);

	gaim_request_action(NULL, NULL, _("Remove Buddy"), text, -1, b, 2,
						_("Remove Buddy"), G_CALLBACK(do_remove_buddy),
						_("Cancel"), NULL);

	g_free(text);
}

void show_confirm_del_blist_chat(GaimChat *chat)
{
	char *name = gaim_chat_get_display_name(chat);
	char *text = g_strdup_printf(_("You are about to remove the chat %s from your buddy list.  Do you want to continue?"), name);

	gaim_request_action(NULL, NULL, _("Remove Chat"), text, -1, chat, 2,
						_("Remove Chat"), G_CALLBACK(do_remove_chat),
						_("Cancel"), NULL);

	g_free(name);
	g_free(text);
}

void show_confirm_del_group(GaimGroup *g)
{
	char *text = g_strdup_printf(_("You are about to remove the group %s and all its members from your buddy list.  Do you want to continue?"),
			       g->name);

	gaim_request_action(NULL, NULL, _("Remove Group"), text, -1, g, 2,
						_("Remove Group"), G_CALLBACK(do_remove_group),
						_("Cancel"), NULL);

	g_free(text);
}

void show_confirm_del_contact(GaimContact *c)
{
	GaimBuddy *b = gaim_contact_get_priority_buddy(c);

	if(!b)
		return;

	if(((GaimBlistNode*)c)->child == (GaimBlistNode*)b &&
			!((GaimBlistNode*)b)->next) {
		show_confirm_del(b);
	} else {
		char *text = g_strdup_printf(_("You are about to remove the contact containing %s and %d other buddies from your buddy list.  Do you want to continue?"),
			       b->name, c->totalsize - 1);

		gaim_request_action(NULL, NULL, _("Remove Contact"), text, -1, c, 2,
				_("Remove Contact"), G_CALLBACK(do_remove_contact),
				_("Cancel"), NULL);

		g_free(text);
	}
}

/*------------------------------------------------------------------------*/
/*  The dialog for getting an error                                       */
/*------------------------------------------------------------------------*/
static void do_im(GtkWidget *widget, int resp, struct getuserinfo *info)
{
	const char *who;
	GaimConversation *conv;
	GaimAccount *account;

	if (resp == GTK_RESPONSE_OK) {
		who = gtk_entry_get_text(GTK_ENTRY(info->entry));

		if (!who || !*who) {
			/* this shouldn't ever happen */
			return;
		}

		account = (info->gc ? info->gc->account : NULL);

		conv = gaim_find_conversation_with_account(who, account);

		if (conv == NULL)
			conv = gaim_conversation_new(GAIM_CONV_IM, account, who);
		else {
			gaim_conv_window_raise(gaim_conversation_get_window(conv));
		}
	}

	destroy_dialog(NULL, imdialog);
	imdialog = NULL;
	g_free(info);
}

static void do_info(GtkWidget *widget, int resp, struct getuserinfo *info)
{
	char *who;

	if (resp == GTK_RESPONSE_OK) {
		who = g_strdup(gaim_normalize(info->gc->account, gtk_entry_get_text(GTK_ENTRY(info->entry))));

		if (!g_ascii_strcasecmp(who, "")) {
			g_free(who);
			return;
		}

		/* what do we want to do about this case? */
		if (info->gc)
			serv_get_info(info->gc, who);
		g_free(who);
	}
	gtk_widget_destroy(GTK_WIDGET(widget));
	g_free(info);
}

void show_ee_dialog(int ee)
{
	GtkWidget *window;
	GtkWidget *hbox;
	GtkWidget *label;
	GaimGtkBuddyList *gtkblist;
	GtkWidget *img = gtk_image_new_from_stock(GAIM_STOCK_DIALOG_COOL, GTK_ICON_SIZE_DIALOG);

	gtkblist = GAIM_GTK_BLIST(gaim_get_blist());

	label = gtk_label_new(NULL);
	if (ee == 0)
		gtk_label_set_markup(GTK_LABEL(label),
				     "<span weight=\"bold\" size=\"large\" foreground=\"purple\">Amazing!  Simply Amazing!</span>");
	else if (ee == 1)
		gtk_label_set_markup(GTK_LABEL(label),
				     "<span weight=\"bold\" size=\"large\" foreground=\"#1f6bad\">Pimpin\' Penguin Style! *Waddle Waddle*</span>");
	else if (ee == 2)
		gtk_label_set_markup(GTK_LABEL(label),
				      "<span weight=\"bold\" size=\"large\" foreground=\"blue\">You should be me.  I'm so cute!</span>");
	else if (ee == 3)
		gtk_label_set_markup(GTK_LABEL(label),
				     "<span weight=\"bold\" size=\"large\" foreground=\"orange\">Now that's what I like!</span>");
	else if (ee == 4)
		gtk_label_set_markup(GTK_LABEL(label),
				     "<span weight=\"bold\" size=\"large\" foreground=\"brown\">Ahh, and excellent choice!</span>");
	else  if (ee == 5)
		gtk_label_set_markup(GTK_LABEL(label),
				     "<span weight=\"bold\" size=\"large\" foreground=\"#009900\">Everytime you click my name, an angel gets its wings.</span>");
	else if (ee == 6)
		gtk_label_set_markup(GTK_LABEL(label),
				     "<span weight=\"bold\" size=\"large\" foreground=\"red\">This sunflower seed taste like pizza.</span>");
	else if (ee == 7)
		gtk_label_set_markup(GTK_LABEL(label),
				     "<span weight=\"bold\" size=\"large\" foreground=\"#6364B1\">Hey!  I was in that tumbleweed!</span>");
	else
		gtk_label_set_markup(GTK_LABEL(label),
				     "<span weight=\"bold\" size=\"large\" foreground=\"gray\">I'm not anything.</span>");

	window = gtk_dialog_new_with_buttons("", GTK_WINDOW(gtkblist->window), 0, GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
	gtk_dialog_set_default_response (GTK_DIALOG(window), GTK_RESPONSE_OK);
	g_signal_connect(G_OBJECT(window), "response", G_CALLBACK(gtk_widget_destroy), NULL);

	gtk_container_set_border_width (GTK_CONTAINER(window), 6);
	gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
	gtk_dialog_set_has_separator(GTK_DIALOG(window), FALSE);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(window)->vbox), 12);
	gtk_container_set_border_width (GTK_CONTAINER(GTK_DIALOG(window)->vbox), 6);

	hbox = gtk_hbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(window)->vbox), hbox);
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);

	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	gtk_widget_show_all(window);
}

static void
show_info_select_account(GObject *w, GaimAccount *account,
						 struct getuserinfo *info)
{
	info->gc = gaim_account_get_connection(account);
}

static void dialog_set_ok_sensitive(GtkWidget *entry, GtkWidget *dlg) {
	const char *txt = gtk_entry_get_text(GTK_ENTRY(entry));
	gtk_dialog_set_response_sensitive(GTK_DIALOG(dlg), GTK_RESPONSE_OK,
			(*txt != '\0'));
}

void show_im_dialog()
{
	GtkWidget *hbox, *vbox;
	GtkWidget *label;
	GtkWidget *table;
	GaimGtkBuddyList *gtkblist;
	GtkWidget *img = gtk_image_new_from_stock(GAIM_STOCK_DIALOG_QUESTION, GTK_ICON_SIZE_DIALOG);
	struct getuserinfo *info = NULL;

	gtkblist = GAIM_GTK_BLIST(gaim_get_blist());

	if (!imdialog) {
		info = g_new0(struct getuserinfo, 1);
		info->gc = gaim_connections_get_all()->data;
		imdialog = gtk_dialog_new_with_buttons(_("New Message"), gtkblist ? GTK_WINDOW(gtkblist->window) : NULL, 0,
						       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
		gtk_dialog_set_default_response (GTK_DIALOG(imdialog), GTK_RESPONSE_OK);
		gtk_container_set_border_width (GTK_CONTAINER(imdialog), 6);
		gtk_window_set_resizable(GTK_WINDOW(imdialog), FALSE);
		gtk_dialog_set_has_separator(GTK_DIALOG(imdialog), FALSE);
		gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(imdialog)->vbox), 12);
		gtk_container_set_border_width (GTK_CONTAINER(GTK_DIALOG(imdialog)->vbox), 6);
		gtk_dialog_set_response_sensitive(GTK_DIALOG(imdialog), GTK_RESPONSE_OK, FALSE);

		hbox = gtk_hbox_new(FALSE, 12);
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(imdialog)->vbox), hbox);
		gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
		gtk_misc_set_alignment(GTK_MISC(img), 0, 0);

		vbox = gtk_vbox_new(FALSE, 0);
		gtk_container_add(GTK_CONTAINER(hbox), vbox);

		label = gtk_label_new(_("Please enter the screenname of the person you would like to IM.\n"));
		gtk_widget_set_size_request(GTK_WIDGET(label), 350, -1);
		gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
		gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

		hbox = gtk_hbox_new(FALSE, 6);
		gtk_container_add(GTK_CONTAINER(vbox), hbox);

		table = gtk_table_new(2, 2, FALSE);
		gtk_table_set_row_spacings(GTK_TABLE(table), 6);
		gtk_table_set_col_spacings(GTK_TABLE(table), 6);
		gtk_container_set_border_width(GTK_CONTAINER(table), 12);
		gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);

		label = gtk_label_new(NULL);
		gtk_label_set_markup_with_mnemonic(GTK_LABEL(label), _("_Screenname:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
		gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 0, 1);

		info->entry = gtk_entry_new();
		gtk_table_attach_defaults(GTK_TABLE(table), info->entry, 1, 2, 0, 1);
		gtk_entry_set_activates_default (GTK_ENTRY(info->entry), TRUE);
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), GTK_WIDGET(info->entry));
		g_signal_connect(G_OBJECT(info->entry), "changed",
				G_CALLBACK(dialog_set_ok_sensitive), imdialog);

		if (gaim_connections_get_all()->next) {

			label = gtk_label_new(NULL);
			gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1, 2);
			gtk_label_set_markup_with_mnemonic(GTK_LABEL(label), _("_Account:"));
			gtk_misc_set_alignment(GTK_MISC(label), 0, 0);

			info->account = gaim_gtk_account_option_menu_new(NULL, FALSE,
					G_CALLBACK(show_info_select_account), NULL, info);

			gtk_table_attach_defaults(GTK_TABLE(table), info->account, 1, 2, 1, 2);
			gtk_label_set_mnemonic_widget(GTK_LABEL(label), GTK_WIDGET(info->account));
		}

		g_signal_connect(G_OBJECT(imdialog), "response", G_CALLBACK(do_im), info);
	}

	gtk_widget_show_all(imdialog);
	if (info)
		gtk_widget_grab_focus(GTK_WIDGET(info->entry));
}

void show_info_dialog()
{
	GtkWidget *window, *hbox, *vbox;
	GtkWidget *label;
	GtkWidget *img = gtk_image_new_from_stock(GAIM_STOCK_DIALOG_QUESTION, GTK_ICON_SIZE_DIALOG);
	GtkWidget *table;
	struct getuserinfo *info = g_new0(struct getuserinfo, 1);
	GaimGtkBuddyList *gtkblist;

	gtkblist = GAIM_GTK_BLIST(gaim_get_blist());

	info->gc = gaim_connections_get_all()->data;

	window = gtk_dialog_new_with_buttons(_("Get User Info"), gtkblist->window ? GTK_WINDOW(gtkblist->window) : NULL, 0,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
	gtk_dialog_set_default_response (GTK_DIALOG(window), GTK_RESPONSE_OK);
	gtk_container_set_border_width (GTK_CONTAINER(window), 6);
	gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
	gtk_dialog_set_has_separator(GTK_DIALOG(window), FALSE);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(window)->vbox), 12);
	gtk_container_set_border_width (GTK_CONTAINER(GTK_DIALOG(window)->vbox), 6);

	hbox = gtk_hbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(window)->vbox), hbox);
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
	gtk_misc_set_alignment(GTK_MISC(img), 0, 0);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(window), GTK_RESPONSE_OK,
			FALSE);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(hbox), vbox);

	label = gtk_label_new(_("Please enter the screenname of the person whose info you would like to view.\n"));
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	table = gtk_table_new(2, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 6);
	gtk_table_set_col_spacings(GTK_TABLE(table), 6);
	gtk_container_set_border_width(GTK_CONTAINER(table), 12);
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);

	label = gtk_label_new(NULL);
	gtk_label_set_markup_with_mnemonic(GTK_LABEL(label), _("_Screenname:"));
	gtk_misc_set_alignment(GTK_MISC(img), 0, 0);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 0, 1);

	info->entry = gtk_entry_new();
	gtk_table_attach_defaults(GTK_TABLE(table), info->entry, 1, 2, 0, 1);
	gtk_entry_set_activates_default (GTK_ENTRY(info->entry), TRUE);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), GTK_WIDGET(info->entry));

	g_signal_connect(G_OBJECT(info->entry), "changed",
			G_CALLBACK(dialog_set_ok_sensitive), window);

	if (gaim_connections_get_all()->next) {

		label = gtk_label_new(NULL);
		gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1, 2);
		gtk_label_set_markup_with_mnemonic(GTK_LABEL(label), _("_Account:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);

		info->account = gaim_gtk_account_option_menu_new(NULL, FALSE,
				G_CALLBACK(show_info_select_account), NULL, info);

		gtk_table_attach_defaults(GTK_TABLE(table), info->account, 1, 2, 1, 2);
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), GTK_WIDGET(info->account));
	}

	g_signal_connect(G_OBJECT(window), "response", G_CALLBACK(do_info), info);


	gtk_widget_show_all(window);
	if (info->entry)
		gtk_widget_grab_focus(GTK_WIDGET(info->entry));
}


static void
free_dialog(GtkWidget *w, void *data)
{
	g_free(data);
}

/*------------------------------------------------------------------------*/
/*  The dialog for SET INFO / SET DIR INFO                                */
/*------------------------------------------------------------------------*/

void do_save_info(GtkWidget *widget, struct set_info_dlg *b)
{
	gchar *junk;
	GaimConnection *gc;

	junk = gtk_text_view_get_text(GTK_TEXT_VIEW(b->text), FALSE);

	if (b->account) {
		gaim_account_set_user_info(b->account, junk);
		gc = b->account->gc;

		if (gc)
			serv_set_info(gc, gaim_account_get_user_info(b->account));
	}
	g_free(junk);
	destroy_dialog(NULL, b->window);
	g_free(b);
}

void show_set_info(GaimConnection *gc)
{
	GtkWidget *buttons;
	GtkWidget *label;
	GtkWidget *vbox;
	GtkTextBuffer *buffer;
	GtkWidget *frame;
	gchar *buf;
	GaimAccount *account;
	const char *user_info;

	struct set_info_dlg *b = g_new0(struct set_info_dlg, 1);
	account = gc->account;
	b->account = account;

	GAIM_DIALOG(b->window);
	gtk_window_set_role(GTK_WINDOW(b->window), "set_info");
	dialogwindows = g_list_prepend(dialogwindows, b->window);
	gtk_window_set_title(GTK_WINDOW(b->window), _("Set User Info"));
	g_signal_connect(G_OBJECT(b->window), "destroy", G_CALLBACK(destroy_dialog), b->window);
	gtk_widget_realize(b->window);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(b->window), vbox);

	buf = g_malloc(256);
	g_snprintf(buf, 256, _("Changing info for %s:"),
			   gaim_account_get_username(account));
	label = gtk_label_new(buf);
	g_free(buf);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 5);

	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);

	b->text = gtk_text_view_new();
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(b->text), GTK_WRAP_WORD_CHAR);
#ifdef USE_GTKSPELL
	if (gaim_prefs_get_bool("/gaim/gtk/conversations/spellcheck"))
		gtkspell_new_attach(GTK_TEXT_VIEW(b->text), NULL, NULL);
#endif
	gtk_widget_set_size_request(b->text, 300, 200);

	if ((user_info = gaim_account_get_user_info(account)) != NULL) {
		buf = g_malloc(strlen(user_info) + 1);
		gaim_strncpy_nohtml(buf, user_info, strlen(user_info) + 1);
		buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(b->text));
		gtk_text_buffer_set_text(buffer, buf, -1);
		g_free(buf);
	}

	gtk_container_add(GTK_CONTAINER(frame), b->text);
	gtk_window_set_focus(GTK_WINDOW(b->window), b->text);

	buttons = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), buttons, FALSE, FALSE, 0);

	b->save = gaim_pixbuf_button_from_stock(_("Save"), GTK_STOCK_SAVE, GAIM_BUTTON_HORIZONTAL);
	gtk_box_pack_end(GTK_BOX(buttons), b->save, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(b->save), "clicked", G_CALLBACK(do_save_info), b);

	b->cancel = gaim_pixbuf_button_from_stock(_("Cancel"), GTK_STOCK_CANCEL, GAIM_BUTTON_HORIZONTAL);
	gtk_box_pack_end(GTK_BOX(buttons), b->cancel, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(b->cancel), "clicked", G_CALLBACK(destroy_dialog), b->window);

	gtk_widget_show_all(b->window);
}


/*------------------------------------------------------------------------*/
/*  Functions Called To Add A Log                                          */
/*------------------------------------------------------------------------*/

void cancel_log(GtkWidget *widget, GaimConversation *c)
{
	GaimGtkConversation *gtkconv;

	gtkconv = GAIM_GTK_CONVERSATION(c);

	if (gtkconv->toolbar.log) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gtkconv->toolbar.log),
									   FALSE);
	}

	dialogwindows = g_list_remove(dialogwindows, gtkconv->dialogs.log);
	gtk_widget_destroy(gtkconv->dialogs.log);
	gtkconv->dialogs.log = NULL;
}

void do_log(GtkWidget *w, GaimConversation *c)
{
	GaimGtkConversation *gtkconv;
	struct log_conversation *l;
	const char *file;
	char path[PATHSIZE];

	gtkconv = GAIM_GTK_CONVERSATION(c);

	if (!find_log_info(c->name)) {
		file = gtk_file_selection_get_filename(
			GTK_FILE_SELECTION(gtkconv->dialogs.log));

		strncpy(path, file, PATHSIZE - 1);

		if (gaim_gtk_check_if_dir(path, GTK_FILE_SELECTION(gtkconv->dialogs.log)))
			return;

		l = (struct log_conversation *)g_new0(struct log_conversation, 1);
		strcpy(l->name, gaim_conversation_get_name(c));
		strcpy(l->filename, file);
		log_conversations = g_list_append(log_conversations, l);

		if (c != NULL)
			gaim_conversation_set_logging(c, TRUE);
	}

	cancel_log(NULL, c);
}

void show_log_dialog(GaimConversation *c)
{
	GaimGtkConversation *gtkconv;
	char *buf = g_malloc(BUF_LEN);

	gtkconv = GAIM_GTK_CONVERSATION(c);

	if (!gtkconv->dialogs.log) {
		gtkconv->dialogs.log = gtk_file_selection_new(_("Log Conversation"));

		gtk_file_selection_hide_fileop_buttons(
			GTK_FILE_SELECTION(gtkconv->dialogs.log));

		g_snprintf(buf, BUF_LEN - 1, "%s" G_DIR_SEPARATOR_S "%s.log",
				   gaim_home_dir(), gaim_normalize(c->account, c->name));
		g_object_set_data(G_OBJECT(gtkconv->dialogs.log), "dialog_type",
								 "log dialog");
		gtk_file_selection_set_filename(GTK_FILE_SELECTION(gtkconv->dialogs.log),
										buf);
		g_signal_connect(G_OBJECT(gtkconv->dialogs.log), "delete_event",
						 G_CALLBACK(delete_event_dialog), c);
		g_signal_connect(G_OBJECT(GTK_FILE_SELECTION(gtkconv->dialogs.log)->ok_button), "clicked",
						 G_CALLBACK(do_log), c);
		g_signal_connect(G_OBJECT(GTK_FILE_SELECTION(gtkconv->dialogs.log)->cancel_button), "clicked",
						 G_CALLBACK(cancel_log), c);
	}

	g_free(buf);

	gtk_widget_show(gtkconv->dialogs.log);
	gdk_window_raise(gtkconv->dialogs.log->window);
}

/*------------------------------------------------------*/
/* Link Dialog                                          */
/*------------------------------------------------------*/

void cancel_link(GtkWidget *widget, GaimConversation *c)
{
	GaimGtkConversation *gtkconv;
	GtkWidget *link_dialog;

	gtkconv = GAIM_GTK_CONVERSATION(c);

	if (gtkconv->toolbar.link) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtkconv->toolbar.link),
									FALSE);
	}

	link_dialog = gtkconv->dialogs.link;
	gtkconv->dialogs.link = NULL;
	destroy_dialog(NULL, link_dialog);
}

void do_insert_link(GtkWidget *w, int resp, struct linkdlg *b)
{
	GaimGtkConversation *gtkconv;
	char *open_tag;
	const char *urltext, *showtext;

	gtkconv = GAIM_GTK_CONVERSATION(b->c);

	if (resp == GTK_RESPONSE_OK) {

		open_tag = g_malloc(2048);

		urltext = gtk_entry_get_text(GTK_ENTRY(b->url));
		showtext = gtk_entry_get_text(GTK_ENTRY(b->text));

		if (!strlen(showtext))
			showtext = urltext;

		g_snprintf(open_tag, 2048, "<A HREF=\"%s\">%s", urltext, showtext);
		gaim_gtk_surround(gtkconv, open_tag, "</A>");

		g_free(open_tag);
	}

	if (gtkconv->toolbar.link) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtkconv->toolbar.link),
									FALSE);
	}

	gtkconv->dialogs.link = NULL;
	destroy_dialog(NULL, b->window);
}

void show_insert_link(GtkWidget *linky, GaimConversation *c)
{
	GaimGtkConversation *gtkconv;
	GaimGtkWindow *gtkwin;
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *hbox;
	GtkWidget *vbox;

	gtkconv = GAIM_GTK_CONVERSATION(c);
	gtkwin  = GAIM_GTK_WINDOW(gaim_conversation_get_window(c));

	if (gtkconv->dialogs.link == NULL) {
		struct linkdlg *a = g_new0(struct linkdlg, 1);
		GtkWidget *img = gtk_image_new_from_stock(GAIM_STOCK_DIALOG_QUESTION, GTK_ICON_SIZE_DIALOG);

		a->c = c;
		a->window = gtk_dialog_new_with_buttons(_("Insert Link"),
				GTK_WINDOW(gtkwin->window), 0, GTK_STOCK_CANCEL,
				GTK_RESPONSE_CANCEL, _("Insert"), GTK_RESPONSE_OK, NULL);
		gtk_dialog_set_default_response(GTK_DIALOG(a->window), GTK_RESPONSE_OK);
		g_signal_connect(G_OBJECT(a->window), "response",
						 G_CALLBACK(do_insert_link), a);

		gtk_dialog_set_default_response(GTK_DIALOG(a->window), GTK_RESPONSE_OK);
		gtk_container_set_border_width(GTK_CONTAINER(a->window), 6);
		gtk_window_set_resizable(GTK_WINDOW(a->window), FALSE);
		gtk_dialog_set_has_separator(GTK_DIALOG(a->window), FALSE);
		gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(a->window)->vbox), 12);
		gtk_container_set_border_width(
			GTK_CONTAINER(GTK_DIALOG(a->window)->vbox), 6);
		gtk_window_set_role(GTK_WINDOW(a->window), "insert_link");

		hbox = gtk_hbox_new(FALSE, 12);
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(a->window)->vbox), hbox);
		gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
		gtk_misc_set_alignment(GTK_MISC(img), 0, 0);

		vbox = gtk_vbox_new(FALSE, 0);
		gtk_container_add(GTK_CONTAINER(hbox), vbox);

		label = gtk_label_new(_("Please enter the URL and description of "
								"the link that you want to insert.  The "
								"description is optional.\n"));

		gtk_widget_set_size_request(GTK_WIDGET(label), 335, -1);
		gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
		gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

		hbox = gtk_hbox_new(FALSE, 6);
		gtk_container_add(GTK_CONTAINER(vbox), hbox);

		table = gtk_table_new(4, 2, FALSE);
		gtk_table_set_row_spacings(GTK_TABLE(table), 5);
		gtk_table_set_col_spacings(GTK_TABLE(table), 5);
		gtk_container_set_border_width(GTK_CONTAINER(table), 0);
		gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);

		label = gtk_label_new(_("URL"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 0, 1);

		a->url = gtk_entry_new();
		gtk_table_attach_defaults(GTK_TABLE(table), a->url, 1, 2, 0, 1);
		gtk_widget_grab_focus(a->url);

		gtk_entry_set_activates_default (GTK_ENTRY(a->url), TRUE);

		label = gtk_label_new(_("Description"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1, 2);

		a->text = gtk_entry_new();
		gtk_table_attach_defaults(GTK_TABLE(table), a->text, 1, 2, 1, 2);
		gtk_entry_set_activates_default (GTK_ENTRY(a->text), TRUE);

		g_signal_connect(G_OBJECT(a->window), "destroy",
						 G_CALLBACK(free_dialog), a);
		dialogwindows = g_list_prepend(dialogwindows, a->window);

		a->toggle = linky;
		gtkconv->dialogs.link = a->window;
	}

	gtk_widget_show_all(gtkconv->dialogs.link);
	gdk_window_raise(gtkconv->dialogs.link->window);
}

/*------------------------------------------------------*/
/* Color Selection Dialog                               */
/*------------------------------------------------------*/

GtkWidget *fgcseld = NULL;
GtkWidget *bgcseld = NULL;

void cancel_fgcolor(GtkWidget *widget, GaimConversation *c)
{
	GaimGtkConversation *gtkconv;

	gtkconv = GAIM_GTK_CONVERSATION(c);

	if (gtkconv->toolbar.fgcolor && widget) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtkconv->toolbar.fgcolor),
									FALSE);
	}

	dialogwindows = g_list_remove(dialogwindows, gtkconv->dialogs.fg_color);
	gtk_widget_destroy(gtkconv->dialogs.fg_color);
	gtkconv->dialogs.fg_color = NULL;
}

void cancel_bgcolor(GtkWidget *widget, GaimConversation *c)
{
	GaimGtkConversation *gtkconv;

	gtkconv = GAIM_GTK_CONVERSATION(c);

	if (gtkconv->toolbar.bgcolor && widget) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtkconv->toolbar.bgcolor),
									FALSE);
	}

	dialogwindows = g_list_remove(dialogwindows, gtkconv->dialogs.bg_color);
	gtk_widget_destroy(gtkconv->dialogs.bg_color);
	gtkconv->dialogs.bg_color = NULL;
}

void do_fgcolor(GtkWidget *widget, GtkColorSelection *colorsel)
{
	GdkColor text_color;
	GaimConversation *c;
	GaimGtkConversation *gtkconv;
	char *open_tag;

	open_tag = g_malloc(30);

	gtk_color_selection_get_current_color(colorsel, &text_color);

	c = g_object_get_data(G_OBJECT(colorsel), "gaim_conversation");
	/* GTK_IS_EDITABLE(c->entry); huh? */

	gtkconv = GAIM_GTK_CONVERSATION(c);

	gtkconv->fg_color = text_color;
	g_snprintf(open_tag, 23, "<FONT COLOR=\"#%02X%02X%02X\">",
			   text_color.red / 256,
			   text_color.green / 256,
			   text_color.blue / 256);
	gaim_gtk_surround(gtkconv, open_tag, "</FONT>");

	gaim_debug(GAIM_DEBUG_MISC, "fgcolor dialog", "#%02X%02X%02X\n",
			   text_color.red / 256,
			   text_color.green / 256,
			   text_color.blue / 256);
	g_free(open_tag);
	cancel_fgcolor(NULL, c);
}

void do_bgcolor(GtkWidget *widget, GtkColorSelection *colorsel)
{
	GdkColor text_color;
	GaimConversation *c;
	GaimGtkConversation *gtkconv;
	char *open_tag;

	open_tag = g_malloc(30);

	gtk_color_selection_get_current_color(colorsel, &text_color);

	c = g_object_get_data(G_OBJECT(colorsel), "gaim_conversation");
	/* GTK_IS_EDITABLE(c->entry); huh? */

	gtkconv = GAIM_GTK_CONVERSATION(c);

	gtkconv->bg_color = text_color;
	g_snprintf(open_tag, 25, "<BODY BGCOLOR=\"#%02X%02X%02X\">",
			   text_color.red / 256,
			   text_color.green / 256,
			   text_color.blue / 256);
	gaim_gtk_surround(gtkconv, open_tag, "</BODY>");
	gaim_debug(GAIM_DEBUG_MISC, "bgcolor dialog", "#%02X%02X%02X\n",
			   text_color.red / 256,
			   text_color.green / 256,
			   text_color.blue / 256);

	g_free(open_tag);
	cancel_bgcolor(NULL, c);
}

void show_fgcolor_dialog(GaimConversation *c, GtkWidget *color)
{
	GaimGtkConversation *gtkconv;
	GtkWidget *colorsel;
	GdkColor fgcolor;

	gtkconv = GAIM_GTK_CONVERSATION(c);

	gdk_color_parse(gaim_prefs_get_string("/gaim/gtk/conversations/fgcolor"),
					&fgcolor);

	if (color == NULL) {	/* we came from the prefs */
		if (fgcseld)
			return;

		fgcseld = gtk_color_selection_dialog_new(_("Select Text Color"));
		gtk_color_selection_set_current_color(GTK_COLOR_SELECTION
					      (GTK_COLOR_SELECTION_DIALOG(fgcseld)->colorsel), &fgcolor);
		g_signal_connect(G_OBJECT(fgcseld), "delete_event",
				   G_CALLBACK(destroy_colorsel), (void *)1);
		g_signal_connect(G_OBJECT(GTK_COLOR_SELECTION_DIALOG(fgcseld)->cancel_button),
				   "clicked", G_CALLBACK(destroy_colorsel), (void *)1);
		g_signal_connect(G_OBJECT(GTK_COLOR_SELECTION_DIALOG(fgcseld)->ok_button), "clicked",
				   G_CALLBACK(apply_color_dlg), (void *)1);
		gtk_widget_realize(fgcseld);
		gtk_widget_show(fgcseld);
		gdk_window_raise(fgcseld->window);
		return;
	}

	if (!gtkconv->dialogs.fg_color) {

		gtkconv->dialogs.fg_color = gtk_color_selection_dialog_new(_("Select Text Color"));
		colorsel = GTK_COLOR_SELECTION_DIALOG(gtkconv->dialogs.fg_color)->colorsel;
		gtk_color_selection_set_current_color(GTK_COLOR_SELECTION(colorsel), &fgcolor);
		g_object_set_data(G_OBJECT(colorsel), "gaim_conversation", c);

		g_signal_connect(G_OBJECT(gtkconv->dialogs.fg_color), "delete_event",
				   G_CALLBACK(delete_event_dialog), c);
		g_signal_connect(G_OBJECT(GTK_COLOR_SELECTION_DIALOG(gtkconv->dialogs.fg_color)->ok_button),
				   "clicked", G_CALLBACK(do_fgcolor), colorsel);
		g_signal_connect(G_OBJECT
				   (GTK_COLOR_SELECTION_DIALOG(gtkconv->dialogs.fg_color)->cancel_button),
				   "clicked", G_CALLBACK(cancel_fgcolor), c);

		gtk_widget_realize(gtkconv->dialogs.fg_color);
	}

	gtk_widget_show(gtkconv->dialogs.fg_color);
	gdk_window_raise(gtkconv->dialogs.fg_color->window);
}

void show_bgcolor_dialog(GaimConversation *c, GtkWidget *color)
{
	GaimGtkConversation *gtkconv;
	GtkWidget *colorsel;
	GdkColor bgcolor;

	gtkconv = GAIM_GTK_CONVERSATION(c);

	gdk_color_parse(gaim_prefs_get_string("/gaim/gtk/conversations/bgcolor"),
					&bgcolor);

	if (color == NULL) {	/* we came from the prefs */
		if (bgcseld)
			return;

		bgcseld = gtk_color_selection_dialog_new(_("Select Background Color"));
		gtk_color_selection_set_current_color(GTK_COLOR_SELECTION
					      (GTK_COLOR_SELECTION_DIALOG(bgcseld)->colorsel), &bgcolor);
		g_signal_connect(G_OBJECT(bgcseld), "delete_event",
				   G_CALLBACK(destroy_colorsel), NULL);
		g_signal_connect(G_OBJECT(GTK_COLOR_SELECTION_DIALOG(bgcseld)->cancel_button),
				   "clicked", G_CALLBACK(destroy_colorsel), NULL);
		g_signal_connect(G_OBJECT(GTK_COLOR_SELECTION_DIALOG(bgcseld)->ok_button), "clicked",
				   G_CALLBACK(apply_color_dlg), (void *)2);
		gtk_widget_realize(bgcseld);
		gtk_widget_show(bgcseld);
		gdk_window_raise(bgcseld->window);
		return;
	}

	if (!gtkconv->dialogs.bg_color) {

		gtkconv->dialogs.bg_color = gtk_color_selection_dialog_new(_("Select Background Color"));
		colorsel = GTK_COLOR_SELECTION_DIALOG(gtkconv->dialogs.bg_color)->colorsel;
		gtk_color_selection_set_current_color(GTK_COLOR_SELECTION(colorsel), &bgcolor);
		g_object_set_data(G_OBJECT(colorsel), "gaim_conversation", c);

		g_signal_connect(G_OBJECT(gtkconv->dialogs.bg_color), "delete_event",
				   G_CALLBACK(delete_event_dialog), c);
		g_signal_connect(G_OBJECT(GTK_COLOR_SELECTION_DIALOG(gtkconv->dialogs.bg_color)->ok_button),
				   "clicked", G_CALLBACK(do_bgcolor), colorsel);
		g_signal_connect(G_OBJECT
				   (GTK_COLOR_SELECTION_DIALOG(gtkconv->dialogs.bg_color)->cancel_button),
				   "clicked", G_CALLBACK(cancel_bgcolor), c);

		gtk_widget_realize(gtkconv->dialogs.bg_color);
	}

	gtk_widget_show(gtkconv->dialogs.bg_color);
	gdk_window_raise(gtkconv->dialogs.bg_color->window);
}

/*------------------------------------------------------------------------*/
/*  Font Selection Dialog                                                 */
/*------------------------------------------------------------------------*/

void cancel_font(GtkWidget *widget, GaimConversation *c)
{
	GaimGtkConversation *gtkconv;

	gtkconv = GAIM_GTK_CONVERSATION(c);

	if (gtkconv->toolbar.font && widget) {
		gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON(gtkconv->toolbar.font), FALSE);
	}

	if (gtkconv->dialogs.font) {
		dialogwindows = g_list_remove(dialogwindows, gtkconv->dialogs.font);
		gtk_widget_destroy(gtkconv->dialogs.font);
		gtkconv->dialogs.font = NULL;
	}
}

void apply_font(GtkWidget *widget, GtkFontSelection *fontsel)
{
	/* this could be expanded to include font size, weight, etc.
	   but for now only works with font face */
	char *fontname;
	char *space;
	GaimConversation *c = g_object_get_data(G_OBJECT(fontsel),
			"gaim_conversation");

	if(!c)
		return;

	fontname = gtk_font_selection_dialog_get_font_name(GTK_FONT_SELECTION_DIALOG(fontsel));

	space = strrchr(fontname, ' ');
	if(space && isdigit(*(space+1)))
		*space = '\0';

	gaim_gtk_set_font_face(GAIM_GTK_CONVERSATION(c), fontname);

	g_free(fontname);

	cancel_font(NULL, c);
}

void destroy_fontsel(GtkWidget *w, gpointer d)
{
	gtk_widget_destroy(fontseld);
	fontseld = NULL;
}

void show_font_dialog(GaimConversation *c, GtkWidget *font)
{
	GaimGtkConversation *gtkconv;
	char fonttif[128];
	const char *fontface;

	gtkconv = GAIM_GTK_CONVERSATION(c);

	if (!font) {		/* we came from the prefs dialog */
		if (fontseld)
			return;

		fontseld = gtk_font_selection_dialog_new(_("Select Font"));

		fontface = gaim_prefs_get_string("/gaim/gtk/conversations/font_face");

		if (fontface != NULL && *fontface != '\0') {
			g_snprintf(fonttif, sizeof(fonttif), "%s 12", fontface);
			gtk_font_selection_dialog_set_font_name(GTK_FONT_SELECTION_DIALOG(fontseld),
								fonttif);
		} else {
			gtk_font_selection_dialog_set_font_name(GTK_FONT_SELECTION_DIALOG(fontseld),
								DEFAULT_FONT_FACE " 12");
		}

		g_signal_connect(G_OBJECT(fontseld), "delete_event",
				   G_CALLBACK(destroy_fontsel), NULL);
		g_signal_connect(G_OBJECT(GTK_FONT_SELECTION_DIALOG(fontseld)->cancel_button),
				   "clicked", G_CALLBACK(destroy_fontsel), NULL);
		g_signal_connect(G_OBJECT(GTK_FONT_SELECTION_DIALOG(fontseld)->ok_button), "clicked",
				   G_CALLBACK(apply_font_dlg), fontseld);
		gtk_widget_realize(fontseld);
		gtk_widget_show(fontseld);
		gdk_window_raise(fontseld->window);
		return;
	}

	if (!gtkconv->dialogs.font) {
		gtkconv->dialogs.font = gtk_font_selection_dialog_new(_("Select Font"));

		g_object_set_data(G_OBJECT(gtkconv->dialogs.font), "gaim_conversation", c);

		if (gtkconv->fontface[0]) {
			g_snprintf(fonttif, sizeof(fonttif), "%s 12", gtkconv->fontface);
			gtk_font_selection_dialog_set_font_name(GTK_FONT_SELECTION_DIALOG(gtkconv->dialogs.font),
							       fonttif);
		} else {
			gtk_font_selection_dialog_set_font_name(GTK_FONT_SELECTION_DIALOG(gtkconv->dialogs.font),
								DEFAULT_FONT_FACE);
		}

		g_signal_connect(G_OBJECT(gtkconv->dialogs.font), "delete_event",
				   G_CALLBACK(delete_event_dialog), c);
		g_signal_connect(G_OBJECT(GTK_FONT_SELECTION_DIALOG(gtkconv->dialogs.font)->ok_button),
				   "clicked", G_CALLBACK(apply_font), gtkconv->dialogs.font);
		g_signal_connect(G_OBJECT(GTK_FONT_SELECTION_DIALOG(gtkconv->dialogs.font)->cancel_button),
				   "clicked", G_CALLBACK(cancel_font), c);

		gtk_widget_realize(gtkconv->dialogs.font);

	}

	gtk_widget_show(gtkconv->dialogs.font);
	gdk_window_raise(gtkconv->dialogs.font->window);
}

/*------------------------------------------------------------------------*/
/*  The dialog for new away messages                                      */
/*------------------------------------------------------------------------*/

static struct away_message *save_away_message(struct create_away *ca)
{
	struct away_message *am;
	gchar *away_message;

	if (!ca->mess)
		am = g_new0(struct away_message, 1);
	else {
		am = ca->mess;
	}


	g_snprintf(am->name, sizeof(am->name), "%s", gtk_entry_get_text(GTK_ENTRY(ca->entry)));
	away_message = gtk_text_view_get_text(GTK_TEXT_VIEW(ca->text), FALSE);

	g_snprintf(am->message, sizeof(am->message), "%s", away_message);
	g_free(away_message);

	if (!ca->mess) {
		away_messages = g_slist_insert_sorted(away_messages, am, sort_awaymsg_list);
	}

	do_away_menu(NULL);
	gaim_status_sync();

	return am;
}

int check_away_mess(struct create_away *ca, int type)
{
	char *msg;
	if ((strlen(gtk_entry_get_text(GTK_ENTRY(ca->entry))) == 0) && (type == 1)) {
		/* We shouldn't allow a blank title */
		gaim_notify_error(NULL, NULL,
						  _("You cannot save an away message with a "
							"blank title"),
						  _("Please give the message a title, or choose "
							"\"Use\" to use without saving."));
		return 0;
	}

	msg = gtk_text_view_get_text(GTK_TEXT_VIEW(ca->text), FALSE);

	if (!msg && (type <= 1)) {
		/* We shouldn't allow a blank message */
		gaim_notify_error(NULL, NULL,
						  _("You cannot create an empty away message"), NULL);
		return 0;
	}

	g_free(msg);

	return 1;
}

void save_away_mess(GtkWidget *widget, struct create_away *ca)
{
	if (!check_away_mess(ca, 1))
		return;

	save_away_message(ca);
	destroy_dialog(NULL, ca->window);
	g_free(ca);
}

void use_away_mess(GtkWidget *widget, struct create_away *ca)
{
	static struct away_message am;
	gchar *away_message;

	if (!check_away_mess(ca, 0))
		return;

	g_snprintf(am.name, sizeof(am.name), "%s", gtk_entry_get_text(GTK_ENTRY(ca->entry)));
	away_message = gtk_text_view_get_text(GTK_TEXT_VIEW(ca->text), FALSE);

	g_snprintf(am.message, sizeof(am.message), "%s", away_message);
	g_free(away_message);

	do_away_message(NULL, &am);

	destroy_dialog(NULL, ca->window);
	g_free(ca);
}

void su_away_mess(GtkWidget *widget, struct create_away *ca)
{
	if (!check_away_mess(ca, 1))
		return;
	do_away_message(NULL, save_away_message(ca));
	destroy_dialog(NULL, ca->window);
	g_free(ca);
}

void create_away_mess(GtkWidget *widget, void *dummy)
{
	GtkWidget *hbox;
	GtkWidget *titlebox;
	GtkWidget *tbox;
	GtkWidget *label;
	GtkWidget *frame;
	GtkWidget *fbox;
	GtkWidget *button;

	struct create_away *ca = g_new0(struct create_away, 1);

	/* Set up window */
	GAIM_DIALOG(ca->window);
	gtk_widget_set_size_request(ca->window, -1, 250);
	gtk_container_set_border_width(GTK_CONTAINER(ca->window), 5);
	gtk_window_set_role(GTK_WINDOW(ca->window), "away_mess");
	gtk_window_set_title(GTK_WINDOW(ca->window), _("New away message"));
	g_signal_connect(G_OBJECT(ca->window), "delete_event",
			   G_CALLBACK(destroy_dialog), ca->window);
	gtk_widget_realize(ca->window);

	tbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(ca->window), tbox);

	frame = gtk_frame_new(_("New away message"));
	gtk_box_pack_start(GTK_BOX(tbox), frame, TRUE, TRUE, 0);

	fbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(fbox), 5);
	gtk_container_add(GTK_CONTAINER(frame), fbox);

	titlebox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(fbox), titlebox, FALSE, FALSE, 0);

	label = gtk_label_new(_("Away title: "));
	gtk_box_pack_start(GTK_BOX(titlebox), label, FALSE, FALSE, 0);

	ca->entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(titlebox), ca->entry, TRUE, TRUE, 0);
	gtk_widget_grab_focus(ca->entry);

	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(fbox), frame, TRUE, TRUE, 0);

	ca->text = gtk_text_view_new();
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(ca->text), GTK_WRAP_WORD_CHAR);

#ifdef USE_GTKSPELL
 	if (gaim_prefs_get_bool("/gaim/gtk/conversations/spellcheck"))
		gtkspell_new_attach(GTK_TEXT_VIEW(ca->text), NULL, NULL);
#endif

	gtk_container_add(GTK_CONTAINER(frame), ca->text);

	if (dummy) {
		struct away_message *amt;
		GtkTreeIter iter;
		int pos = 0;
		GtkListStore *ls = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(dummy)));
		GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(dummy));
		GValue val = { 0, };
		GtkTextIter start;
		GtkTextBuffer *buffer;

		if (! gtk_tree_selection_get_selected (sel, (GtkTreeModel**)&ls, &iter))
			return;
		gtk_tree_model_get_value (GTK_TREE_MODEL(ls), &iter, 1, &val);
		amt = g_value_get_pointer (&val);
		gtk_entry_set_text(GTK_ENTRY(ca->entry), amt->name);
		buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ca->text));
		gtk_text_buffer_get_iter_at_offset(buffer, &start, pos);
		gtk_text_buffer_insert(buffer, &start, amt->message, strlen(amt->message));

		ca->mess = amt;
	}

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(tbox), hbox, FALSE, FALSE, 0);

	button = gaim_pixbuf_button_from_stock(_("Save"), GTK_STOCK_SAVE, GAIM_BUTTON_HORIZONTAL);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(save_away_mess), ca);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);

	button = gaim_pixbuf_button_from_stock(_("Save & Use"), GTK_STOCK_OK, GAIM_BUTTON_HORIZONTAL);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(su_away_mess), ca);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);

	button = gaim_pixbuf_button_from_stock(_("Use"), GTK_STOCK_EXECUTE, GAIM_BUTTON_HORIZONTAL);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(use_away_mess), ca);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);

	button = gaim_pixbuf_button_from_stock(_("Cancel"), GTK_STOCK_CANCEL, GAIM_BUTTON_HORIZONTAL);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(destroy_dialog), ca->window);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);

	gtk_widget_show_all(ca->window);
}

/* smiley dialog */

void close_smiley_dialog(GtkWidget *widget, GaimConversation *c)
{
	GaimGtkConversation *gtkconv;

	gtkconv = GAIM_GTK_CONVERSATION(c);

	if (gtkconv->toolbar.smiley) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtkconv->toolbar.smiley),
									FALSE);
	}
	if(gtkconv->dialogs.smiley) {
		dialogwindows = g_list_remove(dialogwindows, gtkconv->dialogs.smiley);
		gtk_widget_destroy(gtkconv->dialogs.smiley);
		gtkconv->dialogs.smiley = NULL;
	}
}

void insert_smiley_text(GtkWidget *widget, GaimConversation *c)
{
	GaimGtkConversation *gtkconv;
	char *smiley_text = g_object_get_data(G_OBJECT(widget), "smiley_text");
	GtkTextMark *select_mark, *insert_mark;
	GtkTextIter select_iter, insert_iter;

	gtkconv = GAIM_GTK_CONVERSATION(c);

	select_mark = gtk_text_buffer_get_selection_bound(gtkconv->entry_buffer);
	insert_mark = gtk_text_buffer_get_insert(gtkconv->entry_buffer);

	if(insert_mark != select_mark) { /* there is text selected */
		gtk_text_buffer_get_iter_at_mark(gtkconv->entry_buffer, &select_iter, select_mark);
		gtk_text_buffer_get_iter_at_mark(gtkconv->entry_buffer, &insert_iter, insert_mark);
		gtk_text_buffer_delete(gtkconv->entry_buffer, &select_iter, &insert_iter);
	}

	gtk_text_buffer_insert_at_cursor(gtkconv->entry_buffer, smiley_text, -1);
	close_smiley_dialog(NULL, c);
}

static void add_smiley(GaimConversation *c, GtkWidget *table, int row, int col, char *filename, char *face)
{
	GtkWidget *image;
	GtkWidget *button;
	GaimGtkConversation *gtkconv = GAIM_GTK_CONVERSATION(c);

	image = gtk_image_new_from_file(filename);
	button = gtk_button_new();
	gtk_container_add(GTK_CONTAINER(button), image);
	g_object_set_data(G_OBJECT(button), "smiley_text", face);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(insert_smiley_text), c);

	gtk_tooltips_set_tip(gtkconv->tooltips, button, face, NULL);

	gtk_table_attach_defaults(GTK_TABLE(table), button, col, col+1, row, row+1);

	/* these look really weird with borders */
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);

	gtk_widget_show(button);
}

static gboolean smiley_is_unique(GSList *list, GtkIMHtmlSmiley *smiley) {
	while(list) {
		GtkIMHtmlSmiley *cur = list->data;
		if(!strcmp(cur->file, smiley->file))
			return FALSE;
		list = list->next;
	}
	return TRUE;
}

void show_smiley_dialog(GaimConversation *c, GtkWidget *widget)
{
	GaimGtkConversation *gtkconv;
	GtkWidget *dialog;
	GtkWidget *smiley_table = NULL;
	GSList *smileys, *unique_smileys = NULL;
	int width;
	int row = 0, col = 0;

	gtkconv = GAIM_GTK_CONVERSATION(c);

	if (gtkconv->dialogs.smiley)
		return;

	if(c->account)
		smileys = get_proto_smileys(
			gaim_account_get_protocol(gaim_conversation_get_account(c)));
	else
		smileys = get_proto_smileys(GAIM_PROTO_DEFAULT);

	while(smileys) {
		GtkIMHtmlSmiley *smiley = smileys->data;
		if(!smiley->hidden) {
			if(smiley_is_unique(unique_smileys, smiley))
					unique_smileys = g_slist_append(unique_smileys, smiley);
		}
		smileys = smileys->next;
	}


	width = floor(sqrt(g_slist_length(unique_smileys)));

	GAIM_DIALOG(dialog);
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_window_set_role(GTK_WINDOW(dialog), "smiley_dialog");
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);

	smiley_table = gtk_table_new(width, width, TRUE);

	/* pack buttons */

	while(unique_smileys) {
		GtkIMHtmlSmiley *smiley = unique_smileys->data;
		if(!smiley->hidden) {
			add_smiley(c, smiley_table, row, col, smiley->file, smiley->smile);
			if(++col >= width) {
				col = 0;
				row++;
			}
		}
		unique_smileys = unique_smileys->next;
	}

	gtk_container_add(GTK_CONTAINER(dialog), smiley_table);

	gtk_widget_show(smiley_table);

	gtk_container_set_border_width(GTK_CONTAINER(dialog), 5);

	/* connect signals */
	g_object_set_data(G_OBJECT(dialog), "dialog_type", "smiley dialog");
	g_signal_connect(G_OBJECT(dialog), "delete_event",
					 G_CALLBACK(delete_event_dialog), c);

	/* show everything */
	gtk_window_set_title(GTK_WINDOW(dialog), _("Smile!"));
	gtk_widget_show_all(dialog);

	gtkconv->dialogs.smiley = dialog;

	return;
}

static void
alias_chat_cb(GaimChat *chat, const char *new_alias)
{
	gaim_blist_alias_chat(chat, new_alias);
	gaim_blist_save();
}

void
alias_dialog_blist_chat(GaimChat *chat)
{
	gaim_request_input(NULL, _("Alias Chat"), _("Alias chat"),
					   _("Please enter an aliased name for this chat."),
					   chat->alias, FALSE, FALSE,
					   _("OK"), G_CALLBACK(alias_chat_cb),
					   _("Cancel"), NULL, chat);
}

static void
alias_contact_cb(GaimContact *contact, const char *new_alias)
{
	gaim_contact_set_alias(contact, new_alias);
	gaim_blist_save();
}

void
alias_dialog_contact(GaimContact *contact)
{
	gaim_request_input(NULL, _("Alias Contact"), _("Alias contact"),
			_("Please enter an aliased name for this contact."),
			contact->alias, FALSE, FALSE,
			_("OK"), G_CALLBACK(alias_contact_cb),
			_("Cancel"), NULL, contact);
}

static void
alias_buddy_cb(GaimBuddy *buddy, GaimRequestFields *fields)
{
	const char *alias;

	alias = gaim_request_fields_get_string(fields, "alias");

	gaim_blist_alias_buddy(buddy,
						   (alias != NULL && *alias != '\0') ? alias : NULL);
	serv_alias_buddy(buddy);
	gaim_blist_save();
}

void
alias_dialog_bud(GaimBuddy *b)
{
	GaimRequestFields *fields;
	GaimRequestFieldGroup *group;
	GaimRequestField *field;

	fields = gaim_request_fields_new();

	group = gaim_request_field_group_new(NULL);
	gaim_request_fields_add_group(fields, group);

	field = gaim_request_field_string_new("screenname", _("_Screenname"),
										  b->name, FALSE);
	gaim_request_field_string_set_editable(field, FALSE);
	gaim_request_field_group_add_field(group, field);

	field = gaim_request_field_string_new("alias", _("_Alias"),
										  b->alias, FALSE);
	gaim_request_field_group_add_field(group, field);

	gaim_request_fields(NULL, _("Alias Buddy"),
						_("Alias buddy"),
						_("Please enter an aliased name for the person "
						  "below, or rename this contact in your buddy list."),
						fields,
						_("OK"), G_CALLBACK(alias_buddy_cb),
						_("Cancel"), NULL,
						b);
}

static gboolean dont_destroy(gpointer a, gpointer b, gpointer c)
{
	return TRUE;
}

static void do_save_log(GtkWidget *w, GtkWidget *filesel)
{
	const char *file;
	char path[PATHSIZE];
	char buf[BUF_LONG];
	char error[BUF_LEN];
	FILE *fp_old, *fp_new;
	char filename[PATHSIZE];
	char *name;
	char *tmp;

	name = g_object_get_data(G_OBJECT(filesel), "name");
	tmp = gaim_user_dir();
	g_snprintf(filename, PATHSIZE, "%s" G_DIR_SEPARATOR_S "logs" G_DIR_SEPARATOR_S "%s%s", tmp,
		   name ? gaim_normalize(NULL, name) : "system", name ? ".log" : "");

	file = (const char*)gtk_file_selection_get_filename(GTK_FILE_SELECTION(filesel));
	strncpy(path, file, PATHSIZE - 1);
	if (gaim_gtk_check_if_dir(path, GTK_FILE_SELECTION(filesel)))
		return;

	if ((fp_new = fopen(path, "w")) == NULL) {
		g_snprintf(error, BUF_LONG,
			   _("Couldn't write to %s."), path);
		gaim_notify_error(NULL, NULL, error, strerror(errno));
		return;
	}

	if ((fp_old = fopen(filename, "r")) == NULL) {
		g_snprintf(error, BUF_LONG,
			   _("Couldn't write to %s."), filename);
		gaim_notify_error(NULL, NULL, error, strerror(errno));
		fclose(fp_new);
		return;
	}

	while (fgets(buf, BUF_LONG, fp_old))
		fputs(buf, fp_new);
	fclose(fp_old);
	fclose(fp_new);

	gtk_widget_destroy(filesel);

	return;
}

static void show_save_log(GtkWidget *w, gchar *name)
{
	GtkWidget *filesel;
	gchar buf[BUF_LEN];

	g_snprintf(buf, BUF_LEN - 1, "%s" G_DIR_SEPARATOR_S "%s%s", gaim_home_dir(),
		   name ? gaim_normalize(NULL, name) : "system", name ? ".log" : "");

	filesel = gtk_file_selection_new(_("Save Log File"));
	g_signal_connect(G_OBJECT(filesel), "delete_event",
			   G_CALLBACK(destroy_dialog), filesel);

	gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(filesel));
	gtk_file_selection_set_filename(GTK_FILE_SELECTION(filesel), buf);
	g_signal_connect(G_OBJECT(GTK_FILE_SELECTION(filesel)->ok_button),
			   "clicked", G_CALLBACK(do_save_log), filesel);
	g_signal_connect(G_OBJECT(GTK_FILE_SELECTION(filesel)->cancel_button),
			   "clicked", G_CALLBACK(destroy_dialog), filesel);
	g_object_set_data(G_OBJECT(filesel), "name", name);

	gtk_widget_realize(filesel);
	gtk_widget_show(filesel);

	return;
}

static void do_clear_log_file(struct view_log *view)
{
	gchar *filename, *buf;
	char *tmp;

	tmp = gaim_user_dir();
	filename = g_strdup_printf("%s" G_DIR_SEPARATOR_S "logs" G_DIR_SEPARATOR_S "%s%s", tmp,
		   view ? gaim_normalize(NULL, view->name) : "system", view ? ".log" : "");

	if ((remove(filename)) == -1) {
		buf = g_strdup_printf(_("Couldn't remove file %s." ), filename);
		gaim_notify_error(NULL, NULL, buf, strerror(errno));
		g_free(buf);
	}

	g_free(filename);

	gtk_widget_destroy(view->window);
}

static void show_clear_log(GtkWidget *w, struct view_log *view)
{
	char *text;

	if (view->clear_handle != NULL)
		return;

	text = g_strdup_printf(_("You are about to remove the log file for %s.  Do you want to continue?"),
						   view->name);
	view->clear_handle = gaim_request_action(NULL, NULL, _("Remove Log"),
											 text, -1, view, 2,
											 _("Remove Log"),
											 G_CALLBACK(do_clear_log_file),
											 _("Cancel"), NULL);
	g_free(text);
}

static void log_show_convo(struct view_log *view)
{
	gchar buf[BUF_LONG];
	FILE *fp;
	char filename[256];
	int i=0;
	GString *string;
	guint block;

	string = g_string_new("");

	if (view->name) {
		char *tmp = gaim_user_dir();
		g_snprintf(filename, 256, "%s" G_DIR_SEPARATOR_S "logs" G_DIR_SEPARATOR_S "%s.log", tmp, gaim_normalize(NULL, view->name));
	} else {
		char *tmp = gaim_user_dir();
		g_snprintf(filename, 256, "%s" G_DIR_SEPARATOR_S "logs" G_DIR_SEPARATOR_S "system", tmp);
	}
	if ((fp = fopen(filename, "r")) == NULL) {
		if (view->name) {
			g_snprintf(buf, BUF_LONG, _("Couldn't open log file %s."), filename);
			gaim_notify_error(NULL, NULL, buf, strerror(errno));
		}
		/* If the system log doesn't exist.. no message just show empty system log window.
		   That way user knows that the log is empty :)
		*/
		return;
	}

	gtk_widget_set_sensitive(view->bbox, FALSE);
	g_signal_handlers_disconnect_by_func(G_OBJECT(view->window),
				      G_CALLBACK(destroy_dialog), view->window);
	block = g_signal_connect(G_OBJECT(view->window), "delete_event",
				   G_CALLBACK(dont_destroy), view->window);

	fseek(fp, view->offset, SEEK_SET);
	gtk_imhtml_clear(GTK_IMHTML(view->layout));
	/*
	while (gtk_events_pending())
		gtk_main_iteration();
	*/

	while (fgets(buf, BUF_LONG, fp) && !strstr(buf, "---- New C")) {
		i++;
		if (strlen(buf) >= 5 && (!strncmp(buf + strlen(buf) - 5, "<BR>\n", 5)))
			/* take off the \n */
			buf[strlen(buf) - 1] = '\0';

		/* don't lose the thirtieth line of conversation. thanks FeRD */
		g_string_append(string, buf);

		if (i == 30) {
			gtk_imhtml_append_text(GTK_IMHTML(view->layout), string->str, view->options);
			g_string_free(string, TRUE);
			string = g_string_new("");
			/* you can't have these anymore. if someone clicks on another item while one is
			 * drawing, it will try to move to that item, and that causes problems here.
			while (gtk_events_pending())
				gtk_main_iteration();
			*/
			i = 0;
		}

	}
	gtk_imhtml_append_text(GTK_IMHTML(view->layout), string->str, view->options);
	gtk_imhtml_append_text(GTK_IMHTML(view->layout), "<BR>", view->options);

	gtk_widget_set_sensitive(view->bbox, TRUE);
	g_signal_handler_disconnect(G_OBJECT(view->window), block);
	g_signal_connect(G_OBJECT(view->window), "delete_event",
			   G_CALLBACK(destroy_dialog), view->window);
	g_string_free(string, TRUE);
	fclose(fp);
}

static void log_select_convo(GtkTreeSelection *sel, GtkTreeModel *model)
{
	GValue val = { 0, };
	GtkTreeIter iter;

	if(!gtk_tree_selection_get_selected(sel, &model, &iter))
		return;
	gtk_tree_model_get_value(model, &iter, 1, &val);
	log_show_convo(g_value_get_pointer(&val));
}

static void des_view_item(GtkObject *obj, struct view_log *view)
{
	if (view->name)
		g_free(view->name);
	if (view->clear_handle)
		gaim_request_close(GAIM_REQUEST_ACTION, view->clear_handle);
	g_free(view);
}

static void des_log_win(GObject *win, gpointer data)
{
	char *x = g_object_get_data(win, "log_window");
	if (x)
		g_free(x);
	x = g_object_get_data(win, "name");
	if (x)
		g_free(x);
}

void conv_show_log(GtkWidget *w, gpointer data)
{
	char *name = g_strdup(data);
	show_log(name);
	g_free(name);
}

void chat_show_log(GtkWidget *w, gpointer data)
{
	char *name = g_strdup_printf("%s.chat", (char*)data);
	show_log(name);
	g_free(name);
}

static void
url_clicked_cb(GtkWidget *widget, const char *uri)
{
	gaim_notify_uri(NULL, uri);
}

void show_log(char *nm)
{
	gchar filename[256];
	gchar buf[BUF_LONG];
	FILE *fp;
	GtkWidget *window;
	GtkWidget *box;
	GtkWidget *hbox;
	GtkWidget *bbox;
	GtkWidget *sw;
	GtkWidget *layout;
	GtkWidget *close_button;
	GtkWidget *clear_button;
	GtkWidget *save_button;
	GtkListStore *list_store;
	GtkWidget *tree_view;
	GtkTreeSelection *sel = NULL;
	GtkTreePath *path;
	GtkWidget *item = NULL;
	GtkWidget *last = NULL;
	GtkWidget *frame;
	struct view_log *view = NULL;
	char *name = nm ? g_strdup(nm) : NULL;

	int options;
	guint block;
	char convo_start[32];
	long offset = 0;

	options = GTK_IMHTML_NO_COMMENTS | GTK_IMHTML_NO_TITLE | GTK_IMHTML_NO_SCROLL;

	if (gaim_prefs_get_bool("/gaim/gtk/conversations/ignore_colors"))
		options ^= GTK_IMHTML_NO_COLOURS;

	if (gaim_prefs_get_bool("/gaim/gtk/conversations/ignore_fonts"))
		options ^= GTK_IMHTML_NO_FONTS;

	if (gaim_prefs_get_bool("/gaim/gtk/conversations/ignore_font_sizes"))
		options ^= GTK_IMHTML_NO_SIZES;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_object_set_data(G_OBJECT(window), "name", name);
	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(des_log_win), NULL);
	gtk_window_set_role(GTK_WINDOW(window), "log");
	if (name)
		g_snprintf(buf, BUF_LONG, _("Conversations with %s"), name);
	else
		g_snprintf(buf, BUF_LONG, _("System Log"));
	gtk_window_set_title(GTK_WINDOW(window), buf);
	gtk_container_set_border_width(GTK_CONTAINER(window), 10);
	gtk_window_set_resizable(GTK_WINDOW(window), TRUE);
	block = g_signal_connect(G_OBJECT(window), "delete_event",
				   G_CALLBACK(dont_destroy), window);
	gtk_widget_realize(window);

	layout = gtk_imhtml_new(NULL, NULL);
	bbox = gtk_hbox_new(FALSE, 0);

	box = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(window), box);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box), hbox, TRUE, TRUE, 0);

	if (name) {
		char *tmp = gaim_user_dir();
		g_snprintf(filename, 256, "%s" G_DIR_SEPARATOR_S "logs" G_DIR_SEPARATOR_S "%s.log", tmp, gaim_normalize(NULL, name));
		if ((fp = fopen(filename, "r")) == NULL) {
			g_snprintf(buf, BUF_LONG, _("Couldn't open log file %s."), filename);
			gaim_notify_error(NULL, NULL, buf, strerror(errno));
			return;
		}

		list_store = gtk_list_store_new(2,
				G_TYPE_STRING,
				G_TYPE_POINTER);

		tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));

		gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree_view), FALSE);

		gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_view),
				-1, "", gtk_cell_renderer_text_new(), "text", 0, NULL);

		sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));
		g_signal_connect(G_OBJECT(sel), "changed",
				G_CALLBACK(log_select_convo),
				NULL);

		frame = gtk_frame_new(_("Date"));
		gtk_widget_show(frame);

		sw = gtk_scrolled_window_new(NULL, NULL);
		gtk_container_set_border_width(GTK_CONTAINER(sw), 5);
		gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw), tree_view);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
					       GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
		gtk_widget_set_size_request(sw, 220, 220);
		gtk_container_add(GTK_CONTAINER(frame), sw);
		gtk_box_pack_start(GTK_BOX(hbox), frame, TRUE, TRUE, 0);

		while (fgets(buf, BUF_LONG, fp)) {
			if (strstr(buf, "---- New C")) {
				int length;
				char *temp = strchr(buf, '@');
				GtkTreeIter iter;

				if (temp == NULL || strlen(temp) < 2)
					continue;

				temp++;
				length = strcspn(temp, "-");
				if (length > 31) length = 31;

				offset = ftell(fp);
				g_snprintf(convo_start, length, "%s", temp);
				gtk_list_store_append(list_store, &iter);
				view = g_new0(struct view_log, 1);
				view->options = options;
				view->offset = offset;
				view->name = g_strdup(name);
				view->bbox = bbox;
				view->window = window;
				view->layout = layout;
				gtk_list_store_set(list_store, &iter,
						0, convo_start,
						1, view,
						-1);
				g_signal_connect(G_OBJECT(window), "destroy",
						   G_CALLBACK(des_view_item), view);
				last = item;
			}
		}
		fclose(fp);

		path = gtk_tree_path_new_first();
		gtk_tree_selection_select_path(sel, path);
		gtk_tree_path_free(path);

		g_object_unref(G_OBJECT(list_store));
	}


	g_signal_handler_disconnect(G_OBJECT(window), block);
	g_signal_connect(G_OBJECT(window), "delete_event", G_CALLBACK(destroy_dialog), window);

	frame = gtk_frame_new(_("Log"));
	gtk_widget_show(frame);

	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_set_border_width(GTK_CONTAINER(sw), 5);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_IN);
	gtk_widget_set_size_request(sw, 390, 220);
	gtk_container_add(GTK_CONTAINER(frame), sw);
	gtk_box_pack_start(GTK_BOX(hbox), frame, TRUE, TRUE, 0);

	g_signal_connect(G_OBJECT(layout), "url_clicked",
					 G_CALLBACK(url_clicked_cb), NULL);
	gtk_container_add(GTK_CONTAINER(sw), layout);
	gaim_setup_imhtml(layout);

	gtk_box_pack_start(GTK_BOX(box), bbox, FALSE, FALSE, 0);
	gtk_widget_set_sensitive(bbox, FALSE);

	close_button = gaim_pixbuf_button_from_stock(_("Close"), GTK_STOCK_CLOSE, GAIM_BUTTON_HORIZONTAL);
	gtk_box_pack_end(GTK_BOX(bbox), close_button, FALSE, FALSE, 5);
	g_signal_connect(G_OBJECT(close_button), "clicked", G_CALLBACK(destroy_dialog), window);

	clear_button = gaim_pixbuf_button_from_stock(_("Clear"), GTK_STOCK_CLEAR, GAIM_BUTTON_HORIZONTAL);
	g_object_set_data(G_OBJECT(clear_button), "log_window", window);
	gtk_box_pack_end(GTK_BOX(bbox), clear_button, FALSE, FALSE, 5);
	g_signal_connect(G_OBJECT(clear_button), "clicked", G_CALLBACK(show_clear_log), view);

	save_button = gaim_pixbuf_button_from_stock(_("Save"), GTK_STOCK_SAVE, GAIM_BUTTON_HORIZONTAL);
	gtk_box_pack_end(GTK_BOX(bbox), save_button, FALSE, FALSE, 5);
	g_signal_connect(G_OBJECT(save_button), "clicked", G_CALLBACK(show_save_log), name);

	gtk_widget_show_all(window);

	if (!name) {
		view = g_new0(struct view_log, 1);
		view->options = options;
		view->name = NULL;
		view->bbox = bbox;
		view->window = window;
		view->layout = layout;
		log_show_convo(view);
		g_signal_connect(G_OBJECT(layout), "destroy", G_CALLBACK(des_view_item), view);
	}

	gtk_widget_set_sensitive(bbox, TRUE);

	return;
}

