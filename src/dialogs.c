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
#include "internal.h"

#include "debug.h"
#include "log.h"
#include "multi.h"
#include "notify.h"
#include "prefs.h"
#include "privacy.h"
#include "prpl.h"
#include "request.h"
#include "util.h"

#include "gtkblist.h"
#include "gtkconv.h"
#include "gtkimhtml.h"
#include "gtkprefs.h"
#include "gtkutils.h"
#include "stock.h"

#include "ui.h"

/* XXX For the soon-to-be-deprecated MultiEntryDlg stuff */
#include "gaim.h"

static GtkWidget *imdialog = NULL;	/*I only want ONE of these :) */
static GList *dialogwindows = NULL;
static GtkWidget *importdialog;
static GaimConnection *importgc;
static GtkWidget *icondlg;
static GtkWidget *alias_dialog = NULL;
static GtkWidget *rename_dialog = NULL;
static GtkWidget *rename_bud_dialog = NULL;
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

struct addbuddy {
	GtkWidget *window;
	GtkWidget *combo;
	GtkWidget *entry;
	GtkWidget *entry_for_alias;
	GtkWidget *account;
	GaimConnection *gc;
};

struct addperm {
	GtkWidget *window;
	GtkWidget *entry;
	GaimConnection *gc;
	gboolean permit;
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

struct alias_dialog_info
{
	GtkWidget *window;
	GtkWidget *name_entry;
	GtkWidget *alias_entry;
	struct buddy *buddy;
};

static GSList *info_dlgs = NULL;

static struct info_dlg *find_info_dlg(GaimConnection *gc, const char *who)
{
	GSList *i = info_dlgs;
	while (i) {
		struct info_dlg *d = i->data;
		i = i->next;
		if (d->gc != gc)
			continue;
		if (d->who == NULL)
			continue;
		if (!who)
			continue;
		if (!gaim_utf8_strcasecmp(normalize(who), d->who))
			return d;
	}
	return NULL;
}

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
	else if (dest == rename_bud_dialog)
		rename_bud_dialog = NULL;

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
	GaimConversation *c = gaim_find_conversation(who);

	struct warning *w = g_new0(struct warning, 1);
	w->who = who;
	w->gc = gc;

	gtk_misc_set_alignment(GTK_MISC(img), 0, 0);

	w->window = gtk_dialog_new_with_buttons(_("Warn User"), GTK_WINDOW(c->window), 0, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, _("_Warn"), GTK_RESPONSE_OK, NULL);
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

void do_remove_chat(struct chat *chat)
{
	gaim_blist_remove_chat(chat);
	gaim_blist_save();
}

void do_remove_buddy(struct buddy *b)
{
	struct group *g;
	GaimConversation *c;
	gchar *name;

	if (!b)
		return;

	g = gaim_find_buddys_group(b);
	name = g_strdup(b->name); /* b->name is null after remove_buddy */

	gaim_debug(GAIM_DEBUG_INFO, "blist",
			   "Removing '%s' from buddy list.\n", b->name);
	serv_remove_buddy(b->account->gc, name, g->name);
	gaim_blist_remove_buddy(b);
	gaim_blist_save();

	c = gaim_find_conversation(name);

	if (c != NULL)
		gaim_conversation_update(c, GAIM_CONV_UPDATE_REMOVE);

	g_free(name);
}

void do_remove_group(struct group *g)
{
	GaimBlistNode *b = ((GaimBlistNode*)g)->child;
	while (b) {
		if(GAIM_BLIST_NODE_IS_BUDDY(b)) {
			struct buddy *bd = (struct buddy *)b;
			GaimConversation *c = gaim_find_conversation(bd->name);
			if(bd->account->gc) {
				serv_remove_buddy(bd->account->gc, bd->name, g->name);
				gaim_blist_remove_buddy(bd);

				if (c != NULL)
					gaim_conversation_update(c, GAIM_CONV_UPDATE_REMOVE);
			}
		}
		b = b->next;
	}
	gaim_blist_remove_group(g);
	gaim_blist_save();
}

void show_confirm_del(GaimConnection *gc, gchar *name)
{
	struct buddy *bd = gaim_find_buddy(gc->account, name);
	char *text;
	if (!bd)
		return;

	text = g_strdup_printf(_("You are about to remove %s from your buddy list.  Do you want to continue?"), name);

	gaim_request_action(NULL, NULL, _("Remove Buddy"), text, -1, bd, 2,
						_("Remove Buddy"), G_CALLBACK(do_remove_buddy),
						_("Cancel"), NULL);

	g_free(text);
}

void show_confirm_del_chat(struct chat *chat)
{
	char *text = g_strdup_printf(_("You are about to remove the chat %s from your buddy list.  Do you want to continue?"), chat->alias);

	gaim_request_action(NULL, NULL, _("Remove Chat"), text, -1, chat, 2,
						_("Remove Chat"), G_CALLBACK(do_remove_chat),
						_("Cancel"), NULL);

	g_free(text);
}

void show_confirm_del_group(struct group *g)
{
	char *text = g_strdup_printf(_("You are about to remove the group %s and all its members from your buddy list.  Do you want to continue?"), 
			       g->name);

	gaim_request_action(NULL, NULL, _("Remove Group"), text, -1, g, 2,
						_("Remove Group"), G_CALLBACK(do_remove_group),
						_("Cancel"), NULL);

	g_free(text);
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

		conv = gaim_find_conversation(who);

		if (conv == NULL)
			conv = gaim_conversation_new(GAIM_CONV_IM, account, who);
		else {
			gaim_window_raise(gaim_conversation_get_window(conv));

			if (account)
				gaim_conversation_set_account(conv, account);
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
		who = g_strdup(normalize(gtk_entry_get_text(GTK_ENTRY(info->entry))));

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
	struct gaim_gtk_buddy_list *gtkblist;
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

void show_info_select_account(GObject *w, GaimAccount *account,
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
	struct gaim_gtk_buddy_list *gtkblist;
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
					G_CALLBACK(show_info_select_account), info);

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
	struct gaim_gtk_buddy_list *gtkblist;

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
				G_CALLBACK(show_info_select_account), info);

		gtk_table_attach_defaults(GTK_TABLE(table), info->account, 1, 2, 1, 2);
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), GTK_WIDGET(info->account));
	}

	g_signal_connect(G_OBJECT(window), "response", G_CALLBACK(do_info), info);
	
	
	gtk_widget_show_all(window);
	if (info->entry)
		gtk_widget_grab_focus(GTK_WIDGET(info->entry));
}


/*------------------------------------------------------------------------*/
/*  The dialog for adding buddies                                         */
/*------------------------------------------------------------------------*/

extern void add_callback(GtkWidget *, GaimConversation *);

void do_add_buddy(GtkWidget *w, int resp, struct addbuddy *a)
{
	const char *grp, *who, *whoalias;
	GaimConversation *c;
	struct buddy *b;
	struct group *g;
	void *icon_data;
	void *icon_data2;
	int icon_len;

	if (resp == GTK_RESPONSE_OK) {

		who = gtk_entry_get_text(GTK_ENTRY(a->entry));
		grp = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(a->combo)->entry));
		whoalias = gtk_entry_get_text(GTK_ENTRY(a->entry_for_alias));

		c = gaim_find_conversation(who);
		if (!(g = gaim_find_group(grp))) {
			g = gaim_group_new(grp);
			gaim_blist_add_group(g, NULL);
		}
		b = gaim_buddy_new(a->gc->account, who, whoalias);
		gaim_blist_add_buddy(b, g, NULL);
		serv_add_buddy(a->gc, who);

		if (c != NULL)
			gaim_conversation_update(c, GAIM_CONV_UPDATE_ADD);

		icon_data = get_icon_data(a->gc, normalize(who), &icon_len);

		if(icon_data) {
			icon_data2 = g_memdup(icon_data, icon_len);
			set_icon_data(a->gc, who, icon_data2, icon_len);
			g_free(icon_data2);
		}

		gaim_blist_save();
	}

	destroy_dialog(NULL, a->window);
}

void do_add_group(GtkWidget *w, int resp, struct addbuddy *a)
{
	const char *grp;
	struct group *g;

	if (resp == GTK_RESPONSE_OK) {
		grp = gtk_entry_get_text(GTK_ENTRY(a->entry));

		if (!a->gc)
			a->gc = gaim_connections_get_all()->data;

		g = gaim_group_new(grp);
		gaim_blist_add_group (g, NULL);
		gaim_blist_save();
	}

	destroy_dialog(NULL, a->window);
}


static GList *groups_tree()
{
	GList *tmp = NULL;
	char *tmp2;
	struct group *g;

	GaimBlistNode *gnode = gaim_get_blist()->root;

	if (!gnode) {
		tmp2 = g_strdup(_("Buddies"));
		tmp = g_list_append(tmp, tmp2);
	} else {
		while (gnode) {
			if(GAIM_BLIST_NODE_IS_GROUP(gnode)) {
				g = (struct group *)gnode;
				tmp2 = g->name;
				tmp = g_list_append(tmp, tmp2);
			}
			gnode = gnode->next;
		}
	}
	return tmp;
}

static void free_dialog(GtkWidget *w, struct addbuddy *a)
{
	g_free(a);
}


void show_add_group(GaimConnection *gc)
{

	GtkWidget *hbox, *vbox;
	GtkWidget *label;
	struct gaim_gtk_buddy_list *gtkblist;
	GtkWidget *img = gtk_image_new_from_stock(GAIM_STOCK_DIALOG_QUESTION, GTK_ICON_SIZE_DIALOG);
	struct addbuddy *a = g_new0(struct addbuddy, 1);

	gtkblist = GAIM_GTK_BLIST(gaim_get_blist());

	a->gc = gc;

	a->window =  gtk_dialog_new_with_buttons(_("Add Group"), GTK_WINDOW(gtkblist->window), 0, 
						 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_ADD, GTK_RESPONSE_OK, NULL);
	gtk_dialog_set_default_response (GTK_DIALOG(a->window), GTK_RESPONSE_OK);
	gtk_container_set_border_width (GTK_CONTAINER(a->window), 6);
	gtk_window_set_resizable(GTK_WINDOW(a->window), FALSE);
	gtk_dialog_set_has_separator(GTK_DIALOG(a->window), FALSE);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(a->window)->vbox), 12);
	gtk_container_set_border_width (GTK_CONTAINER(GTK_DIALOG(a->window)->vbox), 6);
		
	hbox = gtk_hbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(a->window)->vbox), hbox);
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
	gtk_misc_set_alignment(GTK_MISC(img), 0, 0);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(hbox), vbox);
		
	label = gtk_label_new(_("Please enter the name of the group to be added.\n"));
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
		
	hbox = gtk_hbox_new(FALSE, 6);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);
		
	label = gtk_label_new(NULL);
	gtk_label_set_markup_with_mnemonic(GTK_LABEL(label), _("_Group:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
		
	a->entry = gtk_entry_new();
	gtk_entry_set_activates_default (GTK_ENTRY(a->entry), TRUE);
	gtk_box_pack_start(GTK_BOX(hbox), a->entry, FALSE, FALSE, 0);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), GTK_WIDGET(a->entry));

	g_signal_connect(G_OBJECT(a->window), "response", G_CALLBACK(do_add_group), a);

	gtk_widget_show_all(a->window);
	gtk_widget_grab_focus(GTK_WIDGET(a->entry));
}

static void
addbuddy_select_account(GObject *w, GaimAccount *account,
						struct addbuddy *b)
{
	/* Save our account */
	b->gc = gaim_account_get_connection(account);
}

