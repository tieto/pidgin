/*
 * @file gtkdialogs.c GTK+ Dialogs
 * @ingroup gtkui
 *
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
#include "internal.h"
#include "gtkgaim.h"

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
#include "gtkstock.h"

static GList *dialogwindows = NULL;

static GtkWidget *about = NULL;

struct warning {
	GtkWidget *window;
	GtkWidget *anon;
	char *who;
	GaimConnection *gc;
};

struct developer {
	char *name;
	char *role;
	char *email;
};

struct translator {
	char *language;
	char *abbr;
	char *name;
	char *email;
};

static struct developer developers[] = {
	{"Sean Egan",					N_("lead developer"), "sean.egan@binghamton.edu"},
	{"Daniel 'datallah' Atallah",	N_("developer"),	NULL},
	{"Christian 'ChipX86' Hammond",	N_("developer & webmaster"), NULL},
	{"Herman Bloggs",				N_("win32 port"), "herman@bluedigits.com"},
	{"Nathan 'faceprint' Walp",		N_("developer"), NULL},
	{"Mark 'KingAnt' Doliner",		N_("developer"), NULL},
	{"Ethan 'Paco-Paco' Blanton",	N_("developer"), NULL},
	{"Etan 'deryni' Reisner",       N_("developer"), NULL},
	{"Tim 'marv' Ringenbach",		N_("developer"), NULL},
	{"Luke 'LSchiere' Schierer",	N_("support"), "lschiere@users.sf.net"},
	{"Stu 'nosnilmot' Tomlinson",	N_("developer"), NULL},
	{"Gary 'grim' Kramlich",		N_("developer"), NULL},
	{NULL, NULL, NULL}
};

static struct developer patch_writers[] = {
	{"Ka-Hing 'javabsp' Cheung",	NULL,	NULL},
	{"Felipe 'shx' Contreras",		NULL,	NULL},
	{"Decklin Foster",				NULL,	NULL},
	{"Richard 'rlaager' Laager",	NULL,	NULL},
	{"Robert 'Robot101' McQueen",	NULL,	NULL},
	{"Benjamin Miller",				NULL,	NULL},
	{"Kevin 'SimGuy' Stange",		NULL,	NULL},
	{NULL, NULL, NULL}
};

static struct developer retired_developers[] = {
	{"Rob Flynn",					N_("maintainer"), "gaim@robflynn.com"},
	{"Adam Fritzler",		N_("libfaim maintainer"), NULL},
	{"Eric Warmenhoven",	N_("lead developer"), "warmenhoven@yahoo.com"},
	{"Jim Duchek",			N_("maintainer"), NULL},
	{"Jim Seymour",			N_("Jabber developer"), NULL},
	{"Mark Spencer",		N_("original author"), "markster@marko.net"},
	{"Syd Logan",			N_("hacker and designated driver [lazy bum]"), NULL},
	{NULL, NULL, NULL}
};

static struct translator current_translators[] = {
	{N_("Bulgarian"),           "bg", "Vladimira Girginova", "missing@here.is"},
	{N_("Bulgarian"),           "bg", "Vladimir (Kaladan) Petkov", "vpetkov@i-space.org"},
	{N_("Catalan"),             "ca", "Josep Puigdemont", "tradgnome@softcatala.org"},
	{N_("Czech"),               "cs", "Miloslav Trmac", "mitr@volny.cz"},
	{N_("Danish"),              "da", "Morten Brix Pedersen", "morten@wtf.dk"},
	{N_("German"),              "de", "Björn Voigt", "bjoern@cs.tu-berlin.de"},
	{N_("Australian English"),  "en_AU", "Peter Lawler", "trans@six-by-nine.com.au"},
	{N_("British English"),     "en_GB", "Luke Ross", "lukeross@sys3175.co.uk"},
	{N_("Canadian English"),    "en_CA", "Adam Weinberger", "adamw@gnome.org"},
	{N_("Spanish"),             "es", "Javier Fernández-Sanguino Peña", "jfs@debian.org"},
	{N_("Finnish"),             "fi", "Arto Alakulju", "arto@alakulju.net"},
	{N_("French"),              "fr", "Éric Boumaour", "zongo_fr@users.sourceforge.net"},
	{N_("Hebrew"),              "he", "Pavel Bibergal", "cyberkm203@hotmail.com"},
	{N_("Hindi"),               "hi", "Ravishankar Shrivastava", "raviratlami@yahoo.com"},
	{N_("Hungarian"),           "hu", "Zoltan Sutto", "sutto.zoltan@rutinsoft.hu"},
	{N_("Italian"),             "it", "Claudio Satriano", "satriano@na.infn.it"},
	{N_("Japanese"),            "ja", "Takashi Aihana", "aihana@gnome.gr.jp"},
	{N_("Lithuanian"),          "lt", "Gediminas Čičinskas", "gediminas@parok.lt"},
	{N_("Georgian"),            "ka", "Temuri Doghonadze", "admin@security.gov.ge"},
	{N_("Korean"),              "ko", "Kyung-uk Son", "vvs740@chol.com"},
	{N_("Dutch, Flemish"),      "nl", "Vincent van Adrighem", "V.vanAdrighem@dirck.mine.nu"},
	{N_("Macedonian"),          "mk", "Tomislav Markovski", "herrera@users.sf.net"},
	{N_("Norwegian"),           "no", "Petter Johan Olsen", "petter.olsen@cc.uit.no"},
	{N_("Polish"),              "pl", "Emil Nowak", "emil5@go2.pl"},
	{N_("Polish"),              "pl", "Krzysztof Foltman", "krzysztof@foltman.com"},
	{N_("Portuguese"),          "pt", "Duarte Henriques", "duarte_henriques@myrealbox.com"},
	{N_("Portuguese-Brazil"),   "pt_BR", "Maurício de Lemos Rodrigues Collares Neto", "mauricioc@gmail.com"},
	{N_("Romanian"),            "ro", "Mişu Moldovan", "dumol@go.ro"},
	{N_("Russian"),             "ru", "Dmitry Beloglazov", "dmaa@users.sf.net"},
	{N_("Serbian"),             "sr", "Danilo Šegan", "dsegan@gmx.net"},
	{N_("Serbian"),             "sr", "Aleksandar Urosevic", "urke@users.sourceforge.net"},
	{N_("Slovenian"),           "sl", "Matjaz Horvat", "matjaz@owca.info"},
	{N_("Swedish"),             "sv", "Tore Lundqvist", "tlt@mima.x.se"},
	{N_("Vietnamese"),          "vi", N_("T.M.Thanh and the Gnome-Vi Team"), "gnomevi-list@lists.sf.net"},
	{N_("Simplified Chinese"),  "zh_CN", "Funda Wang", "fundawang@linux.net.cn"},
	{N_("Traditional Chinese"), "zh_TW", "Ambrose C. Li", "acli@ada.dhs.org"},
	{N_("Traditional Chinese"), "zh_TW", "Paladin R. Liu", "paladin@ms1.hinet.net"},
	{NULL, NULL, NULL, NULL}
};


static struct translator past_translators[] = {
	{N_("Amharic"),				"am", "Daniel Yacob", NULL},
	{N_("Bulgarian"),			"bg", "Hristo Todorov", NULL},
	{N_("Catalan"),				"ca", "JM Pérez Cáncer", NULL},
	{N_("Catalan"),				"ca", "Robert Millan", NULL},
	{N_("Czech"),				"cs", "Honza Král", NULL},
	{N_("German"),				"de", "Daniel Seifert, Karsten Weiss", NULL},
	{N_("Spanish"),				"es", "Amaya Rodrigo, Alejandro G Villar, Nicolás Lichtmaier, JM Pérez Cáncer", NULL},
	{N_("Finnish"),				"fi", "Tero Kuusela", NULL},
	{N_("French"),				"fr", "Sébastien François, Stéphane Pontier, Stéphane Wirtel, Loïc Jeannin", NULL},
	{N_("Italian"),				"it", "Salvatore di Maggio", NULL},
	{N_("Japanese"),			"ja", "Ryosuke Kutsuna, Taku Yasui, Junichi Uekawa", NULL},
	{N_("Korean"),				"ko", "Sang-hyun S, A Ho-seok Lee", NULL},
	{N_("Polish"),				"pl", "Przemysław Sułek", NULL},
	{N_("Russian"),				"ru", "Sergey Volozhanin", NULL},
	{N_("Russian"),				"ru", "Alexandre Prokoudine", NULL},
	{N_("Slovak"),				"sk", "Daniel Režný", NULL},
	{N_("Swedish"),				"sv", "Christian Rose", NULL},
	{N_("Simplified Chinese"),	"zh_CN", "Hashao, Rocky S. Lee", NULL},
	{N_("Traditional Chinese"),	"zh_TW", "Hashao, Rocky S. Lee", NULL},
	{NULL, NULL, NULL, NULL}
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
	GtkWidget *frame;
	GtkWidget *text;
	GtkWidget *bbox;
	GtkWidget *button;
	GtkTextIter iter;
	GString *str;
	int i;
	AtkObject *obj;

	if (about != NULL) {
		gtk_window_present(GTK_WINDOW(about));
		return;
	}

	GAIM_DIALOG(about);
	gtk_window_set_default_size(GTK_WINDOW(about), 450, -1);
	gtk_window_set_title(GTK_WINDOW(about), _("About Gaim"));
	gtk_window_set_role(GTK_WINDOW(about), "about");
	gtk_window_set_resizable(GTK_WINDOW(about), TRUE);
	gtk_window_set_default_size(GTK_WINDOW(about), 340, 550); /* Golden ratio in da hizzy */

	hbox = gtk_hbox_new(FALSE, 12);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 12);
	gtk_container_add(GTK_CONTAINER(about), hbox);

	vbox = gtk_vbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(hbox), vbox);

	logo = gtk_image_new_from_stock(GAIM_STOCK_LOGO, gtk_icon_size_from_name(GAIM_ICON_SIZE_LOGO));
	obj = gtk_widget_get_accessible(logo);
	atk_object_set_description(obj, "Gaim " VERSION);
	gtk_box_pack_start(GTK_BOX(vbox), logo, FALSE, FALSE, 0);

	frame = gaim_gtk_create_imhtml(FALSE, &text, NULL);
	gtk_imhtml_set_format_functions(GTK_IMHTML(text), GTK_IMHTML_ALL ^ GTK_IMHTML_SMILEY);
	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);

	str = g_string_sized_new(4096);

	g_string_append(str,
					_("Gaim is a modular messaging client capable of using "
					  "AIM, MSN, Yahoo!, Jabber, ICQ, IRC, SILC, "
					  "Novell GroupWise, Napster, Zephyr, and Gadu-Gadu "
					  "all at once.  It is written using "
					  "GTK+ and is licensed under the GNU GPL.<BR><BR>"));

	g_string_append(str, "<FONT SIZE=\"4\">URL:</FONT> <A HREF=\""
					GAIM_WEBSITE "\">" GAIM_WEBSITE "</A><BR/><BR/>");
