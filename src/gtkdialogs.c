/*
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
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
#include "notify.h"
#include "prpl.h"
#include "request.h"
#include "util.h"

#include "gtkdialogs.h"
#include "gtkimhtml.h"
#include "gtkimhtmltoolbar.h"
#include "gtklog.h"
#include "gtkutils.h"
#include "stock.h"

static GList *dialogwindows = NULL;

static GtkWidget *about = NULL;

struct warning {
	GtkWidget *window;
	GtkWidget *anon;
	char *who;
	GaimConnection *gc;
};

void
gaim_gtkdialogs_destroy_all()
{
	while (dialogwindows) {
		gtk_widget_destroy(dialogwindows->data);
		dialogwindows = g_list_remove(dialogwindows, dialogwindows->data);
	}
}

static void destroy_about()
{
	if (about != NULL)
		gtk_widget_destroy(about);
	about = NULL;
}

void gaim_gtkdialogs_about(GtkWidget *w, void *data)
{
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *logo;
	GtkWidget *label;
	GtkWidget *sw;
	GtkWidget *text;
	GtkWidget *bbox;
	GtkWidget *button;
	GtkTextIter iter;
	gchar *str, *labeltext;

	if (about != NULL) {
		gtk_window_present(GTK_WINDOW(about));
		return;
	}

	GAIM_DIALOG(about);
	gtk_window_set_default_size(GTK_WINDOW(about), 450, -1);
	gtk_window_set_title(GTK_WINDOW(about), _("About Gaim"));
	gtk_window_set_role(GTK_WINDOW(about), "about");
	gtk_window_set_resizable(GTK_WINDOW(about), TRUE);

	hbox = gtk_hbox_new(FALSE, 12);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 12);
	gtk_container_add(GTK_CONTAINER(about), hbox);

	vbox = gtk_vbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(hbox), vbox);

	logo = gtk_image_new_from_stock(GAIM_STOCK_LOGO, gtk_icon_size_from_name(GAIM_ICON_SIZE_LOGO));
	gtk_box_pack_start(GTK_BOX(vbox), logo, FALSE, FALSE, 0);

	labeltext = g_strdup_printf(_("<span weight=\"bold\" size=\"larger\">Gaim v%s</span>"), VERSION);
	label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(label), labeltext);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	g_free(labeltext);

	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
				GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_IN);
	gtk_widget_set_size_request(sw, -1, 350);
	gtk_box_pack_start(GTK_BOX(vbox), sw, FALSE, FALSE, 0);

	text = gtk_imhtml_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(sw), text);
	gaim_setup_imhtml(text);

	gtk_imhtml_append_text(GTK_IMHTML(text),
			  _("Gaim is a modular messaging client capable of using "
				  "AIM, MSN, Yahoo!, Jabber, ICQ, IRC, SILC, "
				  "Novell GroupWise, Napster, Zephyr, and Gadu-Gadu "
				  "all at once.  It is written using "
				  "Gtk+ and is licensed under the GPL.<BR><BR>"), GTK_IMHTML_NO_SCROLL);

	gtk_imhtml_append_text(GTK_IMHTML(text),
			"<FONT SIZE=\"4\">URL:</FONT> <A HREF=\"" GAIM_WEBSITE "\">"
			GAIM_WEBSITE "</A><BR><BR>", GTK_IMHTML_NO_SCROLL);

	gtk_imhtml_append_text(GTK_IMHTML(text),
			_("<FONT SIZE=\"4\">IRC:</FONT> #gaim on irc.freenode.net"
			"<BR><BR>"), GTK_IMHTML_NO_SCROLL);

	/* Active Developers */
	str = g_strconcat(
		"<FONT SIZE=\"4\">", _("Active Developers"), ":</FONT><BR>"
		"  Rob Flynn (", _("maintainer"), ") "
		"&lt;<A HREF=\"mailto:gaim@robflynn.com\">gaim@robflynn.com</A>&gt;<BR>"
		"  Sean Egan (", _("lead developer"), ") "
		"&lt;<A HREF=\"mailto:sean.egan@binghamton.edu\">"
		"bj91704@binghamton.edu</A>&gt;<BR>"
		"  Christian 'ChipX86' Hammond (", _("developer & webmaster"), ")<BR>"
		"  Herman Bloggs (", _("win32 port"), ") "
		"&lt;<A HREF=\"mailto:hermanator12002@yahoo.com\">"
		"hermanator12002@yahoo.com</A>&gt;<BR>"
		"  Nathan 'faceprint' Walp (", _("developer"), ")<BR>"
		"  Mark 'KingAnt' Doliner (", _("developer"), ")<BR>"
		"  Ethan 'Paco-Paco' Blanton (", _("developer"), ")<br>"
		"  Tim 'marv' Ringenbach (", _("developer"), ")<br>"
		"  Luke 'LSchiere' Schierer (", _("support"), ")<BR>"
		"<BR>", NULL);
	gtk_imhtml_append_text(GTK_IMHTML(text), str, GTK_IMHTML_NO_SCROLL);
	g_free(str);

	/* Crazy Patch Writers */
	str = g_strconcat(
		"<FONT SIZE=\"4\">", _("Crazy Patch Writers"), ":</FONT><BR>"
		"  Daniel 'datallah' Atallah<br>"
		"  Ka-Hing 'javabsp' Cheung<br>"
		"  Decklin Foster<BR>"
		"  Gary 'grim' Kramlich<br>"
		"  Robert 'Robot101' McQueen<BR>"
		"  Benjamin Miller<BR>"
		"  Etan 'deryni' Reisner<BR>"
		"  Kevin 'SimGuy' Stange<br>"
		"  Stu 'nosnilmot' Tomlinson<br>"
		"<BR>", NULL);
	gtk_imhtml_append_text(GTK_IMHTML(text), str, GTK_IMHTML_NO_SCROLL);
	g_free(str);

	/* Retired Developers */
	str = g_strconcat(
			"<FONT SIZE=\"4\">", _("Retired Developers"), ":</FONT><BR>"
			"  Adam Fritzler (", _("former libfaim maintainer"), ")<BR>"
			"  Eric Warmenhoven (", _("former lead developer"), ") "
			"&lt;<A HREF=\"mailto:warmenhoven@yahoo.com\">"
			"warmenhoven@yahoo.com</A>&gt;<BR>"
			"  Jim Duchek (", _("former maintainer"), ")<BR>"
			"  Jim Seymour (", _("former Jabber developer"), ")<BR>"
			"  Mark Spencer (", _("original author"), ") "
			"&lt;<A HREF=\"mailto:markster@marko.net\">"
			"markster@marko.net</A>&gt;<BR>"
			"  Syd Logan (", _("hacker and designated driver [lazy bum]"), 
			")<BR>"
			"<BR>", NULL);
	gtk_imhtml_append_text(GTK_IMHTML(text), str, GTK_IMHTML_NO_SCROLL);
	g_free(str);

	/* Current Translators */
	str = g_strconcat(
			"<FONT SIZE=\"4\">", _("Current Translators"), ":</FONT><BR>"
			"  <b>", _("Bulgarian"), " (bg)</b> - Alexander Shopov &lt;<a href=\"mailto: al_shopov@users.sf.net\">al_shopov@users.sf.net</a>&gt;<br>"
			"  <b>", _("Catalan"), " (ca)</b> - Robert Millan &lt;<a href=\"mailto: zeratul2@wanadoo.es\">zeratul2@wanadoo.es</a>&gt;<br>"
			"  <b>", _("Czech"), " (cs)</b> - Miloslav Trmac &lt;<a href=\"mailto: mitr@volny.cz\">mitr@volny.cz</a>&gt;<br>"
			"  <b>", _("Danish"), " (da)</b> - Morten Brix Pedersen &lt;<a href=\"mailto: morten@wtf.dk\">morten@wtf.dk</a>&gt;<br>"
			"  <b>", _("British English"), " (en_GB)</b> - Luke Ross &lt;<a href=\"mailto: lukeross@sys3175.co.uk\">lukeross@sys3175.co.uk</a>&gt;<br>"
			"  <b>", _("Canadian English"), " (en_CA)</b> - Adam Weinberger &lt;<a href=\"mailto: adamw@gnome.org\">adamw@gnome.org</a>&gt;<br>"
			"  <b>", _("German"), " (de)</b> - Björn Voigt &lt;<a href=\"mailto: bjoern@cs.tu-berlin.de\">bjoern@cs.tu-berlin.de</a>&gt;<br>"
			"  <b>", _("Spanish"), " (es)</b> - Javier Fernández-Sanguino Peña &lt;<a href=\"mailto: jfs@debian.org\">jfs@debian.org</a>&gt;<br>"
			"  <b>", _("Finnish"), " (fi)</b> - Arto Alakulju &lt;<a href=\"mailto: arto@alakulju.net\">arto@alakulju.net</a>&gt;<br>"
			"  <b>", _("French"), " (fr)</b> - Éric Boumaour &lt;<a href=\"mailto: zongo_fr@users.sourceforge.net\">zongo_fr@users.sourceforge.net</a>&gt;<br>"
			"  <b>", _("Hebrew"), " (he)</b> - Pavel Bibergal &lt;<a href=\"mailto:cyberkm203@hotmail.com\">cyberkm203@hotmail.com</a>&gt;<br>"
			"  <b>", _("Hindi"), " (hi)</b> - Ravishankar Shrivastava &lt;<a href=\"mailto: raviratlami@yahoo.com\">raviratlami@yahoo.com</a>&gt;<br>"
			"  <b>", _("Hungarian"), " (hu)</b> - Zoltan Sutto &lt;<a href=\"mailto: suttozoltan@chello.hu\">suttozoltan@chello.hu</a>&gt;<br>"
			"  <b>", _("Italian"), " (it)</b> - Claudio Satriano &lt;<a href=\"mailto: satriano@na.infn.it\">satriano@na.infn.it</a>&gt;<br>"
			"  <b>", _("Japanese"), " (ja)</b> - Takashi Aihana &lt;<a href=\"mailto: aihana@gnome.gr.jp\">aihana@gnome.gr.jp</a>&gt;<br>"
			"  <b>", _("Lithuanian"), " (lt)</b> - Gediminas Čičinskas &lt;<a href=\"mailto: gediminas@parok.lt\">gediminas@parok.lt</a>&gt;<br>"
			"  <b>", _("Korean"), " (ko)</b> - Kyung-uk Son &lt;<a href=\"mailto: vvs740@chol.com\">vvs740@chol.com</a>&gt;<br>"
			"  <b>", _("Dutch; Flemish"), " (nl)</b> - Vincent van Adrighem &lt;<a href=\"mailto: V.vanAdrighem@dirck.mine.nu\">V.vanAdrighem@dirck.mine.nu</a>&gt;<br>"
			"  <b>", _("Macedonian"), " (mk)</b> - Tomislav Markovski &lt;<a href=\"mailto: herrera@users.sf.net\">herrera@users.sf.net</a>&gt;<br>"
			"  <b>", _("Norwegian"), " (no)</b> - Petter Johan Olsen &lt;<a href=\"mailto:petter.olsen@cc.uit.no\">petter.olsen@cc.uit.no</a>&gt;<br>"
			"  <b>", _("Polish"), " (pl)</b> - Krzysztof Foltman &lt;<a href=\"mailto:krzysztof@foltman.com\">krzysztof@foltman.com</a>&gt;, Emil Nowak &lt;<a href=\"mailto:emil5@go2.pl\">emil5@go2.pl</a>&gt;<br>"
			"  <b>", _("Portuguese"), " (pt)</b> - Duarte Henriques &lt;<a href=\"mailto:duarte_henriques@myrealbox.com\">duarte_henriques@myrealbox.com</a>&gt;<br>"
			"  <b>", _("Portuguese-Brazil"), " (pt_BR)</b> - Maurício de Lemos Rodrigues Collares Neto &lt;<a href=\"mailto: mauricioc@gmail.com\">mauricioc@gmail.com</a>&gt;<br>"
			"  <b>", _("Romanian"), " (ro)</b> - Mişu Moldovan &lt;<a href=\"mailto: dumol@go.ro\">dumol@go.ro</a>&gt;<br>"
			"  <b>", _("Russian"), " (ru)</b> - Dmitry Beloglazov &lt;<a href=\"mailto: dmaa@users.sf.net\">dmaa@users.sf.net</a>&gt;<br>"
			"  <b>", _("Serbian"), " (sr)</b> - Danilo Šegan &lt;<a href=\"mailto: dsegan@gmx.net\">dsegan@gmx.net</a>&gt;, Aleksandar Urosevic &lt;<a href=\"mailto: urke@users.sourceforge.net\">urke@users.sourceforge.net</a>&gt;<br>"
			"  <b>", _("Slovenian"), " (sl)</b> - Matjaz Horvat &lt;<a href=\"mailto: matjaz@owca.info\">matjaz@owca.info</a>&gt;<br>"
			"  <b>", _("Swedish"), " (sv)</b> - Tore Lundqvist &lt;<a href=\"mailto: tlt@mima.x.se\">tlt@mima.x.se</a>&gt;<br>"
			"  <b>", _("Vietnamese"), " (vi)</b> - T.M.Thanh ", _("and the Gnome-Vi Team"), " &lt;<a href=\"mailto: gnomevi-list@lists.sf.net\">gnomevi-list@lists.sf.net</a>&gt;<br>"
			"  <b>", _("Simplified Chinese"), " (zh_CN)</b> - Funda Wang &lt;<a href=\"mailto: fundawang@linux.net.cn\">fundawang@linux.net.cn</a>&gt;<br>"
			"  <b>", _("Traditional Chinese"), " (zh_TW)</b> - Ambrose C. Li &lt;<a href=\"mailto: acli@ada.dhs.org\">acli@ada.dhs.org</a>&gt;, Paladin R. Liu &lt;<a href=\"mailto: paladin@ms1.hinet.net\">paladin@ms1.hinet.net</a>&gt;<br>"
			"<BR>", NULL);
	gtk_imhtml_append_text(GTK_IMHTML(text), str, GTK_IMHTML_NO_SCROLL);
	g_free(str);

	/* Past Translators */
	str = g_strconcat(
			"<FONT SIZE=\"4\">", _("Past Translators"), ":</FONT><BR>"
			"  <b>", _("Amharic"), " (am)</b> - Daniel Yacob<br>"
			"  <b>", _("Bulgarian"), " (bg)</b> - Hristo Todorov<br>"
			"  <b>", _("Catalan"), " (ca)</b> - JM Pérez Cáncer<br>"
			"  <b>", _("Czech"), " (cs)</b> - Honza Král<br>"
			"  <b>", _("German"), " (de)</b> - Daniel Seifert, Karsten Weiss<br>"
			"  <b>", _("Spanish"), " (es)</b> - Amaya Rodrigo, Alejandro G Villar, Nicolás Lichtmaier, JM Pérez Cáncer<br>"
			"  <b>", _("Finnish"), " (fi)</b> - Tero Kuusela<br>"
			"  <b>", _("French"), " (fr)</b> - Sébastien François, Stéphane Pontier, Stéphane Wirtel, Loïc Jeannin<br>"
			"  <b>", _("Italian"), " (it)</b> - Salvatore di Maggio<br>"
			"  <b>", _("Japanese"), " (ja)</b> - Ryosuke Kutsuna, Taku Yasui, Junichi Uekawa<br>"
			"  <b>", _("Korean"), " (ko)</b> - Sang-hyun S, A Ho-seok Lee<br>"
			"  <b>", _("Polish"), " (pl)</b> - Przemysław Sułek<br>"
			"  <b>", _("Russian"), " (ru)</b> - Sergey Volozhanin<br>"
			"  <b>", _("Russian"), "(ru)</b> - Alexandre Prokoudine<br>"
			"  <b>", _("Slovak"), " (sk)</b> - Daniel Režný<br>"
			"  <b>", _("Swedish"), " (sv)</b> - Christian Rose<br>"
			"  <b>", _("Chinese"), " (zh_CN, zh_TW)</b> - Hashao, Rocky S. Lee<br>"
			"<BR>", NULL);
	gtk_imhtml_append_text(GTK_IMHTML(text), str, GTK_IMHTML_NO_SCROLL);
	g_free(str);

	gtk_adjustment_set_value(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(sw)), 0);
	gtk_text_buffer_get_start_iter(gtk_text_view_get_buffer(GTK_TEXT_VIEW(text)), &iter);
	gtk_text_buffer_place_cursor(gtk_text_view_get_buffer(GTK_TEXT_VIEW(text)), &iter);
	
	/* Close Button */
	bbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 0);

	button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);

	g_signal_connect_swapped(G_OBJECT(button), "clicked",
							 G_CALLBACK(destroy_about), G_OBJECT(about));
	g_signal_connect(G_OBJECT(about), "destroy",
					 G_CALLBACK(destroy_about), G_OBJECT(about));

	/* this makes the sizes not work? */
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_widget_grab_default(button);

	/* Let's give'em something to talk about -- woah woah woah */
	gtk_widget_show_all(about);
	gtk_window_present(GTK_WINDOW(about));
}