void show_add_buddy(GaimConnection *gc, char *buddy, char *group, char *alias)
{
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *hbox;
	GtkWidget *vbox;
	struct gaim_gtk_buddy_list *gtkblist;
	GtkWidget *img = gtk_image_new_from_stock(GAIM_STOCK_DIALOG_QUESTION, GTK_ICON_SIZE_DIALOG);
	struct addbuddy *a = g_new0(struct addbuddy, 1);
	a->gc = gc ? gc : gaim_connections_get_all()->data;

	gtkblist = GAIM_GTK_BLIST(gaim_get_blist());

	GAIM_DIALOG(a->window);
	a->window = gtk_dialog_new_with_buttons(_("Add Buddy"), gtkblist->window ? GTK_WINDOW(gtkblist->window) : NULL, 0,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_ADD, GTK_RESPONSE_OK, NULL);

	gtk_dialog_set_default_response(GTK_DIALOG(a->window), GTK_RESPONSE_OK);
	gtk_container_set_border_width(GTK_CONTAINER(a->window), 6);
	gtk_window_set_resizable(GTK_WINDOW(a->window), FALSE);
	gtk_dialog_set_has_separator(GTK_DIALOG(a->window), FALSE);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(a->window)->vbox), 12);
	gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(a->window)->vbox), 6);
	gtk_window_set_role(GTK_WINDOW(a->window), "add_buddy");

	hbox = gtk_hbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(a->window)->vbox), hbox);
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
	gtk_misc_set_alignment(GTK_MISC(img), 0, 0);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(hbox), vbox);

	label = gtk_label_new(_("Please enter the screen name of the person you would like to add to your buddy list. You may optionally enter an alias, or nickname,  for the buddy. The alias will be displayed in place of the screen name whenever possible.\n"));
	gtk_widget_set_size_request(GTK_WIDGET(label), 400, -1);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	hbox = gtk_hbox_new(FALSE, 6);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);

	g_signal_connect(G_OBJECT(a->window), "destroy", G_CALLBACK(destroy_dialog), a->window);
	g_signal_connect(G_OBJECT(a->window), "destroy", G_CALLBACK(free_dialog), a);
	dialogwindows = g_list_prepend(dialogwindows, a->window);

	table = gtk_table_new(4, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5);
	gtk_container_set_border_width(GTK_CONTAINER(table), 0);
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);

	label = gtk_label_new(_("Screen Name"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 0, 1);

	a->entry = gtk_entry_new();
	gtk_table_attach_defaults(GTK_TABLE(table), a->entry, 1, 2, 0, 1);
	gtk_widget_grab_focus(a->entry);
	
	if (buddy != NULL)
		gtk_entry_set_text(GTK_ENTRY(a->entry), buddy);

	gtk_entry_set_activates_default (GTK_ENTRY(a->entry), TRUE);

	label = gtk_label_new(_("Alias"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1, 2);

	a->entry_for_alias = gtk_entry_new();
	gtk_table_attach_defaults(GTK_TABLE(table), a->entry_for_alias, 1, 2, 1, 2);
	if (alias != NULL)
		gtk_entry_set_text(GTK_ENTRY(a->entry_for_alias), alias);
	gtk_entry_set_activates_default (GTK_ENTRY(a->entry_for_alias), TRUE);

	label = gtk_label_new(_("Group"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 2, 3);

	a->combo = gtk_combo_new();
	gtk_combo_set_popdown_strings(GTK_COMBO(a->combo), groups_tree());
	gtk_table_attach_defaults(GTK_TABLE(table), a->combo, 1, 2, 2, 3);

	/* Set up stuff for the account box */
	label = gtk_label_new(_("Add To"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 3, 4);

	a->account = gaim_gtk_account_option_menu_new(NULL, FALSE,
			G_CALLBACK(addbuddy_select_account), a);

	gtk_table_attach_defaults(GTK_TABLE(table), a->account, 1, 2, 3, 4);

	/* End of account box */

	g_signal_connect(G_OBJECT(a->window), "response",
					 G_CALLBACK(do_add_buddy), a);

	gtk_widget_show_all(a->window);

	if (group != NULL) 
		gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(a->combo)->entry), group);
}

struct addchat {
    GaimAccount *account;
    GtkWidget *window;
	GtkWidget *account_menu;
	GtkWidget *alias_entry;
	GtkWidget *group_combo;
	GtkWidget *entries_box;
	GtkSizeGroup *sg;
	GList *entries;
};

static void do_add_chat(GtkWidget *w, struct addchat *ac) {
	GHashTable *components = g_hash_table_new_full(g_str_hash, g_str_equal,
			g_free, g_free);
	GList *tmp;

	struct chat *chat;
	struct group *group;
	const char *group_name;

	for(tmp = ac->entries; tmp; tmp = tmp->next) {
		if(g_object_get_data(tmp->data, "is_spin")) {
			g_hash_table_replace(components,
					g_strdup(g_object_get_data(tmp->data, "identifier")),
					g_strdup_printf("%d",
						gtk_spin_button_get_value_as_int(tmp->data)));
		} else {
			g_hash_table_replace(components,
					g_strdup(g_object_get_data(tmp->data, "identifier")),
					g_strdup(gtk_entry_get_text(tmp->data)));
		}
	}

	chat = gaim_chat_new(ac->account, gtk_entry_get_text(GTK_ENTRY(ac->alias_entry)), components);

	group_name = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(ac->group_combo)->entry));
	if (!(group = gaim_find_group(group_name))) {
		group = gaim_group_new(group_name);
		gaim_blist_add_group(group, NULL);
	}

	if(chat) {
		gaim_blist_add_chat(chat, group, NULL);
		gaim_blist_save();
	}

	gtk_widget_destroy(ac->window);
	g_list_free(ac->entries);

	g_free(ac);
}

static void do_add_chat_resp(GtkWidget *w, int resp, struct addchat *ac) {
	if(resp == GTK_RESPONSE_OK) {
		do_add_chat(NULL, ac);
	} else {
		gtk_widget_destroy(ac->window);
		g_list_free(ac->entries);
		g_free(ac);
	}
}


static void rebuild_addchat_entries(struct addchat *ac) {
	GList *list, *tmp;
	struct proto_chat_entry *pce;
	gboolean focus = TRUE;

	while(GTK_BOX(ac->entries_box)->children)
		gtk_container_remove(GTK_CONTAINER(ac->entries_box),
				((GtkBoxChild *)GTK_BOX(ac->entries_box)->children->data)->widget);

	if(ac->entries)
		g_list_free(ac->entries);

	ac->entries = NULL;

	list = GAIM_PLUGIN_PROTOCOL_INFO(ac->account->gc->prpl)->chat_info(ac->account->gc);

	for(tmp = list; tmp; tmp = tmp->next) {
		GtkWidget *label;
		GtkWidget *rowbox;
		pce = tmp->data;

		rowbox = gtk_hbox_new(FALSE, 5);
		gtk_box_pack_start(GTK_BOX(ac->entries_box), rowbox, FALSE, FALSE, 0);

		label = gtk_label_new(pce->label);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_size_group_add_widget(ac->sg, label);
		gtk_box_pack_start(GTK_BOX(rowbox), label, FALSE, FALSE, 0);

		if(pce->is_int) {
			GtkObject *adjust;
			GtkWidget *spin;
			adjust = gtk_adjustment_new(pce->min, pce->min, pce->max,
					1, 10, 10);
			spin = gtk_spin_button_new(GTK_ADJUSTMENT(adjust), 1, 0);
			g_object_set_data(G_OBJECT(spin), "is_spin", GINT_TO_POINTER(TRUE));
			g_object_set_data(G_OBJECT(spin), "identifier", pce->identifier);
			ac->entries = g_list_append(ac->entries, spin);
			gtk_widget_set_size_request(spin, 50, -1);
			gtk_box_pack_end(GTK_BOX(rowbox), spin, FALSE, FALSE, 0);
		} else {
			GtkWidget *entry = gtk_entry_new();
			g_object_set_data(G_OBJECT(entry), "identifier", pce->identifier);
			ac->entries = g_list_append(ac->entries, entry);

			if(pce->def)
				gtk_entry_set_text(GTK_ENTRY(entry), pce->def);

			if(focus) {
				gtk_widget_grab_focus(entry);
				focus = FALSE;
			}

			gtk_box_pack_end(GTK_BOX(rowbox), entry, TRUE, TRUE, 0);

			g_signal_connect(G_OBJECT(entry), "activate",
					G_CALLBACK(do_add_chat), ac);
		}
		g_free(pce);
	}

	gtk_widget_show_all(ac->entries_box);
}

static void addchat_select_account(GObject *w, GaimConnection *gc)
{
	struct addchat *ac = g_object_get_data(w, "addchat");

	if(ac->account->protocol == gc->account->protocol) {
		ac->account = gc->account;
	} else {
		ac->account = gc->account;
		rebuild_addchat_entries(ac);
	}
}

static void create_online_account_menu_for_add_chat(struct addchat *ac)
{
	char buf[2048]; /* Never hurts to be safe ;-) */
	GList *g;
	GaimConnection *c;
	GaimAccount *account;
	GtkWidget *menu, *opt;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *image;
	GdkPixbuf *pixbuf;
	GdkPixbuf *scale;
	GtkSizeGroup *sg;
	char *filename;
	const char *proto_name;
	int count = 0;
	int place = 0;

	menu = gtk_menu_new();

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	for (g = gaim_connections_get_all(); g != NULL; g = g->next) {
		GaimPluginProtocolInfo *prpl_info = NULL;
		GaimPlugin *plugin;

		c = (GaimConnection *)g->data;
		account = gaim_connection_get_account(c);

		plugin = c->prpl;

		if (plugin == NULL)
			continue;

		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(plugin);

		if (prpl_info == NULL || prpl_info->join_chat == NULL)
			continue;

		opt = gtk_menu_item_new();

		/* Create the hbox. */
		hbox = gtk_hbox_new(FALSE, 4);
		gtk_container_add(GTK_CONTAINER(opt), hbox);
		gtk_widget_show(hbox);

		/* Load the image. */
		if (prpl_info != NULL) {
			proto_name = prpl_info->list_icon(NULL, NULL);
			g_snprintf(buf, sizeof(buf), "%s.png", proto_name);

			filename = g_build_filename(DATADIR, "pixmaps", "gaim", "status",
										"default", buf, NULL);
			pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
			g_free(filename);

			if (pixbuf != NULL) {
				/* Scale and insert the image */
				scale = gdk_pixbuf_scale_simple(pixbuf, 16, 16,
												GDK_INTERP_BILINEAR);
				image = gtk_image_new_from_pixbuf(scale);

				g_object_unref(G_OBJECT(pixbuf));
				g_object_unref(G_OBJECT(scale));
			}
			else
				image = gtk_image_new();
		}
		else
			image = gtk_image_new();

		gtk_size_group_add_widget(sg, image);

		gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
		gtk_widget_show(image);

		g_snprintf(buf, sizeof(buf), "%s (%s)",
				   gaim_account_get_username(account),
				   c->prpl->info->name);

		/* Create the label. */
		label = gtk_label_new(buf);
		gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
		gtk_widget_show(label);


		g_object_set_data(G_OBJECT(opt), "addchat", ac);
		g_signal_connect(G_OBJECT(opt), "activate",
						 G_CALLBACK(addchat_select_account), c);
		gtk_widget_show(opt);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), opt);

		/* Now check to see if it's our current menu */
		if (c->account == ac->account) {
			place = count;
			gtk_menu_item_activate(GTK_MENU_ITEM(opt));
			gtk_option_menu_set_history(GTK_OPTION_MENU(ac->account_menu),
										count);

			/* Do the cha cha cha */
		}

		count++;
	}

	g_object_unref(sg);

	gtk_option_menu_remove_menu(GTK_OPTION_MENU(ac->account_menu));
	gtk_option_menu_set_menu(GTK_OPTION_MENU(ac->account_menu), menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(ac->account_menu), place);
}

void show_add_chat(GaimAccount *account, struct group *group) {
    struct addchat *ac = g_new0(struct addchat, 1);
    struct gaim_gtk_buddy_list *gtkblist;
    GList *c;
    GaimConnection *gc;

	GtkWidget *label;
	GtkWidget *rowbox;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *img = gtk_image_new_from_stock(GAIM_STOCK_DIALOG_QUESTION,
			GTK_ICON_SIZE_DIALOG);

    gtkblist = GAIM_GTK_BLIST(gaim_get_blist());

    if (account) {
	ac->account = account;
    } else {
	/* Select an account with chat capabilities */
	for (c = gaim_connections_get_all(); c != NULL; c = c->next) {
	    gc = c->data;

	    if (GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl)->join_chat) {
		ac->account = gc->account;
		break;
	    }
	}
    }

    if (!ac->account) {
		gaim_notify_error(NULL, NULL,
						  _("You are not currently signed on with any "
							"protocols that have the ability to chat."), NULL);
		return;
    }

	ac->sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

    ac->window = gtk_dialog_new_with_buttons(_("Add Chat"),
            GTK_WINDOW(gtkblist->window), 0,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
            GTK_STOCK_ADD, GTK_RESPONSE_OK,
            NULL);

    gtk_dialog_set_default_response(GTK_DIALOG(ac->window), GTK_RESPONSE_OK);
    gtk_container_set_border_width(GTK_CONTAINER(ac->window), 6);
    gtk_window_set_resizable(GTK_WINDOW(ac->window), FALSE);
    gtk_dialog_set_has_separator(GTK_DIALOG(ac->window), FALSE);
    gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(ac->window)->vbox), 12);
    gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(ac->window)->vbox),
            6);
	gtk_window_set_role(GTK_WINDOW(ac->window), "add_chat");

	hbox = gtk_hbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(ac->window)->vbox), hbox);
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
	gtk_misc_set_alignment(GTK_MISC(img), 0, 0);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(hbox), vbox);

	label = gtk_label_new(_("Please enter an alias, and the appropriate information about the chat you would like to add to your buddy list.\n"));
	gtk_widget_set_size_request(label, 400, -1);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	rowbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), rowbox, FALSE, FALSE, 0);

	label = gtk_label_new(_("Account:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_size_group_add_widget(ac->sg, label);
	gtk_box_pack_start(GTK_BOX(rowbox), label, FALSE, FALSE, 0);

	ac->account_menu = gtk_option_menu_new();
	gtk_box_pack_end(GTK_BOX(rowbox), ac->account_menu, TRUE, TRUE, 0);

	create_online_account_menu_for_add_chat(ac);

	ac->entries_box = gtk_vbox_new(FALSE, 5);
    gtk_container_set_border_width(GTK_CONTAINER(ac->entries_box), 0);
	gtk_box_pack_start(GTK_BOX(vbox), ac->entries_box, TRUE, TRUE, 0);

	rebuild_addchat_entries(ac);

	rowbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), rowbox, FALSE, FALSE, 0);

	label = gtk_label_new(_("Alias:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_size_group_add_widget(ac->sg, label);
	gtk_box_pack_start(GTK_BOX(rowbox), label, FALSE, FALSE, 0);

	ac->alias_entry = gtk_entry_new();
	gtk_box_pack_end(GTK_BOX(rowbox), ac->alias_entry, TRUE, TRUE, 0);

	rowbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), rowbox, FALSE, FALSE, 0);

    label = gtk_label_new(_("Group:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_size_group_add_widget(ac->sg, label);
	gtk_box_pack_start(GTK_BOX(rowbox), label, FALSE, FALSE, 0);

    ac->group_combo = gtk_combo_new();
    gtk_combo_set_popdown_strings(GTK_COMBO(ac->group_combo), groups_tree());
	gtk_box_pack_end(GTK_BOX(rowbox), ac->group_combo, TRUE, TRUE, 0);

	if (group)
		gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(ac->group_combo)->entry), group->name);

	g_signal_connect(G_OBJECT(ac->window), "response", G_CALLBACK(do_add_chat_resp), ac);

	gtk_widget_show_all(ac->window);
}