#ifdef _WIN32
	g_string_append_printf(str, _("<FONT SIZE=\"4\">IRC:</FONT> "
						   "#wingaim on irc.freenode.net<BR><BR>"));
#else
	g_string_append_printf(str, _("<FONT SIZE=\"4\">IRC:</FONT> "
						   "#gaim on irc.freenode.net<BR><BR>"));
#endif

	/* Active Developers */
	g_string_append_printf(str, "<FONT SIZE=\"4\">%s:</FONT><BR/>",
						   _("Active Developers"));
	for (i = 0; developers[i].name != NULL; i++) {
		if (developers[i].email != NULL) {
			g_string_append_printf(str, "  %s (%s) &lt;<a href=\"mailto:%s\">%s</a>&gt;<br/>",
					developers[i].name, _(developers[i].role),
					developers[i].email, developers[i].email);
		} else {
			g_string_append_printf(str, "  %s (%s)<br/>",
					developers[i].name, _(developers[i].role));
		}
	}
	g_string_append(str, "<BR/>");

	/* Crazy Patch Writers */
	g_string_append_printf(str, "<FONT SIZE=\"4\">%s:</FONT><BR/>",
						   _("Crazy Patch Writers"));
	for (i = 0; patch_writers[i].name != NULL; i++) {
		if (patch_writers[i].email != NULL) {
			g_string_append_printf(str, "  %s &lt;<a href=\"mailto:%s\">%s</a>&gt;<br/>",
					patch_writers[i].name,
					patch_writers[i].email, patch_writers[i].email);
		} else {
			g_string_append_printf(str, "  %s<br/>",
					patch_writers[i].name);
		}
	}
	g_string_append(str, "<BR/>");

	/* Retired Developers */
	g_string_append_printf(str, "<FONT SIZE=\"4\">%s:</FONT><BR/>",
						   _("Retired Developers"));
	for (i = 0; retired_developers[i].name != NULL; i++) {
		if (retired_developers[i].email != NULL) {
			g_string_append_printf(str, "  %s (%s) &lt;<a href=\"mailto:%s\">%s</a>&gt;<br/>",
					retired_developers[i].name, _(retired_developers[i].role),
					retired_developers[i].email, retired_developers[i].email);
		} else {
			g_string_append_printf(str, "  %s (%s)<br/>",
					retired_developers[i].name, _(retired_developers[i].role));
		}
	}
	g_string_append(str, "<BR/>");

	/* Current Translators */
	g_string_append_printf(str, "<FONT SIZE=\"4\">%s:</FONT><BR/>",
						   _("Current Translators"));
	for (i = 0; current_translators[i].language != NULL; i++) {
		if (current_translators[i].email != NULL) {
			g_string_append_printf(str, "  <b>%s (%s)</b> - %s &lt;<a href=\"mailto:%s\">%s</a>&gt;<br/>",
							_(current_translators[i].language),
							current_translators[i].abbr,
							_(current_translators[i].name),
							current_translators[i].email,
							current_translators[i].email);
		} else {
			g_string_append_printf(str, "  <b>%s (%s)</b> - %s<br/>",
							_(current_translators[i].language),
							current_translators[i].abbr,
							_(current_translators[i].name));
		}
	}
	g_string_append(str, "<BR/>");

	/* Past Translators */
	g_string_append_printf(str, "<FONT SIZE=\"4\">%s:</FONT><BR/>",
						   _("Past Translators"));
	for (i = 0; past_translators[i].language != NULL; i++) {
		if (past_translators[i].email != NULL) {
			g_string_append_printf(str, "  <b>%s (%s)</b> - %s &lt;<a href=\"mailto:%s\">%s</a>&gt;<br/>",
							_(past_translators[i].language),
							past_translators[i].abbr,
							_(past_translators[i].name),
							past_translators[i].email,
							past_translators[i].email);
		} else {
			g_string_append_printf(str, "  <b>%s (%s)</b> - %s<br/>",
							_(past_translators[i].language),
							past_translators[i].abbr,
							_(past_translators[i].name));
		}
	}
	g_string_append(str, "<BR/>");

	g_string_append_printf(str, "<FONT SIZE=\"4\">%s</FONT><br/>", _("Debugging Information"));

	/* The following primarly intented for user/developer interaction and thus
	   ought not be translated */