static void
gaim_gtkdialogs_im_cb(gpointer data, GaimRequestFields *fields)
{
	GaimAccount *account;
	const char *username;

	account  = gaim_request_fields_get_account(fields, "account");
	username = gaim_request_fields_get_string(fields,  "screenname");

	gaim_gtkdialogs_im_with_user(account, username);
}

void
gaim_gtkdialogs_im(void)
{
	GaimRequestFields *fields;
	GaimRequestFieldGroup *group;
	GaimRequestField *field;

	fields = gaim_request_fields_new();

	group = gaim_request_field_group_new(NULL);
	gaim_request_fields_add_group(fields, group);

	field = gaim_request_field_string_new("screenname", _("_Screen name"),
										  NULL, FALSE);
	gaim_request_field_set_required(field, TRUE);
	gaim_request_field_set_type_hint(field, "screenname");
	gaim_request_field_group_add_field(group, field);

	field = gaim_request_field_account_new("account", _("_Account"), NULL);
	gaim_request_field_set_visible(field,
		(gaim_connections_get_all() != NULL &&
		 gaim_connections_get_all()->next != NULL));
	gaim_request_field_set_required(field, TRUE);
	gaim_request_field_group_add_field(group, field);

	gaim_request_fields(gaim_get_blist(), _("New Instant Message"),
						NULL,
						_("Please enter the screen name of the person you "
						  "would like to IM."),
						fields,
						_("OK"), G_CALLBACK(gaim_gtkdialogs_im_cb),
						_("Cancel"), NULL,
						NULL);
}