/*------------------------------------------------------------------------*
 *  Privacy Settings                                                      *
 *------------------------------------------------------------------------*/
static GtkWidget *deny_type = NULL;
static GtkWidget *deny_conn_hbox = NULL;
static GtkWidget *deny_opt_menu = NULL;
static GaimConnection *current_deny_gc = NULL;
static gboolean current_is_deny = FALSE;
static GtkWidget *allow_list = NULL;
static GtkWidget *block_list = NULL;

static GtkListStore *block_store = NULL;
static GtkListStore *allow_store = NULL;

static void set_deny_mode(GtkWidget *w, int data)
{
	if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w)))
		return;

	gaim_debug(GAIM_DEBUG_INFO, "privacy", "Setting deny mode %d\n", data);
	current_deny_gc->account->perm_deny = data;
	serv_set_permit_deny(current_deny_gc);
	gaim_blist_save();
}

static GtkWidget *deny_opt(char *label, int which, GtkWidget *set)
{
	GtkWidget *opt;

	if (!set)
		opt = gtk_radio_button_new_with_label(NULL, label);
	else
		opt =
		    gtk_radio_button_new_with_label(gtk_radio_button_get_group(
						GTK_RADIO_BUTTON(set)),
						    label);

	g_signal_connect(G_OBJECT(opt), "toggled", G_CALLBACK(set_deny_mode), (void *)which);
	gtk_widget_show(opt);
	if (current_deny_gc->account->perm_deny == which)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(opt), TRUE);

	return opt;
}

static void des_deny_opt(GtkWidget *d, gpointer e)
{
	gtk_widget_destroy(d);
	current_deny_gc = NULL;
	deny_conn_hbox = NULL;
	deny_type = NULL;
	deny_opt_menu = NULL;
	current_is_deny = FALSE;
	allow_list = NULL;
	allow_store = NULL;

	block_list = NULL;
	block_store = NULL;
}

static void set_deny_type()
{
	GSList *bg = gtk_radio_button_get_group(GTK_RADIO_BUTTON(deny_type));

	switch (current_deny_gc->account->perm_deny) {
	case 5:
		bg = bg->next->next;
		break;
	case 4:
		break;
	case 3:
		bg = bg->next->next->next;
		break;
	case 2:
		bg = bg->next;
		break;
	case 1:
		bg = bg->next->next->next->next;
		break;
	}

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(bg->data), TRUE);
}

void build_allow_list()
{
	GSList *p;
	GtkListStore *ls;
	GtkTreeIter iter;

	if (!current_is_deny)
		return;

	p = current_deny_gc->account->permit;

	gtk_list_store_clear(GTK_LIST_STORE(allow_store));

	while (p) {
		ls = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(allow_list)));

		gtk_list_store_append(ls, &iter);
		gtk_list_store_set(ls, &iter, 0, p->data, -1);

		p = p->next;
	}
}


void build_block_list()
{
	GSList *d;
	GtkListStore *ls;
	GtkTreeIter iter;

	if (!current_is_deny)
		return;

	d = current_deny_gc->account->deny;

	gtk_list_store_clear(GTK_LIST_STORE(block_store));

	while (d) {
		ls = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(block_list)));

		gtk_list_store_append(ls, &iter);
		gtk_list_store_set(ls, &iter, 0, d->data, -1);

		d = d->next;
	}
}

static void deny_gc_opt(GtkWidget *opt, GaimConnection *gc)
{
	current_deny_gc = gc;
	set_deny_type();
	build_allow_list();
	build_block_list();
}

static void build_deny_menu()
{
	GtkWidget *menu;
	GtkWidget *opt;
	GList *c = gaim_connections_get_all();
	GaimConnection *gc;
	GaimAccount *account;
	int count = 0;
	gboolean found = FALSE;
	char buf[2048];

	if (g_list_length(gaim_connections_get_all()) == 1) {
		gtk_widget_hide(deny_conn_hbox);
		return;
	} else
		gtk_widget_show(deny_conn_hbox);

	menu = gtk_menu_new();

	while (c) {
		gc = (GaimConnection *)c->data;
		c = c->next;

		if (!GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl)->set_permit_deny)
			continue;

		account = gaim_connection_get_account(gc);

		g_snprintf(buf, sizeof buf, "%s (%s)",
				   gaim_account_get_username(account), gc->prpl->info->name);
		opt = gtk_menu_item_new_with_label(buf);
		g_signal_connect(G_OBJECT(opt), "activate", G_CALLBACK(deny_gc_opt), gc);
		gtk_widget_show(opt);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), opt);
		if (gc == current_deny_gc)
			found = TRUE;
		else if (!found)
			count++;
	}

	if (!found) {
		current_deny_gc = gaim_connections_get_all()->data;
		count = 0;
	}

	gtk_option_menu_remove_menu(GTK_OPTION_MENU(deny_opt_menu));
	gtk_option_menu_set_menu(GTK_OPTION_MENU(deny_opt_menu), menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(deny_opt_menu), count);

	gtk_widget_show(menu);
	gtk_widget_show(deny_opt_menu);
}

static void pref_deny_add(GtkWidget *button, gboolean permit)
{
	show_add_perm(current_deny_gc, NULL, permit);
}


gchar *find_permdeny_by_name(GSList *l, char *who) {
	gchar *name;
	
	while (l) {
		name = (gchar *)l->data;
		if (!strcmp(name, who)) {
			return name;
		}
		
		l = l->next;
	}

	return NULL;
}

static void pref_deny_rem(GtkWidget *button, gboolean permit)
{
	gchar *who;
	GtkTreeIter iter;
	GtkTreeModel *mod;
	GtkTreeSelection *sel;

	if (permit) {
		mod = gtk_tree_view_get_model(GTK_TREE_VIEW(allow_list));
		sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(allow_list));
	} else {
		mod = gtk_tree_view_get_model(GTK_TREE_VIEW(block_list));
		sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(block_list));
	}

	if (gtk_tree_selection_get_selected(sel, NULL, &iter))
		gtk_tree_model_get(GTK_TREE_MODEL(mod), &iter, 0, &who, -1);
	else {
		return;
	}

	if (permit && !allow_list)
		return;

	if (!permit && !block_list)
		return;

	if (permit) {
		char *name = find_permdeny_by_name(current_deny_gc->account->permit, who);

		if (name) {
			gaim_privacy_permit_remove(current_deny_gc->account, name);
			serv_rem_permit(current_deny_gc, who);
			build_allow_list();
		}
	} else {
		char *name = find_permdeny_by_name(current_deny_gc->account->deny, who);

		if (name) {
			gaim_privacy_deny_remove(current_deny_gc->account, name);
			serv_rem_deny(current_deny_gc, who);
			build_block_list();
		}
	}

	gaim_blist_save();
}

static GtkWidget *privacy_win;

void update_privacy_connections() { /* This is a slightly better name */
	gboolean needdeny = FALSE;
	GList *c = gaim_connections_get_all();
	GaimConnection *gc = NULL;

	if (!privacy_win)
		return;

	while (c) {
		gc = c->data;
		if (GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl)->set_permit_deny)
			break;
		gc = NULL;
		c = c->next;
	}
	needdeny = (gc != NULL);
	

	if (needdeny) {
		gtk_widget_set_sensitive(privacy_win, TRUE);
		build_deny_menu();
		build_allow_list();
		build_block_list();
	} else {
		gtk_widget_set_sensitive(privacy_win, FALSE);
	}
}
static void destroy_privacy() {
	current_deny_gc = NULL;
	privacy_win = NULL;
}

void show_privacy_options() {
	GtkWidget *pwin;
	GtkWidget *box;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *sw;
	GtkWidget *bbox;
	GtkWidget *button;
	GtkWidget *sep;
	GtkWidget *close_button;
	GtkSizeGroup *sg1 = gtk_size_group_new(GTK_SIZE_GROUP_BOTH);
	GtkSizeGroup *sg2 = gtk_size_group_new(GTK_SIZE_GROUP_BOTH);
	GtkCellRenderer *rend;
	GtkTreeViewColumn *col;
	GtkWidget *table;

	current_deny_gc = gaim_connections_get_all()->data;	/* this is safe because this screen will only be
						   available when there are gaim_connections_get_all() */
	current_is_deny = TRUE;

	privacy_win = pwin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_resizable(GTK_WINDOW(pwin), FALSE);
	gtk_window_set_role(GTK_WINDOW(pwin), "privacy");
	gtk_window_set_title(GTK_WINDOW(pwin), _("Privacy"));
	g_signal_connect(G_OBJECT(pwin), "destroy", G_CALLBACK(destroy_privacy), NULL);
	gtk_widget_realize(pwin);

	gtk_widget_set_size_request(pwin, -1, 400);

	box = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(box), 5);
	gtk_container_add(GTK_CONTAINER(pwin), box);
	gtk_widget_show(box);

	label = gtk_label_new(_("Changes to privacy settings take effect immediately."));
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 5);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_widget_show(label);

	deny_conn_hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box), deny_conn_hbox, FALSE, FALSE, 0);
	gtk_widget_show(deny_conn_hbox);

	label = gtk_label_new(_("Set privacy for:"));
	gtk_box_pack_start(GTK_BOX(deny_conn_hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	deny_opt_menu = gtk_option_menu_new();
	gtk_box_pack_start(GTK_BOX(deny_conn_hbox), deny_opt_menu, FALSE, FALSE, 5);
	g_signal_connect(G_OBJECT(deny_opt_menu), "destroy", G_CALLBACK(des_deny_opt), NULL);
	gtk_widget_show(deny_opt_menu);

	build_deny_menu();

	table = gtk_table_new(5, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(box), table, TRUE, TRUE, 0);
	gtk_table_set_row_spacings(GTK_TABLE(table), 7);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5);
	gtk_widget_show(table);

	deny_type = deny_opt(_("Allow all users to contact me"), 1, NULL);
	gtk_size_group_add_widget(sg1, deny_type);
	gtk_table_attach(GTK_TABLE(table), deny_type, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);
	
	deny_type = deny_opt(_("Allow only users on my buddy list"), 5, deny_type);
	gtk_size_group_add_widget(sg1, deny_type);
	gtk_table_attach(GTK_TABLE(table), deny_type, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);

	deny_type = deny_opt(_("Allow only the users below"), 3, deny_type);
	gtk_size_group_add_widget(sg1, deny_type);
	gtk_table_attach(GTK_TABLE(table), deny_type, 0, 1, 2, 3, GTK_FILL, 0, 0, 0);

	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_table_attach(GTK_TABLE(table), sw, 0, 1, 3, 4, GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_widget_show(sw);

	allow_store = gtk_list_store_new(1, G_TYPE_STRING);
	allow_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(allow_store));

	rend = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes(NULL, rend, "text", 0, NULL);
	gtk_tree_view_column_set_clickable(GTK_TREE_VIEW_COLUMN(col), TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(allow_list), col);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(allow_list), FALSE);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw), allow_list);
	gtk_widget_show(allow_list);

	build_allow_list();

	bbox = gtk_hbox_new(TRUE, 0);
	gtk_widget_show(bbox);
	gtk_table_attach(GTK_TABLE(table), bbox, 0, 1, 4, 5, GTK_FILL, 0, 0, 0);

	button = gtk_button_new_from_stock(GTK_STOCK_ADD);
	gtk_size_group_add_widget(sg2, button);
	gtk_widget_show(button);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(pref_deny_add), (void *)TRUE);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);

	button = gtk_button_new_from_stock(GTK_STOCK_REMOVE);
	gtk_size_group_add_widget(sg2, button);
	gtk_widget_show(button);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(pref_deny_rem), (void *)TRUE);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);

	deny_type = deny_opt(_("Deny all users"), 2, deny_type);
	gtk_size_group_add_widget(sg1, deny_type);
	gtk_table_attach(GTK_TABLE(table), deny_type, 1, 2, 1, 2, GTK_FILL, 0, 0, 0);

	deny_type = deny_opt(_("Block the users below"), 4, deny_type);
	gtk_size_group_add_widget(sg1, deny_type);
	gtk_table_attach(GTK_TABLE(table), deny_type, 1, 2, 2, 3, GTK_FILL, 0, 0, 0);

	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_table_attach(GTK_TABLE(table), sw, 1, 2, 3, 4, GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_widget_show(sw);

	block_store = gtk_list_store_new(1, G_TYPE_STRING);
	block_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(block_store));

	rend = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes(NULL, rend, "text", 0, NULL);
	gtk_tree_view_column_set_clickable(GTK_TREE_VIEW_COLUMN(col), TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(block_list), col);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(block_list), FALSE);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw), block_list);
	gtk_widget_show(block_list);

	build_block_list();

	bbox = gtk_hbox_new(TRUE, 0);
	gtk_table_attach(GTK_TABLE(table), bbox, 1, 2, 4, 5, GTK_FILL, 0, 0, 0);
	gtk_widget_show(bbox);

	button = gtk_button_new_from_stock(GTK_STOCK_ADD);
	gtk_size_group_add_widget(sg2, button);
	gtk_widget_show(button);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(pref_deny_add), FALSE);	
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);

	button = gtk_button_new_from_stock(GTK_STOCK_REMOVE);
	gtk_size_group_add_widget(sg2, button);
	gtk_widget_show(button);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(pref_deny_rem), FALSE);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);

	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(box), sep, FALSE, FALSE, 5);
	gtk_widget_show(sep);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	close_button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	gtk_box_pack_end(GTK_BOX(hbox), close_button, FALSE, FALSE, 0);
	g_signal_connect_swapped(G_OBJECT(close_button), "clicked", G_CALLBACK(gtk_widget_destroy), pwin);
	gtk_widget_show(close_button);

	gtk_widget_show(pwin);
	
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
		strncpy_withhtml(b->account->user_info, junk, sizeof b->account->user_info);
		gc = b->account->gc;

		if (gc)
			serv_set_info(gc, b->account->user_info);
	}
	g_free(junk);
	destroy_dialog(NULL, b->window);
	g_free(b);
}