#ifdef CONFIG_ARGS /* win32 build doesn't use configure */
	g_string_append(str, "  <b>Arguments to <i>./configure</i>:</b>  " CONFIG_ARGS "<br/>");
#endif

#ifdef DEBUG
	g_string_append(str, "  <b>Print debugging messages:</b> Yes<br/>");
#else
	g_string_append(str, "  <b>Print debugging messages:</b> No<br/>");
#endif

#ifdef ENABLE_BINRELOC
	g_string_append(str, "  <b>Binary relocation:</b> Enabled<br/>");
#else
	g_string_append(str, "  <b>Binary relocation:</b> Disabled<br/>");
#endif

#ifdef GAIM_PLUGINS
	g_string_append(str, "  <b>Plugins:</b> Enabled<br/>");
#else
	g_string_append(str, "  <b>Plugins:</b> Disabled<br/>");
#endif

#ifdef HAVE_SSL
	g_string_append(str, "  <b>SSL:</b> Gaim was compiled with SSL support.<br/>");
#else
	g_string_append(str, "  <b>SSL:</b> Gaim was <b><i>NOT</i></b> compiled with any SSL support!<br/>");
#endif


g_string_append(str, "<br/>  <b>Library Support</b><br/>");

#ifdef HAVE_EVOLUTION_ADDRESSBOOK
	g_string_append_printf(str, "    <b>Evolution Addressbook:</b> Enabled<br/>");