void
gaim_gtkdialogs_im_with_user(GaimAccount *account, const char *username)
{
	GaimConversation *conv;
	GaimConvWindow *win;
	GaimGtkWindow *gtkwin;

	g_return_if_fail(account != NULL);
	g_return_if_fail(username != NULL);

	conv = gaim_find_conversation_with_account(username, account);

	if (conv == NULL)
		conv = gaim_conversation_new(GAIM_CONV_IM, account, username);

	win = gaim_conversation_get_window(conv);
	gtkwin = GAIM_GTK_WINDOW(win);

	gtk_window_present(GTK_WINDOW(gtkwin->window));
	gaim_conv_window_switch_conversation(win, gaim_conversation_get_index(conv));
}

static gboolean
gaim_gtkdialogs_ee(const char *ee)
{
	GtkWidget *window;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *img = gtk_image_new_from_stock(GAIM_STOCK_DIALOG_COOL, GTK_ICON_SIZE_DIALOG);
	gchar *norm = gaim_strreplace(ee, "rocksmyworld", "");

	label = gtk_label_new(NULL);
	if (!strcmp(norm, "zilding"))
		gtk_label_set_markup(GTK_LABEL(label),
				     "<span weight=\"bold\" size=\"large\" foreground=\"purple\">Amazing!  Simply Amazing!</span>");
	else if (!strcmp(norm, "robflynn"))
		gtk_label_set_markup(GTK_LABEL(label),
				     "<span weight=\"bold\" size=\"large\" foreground=\"#1f6bad\">Pimpin\' Penguin Style! *Waddle Waddle*</span>");
	else if (!strcmp(norm, "flynorange"))
		gtk_label_set_markup(GTK_LABEL(label),
				      "<span weight=\"bold\" size=\"large\" foreground=\"blue\">You should be me.  I'm so cute!</span>");
	else if (!strcmp(norm, "ewarmenhoven"))
		gtk_label_set_markup(GTK_LABEL(label),
				     "<span weight=\"bold\" size=\"large\" foreground=\"orange\">Now that's what I like!</span>");
	else if (!strcmp(norm, "markster97"))
		gtk_label_set_markup(GTK_LABEL(label),
				     "<span weight=\"bold\" size=\"large\" foreground=\"brown\">Ahh, and excellent choice!</span>");
	else if (!strcmp(norm, "seanegn"))
		gtk_label_set_markup(GTK_LABEL(label),
				     "<span weight=\"bold\" size=\"large\" foreground=\"#009900\">Everytime you click my name, an angel gets its wings.</span>");
	else if (!strcmp(norm, "chipx86"))
		gtk_label_set_markup(GTK_LABEL(label),
				     "<span weight=\"bold\" size=\"large\" foreground=\"red\">This sunflower seed taste like pizza.</span>");
	else if (!strcmp(norm, "markdoliner"))
		gtk_label_set_markup(GTK_LABEL(label),
				     "<span weight=\"bold\" size=\"large\" foreground=\"#6364B1\">Hey!  I was in that tumbleweed!</span>");
	else if (!strcmp(norm, "lschiere"))
		gtk_label_set_markup(GTK_LABEL(label),
				     "<span weight=\"bold\" size=\"large\" foreground=\"gray\">I'm not anything.</span>");
	g_free(norm);

	if (strlen(gtk_label_get_label(GTK_LABEL(label))) <= 0)
		return FALSE;

	window = gtk_dialog_new_with_buttons(GAIM_ALERT_TITLE, NULL, 0, GTK_STOCK_CLOSE, GTK_RESPONSE_OK, NULL);
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
	return TRUE;
}