void do_set_dir(GtkWidget *widget, struct set_dir_dlg *b)
{
	const char *first = gtk_entry_get_text(GTK_ENTRY(b->first));
	int web = GTK_TOGGLE_BUTTON(b->web)->active;
	const char *middle = gtk_entry_get_text(GTK_ENTRY(b->middle));
	const char *last = gtk_entry_get_text(GTK_ENTRY(b->last));
	const char *maiden = gtk_entry_get_text(GTK_ENTRY(b->maiden));
	const char *city = gtk_entry_get_text(GTK_ENTRY(b->city));
	const char *state = gtk_entry_get_text(GTK_ENTRY(b->state));
	const char *country = gtk_entry_get_text(GTK_ENTRY(b->country));

	serv_set_dir(b->gc, first, middle, last, maiden, city, state, country, web);

	destroy_dialog(NULL, b->window);
	g_free(b);
}

void show_set_dir(GaimConnection *gc)
{
	GaimAccount *account;
	GtkWidget *label;
	GtkWidget *bot;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *frame;
	GtkWidget *fbox;
	char buf[256];

	struct set_dir_dlg *b = g_new0(struct set_dir_dlg, 1);

	b->gc = gc;

	account = gaim_connection_get_account(gc);

	GAIM_DIALOG(b->window);
	dialogwindows = g_list_prepend(dialogwindows, b->window);
	gtk_window_set_role(GTK_WINDOW(b->window), "set_dir");
	gtk_window_set_resizable(GTK_WINDOW(b->window), TRUE);
	gtk_window_set_title(GTK_WINDOW(b->window), _("Set Directory Info"));
	g_signal_connect(G_OBJECT(b->window), "destroy", G_CALLBACK(destroy_dialog), b->window);
	gtk_widget_realize(b->window);

	fbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(b->window), fbox);
	gtk_widget_show(fbox);

	frame = gtk_frame_new(_("Directory Info"));
	gtk_container_set_border_width(GTK_CONTAINER(fbox), 5);
	gtk_box_pack_start(GTK_BOX(fbox), frame, FALSE, FALSE, 0);
	gtk_widget_show(frame);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show(vbox);

	g_snprintf(buf, sizeof(buf), _("Setting Dir Info for %s:"),
			   gaim_account_get_username(account));
	label = gtk_label_new(buf);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	b->first = gtk_entry_new();
	b->middle = gtk_entry_new();
	b->last = gtk_entry_new();
	b->maiden = gtk_entry_new();
	b->city = gtk_entry_new();
	b->state = gtk_entry_new();
	b->country = gtk_entry_new();
	b->web = gtk_check_button_new_with_label(_("Allow Web Searches To Find Your Info"));

	/* Line 1 */
	label = gtk_label_new(_("First Name"));
	gtk_widget_show(label);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), b->first, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	/* Line 2 */
	label = gtk_label_new(_("Middle Name"));
	gtk_widget_show(label);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), b->middle, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);


	/* Line 3 */
	label = gtk_label_new(_("Last Name"));
	gtk_widget_show(label);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), b->last, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	/* Line 4 */
	label = gtk_label_new(_("Maiden Name"));
	gtk_widget_show(label);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), b->maiden, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	/* Line 5 */
	label = gtk_label_new(_("City"));
	gtk_widget_show(label);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), b->city, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	/* Line 6 */
	label = gtk_label_new(_("State"));
	gtk_widget_show(label);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), b->state, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	/* Line 7 */
	label = gtk_label_new(_("Country"));
	gtk_widget_show(label);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), b->country, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	/* Line 8 */

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), b->web, TRUE, TRUE, 0);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	gtk_widget_show(b->first);
	gtk_widget_show(b->middle);
	gtk_widget_show(b->last);
	gtk_widget_show(b->maiden);
	gtk_widget_show(b->city);
	gtk_widget_show(b->state);
	gtk_widget_show(b->country);
	gtk_widget_show(b->web);

	/* And add the buttons */

	bot = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(fbox), bot, FALSE, FALSE, 0);

	b->save = gaim_pixbuf_button_from_stock(_("Save"), GTK_STOCK_SAVE, GAIM_BUTTON_HORIZONTAL);
	gtk_box_pack_end(GTK_BOX(bot), b->save, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(b->save), "clicked", G_CALLBACK(do_set_dir), b);

	b->cancel = gaim_pixbuf_button_from_stock(_("Cancel"), GTK_STOCK_CANCEL, GAIM_BUTTON_HORIZONTAL);
	gtk_box_pack_end(GTK_BOX(bot), b->cancel, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(b->cancel), "clicked", G_CALLBACK(destroy_dialog), b->window);

	gtk_window_set_focus(GTK_WINDOW(b->window), b->first);

	gtk_widget_show_all(b->window);
}

void do_change_password(GtkWidget *widget, struct passwddlg *b)
{
	const gchar *orig, *new1, *new2;

	orig = gtk_entry_get_text(GTK_ENTRY(b->original));
	new1 = gtk_entry_get_text(GTK_ENTRY(b->new1));
	new2 = gtk_entry_get_text(GTK_ENTRY(b->new2));

	if (g_utf8_collate(new1, new2)) {
		gaim_notify_error(NULL, NULL,
						  _("New passwords do not match."), NULL);
		return;
	}

	if ((strlen(orig) < 1) || (strlen(new1) < 1) || (strlen(new2) < 1)) {
		gaim_notify_error(NULL, NULL,
						  _("Fill out all fields completely."), NULL);
		return;
	}

	serv_change_passwd(b->gc, orig, new1);
	g_snprintf(b->gc->account->password, sizeof(b->gc->account->password), "%s", new1);

	destroy_dialog(NULL, b->window);
	g_free(b);
}

void show_change_passwd(GaimConnection *gc)
{
	GaimAccount *account;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *vbox;
	GtkWidget *fbox;
	GtkWidget *frame;
	char buf[256];

	struct passwddlg *b = g_new0(struct passwddlg, 1);
	b->gc = gc;

	account = gaim_connection_get_account(gc);

	GAIM_DIALOG(b->window);
	gtk_window_set_resizable(GTK_WINDOW(b->window), TRUE);
	gtk_window_set_role(GTK_WINDOW(b->window), "change_passwd");
	gtk_window_set_title(GTK_WINDOW(b->window), _("Change Password"));
	g_signal_connect(G_OBJECT(b->window), "destroy", G_CALLBACK(destroy_dialog), b->window);
	gtk_widget_realize(b->window);
	dialogwindows = g_list_prepend(dialogwindows, b->window);

	fbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(fbox), 5);
	gtk_container_add(GTK_CONTAINER(b->window), fbox);

	frame = gtk_frame_new(_("Change Password"));
	gtk_box_pack_start(GTK_BOX(fbox), frame, FALSE, FALSE, 0);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);

	g_snprintf(buf, sizeof(buf), _("Changing password for %s:"), gaim_account_get_username(account));
	label = gtk_label_new(buf);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 5);

	/* First Line */
	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new(_("Original Password"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	b->original = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(b->original), FALSE);
	gtk_box_pack_end(GTK_BOX(hbox), b->original, FALSE, FALSE, 0);

	/* Next Line */
	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new(_("New Password"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	b->new1 = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(b->new1), FALSE);
	gtk_box_pack_end(GTK_BOX(hbox), b->new1, FALSE, FALSE, 0);

	/* Next Line */
	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new(_("New Password (again)"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	b->new2 = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(b->new2), FALSE);
	gtk_box_pack_end(GTK_BOX(hbox), b->new2, FALSE, FALSE, 0);

	/* Now do our row of buttons */
	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(fbox), hbox, FALSE, FALSE, 0);

	b->ok = gaim_pixbuf_button_from_stock(_("OK"), GTK_STOCK_OK, GAIM_BUTTON_HORIZONTAL);
	gtk_box_pack_end(GTK_BOX(hbox), b->ok, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(b->ok), "clicked", G_CALLBACK(do_change_password), b);

	b->cancel = gaim_pixbuf_button_from_stock(_("Cancel"), GTK_STOCK_CANCEL, GAIM_BUTTON_HORIZONTAL);
	gtk_box_pack_end(GTK_BOX(hbox), b->cancel, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(b->cancel), "clicked", G_CALLBACK(destroy_dialog), b->window);

	gtk_widget_show_all(b->window);
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
	gtk_widget_set_size_request(b->text, 300, 200);
	buf = g_malloc(strlen(account->user_info) + 1);
	strncpy_nohtml(buf, account->user_info, strlen(account->user_info) + 1);
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(b->text));
	gtk_text_buffer_set_text(buffer, buf, -1);
	g_free(buf);
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
/*  The dialog for the info requests                                      */
/*------------------------------------------------------------------------*/

static void info_dlg_free(GtkWidget *b, struct info_dlg *d)
{
	if (g_slist_find(info_dlgs, d))
		info_dlgs = g_slist_remove(info_dlgs, d);
	g_free(d->who);
	g_free(d);
}

/* if away is 0, show regardless and try to get away message
 *            1, don't show if regular info isn't shown
 *            2, show regardless but don't try to get away message
 *
 * i wish this were my client. if i were i wouldn't have to deal with this shit.
 */
void g_show_info_text(GaimConnection *gc, const char *who, int away, const char *info, ...)
{
	GtkWidget *ok;
	GtkWidget *label;
	GtkWidget *text;
	GtkWidget *bbox;
	GtkWidget *sw;
	gint options = 0;
	char *more_info;
	va_list ap;

	struct info_dlg *b = find_info_dlg(gc, who);
	if (!b && (away == 1))
		return;
	if (!b) {
		b = g_new0(struct info_dlg, 1);
		b->gc = gc;
		b->who = who ? g_strdup(normalize(who)) : NULL;
		info_dlgs = g_slist_append(info_dlgs, b);

		GAIM_DIALOG(b->window);
		gtk_window_set_title(GTK_WINDOW(b->window), "Gaim");
		gtk_container_set_border_width(GTK_CONTAINER(b->window), 5);
		gtk_widget_realize(GTK_WIDGET(b->window));
		g_signal_connect(G_OBJECT(b->window), "destroy", G_CALLBACK(info_dlg_free), b);

		bbox = gtk_vbox_new(FALSE, 5);
		gtk_container_add(GTK_CONTAINER(b->window), bbox);

		label = gtk_label_new(_("Below are the results of your search: "));
		gtk_box_pack_start(GTK_BOX(bbox), label, FALSE, FALSE, 0);

		sw = gtk_scrolled_window_new(NULL, NULL);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
		gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_IN);
		gtk_box_pack_start(GTK_BOX(bbox), sw, TRUE, TRUE, 0);

		text = gtk_imhtml_new(NULL, NULL);
		b->text = text;
		gtk_container_add(GTK_CONTAINER(sw), text);
		gtk_widget_set_size_request(sw, 300, 250);
		gaim_setup_imhtml(text);

		ok = gaim_pixbuf_button_from_stock(_("OK"), GTK_STOCK_OK, GAIM_BUTTON_HORIZONTAL);
		g_signal_connect_swapped(G_OBJECT(ok), "clicked", G_CALLBACK(gtk_widget_destroy),
					  G_OBJECT(b->window));
		gtk_box_pack_start(GTK_BOX(bbox), ok, FALSE, FALSE, 0);

		gtk_widget_show_all(b->window);
	}

	if (gaim_prefs_get_bool("/gaim/gtk/conversations/ignore_colors"))
		options ^= GTK_IMHTML_NO_COLOURS;

	if (gaim_prefs_get_bool("/gaim/gtk/conversations/ignore_fonts"))
		options ^= GTK_IMHTML_NO_FONTS;

	if (gaim_prefs_get_bool("/gaim/gtk/conversations/ignore_font_sizes"))
		options ^= GTK_IMHTML_NO_SIZES;

	options ^= GTK_IMHTML_NO_COMMENTS;
	options ^= GTK_IMHTML_NO_TITLE;
	options ^= GTK_IMHTML_NO_NEWLINE;
	options ^= GTK_IMHTML_NO_SCROLL;

	gtk_imhtml_append_text(GTK_IMHTML(b->text), info, -1, options);

	va_start(ap, info);
	while ((more_info = va_arg(ap, char *)) != NULL) {
		gchar *linkifyinated = linkify_text(more_info);
		gtk_imhtml_append_text(GTK_IMHTML(b->text), linkifyinated, -1, options);
		g_free(linkifyinated);
	}
	va_end(ap);

	if (away)
		info_dlgs = g_slist_remove(info_dlgs, b);
	else
		serv_get_away(gc, who);
}