#else
	g_string_append_printf(str, "    <b>Evolution Addressbook:</b> Disabled<br/>");
#endif

#ifdef USE_GTKSPELL
	g_string_append(str, "    <b>GtkSpell:</b> Enabled<br/>");
#else
	g_string_append(str, "    <b>GtkSpell:</b> Disabled<br/>");
#endif

#ifdef HAVE_GNUTLS
	g_string_append(str, "    <b>GnuTLS:</b> Enabled<br/>");
#else
	g_string_append(str, "    <b>GnuTLS:</b> Disabled<br/>");
#endif

#ifdef USE_AO
	g_string_append(str, "    <b>libao:</b> Enabled<br/>");
#else
	g_string_append(str, "    <b>libao:</b> Disabled<br/>");
#endif

#ifdef USE_NAS_AUDIO
	g_string_append(str, "    <b>Network Audio System (NAS):</b> Enabled<br/>");
#else
	g_string_append(str, "    <b>Network Audio System (NAS):</b> Disabled<br/>");
#endif

#ifdef HAVE_NSS
	g_string_append(str, "    <b>Network Security Services (NSS):</b> Enabled<br/>");
#else
	g_string_append(str, "    <b>Network Security Services (NSS):</b> Disabled<br/>");
#endif

#ifdef HAVE_STARTUP_NOTIFICATION
	g_string_append(str, "    <b>Startup Notification:</b> Enabled<br/>");