static void
gaim_gtkdialogs_info_cb(gpointer data, GaimRequestFields *fields)
{
	char *username;
	gboolean found = FALSE;
	GaimAccount *account;

	account  = gaim_request_fields_get_account(fields, "account");

	username = g_strdup(gaim_normalize(account,
		gaim_request_fields_get_string(fields,  "screenname")));

	if (username != NULL && gaim_str_has_suffix(username, "rocksmyworld"))
		found = gaim_gtkdialogs_ee(username);

	if (!found && username != NULL && *username != '\0' && account != NULL)
		serv_get_info(gaim_account_get_connection(account), username);

	g_free(username);
}

void
gaim_gtkdialogs_info(void)
{
	GaimRequestFields *fields;
	GaimRequestFieldGroup *group;
	GaimRequestField *field;

	fields = gaim_request_fields_new();

	group = gaim_request_field_group_new(NULL);
	gaim_request_fields_add_group(fields, group);

	field = gaim_request_field_string_new("screenname", _("_Screen name"),
										  NULL, FALSE);
	gaim_request_field_set_type_hint(field, "screenname");
	gaim_request_field_set_required(field, TRUE);
	gaim_request_field_group_add_field(group, field);

	field = gaim_request_field_account_new("account", _("_Account"), NULL);
	gaim_request_field_set_visible(field,
		(gaim_connections_get_all() != NULL &&
		 gaim_connections_get_all()->next != NULL));
	gaim_request_field_set_required(field, TRUE);
	gaim_request_field_group_add_field(group, field);

	gaim_request_fields(gaim_get_blist(), _("Get User Info"),
						NULL,
						_("Please enter the screen name of the person whose "
						  "info you would like to view."),
						fields,
						_("OK"), G_CALLBACK(gaim_gtkdialogs_info_cb),
						_("Cancel"), NULL,
						NULL);
}