/*------------------------------------------------------------------------*/
/*  The dialog for adding to permit/deny                                  */
/*------------------------------------------------------------------------*/


static void do_add_perm(GtkWidget *w, struct addperm *p)
{

	const char *who;

	who = gtk_entry_get_text(GTK_ENTRY(p->entry));

	if (!p->permit) {
		if (gaim_privacy_deny_add(p->gc->account, who)) {
			serv_add_deny(p->gc, who);
			build_block_list();
			gaim_blist_save();
		}
	} else {
		if (gaim_privacy_permit_add(p->gc->account, who)) {
			serv_add_permit(p->gc, who);
			build_allow_list();
			gaim_blist_save();
		}
	}

	destroy_dialog(NULL, p->window);
}



void show_add_perm(GaimConnection *gc, char *who, gboolean permit)
{
	GtkWidget *cancel;
	GtkWidget *add;
	GtkWidget *label;
	GtkWidget *bbox;
	GtkWidget *vbox;
	GtkWidget *topbox;

	struct addperm *p = g_new0(struct addperm, 1);
	p->gc = gc;
	p->permit = permit;

	GAIM_DIALOG(p->window);
	gtk_container_set_border_width(GTK_CONTAINER(p->window), 5);
	gtk_window_set_resizable(GTK_WINDOW(p->window), FALSE);
	gtk_widget_realize(p->window);

	dialogwindows = g_list_prepend(dialogwindows, p->window);

	bbox = gtk_hbox_new(FALSE, 5);
	topbox = gtk_hbox_new(FALSE, 5);
	vbox = gtk_vbox_new(FALSE, 5);
	p->entry = gtk_entry_new();

	/* Build Add Button */

	if (permit)
		add = gaim_pixbuf_button_from_stock(_("Permit"), GTK_STOCK_ADD, GAIM_BUTTON_HORIZONTAL);
	else
		add = gaim_pixbuf_button_from_stock(_("Deny"), GTK_STOCK_ADD, GAIM_BUTTON_HORIZONTAL);
	cancel = gaim_pixbuf_button_from_stock(_("Cancel"), GTK_STOCK_CANCEL, GAIM_BUTTON_HORIZONTAL);

	/* End of Cancel Button */
	if (who != NULL)
		gtk_entry_set_text(GTK_ENTRY(p->entry), who);

	/* Put the buttons in the box */

	gtk_box_pack_end(GTK_BOX(bbox), add, FALSE, FALSE, 5);
	gtk_box_pack_end(GTK_BOX(bbox), cancel, FALSE, FALSE, 5);

	label = gtk_label_new(_("Add"));
	gtk_box_pack_start(GTK_BOX(topbox), label, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(topbox), p->entry, FALSE, FALSE, 5);
	/* And the boxes in the box */
	gtk_box_pack_start(GTK_BOX(vbox), topbox, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 5);
	topbox=gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(topbox), vbox, FALSE, FALSE, 5);


	/* Handle closes right */
	g_signal_connect(G_OBJECT(p->window), "destroy", G_CALLBACK(destroy_dialog), p->window);
	g_signal_connect(G_OBJECT(cancel), "clicked", G_CALLBACK(destroy_dialog), p->window);
	g_signal_connect(G_OBJECT(add), "clicked", G_CALLBACK(do_add_perm), p);
	g_signal_connect(G_OBJECT(p->entry), "activate", G_CALLBACK(do_add_perm), p);

	/* Finish up */
	if (permit)
		gtk_window_set_title(GTK_WINDOW(p->window), _("Add Permit"));
	else
		gtk_window_set_title(GTK_WINDOW(p->window), _("Add Deny"));
	gtk_window_set_focus(GTK_WINDOW(p->window), p->entry);
	gtk_container_add(GTK_CONTAINER(p->window), topbox);
	gtk_widget_realize(p->window);

	gtk_widget_show_all(p->window);
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

		if (file_is_dir(path, gtkconv->dialogs.log))
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
				   gaim_home_dir(), normalize(c->name));
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
/* Find Buddy By Email                                  */
/*------------------------------------------------------*/

void do_find_info(GtkWidget *w, struct findbyinfo *b)
{
	const char *first;
	const char *middle;
	const char *last;
	const char *maiden;
	const char *city;
	const char *state;
	const char *country;

	first = gtk_entry_get_text(GTK_ENTRY(b->firstentry));
	middle = gtk_entry_get_text(GTK_ENTRY(b->middleentry));
	last = gtk_entry_get_text(GTK_ENTRY(b->lastentry));
	maiden = gtk_entry_get_text(GTK_ENTRY(b->maidenentry));
	city = gtk_entry_get_text(GTK_ENTRY(b->cityentry));
	state = gtk_entry_get_text(GTK_ENTRY(b->stateentry));
	country = gtk_entry_get_text(GTK_ENTRY(b->countryentry));

	serv_dir_search(b->gc, first, middle, last, maiden, city, state, country, "");
	destroy_dialog(NULL, b->window);
}

void do_find_email(GtkWidget *w, struct findbyemail *b)
{
	const char *email;

	email = gtk_entry_get_text(GTK_ENTRY(b->emailentry));

	serv_dir_search(b->gc, "", "", "", "", "", "", "", email);

	destroy_dialog(NULL, b->window);
}

void show_find_info(GaimConnection *gc)
{
	GtkWidget *cancel;
	GtkWidget *ok;
	GtkWidget *label;
	GtkWidget *bbox;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *fbox;
	GtkWidget *frame;

	struct findbyinfo *b = g_new0(struct findbyinfo, 1);
	b->gc = gc;
	GAIM_DIALOG(b->window);
	gtk_window_set_resizable(GTK_WINDOW(b->window), TRUE);
	gtk_window_set_role(GTK_WINDOW(b->window), "find_info");

	dialogwindows = g_list_prepend(dialogwindows, b->window);

	frame = gtk_frame_new(_("Search for Buddy"));
	fbox = gtk_vbox_new(FALSE, 5);

	/* Build OK Button */

	ok = gaim_pixbuf_button_from_stock(_("OK"), GTK_STOCK_OK, GAIM_BUTTON_HORIZONTAL);
	cancel = gaim_pixbuf_button_from_stock(_("Cancel"), GTK_STOCK_CANCEL, GAIM_BUTTON_HORIZONTAL);

	bbox = gtk_hbox_new(FALSE, 5);
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);

	b->firstentry = gtk_entry_new();
	b->middleentry = gtk_entry_new();
	b->lastentry = gtk_entry_new();
	b->maidenentry = gtk_entry_new();
	b->cityentry = gtk_entry_new();
	b->stateentry = gtk_entry_new();
	b->countryentry = gtk_entry_new();

	gtk_box_pack_end(GTK_BOX(bbox), ok, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(bbox), cancel, FALSE, FALSE, 0);

	/* Line 1 */
	label = gtk_label_new(_("First Name"));

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), b->firstentry, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	/* Line 2 */

	label = gtk_label_new(_("Middle Name"));

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), b->middleentry, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	/* Line 3 */

	label = gtk_label_new(_("Last Name"));

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), b->lastentry, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	/* Line 4 */

	label = gtk_label_new(_("Maiden Name"));

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), b->maidenentry, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	/* Line 5 */

	label = gtk_label_new(_("City"));

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), b->cityentry, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	/* Line 6 */
	label = gtk_label_new(_("State"));

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), b->stateentry, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	/* Line 7 */
	label = gtk_label_new(_("Country"));

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), b->countryentry, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	/* Merge The Boxes */

	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_box_pack_start(GTK_BOX(fbox), frame, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(fbox), bbox, FALSE, FALSE, 0);

	g_signal_connect(G_OBJECT(b->window), "destroy", G_CALLBACK(destroy_dialog), b->window);
	g_signal_connect(G_OBJECT(cancel), "clicked", G_CALLBACK(destroy_dialog), b->window);
	g_signal_connect(G_OBJECT(ok), "clicked", G_CALLBACK(do_find_info), b);

	gtk_window_set_title(GTK_WINDOW(b->window), _("Find Buddy By Info"));
	gtk_window_set_focus(GTK_WINDOW(b->window), b->firstentry);
	gtk_container_add(GTK_CONTAINER(b->window), fbox);
	gtk_container_set_border_width(GTK_CONTAINER(b->window), 5);
	gtk_widget_realize(b->window);

	gtk_widget_show_all(b->window);
}

void show_find_email(GaimConnection *gc)
{
	GtkWidget *label;
	GtkWidget *bbox;
	GtkWidget *vbox;
	GtkWidget *frame;
	GtkWidget *topbox;
	GtkWidget *button;

	struct findbyemail *b = g_new0(struct findbyemail, 1);
	if (g_list_find(gaim_connections_get_all(), gc))
		b->gc = gc;
	GAIM_DIALOG(b->window);
	gtk_window_set_resizable(GTK_WINDOW(b->window), TRUE);
	gtk_window_set_role(GTK_WINDOW(b->window), "find_email");
	gtk_widget_realize(b->window);
	dialogwindows = g_list_prepend(dialogwindows, b->window);
	g_signal_connect(G_OBJECT(b->window), "destroy", G_CALLBACK(destroy_dialog), b->window);
	gtk_window_set_title(GTK_WINDOW(b->window), _("Find Buddy By Email"));

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(b->window), vbox);

	frame = gtk_frame_new(_("Search for Buddy"));
	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);

	topbox = gtk_hbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), topbox);
	gtk_container_set_border_width(GTK_CONTAINER(topbox), 5);

	label = gtk_label_new(_("Email"));
	gtk_box_pack_start(GTK_BOX(topbox), label, FALSE, FALSE, 0);

	b->emailentry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(topbox), b->emailentry, TRUE, TRUE, 0);
	g_signal_connect(G_OBJECT(b->emailentry), "activate", G_CALLBACK(do_find_email), b);
	gtk_window_set_focus(GTK_WINDOW(b->window), b->emailentry);

	bbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 0);

	button = gaim_pixbuf_button_from_stock(_("OK"), GTK_STOCK_OK, GAIM_BUTTON_HORIZONTAL);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(do_find_email), b);
	gtk_box_pack_end(GTK_BOX(bbox), button, FALSE, FALSE, 0);

	button = gaim_pixbuf_button_from_stock(_("Cancel"), GTK_STOCK_CANCEL, GAIM_BUTTON_HORIZONTAL);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(destroy_dialog), b->window);
	gtk_box_pack_end(GTK_BOX(bbox), button, FALSE, FALSE, 0);

	gtk_widget_show_all(b->window);
}

/*------------------------------------------------------*/
/* Link Dialog                                          */
/*------------------------------------------------------*/

void cancel_link(GtkWidget *widget, GaimConversation *c)
{
	GaimGtkConversation *gtkconv;

	gtkconv = GAIM_GTK_CONVERSATION(c);

	if (gtkconv->toolbar.link) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtkconv->toolbar.link),
									FALSE);
	}

	destroy_dialog(NULL, gtkconv->dialogs.link);
	gtkconv->dialogs.link = NULL;
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
	
		g_signal_connect(G_OBJECT(a->window), "destroy",
						 G_CALLBACK(destroy_dialog), a->window);
		g_signal_connect(G_OBJECT(a->window), "destroy",
						 G_CALLBACK(free_dialog), a);
		dialogwindows = g_list_prepend(dialogwindows, a->window);
	
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

		g_signal_connect(G_OBJECT(a->window), "response",
						 G_CALLBACK(do_insert_link), a);

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

	dialogwindows = g_list_remove(dialogwindows, gtkconv->dialogs.font);
	gtk_widget_destroy(gtkconv->dialogs.font);
	gtkconv->dialogs.font = NULL;
}