#else
	g_string_append(str, "    <b>Startup Notification:</b> Disabled<br/>");
#endif

#ifdef HAVE_TCL
	g_string_append(str, "    <b>Tcl:</b> Enabled<br/>");
#else
	g_string_append(str, "    <b>Tcl:</b> Disabled<br/>");
#endif

#ifdef HAVE_TK
	g_string_append(str, "    <b>Tk:</b> Enabled<br/>");
#else
	g_string_append(str, "    <b>Tk:</b> Disabled<br/>");
#endif

#ifdef USE_SM
	g_string_append(str, "    <b>X Session Management:</b> Enabled<br/>");
#else
	g_string_append(str, "    <b>X Session Management:</b> Disabled<br/>");
#endif

#ifdef USE_SCREENSAVER
	g_string_append(str, "    <b>XScreenSaver:</b> Enabled<br/>");
#else
	g_string_append(str, "    <b>XScreenSaver:</b> Disabled<br/>");
#endif

#ifdef LIBZEPHYR_EXT
	g_string_append(str, "    <b>Zephyr library (libzephyr):</b> External<br/>");
#else
	g_string_append(str, "    <b>Zephyr library (libzephyr):</b> Not External<br/>");
#endif

#ifdef ZEPHYR_USES_KERBEROS
	g_string_append(str, "    <b>Zephyr uses Kerberos:</b> Yes<br/>");
#else
	g_string_append(str, "    <b>Zephyr uses Kerberos:</b> No<br/>");
#endif

	/* End of not to be translated section */

	gtk_imhtml_append_text(GTK_IMHTML(text), str->str, GTK_IMHTML_NO_SCROLL);
	g_string_free(str, TRUE);

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

	field = gaim_request_field_string_new("screenname", _("_Name"), NULL, FALSE);
	gaim_request_field_set_type_hint(field, "screenname");
	gaim_request_field_set_required(field, TRUE);
	gaim_request_field_group_add_field(group, field);

	field = gaim_request_field_account_new("account", _("_Account"), NULL);
	gaim_request_field_set_type_hint(field, "account");
	gaim_request_field_set_visible(field,
		(gaim_connections_get_all() != NULL &&
		 gaim_connections_get_all()->next != NULL));
	gaim_request_field_set_required(field, TRUE);
	gaim_request_field_group_add_field(group, field);

	gaim_request_fields(gaim_get_blist(), _("New Instant Message"),
						NULL,
						_("Please enter the screen name or alias of the person "
						  "you would like to IM."),
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

	conv = gaim_find_conversation_with_account(GAIM_CONV_IM, username, account);

	if (conv == NULL)
		conv = gaim_conversation_new(GAIM_CONV_IM, account, username);

	win = gaim_conversation_get_window(conv);
	gtkwin = GAIM_GTK_WINDOW(win);

	gtk_window_present(GTK_WINDOW(gtkwin->window));
	gaim_conv_window_switch_conversation(win, conv);
}