static void
gaim_gtkdialogs_log_cb(gpointer data, GaimRequestFields *fields)
{
	char *username;
	GaimAccount *account;

	account  = gaim_request_fields_get_account(fields, "account");

	username = g_strdup(gaim_normalize(account,
		gaim_request_fields_get_string(fields,  "screenname")));

	if( username != NULL && *username != '\0' && account != NULL )
		gaim_gtk_log_show( username, account );

	g_free(username);
}

void
gaim_gtkdialogs_log(void)
{
	GaimRequestFields *fields;
	GaimRequestFieldGroup *group;
	GaimRequestField *field;

	fields = gaim_request_fields_new();

	group = gaim_request_field_group_new(NULL);
	gaim_request_fields_add_group(fields, group);

	field = gaim_request_field_string_new("screenname", _("_Screen name"),
										  NULL, FALSE);
	gaim_request_field_set_type_hint(field, "screenname");
	gaim_request_field_set_required(field, TRUE);
	gaim_request_field_group_add_field(group, field);

	field = gaim_request_field_account_new("account", _("_Account"), NULL);
	gaim_request_field_account_set_show_all(field, TRUE);
	gaim_request_field_set_visible(field,
		(gaim_accounts_get_all() != NULL &&
		 gaim_accounts_get_all()->next != NULL));
	gaim_request_field_set_required(field, TRUE);
	gaim_request_field_group_add_field(group, field);

	gaim_request_fields(gaim_get_blist(), _("Get User Log"),
						NULL,
						_("Please enter the screen name of the person whose "
						  "log you would like to view."),
						fields,
						_("OK"), G_CALLBACK(gaim_gtkdialogs_log_cb),
						_("Cancel"), NULL,
						NULL);
}