void apply_font(GtkWidget *widget, GtkFontSelection *fontsel)
{
	/* this could be expanded to include font size, weight, etc.
	   but for now only works with font face */
	int i = 0;
	char *fontname;
	GaimConversation *c = g_object_get_data(G_OBJECT(fontsel),
			"gaim_conversation");

	fontname = gtk_font_selection_dialog_get_font_name(GTK_FONT_SELECTION_DIALOG(fontsel));

	if (c) {
		while(fontname[i] && !isdigit(fontname[i])) {
			i++;
		}
		fontname[i] = 0;
		gaim_gtk_set_font_face(GAIM_GTK_CONVERSATION(c), fontname);
	} else {
		char *c;

		fontname = gtk_font_selection_dialog_get_font_name(GTK_FONT_SELECTION_DIALOG(fontsel));

		for (c = fontname; *c != '\0'; c++) {
			if (isdigit(*c)) {
				*(--c) = '\0';
				break;
			}
		}

		gaim_prefs_set_string("/gaim/gtk/conversations/font_face", fontname);
	}

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
		smileys = get_proto_smileys(c->account->protocol);
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

static void do_alias_chat(GtkWidget *w, int resp, struct chat *chat)
{
	if(resp == GTK_RESPONSE_OK) {
		GtkWidget *entry = g_object_get_data(G_OBJECT(w), "alias_entry");
		const char *text = gtk_entry_get_text(GTK_ENTRY(entry));
		gaim_blist_alias_chat(chat, text);
		gaim_blist_save();
	}
	gtk_widget_destroy(w);
}

static void
do_alias_buddy(GtkWidget *w, int resp, struct alias_dialog_info *info)
{
	if (resp == GTK_RESPONSE_OK) {
		const char *alias;
	
		alias = gtk_entry_get_text(GTK_ENTRY(info->alias_entry));

		gaim_blist_alias_buddy(info->buddy, (alias && *alias) ? alias : NULL);
		serv_alias_buddy(info->buddy);
		gaim_blist_save();
	}

	destroy_dialog(NULL, alias_dialog);
	alias_dialog = NULL;

	g_free(info);
}

void alias_dialog_chat(struct chat *chat) {
	GtkWidget *dialog;
	GtkWidget *hbox;
	GtkWidget *img;
	GtkWidget *vbox;
	GtkWidget *label;
	GtkWidget *alias_entry;

	dialog = gtk_dialog_new_with_buttons(_("Alias Chat"), NULL,
			GTK_DIALOG_NO_SEPARATOR,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OK, GTK_RESPONSE_OK,
			NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog),
										GTK_RESPONSE_OK);

	gtk_container_set_border_width(GTK_CONTAINER(dialog), 6);
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog)->vbox), 12);
	gtk_container_set_border_width(
			GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), 6);

	/* The main hbox container. */
	hbox = gtk_hbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), hbox);

	/* The dialog image. */
	img = gtk_image_new_from_stock(GAIM_STOCK_DIALOG_QUESTION,
			GTK_ICON_SIZE_DIALOG);
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
	gtk_misc_set_alignment(GTK_MISC(img), 0, 0);

	/* The main vbox container. */
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(hbox), vbox);

	/* Setup the label containing the description. */
	label = gtk_label_new(_("Please enter an aliased name for this chat.\n"));
	gtk_widget_set_size_request(GTK_WIDGET(label), 350, -1);

	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	hbox = gtk_hbox_new(FALSE, 6);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);

	/* The "Alias:" label. */
	label = gtk_label_new(NULL);
	gtk_label_set_markup_with_mnemonic(GTK_LABEL(label), _("_Alias:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	/* The alias entry field. */
	alias_entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), alias_entry, FALSE, FALSE, 0);
	gtk_entry_set_activates_default(GTK_ENTRY(alias_entry), TRUE);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), alias_entry);

	gtk_entry_set_text(GTK_ENTRY(alias_entry), chat->alias);

	g_object_set_data(G_OBJECT(dialog), "alias_entry", alias_entry);

	g_signal_connect(G_OBJECT(dialog), "response",
			G_CALLBACK(do_alias_chat), chat);

	gtk_widget_show_all(dialog);
}

void
alias_dialog_bud(struct buddy *b)
{
	struct alias_dialog_info *info = NULL;
	struct gaim_gtk_buddy_list *gtkblist;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *label;
	GtkWidget *table;
	GtkWidget *img;

	gtkblist = GAIM_GTK_BLIST(gaim_get_blist());

	if (!alias_dialog) {
		info = g_new0(struct alias_dialog_info, 1);
		info->buddy = b;

		alias_dialog = gtk_dialog_new_with_buttons(_("Alias Buddy"),
			(gtkblist ? GTK_WINDOW(gtkblist->window) : NULL),
			GTK_DIALOG_NO_SEPARATOR,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OK, GTK_RESPONSE_OK,
			NULL);

		gtk_dialog_set_default_response(GTK_DIALOG(alias_dialog),
										GTK_RESPONSE_OK);
		gtk_container_set_border_width(GTK_CONTAINER(alias_dialog), 6);
		gtk_window_set_resizable(GTK_WINDOW(alias_dialog), FALSE);
		gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(alias_dialog)->vbox), 12);
		gtk_container_set_border_width(
				GTK_CONTAINER(GTK_DIALOG(alias_dialog)->vbox), 6);

		/* The main hbox container. */
		hbox = gtk_hbox_new(FALSE, 12);
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(alias_dialog)->vbox), hbox);

		/* The dialog image. */
		img = gtk_image_new_from_stock(GAIM_STOCK_DIALOG_QUESTION,
									   GTK_ICON_SIZE_DIALOG);
		gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
		gtk_misc_set_alignment(GTK_MISC(img), 0, 0);

		/* The main vbox container. */
		vbox = gtk_vbox_new(FALSE, 0);
		gtk_container_add(GTK_CONTAINER(hbox), vbox);

		/* Setup the label containing the description. */
		label = gtk_label_new(_("Please enter an aliased name for the "
								"person below, or rename this contact "
								"in your buddy list.\n"));
		gtk_widget_set_size_request(GTK_WIDGET(label), 350, -1);

		gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
		gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

		hbox = gtk_hbox_new(FALSE, 6);
		gtk_container_add(GTK_CONTAINER(vbox), hbox);

		/* The table containing the entry widgets and labels. */
		table = gtk_table_new(2, 2, FALSE);
		gtk_table_set_row_spacings(GTK_TABLE(table), 6);
		gtk_table_set_col_spacings(GTK_TABLE(table), 6);
		gtk_container_set_border_width(GTK_CONTAINER(table), 12);
		gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);

		/* The "Screenname:" label. */
		label = gtk_label_new(NULL);
		gtk_label_set_markup_with_mnemonic(GTK_LABEL(label), _("_Screenname:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
		gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 0, 1);

		/* The Screen name entry field. */
		info->name_entry = gtk_entry_new();
		gtk_table_attach_defaults(GTK_TABLE(table), info->name_entry,
								  1, 2, 0, 1);
		gtk_entry_set_activates_default(GTK_ENTRY(info->name_entry), TRUE);
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), info->name_entry);
		gtk_entry_set_text(GTK_ENTRY(info->name_entry), info->buddy->name);

		/* The "Alias:" label. */
		label = gtk_label_new(NULL);
		gtk_label_set_markup_with_mnemonic(GTK_LABEL(label), _("_Alias:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
		gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1, 2);

		/* The alias entry field. */
		info->alias_entry = gtk_entry_new();
		gtk_table_attach_defaults(GTK_TABLE(table), info->alias_entry,
								  1, 2, 1, 2);
		gtk_entry_set_activates_default(GTK_ENTRY(info->alias_entry), TRUE);
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), info->alias_entry);

		if (info->buddy->alias != NULL)
			gtk_entry_set_text(GTK_ENTRY(info->alias_entry),
							   info->buddy->alias);

		g_signal_connect(G_OBJECT(alias_dialog), "response",
						 G_CALLBACK(do_alias_buddy), info);
	}

	gtk_widget_show_all(alias_dialog);

	if (info)
		gtk_widget_grab_focus(info->name_entry);
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
		   name ? normalize(name) : "system", name ? ".log" : "");

	file = (const char*)gtk_file_selection_get_filename(GTK_FILE_SELECTION(filesel));
	strncpy(path, file, PATHSIZE - 1);
	if (file_is_dir(path, filesel))
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
		   name ? normalize(name) : "system", name ? ".log" : "");

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

static void do_clear_log_file(GtkWidget *w, gchar *name)
{
	gchar buf[256];
	gchar filename[256];
	GtkWidget *window;
	char *tmp;

	tmp = gaim_user_dir();
	g_snprintf(filename, 256, "%s" G_DIR_SEPARATOR_S "logs" G_DIR_SEPARATOR_S "%s%s", tmp,
		   name ? normalize(name) : "system", name ? ".log" : "");

	if ((remove(filename)) == -1) {
		g_snprintf(buf, 256, _("Couldn't remove file %s." ), filename);
		gaim_notify_error(NULL, NULL, buf, strerror(errno));
	}

	window = g_object_get_data(G_OBJECT(w), "log_window");
	destroy_dialog(NULL, window);
}

static void show_clear_log(GtkWidget *w, gchar *name)
{
	GtkWidget *window;
	GtkWidget *box;
	GtkWidget *hbox;
	GtkWidget *button;
	GtkWidget *label;
	GtkWidget *hsep;

	GAIM_DIALOG(window);
	gtk_window_set_role(GTK_WINDOW(window), "dialog");
	gtk_window_set_title(GTK_WINDOW(window), _("Clear Log"));
	gtk_container_set_border_width(GTK_CONTAINER(window), 10);
	gtk_window_set_resizable(GTK_WINDOW(window), TRUE);
	g_signal_connect(G_OBJECT(window), "delete_event", G_CALLBACK(destroy_dialog), window);
	gtk_widget_realize(window);

	box = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(window), box);

	label = gtk_label_new(_("Really clear log?"));
	gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 15);

	hsep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(box), hsep, FALSE, FALSE, 0);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 0);

	button = gaim_pixbuf_button_from_stock(_("OK"), GTK_STOCK_OK, GAIM_BUTTON_HORIZONTAL);
	g_object_set_data(G_OBJECT(button), "log_window", g_object_get_data(G_OBJECT(w),
				"log_window"));
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(do_clear_log_file), name);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(destroy_dialog), window);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 5);

	button = gaim_pixbuf_button_from_stock(_("Cancel"), GTK_STOCK_CANCEL, GAIM_BUTTON_HORIZONTAL);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(destroy_dialog), window);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 5);

	gtk_widget_show_all(window);

	return;
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
		g_snprintf(filename, 256, "%s" G_DIR_SEPARATOR_S "logs" G_DIR_SEPARATOR_S "%s.log", tmp, normalize(view->name));
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
			gtk_imhtml_append_text(GTK_IMHTML(view->layout), string->str, -1, view->options);
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
	gtk_imhtml_append_text(GTK_IMHTML(view->layout), string->str, -1, view->options);
	gtk_imhtml_append_text(GTK_IMHTML(view->layout), "<BR>", -1, view->options);

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
	struct view_log *view;
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
		g_snprintf(filename, 256, "%s" G_DIR_SEPARATOR_S "logs" G_DIR_SEPARATOR_S "%s.log", tmp, normalize(name));
		if ((fp = fopen(filename, "r")) == NULL) {
			g_snprintf(buf, BUF_LONG, _("Couldn't open log file %s"), filename);
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

	g_signal_connect(G_OBJECT(layout), "url_clicked", G_CALLBACK(open_url), NULL);
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
	g_signal_connect(G_OBJECT(clear_button), "clicked", G_CALLBACK(show_clear_log), name);

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

/*------------------------------------------------------------------------*/
/*  The dialog for renaming groups                                        */
/*------------------------------------------------------------------------*/

static void do_rename_group(GtkObject *obj, int resp, GtkWidget *entry)
{
	const char *new_name;
	struct group *g;

	if (resp == GTK_RESPONSE_OK) {
		new_name = gtk_entry_get_text(GTK_ENTRY(entry));
		g = g_object_get_data(G_OBJECT(entry), "group");

		gaim_blist_rename_group(g, new_name);
		gaim_blist_save();
	}
	destroy_dialog(rename_dialog, rename_dialog);
}

void show_rename_group(GtkWidget *unused, struct group *g)
{

	GtkWidget *hbox, *vbox;
	GtkWidget *label;
	struct gaim_gtk_buddy_list *gtkblist;
	GtkWidget *img = gtk_image_new_from_stock(GAIM_STOCK_DIALOG_QUESTION, GTK_ICON_SIZE_DIALOG);
	GtkWidget *name_entry = NULL;

	gtkblist = GAIM_GTK_BLIST(gaim_get_blist());

	if (!rename_dialog) {
		rename_dialog =  gtk_dialog_new_with_buttons(_("Rename Group"), GTK_WINDOW(gtkblist->window), 0, 
						 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
		gtk_dialog_set_default_response (GTK_DIALOG(rename_dialog), GTK_RESPONSE_OK);
		gtk_container_set_border_width (GTK_CONTAINER(rename_dialog), 6);
		gtk_window_set_resizable(GTK_WINDOW(rename_dialog), FALSE);
		gtk_dialog_set_has_separator(GTK_DIALOG(rename_dialog), FALSE);
		gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(rename_dialog)->vbox), 12);
		gtk_container_set_border_width (GTK_CONTAINER(GTK_DIALOG(rename_dialog)->vbox), 6);

		hbox = gtk_hbox_new(FALSE, 12);
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(rename_dialog)->vbox), hbox);
		gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
		gtk_misc_set_alignment(GTK_MISC(img), 0, 0);

		vbox = gtk_vbox_new(FALSE, 0);
		gtk_container_add(GTK_CONTAINER(hbox), vbox);

		label = gtk_label_new(_("Please enter a new name for the selected group.\n"));
		gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
		gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

		hbox = gtk_hbox_new(FALSE, 6);
		gtk_container_add(GTK_CONTAINER(vbox), hbox);

		label = gtk_label_new(NULL);
		gtk_label_set_markup_with_mnemonic(GTK_LABEL(label), _("_Group:"));
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

		name_entry = gtk_entry_new();
		gtk_entry_set_activates_default (GTK_ENTRY(name_entry), TRUE);
		g_object_set_data(G_OBJECT(name_entry), "group", g);
		gtk_entry_set_text(GTK_ENTRY(name_entry), g->name);
		gtk_box_pack_start(GTK_BOX(hbox), name_entry, FALSE, FALSE, 0);
		gtk_entry_set_activates_default (GTK_ENTRY(name_entry), TRUE);
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), GTK_WIDGET(name_entry));

		g_signal_connect(G_OBJECT(rename_dialog), "response", G_CALLBACK(do_rename_group), name_entry);

	}

	gtk_widget_show_all(rename_dialog);
	if(name_entry)
		gtk_widget_grab_focus(GTK_WIDGET(name_entry));
}