static gboolean
gaim_gtkdialogs_ee(const char *ee)
{
	GtkWidget *window;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *img;
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
	img = gtk_image_new_from_stock(GAIM_STOCK_DIALOG_COOL, gtk_icon_size_from_name(GAIM_ICON_SIZE_DIALOG_COOL));
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

	field = gaim_request_field_string_new("screenname", _("_Name"), NULL, FALSE);
	gaim_request_field_set_type_hint(field, "screenname");
	gaim_request_field_set_required(field, TRUE);
	gaim_request_field_group_add_field(group, field);

	field = gaim_request_field_account_new("account", _("_Account"), NULL);
	gaim_request_field_set_type_hint(field, "account");
	gaim_request_field_set_visible(field,
		(gaim_connections_get_all() != NULL &&
		 gaim_connections_get_all()->next != NULL));
	gaim_request_field_set_required(field, TRUE);
	gaim_request_field_group_add_field(group, field);

	gaim_request_fields(gaim_get_blist(), _("Get User Info"),
						NULL,
						_("Please enter the screen name or alias of the person "
						  "whose info you would like to view."),
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

	if(username != NULL && *username != '\0' && account != NULL )
		gaim_gtk_log_show(GAIM_LOG_IM, username, account);

	g_free(username);
}

/*
 * TODO - This needs to deal with logs of all types, not just IM logs.
 */
void
gaim_gtkdialogs_log(void)
{
	GaimRequestFields *fields;
	GaimRequestFieldGroup *group;
	GaimRequestField *field;

	fields = gaim_request_fields_new();

	group = gaim_request_field_group_new(NULL);
	gaim_request_fields_add_group(fields, group);

	field = gaim_request_field_string_new("screenname", _("_Name"), NULL, FALSE);
	gaim_request_field_set_type_hint(field, "screenname");
	gaim_request_field_set_required(field, TRUE);
	gaim_request_field_group_add_field(group, field);

	field = gaim_request_field_account_new("account", _("_Account"), NULL);
	gaim_request_field_set_type_hint(field, "account");
	gaim_request_field_account_set_show_all(field, TRUE);
	gaim_request_field_set_visible(field,
		(gaim_accounts_get_all() != NULL &&
		 gaim_accounts_get_all()->next != NULL));
	gaim_request_field_set_required(field, TRUE);
	gaim_request_field_group_add_field(group, field);

	gaim_request_fields(gaim_get_blist(), _("Get User Log"),
						NULL,
						_("Please enter the screen name or alias of the person "
						  "whose log you would like to view."),
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
 * TODO - Make this use the request API, if possible.
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
		gchar *text;
		text = g_strdup_printf(
					ngettext(
						"You are about to remove the contact containing %s "
						"and %d other buddy from your buddy list.  Do you "
						"want to continue?",
						"You are about to remove the contact containing %s "
						"and %d other buddies from your buddy list.  Do you "
						"want to continue?", contact->totalsize - 1),
					buddy->name, contact->totalsize - 1);

		gaim_request_action(contact, NULL, _("Remove Contact"), text, 0, contact, 2,
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
					conv = gaim_find_conversation_with_account(GAIM_CONV_IM,
															   buddy->name,
															   buddy->account);
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

	gaim_request_action(group, NULL, _("Remove Group"), text, 0, group, 2,
						_("Remove Group"), G_CALLBACK(gaim_gtkdialogs_remove_group_cb),
						_("Cancel"), NULL);

	g_free(text);
}

/* XXX - Some of this should be moved into the core, methinks. */
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
	/* TODO - Should remove from blist first... then call serv_remove_buddy()? */
	serv_remove_buddy(buddy->account->gc, buddy, group);
	gaim_blist_remove_buddy(buddy);

	conv = gaim_find_conversation_with_account(GAIM_CONV_IM, name, account);
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

	gaim_request_action(buddy, NULL, _("Remove Buddy"), text, 0, buddy, 2,
						_("Remove Buddy"), G_CALLBACK(gaim_gtkdialogs_remove_buddy_cb),
						_("Cancel"), NULL);

	g_free(text);
}

static void
gaim_gtkdialogs_remove_chat_cb(GaimChat *chat)
{
	char *name = NULL;
	GaimAccount *account;
	GaimConversation *conv = NULL;

	account = chat->account;

	if (GAIM_PLUGIN_PROTOCOL_INFO(account->gc->prpl)->get_chat_name != NULL)
		name = GAIM_PLUGIN_PROTOCOL_INFO(account->gc->prpl)->get_chat_name(chat->components);

	gaim_blist_remove_chat(chat);

	if (name != NULL) {
		conv = gaim_find_conversation_with_account(GAIM_CONV_CHAT, name, account);
		g_free(name);
	}

	if (conv != NULL)
		gaim_conversation_update(conv, GAIM_CONV_UPDATE_REMOVE);
}

void
gaim_gtkdialogs_remove_chat(GaimChat *chat)
{
	const gchar *name = gaim_chat_get_name(chat);
	gchar *text = g_strdup_printf(_("You are about to remove the chat %s from your buddy list.  Do you want to continue?"), name);

	g_return_if_fail(chat != NULL);

	gaim_request_action(chat, NULL, _("Remove Chat"), text, 0, chat, 2,
						_("Remove Chat"), G_CALLBACK(gaim_gtkdialogs_remove_chat_cb),
						_("Cancel"), NULL);

	g_free(text);
}