static void
gaim_gtkdialogs_warn_cb(GtkWidget *widget, gint resp, struct warning *w)
{
	if (resp == GTK_RESPONSE_OK)
		serv_warn(w->gc, w->who, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->anon)));

	dialogwindows = g_list_remove(dialogwindows, w->window);
	gtk_widget_destroy(w->window);

	g_free(w->who);
	g_free(w);
}

/*
 * XXX - Make this use the request API, if possible.
 */
void
gaim_gtkdialogs_warn(GaimConnection *gc, const char *who)
{
	gchar *labeltext;
	GtkWidget *hbox, *vbox;
	GtkWidget *label;
	GtkWidget *img;
	struct warning *w;

	g_return_if_fail(gc != NULL);
	g_return_if_fail(who != NULL);

	w = g_new0(struct warning, 1);
	w->who = g_strdup(who);
	w->gc = gc;

	w->window = gtk_dialog_new_with_buttons(_("Warn User"), NULL, 0,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GAIM_STOCK_WARN, GTK_RESPONSE_OK, NULL);
	gtk_dialog_set_default_response (GTK_DIALOG(w->window), GTK_RESPONSE_OK);
	g_signal_connect(G_OBJECT(w->window), "response", G_CALLBACK(gaim_gtkdialogs_warn_cb), w);

	gtk_container_set_border_width (GTK_CONTAINER(w->window), 6);
	gtk_window_set_resizable(GTK_WINDOW(w->window), FALSE);
	gtk_dialog_set_has_separator(GTK_DIALOG(w->window), FALSE);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(w->window)->vbox), 12);
	gtk_container_set_border_width (GTK_CONTAINER(GTK_DIALOG(w->window)->vbox), 6);

	hbox = gtk_hbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(w->window)->vbox), hbox);

	img = gtk_image_new_from_stock(GAIM_STOCK_DIALOG_WARNING, GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment(GTK_MISC(img), 0, 0);
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
	label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(label), labeltext);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	dialogwindows = g_list_prepend(dialogwindows, w->window);
	gtk_widget_show_all(w->window);
}