/*------------------------------------------------------------------------*/
/*  The dialog for renaming buddies                                       */
/*------------------------------------------------------------------------*/

static void do_rename_buddy(GObject *obj, GtkWidget *entry)
{
	const char *new_name;
	struct buddy *b;

	new_name = gtk_entry_get_text(GTK_ENTRY(entry));
	b = g_object_get_data(obj, "buddy");

	if (!g_list_find(gaim_connections_get_all(), b->account->gc)) {
		destroy_dialog(rename_bud_dialog, rename_bud_dialog);
		return;
	}

	if (new_name && (strlen(new_name) != 0) && strcmp(new_name, b->name)) {
		struct group *g = gaim_find_buddys_group(b);
		char *prevname = b->name;
		if (g)
			serv_remove_buddy(b->account->gc, b->name, g->name);
		b->name = g_strdup(new_name);
		serv_add_buddy(b->account->gc, b->name);
		gaim_blist_rename_buddy(b, prevname);
		gaim_blist_save();
		g_free(prevname);
	}

	destroy_dialog(rename_bud_dialog, rename_bud_dialog);
}

void show_rename_buddy(GtkWidget *unused, struct buddy *b)
{
	GtkWidget *mainbox;
	GtkWidget *frame;
	GtkWidget *fbox;
	GtkWidget *bbox;
	GtkWidget *button;
	GtkWidget *name_entry;
	GtkWidget *label;

	if (!rename_bud_dialog) {
		GAIM_DIALOG(rename_bud_dialog);
		gtk_window_set_role(GTK_WINDOW(rename_bud_dialog), "rename_bud_dialog");
		gtk_window_set_resizable(GTK_WINDOW(rename_bud_dialog), TRUE);
		gtk_window_set_title(GTK_WINDOW(rename_bud_dialog), _("Rename Buddy"));
		g_signal_connect(G_OBJECT(rename_bud_dialog), "destroy",
				   G_CALLBACK(destroy_dialog), rename_bud_dialog);
		gtk_widget_realize(rename_bud_dialog);

		mainbox = gtk_vbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(mainbox), 5);
		gtk_container_add(GTK_CONTAINER(rename_bud_dialog), mainbox);

		frame = gtk_frame_new(_("Rename Buddy"));
		gtk_box_pack_start(GTK_BOX(mainbox), frame, TRUE, TRUE, 0);

		fbox = gtk_hbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(fbox), 5);
		gtk_container_add(GTK_CONTAINER(frame), fbox);

		label = gtk_label_new(_("New name:"));
		gtk_box_pack_start(GTK_BOX(fbox), label, FALSE, FALSE, 0);

		name_entry = gtk_entry_new();
		gtk_box_pack_start(GTK_BOX(fbox), name_entry, TRUE, TRUE, 0);
		g_object_set_data(G_OBJECT(name_entry), "buddy", b);
		gtk_entry_set_text(GTK_ENTRY(name_entry), b->name);
		g_signal_connect(G_OBJECT(name_entry), "activate",
				   G_CALLBACK(do_rename_buddy), name_entry);
		gtk_widget_grab_focus(name_entry);

		bbox = gtk_hbox_new(FALSE, 5);
		gtk_box_pack_start(GTK_BOX(mainbox), bbox, FALSE, FALSE, 0);

		button = gaim_pixbuf_button_from_stock(_("OK"), GTK_STOCK_OK, GAIM_BUTTON_HORIZONTAL);
		g_object_set_data(G_OBJECT(button), "buddy", b);
		gtk_box_pack_end(GTK_BOX(bbox), button, FALSE, FALSE, 0);
		g_signal_connect(G_OBJECT(button), "clicked",
				   G_CALLBACK(do_rename_buddy), name_entry);

		button = gaim_pixbuf_button_from_stock(_("Cancel"), GTK_STOCK_CANCEL, GAIM_BUTTON_HORIZONTAL);
		gtk_box_pack_end(GTK_BOX(bbox), button, FALSE, FALSE, 0);
		g_signal_connect(G_OBJECT(button), "clicked",
				   G_CALLBACK(destroy_dialog), rename_bud_dialog);
	}

	gtk_widget_show_all(rename_bud_dialog);
}


GtkWidget *gaim_pixbuf_toolbar_button_from_stock(char *icon)
{
	GtkWidget *button, *image,  *bbox;

	button = gtk_toggle_button_new();
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);

	bbox = gtk_vbox_new(FALSE, 0);

	gtk_container_add (GTK_CONTAINER(button), bbox);

	image = gtk_image_new_from_stock(icon, GTK_ICON_SIZE_MENU);
	gtk_box_pack_start(GTK_BOX(bbox), image, FALSE, FALSE, 0);

	gtk_widget_show_all(bbox);
	return button;
}

GtkWidget *
gaim_pixbuf_button_from_stock(const char *text, const char *icon,
							  GaimButtonOrientation style)
{
	GtkWidget *button, *image, *label, *bbox, *ibox, *lbox;
	button = gtk_button_new();

	if (style == GAIM_BUTTON_HORIZONTAL) {
			bbox = gtk_hbox_new(FALSE, 5);
			ibox = gtk_hbox_new(FALSE, 0);
			lbox = gtk_hbox_new(FALSE, 0);
	} else {
			bbox = gtk_vbox_new(FALSE, 5);
			ibox = gtk_vbox_new(FALSE, 0);
			lbox = gtk_vbox_new(FALSE, 0);
	}

	gtk_container_add (GTK_CONTAINER(button), bbox);

	gtk_box_pack_start_defaults(GTK_BOX(bbox), ibox);
	gtk_box_pack_start_defaults(GTK_BOX(bbox), lbox);

	if (icon) {
		image = gtk_image_new_from_stock(icon, GTK_ICON_SIZE_BUTTON);
		gtk_box_pack_end(GTK_BOX(ibox), image, FALSE, FALSE, 0);
	}

	if (text) {
		label = gtk_label_new(NULL);
		gtk_label_set_text_with_mnemonic(GTK_LABEL(label), text);
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), button);
		gtk_box_pack_start(GTK_BOX(lbox), label, FALSE, FALSE, 0);
	}

	gtk_widget_show_all(bbox);
	return button;
}

GtkWidget *gaim_pixbuf_button(char *text, char *iconfile, GaimButtonOrientation style)
{
	GtkWidget *button, *image, *label, *bbox, *ibox, *lbox;
	button = gtk_button_new();

	if (style == GAIM_BUTTON_HORIZONTAL) {
			bbox = gtk_hbox_new(FALSE, 5);
			ibox = gtk_hbox_new(FALSE, 0);
			lbox = gtk_hbox_new(FALSE, 0);
	} else {
			bbox = gtk_vbox_new(FALSE, 5);
			ibox = gtk_vbox_new(FALSE, 0);
			lbox = gtk_vbox_new(FALSE, 0);
	}

	gtk_container_add (GTK_CONTAINER(button), bbox);

	gtk_box_pack_start_defaults(GTK_BOX(bbox), ibox);
	gtk_box_pack_start_defaults(GTK_BOX(bbox), lbox);

	if (iconfile) {
		char *filename;
		filename = g_build_filename (DATADIR, "pixmaps", "gaim", "buttons", iconfile, NULL);
		gaim_debug(GAIM_DEBUG_MISC, "gaim_pixbuf_button",
				   "Loading: %s\n", filename);
		image = gtk_image_new_from_file(filename);
		gtk_box_pack_end(GTK_BOX(ibox), image, FALSE, FALSE, 0);
		g_free(filename);
	}

	if (text) {
		label = gtk_label_new(NULL);
		gtk_label_set_text_with_mnemonic(GTK_LABEL(label), text);
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), button);
		gtk_box_pack_start(GTK_BOX(lbox), label, FALSE, FALSE, 0);
	}

	gtk_widget_show_all(bbox);
	return button;
}

int file_is_dir(const char *path, GtkWidget *w)
{
	struct stat st;
	char *name;

	if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
		/* append a / if needed */
		if (path[strlen(path) - 1] != '/') {
			name = g_strconcat(path, "/", NULL);
		} else {
			name = g_strdup(path);
		}
		gtk_file_selection_set_filename(GTK_FILE_SELECTION(w), name);
		g_free(name);
		return 1;
	}

	return 0;
}

/*------------------------------------------------------------------------*/
/*  The dialog for setting V-Card info                                    */
/*------------------------------------------------------------------------*/
/*
 * There are actually two "chunks" of code following:  generic "multi-entry dialog"
 * support and V-Card dialog specific support.
 *
 * At first blush, this may seem like an unnecessary duplication of effort given
 * that a "set dir info" dialog already exists.  However, this is not so because:
 *
 *	1. V-Cards can have a lot more data in them than what the current
 *	   "set dir" dialog supports.
 *
 *	2. V-Card data, at least with respect to Jabber, is currently in a
 *	   state of flux.  As the data and format changes, all that need be
 *	   changed with the V-Card support I've written is the "template"
 *	   data.
 *
 *	3. The "multi entry dialog" support itself was originally written
 *	   to support Jabber server user registration (TBD).  A "dynamically
 *	   configurable" multi-entry dialog is needed for that, as different
 *	   servers may require different registration information.  It just
 *	   turned out to be well-suited to adding V-Card setting support, as
 *	   well :-).
 *
 * TBD: Add check-box support to the generic multi-entry dialog support so that
 *      it can be used to "replace" the "set dir info" support?
 *
 *      Multiple-language support.  Currently Not In There.  I think this should
 *      be easy.  Note that when it's added: if anybody saved their data in
 *      English, it'll be lost when MLS is added and they'll have to re-enter it.
 *
 * More "TBDs" noted in the code.
 */


/*------------------------------------*/
/* generic multi-entry dialog support */
/*------------------------------------*/

/*
 * Print all multi-entry items
 *
 * Note: Simply a debug helper
 */
void multi_entry_item_print_all(const GSList *list) {

	int cnt = 0;

	/* While there's something to print... */
	while(list != NULL) {
		fprintf(stderr, "label %2d: \"%s\"", ++cnt, ((MultiEntryData *) (list->data))->label);
		if(((MultiEntryData *) (list->data))->text != NULL) {
			fprintf(stderr, ", text: \"%s\"", ((MultiEntryData *) (list->data))->text);
		}
		fputs("\n", stderr);
		list = list->next;
	}
}

/*
 * Print all multi-text items
 *
 * Note: Simply a debug helper
 */
void multi_text_item_print_all(const GSList *list) {

	int cnt = 0;

	/* While there's something to print... */
	while(list != NULL) {
		fprintf(stderr, "label %2d: \"%s\"", ++cnt, ((MultiTextData *) (list->data))->label);
		if(((MultiTextData *) (list->data))->text != NULL) {
			fprintf(stderr, ", text: \"%s\"", ((MultiTextData *) (list->data))->text);
		}
		fputs("\n", stderr);
		list = list->next;
	}
}


/*
 * Free all multi-entry item allocs and NULL the list pointer
 */
void multi_entry_items_free_all(GSList **list)
{

	GSList *next = *list;
	MultiEntryData *data;

	/* While there's something to free() ... */
	while(next != NULL) {
		data = (MultiEntryData *) next->data;
		g_free(data->label);
		g_free(data->text);
		g_free(data);
		next = next->next;
	}
	g_slist_free(*list);
	*list = NULL;
}

/*
 * Free all multi-text item allocs and NULL the list pointer
 */
void multi_text_items_free_all(GSList **list)
{

	GSList *next = *list;
	MultiTextData *data;

	/* While there's something to free() ... */
	while(next != NULL) {
		data = (MultiTextData *) next->data;
		g_free(data->label);
		g_free(data->text);
		g_free(data);
		next = next->next;
	}
	g_slist_free(*list);
	*list = NULL;
}

/*
 * See if a MultiEntryData item contains a given label
 *
 * See: glib docs for g_slist_compare_custom() for details
 */
static gint multi_entry_data_label_compare(gconstpointer data, gconstpointer label)
{
	return(strcmp(((MultiEntryData *) (data))->label, (char *) label));
}

/*
 * Add a new multi-entry item to list
 *
 * If adding to existing list: will search the list for existence of 
 * "label" and change/create "text" entry if necessary.
 */

MultiEntryData *multi_entry_list_update(GSList **list, const char *label, const char *text, int add_it)
{
	GSList *found;
	MultiEntryData *data;

	if((found = g_slist_find_custom(*list, (void *)label, multi_entry_data_label_compare)) == NULL) {
		if(add_it) {
			data = (MultiEntryData *) g_slist_last(*list =
				g_slist_append(*list, g_malloc(sizeof(MultiEntryData))))->data;
			data->label = strcpy(g_malloc(strlen(label) +1), label);
			data->text = NULL;
			/*
			 * default to setting "visible" and editable to TRUE - they can be
			 * overridden later, of course.
			 */
			data->visible  = TRUE;
			data->editable = TRUE;
		} else {
			data = NULL;
		}
	} else {
		data = found->data;
	}

	if(data != NULL && text != NULL && text[0] != '\0') {
		if(data->text == NULL) {
			data->text = g_malloc(strlen(text) + 1);
		} else {
			data->text = g_realloc(data->text, strlen(text) + 1);
		}
		strcpy(data->text, text);
	}

	return(data);
}

/*
 * See if a MultiTextData item contains a given label
 *
 * See: glib docs for g_slist_compare_custom() for details
 */
static gint multi_text_data_label_compare(gconstpointer data, gconstpointer label)
{
	return(strcmp(((MultiTextData *) (data))->label, (char *) label));
}

/*
 * Add a new multi-text item to list
 *
 * If adding to existing list: will search the list for existence of 
 * "label" and change/create "text" text if necessary.
 */

MultiTextData *multi_text_list_update(GSList **list, const char *label, const char *text, int add_it)
{
	GSList *found;
	MultiTextData *data;

	if((found = g_slist_find_custom(*list, (void *)label, multi_text_data_label_compare)) == NULL) {
		if(add_it) {
			data = (MultiTextData *) g_slist_last(*list =
				g_slist_append(*list, g_malloc(sizeof(MultiTextData))))->data;
			data->label = strcpy(g_malloc(strlen(label) +1), label);
			data->text = NULL;
		} else {
			data = NULL;
		}
	} else {
		data = found->data;
	}

	if(data != NULL && text != NULL && text[0] != '\0') {
		if(data->text == NULL) {
			data->text = g_malloc(strlen(text) + 1);
		} else {
			data->text = g_realloc(data->text, strlen(text) + 1);
		}
		strcpy(data->text, text);
	}

	return(data);
}

/*
 * Free-up the multi-entry item list and the MultiEntryDlg
 * struct alloc.
 */
void multi_entry_free(struct multi_entry_dlg *b)
{
	multi_entry_items_free_all(&(b->multi_entry_items));
	multi_text_items_free_all(&(b->multi_text_items));
	g_free(b->instructions->text);
	g_free(b->instructions);
	g_free(b->entries_title);
	g_free(b);
}

/*
 * Multi-Entry dialog "destroyed" catcher
 *
 * Free-up the multi-entry item list, destroy the dialog widget
 * and free the MultiEntryDlg struct alloc.
 *
 */
void multi_entry_dialog_destroy(GtkWidget *widget, gpointer  data)
{
	MultiEntryDlg *b = data;

	multi_entry_free(b);
}

/*
 * Show/Re-show instructions
 */
void re_show_multi_entry_instr(MultiInstrData *instructions)
{
	if(instructions->label != NULL) {
		if(instructions->text == NULL) {
			gtk_widget_hide(instructions->label);
		} else {
			gtk_label_set_text(GTK_LABEL (instructions->label), _(instructions->text));
			gtk_widget_show(instructions->label);
		}
	}
}

/*
 * Show/Re-show entry boxes
 */
void re_show_multi_entry_entries(GtkWidget **entries_table,
				 GtkWidget *entries_frame,
				 GSList *multi_entry_items)
{
	GtkWidget *label;
	GSList *multi_entry;
	MultiEntryData *med;
	int rows, row_num, col_num, col_offset;
	int cols = 1;

	/* Figure-out number of rows needed for table */
	if((rows = g_slist_length(multi_entry_items)) > 9) {
		rows /= 2;
		++cols;
	}

	if(*entries_table != NULL) {
		gtk_widget_destroy(GTK_WIDGET (*entries_table));
	}
	*entries_table = gtk_table_new(rows, 3 * cols, FALSE);
	gtk_container_add(GTK_CONTAINER (entries_frame), *entries_table);

	for(col_num = 0, multi_entry = multi_entry_items; col_num < cols && multi_entry != NULL;
			++col_num) {
		col_offset = col_num * 3;
		for(row_num = 0; row_num < rows && multi_entry != NULL;
				++row_num, multi_entry = multi_entry->next) {

			med = (MultiEntryData *) multi_entry->data;

			label = gtk_label_new(_(med->label));
			gtk_misc_set_alignment(GTK_MISC(label), (gfloat) 1.0, (gfloat) 0.5);
			gtk_table_attach_defaults(GTK_TABLE (*entries_table), label,
				    col_offset, 1 + col_offset, row_num, row_num +1);
			gtk_widget_show(label);

			label = gtk_label_new(": ");
			gtk_misc_set_alignment(GTK_MISC(label), (gfloat) 0.0, (gfloat) 0.5);
			gtk_table_attach_defaults(GTK_TABLE (*entries_table), label,
				1 + col_offset, 2 + col_offset, row_num, row_num +1);
			gtk_widget_show(label);

			med->widget = gtk_entry_new();
			gtk_entry_set_max_length(GTK_ENTRY(med->widget), 50);
			if(med->text != NULL) {
				gtk_entry_set_text(GTK_ENTRY (med->widget), med->text);
			}
			gtk_entry_set_visibility(GTK_ENTRY (med->widget), med->visible);
			gtk_editable_set_editable(GTK_EDITABLE(med->widget), med->editable);
			gtk_table_attach(GTK_TABLE (*entries_table), med->widget,
				2 + col_offset, 3 + col_offset, row_num, row_num +1,
				GTK_FILL|GTK_EXPAND, GTK_FILL|GTK_EXPAND, 5, 0);
			gtk_widget_show(med->widget);
		}
	}

	gtk_widget_show(*entries_table);
}

/*
 * Show/Re-show textboxes
 */
void re_show_multi_entry_textboxes(GtkWidget **texts_ibox,
				   GtkWidget *texts_obox,
				   GSList *multi_text_items)
{
	GSList *multi_text;
	MultiTextData *mtd;
	GtkWidget *frame;
	GtkWidget *sw;

	if(*texts_ibox != NULL) {
		gtk_widget_destroy(GTK_WIDGET (*texts_ibox));
	}
	*texts_ibox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER (texts_obox), *texts_ibox);

	for(multi_text = multi_text_items; multi_text != NULL; multi_text = multi_text->next) {
		mtd = (MultiTextData *) multi_text->data;
		frame = gtk_frame_new(_(mtd->label));
		sw = gtk_scrolled_window_new(NULL, NULL);
		gtk_container_set_border_width(GTK_CONTAINER(sw), 5);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
				GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw),
				GTK_SHADOW_IN);
		gtk_widget_set_size_request(sw, 300, 100);
		gtk_container_add(GTK_CONTAINER (frame), sw);
		gtk_container_add(GTK_CONTAINER (*texts_ibox), frame);
		mtd->textbox = gtk_text_view_new();
		gtk_text_view_set_editable(GTK_TEXT_VIEW(mtd->textbox), TRUE);
		gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(mtd->textbox), GTK_WRAP_WORD_CHAR);
		gtk_text_buffer_set_text(
				gtk_text_view_get_buffer(GTK_TEXT_VIEW(mtd->textbox)),
					mtd->text?mtd->text:"", -1);
		gtk_container_add(GTK_CONTAINER (sw), mtd->textbox);
		gtk_widget_show(mtd->textbox);
		gtk_widget_show(sw);
		gtk_widget_show(frame);
	}

	gtk_widget_show(*texts_ibox);
}

/*
 *  Create and initialize a new Multi-Entry Dialog struct
 */
MultiEntryDlg *multi_entry_dialog_new()
{
	MultiEntryDlg *b = g_new0(MultiEntryDlg, 1);
	b->instructions = g_new0(MultiInstrData, 1);
	b->multi_entry_items = NULL;
	b->multi_text_items = NULL;
	return(b);
}

/*
 * Instantiate a new multi-entry dialog
 *
 * data == pointer to MultiEntryDlg with the following
 *         initialized:
 *
 *           role
 *           title
 *	     user
 *           multi_entry_items - pointers to MultiEntryData list
 *	       and MultiTextData list
 *           instructions (optional)
 *           ok function pointer
 *           cancel function pointer (actually used to set
 *             window destroy signal--cancel asserts destroy)
 *
 *         sets the following in the MultiEntryDialog struct:
 *
 *           window
 */
void show_multi_entry_dialog(gpointer data)
{
	GtkWidget *vbox, *hbox;
	GtkWidget *button;
	MultiEntryDlg *b = data;

	GAIM_DIALOG(b->window);
	gtk_container_set_border_width(GTK_CONTAINER(b->window), 5);
	gtk_window_set_role(GTK_WINDOW(b->window), b->role);
	gtk_window_set_title(GTK_WINDOW (b->window), b->title);

	/* Clean up if user dismisses window via window manager! */
	g_signal_connect(G_OBJECT(b->window), "destroy", G_CALLBACK(b->cancel), (gpointer) b);
	gtk_widget_realize(b->window);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER (b->window), vbox);

	b->instructions->label = gtk_label_new(NULL);
	gtk_label_set_line_wrap(GTK_LABEL (b->instructions->label), TRUE);
	gtk_box_pack_start(GTK_BOX (vbox), b->instructions->label, TRUE, TRUE, 5);
	re_show_multi_entry_instr(b->instructions);

	b->entries_frame = gtk_frame_new(b->entries_title);
	gtk_box_pack_start(GTK_BOX (vbox), b->entries_frame, TRUE, TRUE, 5);
	b->entries_table = NULL;
	re_show_multi_entry_entries(&(b->entries_table), b->entries_frame, b->multi_entry_items);

	b->texts_obox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX (vbox), b->texts_obox, TRUE, TRUE, 5);
	b->texts_ibox = NULL;
	re_show_multi_entry_textboxes(&(b->texts_ibox), b->texts_obox, b->multi_text_items);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX (vbox), hbox, FALSE, FALSE, 5);

	button = gaim_pixbuf_button_from_stock(_("Save"), GTK_STOCK_SAVE, GAIM_BUTTON_HORIZONTAL);
	g_signal_connect(G_OBJECT (button), "clicked",
		G_CALLBACK (b->ok), (gpointer) b);
	gtk_box_pack_end(GTK_BOX (hbox), button, FALSE, FALSE, 5);

	button = gaim_pixbuf_button_from_stock(_("Cancel"), GTK_STOCK_CANCEL, GAIM_BUTTON_HORIZONTAL);

	/* Let "destroy handling" (set above) handle cleanup */
	g_signal_connect_swapped(G_OBJECT (button), "clicked",
		G_CALLBACK (gtk_widget_destroy), G_OBJECT (b->window));
	gtk_box_pack_end(GTK_BOX (hbox), button, FALSE, FALSE, 5);

	gtk_widget_show_all(b->window);
}


/*------------------------------------*/
/* V-Card dialog specific support     */
/*------------------------------------*/

/*
 * V-Card "set info" dialog "Save" clicked
 *
 * Copy data from GTK+ dialogs into GSLists, call protocol-specific
 * formatter and save the user info data.
 */
void set_vcard_dialog_ok_clicked(GtkWidget *widget, gpointer  data)
{
	MultiEntryDlg *b = (MultiEntryDlg *) data;
	GaimConnection *gc;
	gchar *tmp;
	GSList *list;

	for(list = b->multi_entry_items; list != NULL; list = list->next) {
		if(((MultiEntryData *) list->data)->text != NULL) {
			g_free(((MultiEntryData *) list->data)->text);
		}
		((MultiEntryData *) list->data)->text =
			g_strdup(gtk_entry_get_text(GTK_ENTRY(((MultiEntryData *) list->data)->widget)));
	}

	for(list = b->multi_text_items; list != NULL; list = list->next) {
		if(((MultiTextData *) list->data)->text != NULL) {
			g_free(((MultiTextData *) list->data)->text);
		}
		((MultiTextData *) list->data)->text =
			gtk_text_view_get_text(GTK_TEXT_VIEW(((MultiTextData *) list->data)->textbox), FALSE);
	}


	tmp = b->custom(b);

	/*
	 * Set the user info and (possibly) send to the server
	 */
        if (b->account) {
                strncpy(b->account->user_info, tmp, sizeof b->account->user_info);
                gc = b->account->gc;

                if (gc)
                        serv_set_info(gc, b->account->user_info);
        }

	g_free(tmp);

	/* Let multi-edit dialog window "destroy" event catching handle remaining cleanup */
	gtk_widget_destroy(GTK_WIDGET (b->window));
}

/*
 * Instantiate a v-card dialog
 */
void show_set_vcard(MultiEntryDlg *b)
{
	b->ok = set_vcard_dialog_ok_clicked;
	b->cancel = multi_entry_dialog_destroy;

	show_multi_entry_dialog(b);
}


/*------------------------------------------------------------------------*/
/*  End dialog for setting v-card info                                    */
/*------------------------------------------------------------------------*/