static void
gaim_gtkdialogs_alias_contact_cb(GaimContact *contact, const char *new_alias)
{
	gaim_contact_set_alias(contact, new_alias);
}

void
gaim_gtkdialogs_alias_contact(GaimContact *contact)
{
	g_return_if_fail(contact != NULL);

	gaim_request_input(NULL, _("Alias Contact"), NULL,
					   _("Enter an alias for this contact."),
					   contact->alias, FALSE, FALSE, NULL,
					   _("Alias"), G_CALLBACK(gaim_gtkdialogs_alias_contact_cb),
					   _("Cancel"), NULL, contact);
}

static void
gaim_gtkdialogs_alias_buddy_cb(GaimBuddy *buddy, const char *new_alias)
{
	gaim_blist_alias_buddy(buddy, new_alias);
	serv_alias_buddy(buddy);
}

void
gaim_gtkdialogs_alias_buddy(GaimBuddy *buddy)
{
	gchar *secondary;

	g_return_if_fail(buddy != NULL);

	secondary = g_strdup_printf(_("Enter an alias for %s."), buddy->name);

	gaim_request_input(NULL, _("Alias Buddy"), NULL,
					   secondary, buddy->alias, FALSE, FALSE, NULL,
					   _("Alias"), G_CALLBACK(gaim_gtkdialogs_alias_buddy_cb),
					   _("Cancel"), NULL, buddy);

	g_free(secondary);
}

static void
gaim_gtkdialogs_alias_chat_cb(GaimChat *chat, const char *new_alias)
{
	gaim_blist_alias_chat(chat, new_alias);
}

void
gaim_gtkdialogs_alias_chat(GaimChat *chat)
{
	g_return_if_fail(chat != NULL);

	gaim_request_input(NULL, _("Alias Chat"), NULL,
					   _("Enter an alias for this chat."),
					   chat->alias, FALSE, FALSE, NULL,
					   _("Alias"), G_CALLBACK(gaim_gtkdialogs_alias_chat_cb),
					   _("Cancel"), NULL, chat);
}

static void
gaim_gtkdialogs_remove_contact_cb(GaimContact *contact)
{
	GaimBlistNode *bnode, *cnode;
	GaimGroup *group;

	cnode = (GaimBlistNode *)contact;
	group = (GaimGroup*)cnode->parent;
	for (bnode = cnode->child; bnode; bnode = bnode->next) {
		GaimBuddy *buddy = (GaimBuddy*)bnode;
		if (gaim_account_is_connected(buddy->account))
			serv_remove_buddy(buddy->account->gc, buddy, group);
	}
	gaim_blist_remove_contact(contact);
}

void
gaim_gtkdialogs_remove_contact(GaimContact *contact)
{
	GaimBuddy *buddy = gaim_contact_get_priority_buddy(contact);

	g_return_if_fail(contact != NULL);
	g_return_if_fail(buddy != NULL);

	if (((GaimBlistNode*)contact)->child == (GaimBlistNode*)buddy &&
			!((GaimBlistNode*)buddy)->next) {
		gaim_gtkdialogs_remove_buddy(buddy);
	} else {
		gchar *text = g_strdup_printf(_("You are about to remove the contact containing %s and %d other buddies from your buddy list.  Do you want to continue?"),
			       buddy->name, contact->totalsize - 1);

		gaim_request_action(NULL, NULL, _("Remove Contact"), text, -1, contact, 2,
				_("Remove Contact"), G_CALLBACK(gaim_gtkdialogs_remove_contact_cb),
				_("Cancel"), NULL);

		g_free(text);
	}
}

void
gaim_gtkdialogs_remove_group_cb(GaimGroup *group)
{
	GaimBlistNode *cnode, *bnode;

	cnode = ((GaimBlistNode*)group)->child;

	while (cnode) {
		if (GAIM_BLIST_NODE_IS_CONTACT(cnode)) {
			bnode = cnode->child;
			cnode = cnode->next;
			while (bnode) {
				GaimBuddy *buddy;
				if (GAIM_BLIST_NODE_IS_BUDDY(bnode)) {
					GaimConversation *conv;
					buddy = (GaimBuddy*)bnode;
					bnode = bnode->next;
					conv = gaim_find_conversation_with_account(buddy->name, buddy->account);
					if (gaim_account_is_connected(buddy->account)) {
						serv_remove_buddy(buddy->account->gc, buddy, group);
						gaim_blist_remove_buddy(buddy);
						if (conv)
							gaim_conversation_update(conv,
									GAIM_CONV_UPDATE_REMOVE);
					}
				} else {
					bnode = bnode->next;
				}
			}
		} else if (GAIM_BLIST_NODE_IS_CHAT(cnode)) {
			GaimChat *chat = (GaimChat *)cnode;
			cnode = cnode->next;
			if (gaim_account_is_connected(chat->account))
				gaim_blist_remove_chat(chat);
		} else {
			cnode = cnode->next;
		}
	}

	gaim_blist_remove_group(group);
}

void
gaim_gtkdialogs_remove_group(GaimGroup *group)
{
	gchar *text;

	g_return_if_fail(group != NULL);

	text = g_strdup_printf(_("You are about to remove the group %s and all its members from your buddy list.  Do you want to continue?"),
						   group->name);

	gaim_request_action(NULL, NULL, _("Remove Group"), text, -1, group, 2,
						_("Remove Group"), G_CALLBACK(gaim_gtkdialogs_remove_group_cb),
						_("Cancel"), NULL);

	g_free(text);
}

static void
gaim_gtkdialogs_remove_buddy_cb(GaimBuddy *buddy)
{
	GaimGroup *group;
	GaimConversation *conv;
	gchar *name;
	GaimAccount *account;

	group = gaim_find_buddys_group(buddy);
	name = g_strdup(buddy->name); /* b->name is a crasher after remove_buddy */
	account = buddy->account;

	gaim_debug_info("blist", "Removing '%s' from buddy list.\n", buddy->name);
	/* XXX - Should remove from blist first... then call serv_remove_buddy()? */
	serv_remove_buddy(buddy->account->gc, buddy, group);
	gaim_blist_remove_buddy(buddy);

	conv = gaim_find_conversation_with_account(name, account);

	if (conv != NULL)
		gaim_conversation_update(conv, GAIM_CONV_UPDATE_REMOVE);

	g_free(name);
}

void
gaim_gtkdialogs_remove_buddy(GaimBuddy *buddy)
{
	gchar *text;

	g_return_if_fail(buddy != NULL);

	text = g_strdup_printf(_("You are about to remove %s from your buddy list.  Do you want to continue?"),
						   buddy->name);

	gaim_request_action(NULL, NULL, _("Remove Buddy"), text, -1, buddy, 2,
						_("Remove Buddy"), G_CALLBACK(gaim_gtkdialogs_remove_buddy_cb),
						_("Cancel"), NULL);

	g_free(text);
}

static void
gaim_gtkdialogs_remove_chat_cb(GaimChat *chat)
{
	gaim_blist_remove_chat(chat);
}

void
gaim_gtkdialogs_remove_chat(GaimChat *chat)
{
	gchar *name = gaim_chat_get_display_name(chat);
	gchar *text = g_strdup_printf(_("You are about to remove the chat %s from your buddy list.  Do you want to continue?"), name);

	g_return_if_fail(chat != NULL);

	gaim_request_action(NULL, NULL, _("Remove Chat"), text, -1, chat, 2,
						_("Remove Chat"), G_CALLBACK(gaim_gtkdialogs_remove_chat_cb),
						_("Cancel"), NULL);

	g_free(name);
	g_free(text);
}
