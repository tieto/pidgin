/*
 * @file gtkdialogs.c GTK+ Dialogs
 * @ingroup pidgin
 */

/* pidgin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */
#define _PIDGIN_GTKDIALOGS_C_

#include "internal.h"
#include "pidgin.h"
#include "package_revision.h"

#include "debug.h"
#include "notify.h"
#include "prpl.h"
#include "request.h"
#include "util.h"
#include "core.h"

#include "gtkblist.h"
#include "gtkdialogs.h"
#include "gtkimhtml.h"
#include "gtkimhtmltoolbar.h"
#include "gtklog.h"
#include "gtkutils.h"
#include "pidginstock.h"

static GList *dialogwindows = NULL;

struct _PidginGroupMergeObject {
	PurpleGroup* parent;
	char *new_name;
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

struct artist {
	char *name;
	char *email;
};

/* Order: Alphabetical by Last Name */
static const struct developer developers[] = {
	{"Daniel 'datallah' Atallah",          NULL,                  NULL},
	{"Paul 'darkrain42' Aurich",           NULL,                  NULL},
	{"John 'rekkanoryo' Bailey",           N_("bug master"),      NULL},
	{"Ethan 'Paco-Paco' Blanton",          NULL,                  NULL},
	{"Hylke Bons",                         N_("artist"),          "hylkebons@gmail.com"},
	/* feel free to not translate this */
	{N_("Ka-Hing Cheung"),                 NULL,                  NULL},
	{"Sadrul Habib Chowdhury",             NULL,                  NULL},
	{"Mark 'KingAnt' Doliner",             NULL,                  "mark@kingant.net"},
	{"Sean Egan",                          NULL,                  "sean.egan@gmail.com"},
	{"Casey Harkins",                      NULL,                  NULL},
	{"Gary 'grim' Kramlich",               NULL,                  "grim@pidgin.im"},
	{"Richard 'rlaager' Laager",           NULL,                  "rlaager@pidgin.im"},
	{"Sulabh 'sulabh_m' Mahajan",          NULL,                  NULL},
	{"Richard 'wabz' Nelson",              NULL,                  NULL},
	{"Etan 'deryni' Reisner",              NULL,                  NULL},
	{"Tim 'marv' Ringenbach",              NULL,                  NULL},
	{"Michael 'Maiku' Ruprecht",           N_("voice and video"), NULL},
	{"Elliott 'QuLogic' Sales de Andrade", NULL,                  NULL},
	{"Luke 'LSchiere' Schierer",           N_("support"),         "lschiere@users.sf.net"},
	{"Evan Schoenberg",                    NULL,                  NULL},
	{"Kevin 'SimGuy' Stange",              N_("webmaster"),       NULL},
	{"Will 'resiak' Thompson",             NULL,                  NULL},
	{"Stu 'nosnilmot' Tomlinson",          NULL,                  NULL},
	{NULL, NULL, NULL}
};

/* Order: Alphabetical by Last Name */
static const struct developer patch_writers[] = {
	{"Marcus 'malu' Lundblad",         NULL,                        NULL},
	{"Dennis 'EvilDennisR' Ristuccia", N_("Senior Contributor/QA"), NULL},
	{"Peter 'Fmoo' Ruibal",            NULL,                        NULL},
	{"Gabriel 'Nix' Schulhof",         NULL,                        NULL},
	{"Jorge 'Masca' Villaseñor",       NULL,                        NULL},
	{NULL, NULL, NULL}
};

/* Order: Alphabetical by Last Name */
static const struct developer retired_developers[] = {
	{"Herman Bloggs",               N_("win32 port"),          "herman@bluedigits.com"},
	{"Thomas Butter",               NULL,                      NULL},
	{"Jim Duchek",                  N_("maintainer"),          "jim@linuxpimps.com"},
	{"Rob Flynn",                   N_("maintainer"),          NULL},
	{"Adam Fritzler",               N_("libfaim maintainer"),  NULL},
	{"Christian 'ChipX86' Hammond", N_("webmaster"),           NULL},
	/* If "lazy bum" translates literally into a serious insult, use something else or omit it. */
	{"Syd Logan",                   N_("hacker and designated driver [lazy bum]"), NULL},
	{"Christopher 'siege' O'Brien", NULL,                      "taliesein@users.sf.net"},
	{"Bartosz Oler",                NULL,                      NULL},
	{"Megan 'Cae' Schneider",       N_("support/QA"),          NULL},
	{"Jim Seymour",                 N_("XMPP"),                NULL},
	{"Mark Spencer",                N_("original author"),     "markster@marko.net"},
	{"Nathan 'faceprint' Walp",     NULL,                      NULL},
	{"Eric Warmenhoven",            N_("lead developer"),      "warmenhoven@yahoo.com"},
	{NULL, NULL, NULL}
};

/* Order: Alphabetical by Last Name */
static const struct developer retired_patch_writers[] = {
	{"Felipe 'shx' Contreras",    NULL, NULL},
	{"Decklin Foster",            NULL, NULL},
	{"Peter 'Bleeter' Lawler",    NULL, NULL},
	{"Robert 'Robot101' McQueen", NULL, NULL},
	{"Benjamin Miller",           NULL, NULL},
	{NULL, NULL, NULL}
};

/* Order: Code, then Alphabetical by Last Name */
static const struct translator translators[] = {
	{N_("Afrikaans"),           "af", "Samuel Murray", "afrikaans@gmail.com"},
	{N_("Afrikaans"),           "af", "Friedel Wolff", "friedel@translate.org.za"},
	{N_("Arabic"),              "ar", "Khaled Hosny", "khaledhosny@eglug.org"},
	{N_("Belarusian Latin"),    "be@latin", "Ihar Hrachyshka", "ihar.hrachyshka@gmail.com"},
	{N_("Bulgarian"),           "bg", "Vladimira Girginova", "missing@here.is"},
	{N_("Bulgarian"),           "bg", "Vladimir (Kaladan) Petkov", "vpetkov@i-space.org"},
	{N_("Bengali"),             "bn", "Israt Jahan", "israt@ankur.org.bd"},
	{N_("Bengali"),             "bn", "Jamil Ahmed", "jamil@bengalinux.org"},
	{N_("Bengali"),             "bn", "Samia Nimatullah", "mailsamia2001@yahoo.com"},
	{N_("Bengali-India"),       "bn", "Runa Bhattacharjee", "runab@fedoraproject.org"},
	{N_("Bosnian"),             "bs", "Lejla Hadzialic", "lejlah@gmail.com"},
	{N_("Catalan"),             "ca", "Josep Puigdemont", "josep.puigdemont@gmail.com"},
	{N_("Valencian-Catalan"),   "ca@valencia", "Toni Hermoso", "toniher@softcatala.org"},
	{N_("Valencian-Catalan"),   "ca@valencia", "Josep Puigdemont", "tradgnome@softcatala.org"},
	{N_("Czech"),               "cs", "David Vachulka", "david@konstrukce-cad.com"},
	{N_("Danish"),              "da", "Morten Brix Pedersen", "morten@wtf.dk"},
	{N_("Danish"),              "da", "Peter Bach", "bach.peter@gmail.com"},
	{N_("German"),              "de", "Björn Voigt", "bjoern@cs.tu-berlin.de"},
	{N_("German"),              "de", "Jochen Kemnade", "jochenkemnade@web.de"},
	{N_("Dzongkha"),            "dz", "Norbu", "nor_den@hotmail.com"},
	{N_("Dzongkha"),            "dz", "Jurmey Rabgay", "jur_gay@yahoo.com"},
	{N_("Dzongkha"),            "dz", "Wangmo Sherpa", "rinwanshe@yahoo.com"},
	{N_("Greek"),               "el", "Katsaloulis Panayotis", "panayotis@panayotis.com"},
	{N_("Greek"),               "el", "Bouklis Panos", "panos@echidna-band.com"},
	{N_("Australian English"),  "en_AU", "Peter Lawler", "trans@six-by-nine.com.au"},
	{N_("Canadian English"),    "en_CA", "Adam Weinberger", "adamw@gnome.org"},
	{N_("British English"),     "en_GB", "Luke Ross", "lukeross@sys3175.co.uk"},
	{N_("Esperanto"),           "eo", "Stéphane Fillod", "fillods@users.sourceforge.net"},
	{N_("Spanish"),             "es", "Javier Fernández-Sanguino Peña", "jfs@debian.org"},
	{N_("Estonian"),            "et", "Ivar Smolin", "okul@linux.ee"},
	{N_("Basque"),              "eu", "Mikel Pascual Aldabaldetreku", "mikel.paskual@gmail.com"},
	{N_("Persian"),             "fa", "Elnaz Sarbar", "elnaz@farsiweb.info"},
	{N_("Persian"),             "fa", "Meelad Zakaria", "meelad@farsiweb.info"},
	{N_("Persian"),             "fa", "Roozbeh Pournader ", "roozbeh@farsiweb.info"},
	{N_("Finnish"),             "fi", "Timo Jyrinki", "timo.jyrinki@iki.fi"},
	{N_("French"),              "fr", "Éric Boumaour", "zongo_fr@users.sourceforge.net"},
	{N_("Irish"),               "ga", "Aaron Kearns", "ajkearns6@gmail.com"},
	{N_("Galician"),            "gl", "Mar Castro", "mariamarcp@gmail.com"},
	{N_("Galician"),            "gl", "Frco. Javier Rial", "fjrial@cesga.es"},
	{N_("Gujarati"),            "gu", "Ankit Patel", "ankit_patel@users.sf.net"},
	{N_("Gujarati"),            "gu", N_("Gujarati Language Team"), "indianoss-gujarati@lists.sourceforge.net"},
	{N_("Hebrew"),              "he", "Shalom Craimer", "scraimer@gmail.com"},
	{N_("Hindi"),               "hi", "Rajesh Ranjan", "rajeshkajha@yahoo.com"},
	{N_("Hungarian"),           "hu", "Kelemen Gábor", "kelemeng@gnome.hu"},
	{N_("Armenian"),            "hy", "David Avsharyan", "avsharyan@gmail.com"},
	{N_("Indonesian"),          "id", "Rai S. Regawa", "raireg@yahoo.com"},
	{N_("Italian"),             "it", "Claudio Satriano", "satriano@na.infn.it"},
	{N_("Japanese"),            "ja", "Takashi Aihana", "aihana@gnome.gr.jp"},
	{N_("Georgian"),            "ka", N_("Ubuntu Georgian Translators"), "alexander.didebulidze@stusta.mhn.de"},
	{N_("Khmer"),               "km", "Khoem Sokhem", "khoemsokhem@khmeros.info"},
	{N_("Kannada"),             "kn", N_("Kannada Translation team"), "translation@sampada.info"},
	{N_("Korean"),              "ko", "Sushizang", "sushizang@empal.com"},
	{N_("Kurdish"),             "ku", "Erdal Ronahi", "erdal.ronahi@gmail.com"},
	{N_("Kurdish"),             "ku", "Amed Ç. Jiyan", "amedcj@hotmail.com"},
	{N_("Kurdish"),             "ku", "Rizoyê Xerzî", "rizoxerzi@hotmail.com"},
	{N_("Lao"),                 "lo", "Anousak Souphavah", "anousak@gmail.com"},
	{N_("Macedonian"),          "mk", "Arangel Angov ", "arangel@linux.net.mk"},
	{N_("Macedonian"),          "mk", "Ivana Kirkovska", "ivana.kirkovska@gmail.com"},
	{N_("Macedonian"),          "mk", "Jovan Naumovski", "jovan@lugola.net"},
	{N_("Mongolian"),           "mn", "gooyo", NULL},
	{N_("Marathi"),             "mr", "Sandeep Shedmake", "sandeep.shedmake@gmail.com"},
	{N_("Malay"),               "ms_MY", "Muhammad Najmi bin Ahmad Zabidi", "najmi.zabidi@gmail.com"},
	{N_("Bokmål Norwegian"),    "nb", "Hans Fredrik Nordhaug", "hans@nordhaug.priv.no"},
	{N_("Nepali"),              "ne", "Shyam Krishna Bal", "shyamkrishna_bal@yahoo.com"},
	{N_("Dutch, Flemish"),      "nl", "Vincent van Adrighem", "V.vanAdrighem@dirck.mine.nu"},
	{N_("Norwegian Nynorsk"),   "nn", "Yngve Spjeld Landro", "nynorsk@strilen.net"},
	{N_("Occitan"),             "oc", "Yannig Marchegay", "yannig@marchegay.org"},
	{N_("Oriya"),               "or", "Manoj Kumar Giri", "giri.manojkr@gmail.com"},
	{N_("Punjabi"),             "pa", "Amanpreet Singh Alam", "aalam@users.sf.net"},
	{N_("Polish"),              "pl", "Piotr Drąg", "piotrdrag@gmail.com"},
	{N_("Polish"),              "pl", "Piotr Makowski", "pmakowski@aviary.pl"},
	{N_("Portuguese"),          "pt", "Duarte Henriques", "duarte_henriques@myrealbox.com"},
	{N_("Portuguese-Brazil"),   "pt_BR", "Rodrigo Luiz Marques Flores", "rodrigomarquesflores@gmail.com"},
	{N_("Pashto"),              "ps", "Kashif Masood", "masudmails@yahoo.com"},
	{N_("Romanian"),            "ro", "Mişu Moldovan", "dumol@gnome.ro"},
	{N_("Romanian"),            "ro", "Andrei Popescu", "andreimpopescu@gmail.com"},
	{N_("Russian"),             "ru", "Антон Самохвалов", "samant.ua@mail.ru"},
	{N_("Slovak"),              "sk", "Jozef Káčer", "quickparser@gmail.com"},
	{N_("Slovak"),              "sk", "loptosko", "loptosko@gmail.com"},
	{N_("Slovenian"),           "sl", "Martin Srebotnjak", "miles@filmsi.net"},
	{N_("Albanian"),            "sq", "Besnik Bleta", "besnik@programeshqip.org"},
	{N_("Serbian"),             "sr", "Miloš Popović", "gpopac@gmail.com"},
	{N_("Serbian"),             "sr@Latn", "Miloš Popović", "gpopac@gmail.com"},
	{N_("Sinhala"),             "si", "Danishka Navin", "snavin@redhat.com"},
	{N_("Sinhala"),             "si", "Yajith Ajantha Dayarathna", "yajith@gmail.com"},
	{N_("Swedish"),             "sv", "Peter Hjalmarsson", "xake@telia.com"},
	{N_("Swahili"),             "sw", "Paul Msegeya", "msegeya@gmail.com"},
	{N_("Tamil"),               "ta", "I. Felix", "ifelix25@gmail.com"},
	{N_("Tamil"),               "ta", "Viveka Nathan K", "vivekanathan@users.sourceforge.net"},
	{N_("Telugu"),              "te", "Mr. Subbaramaih", "info.gist@cdac.in"},
	{N_("Thai"),                "th", "Isriya Paireepairit", "markpeak@gmail.com"},
	{N_("Turkish"),             "tr", "Serdar Soytetir", "tulliana@gmail.com"},
	{N_("Ukranian"),            "uk", "Oleksandr Kovalenko", "alx.kovalenko@gmail.com"},
	{N_("Urdu"),                "ur", "RKVS Raman", "rkvsraman@gmail.com"},
	{N_("Vietnamese"),          "vi", N_("T.M.Thanh and the Gnome-Vi Team"), "gnomevi-list@lists.sf.net"},
	{N_("Simplified Chinese"),  "zh_CN", "Aron Xu", "happyaron.xu@gmail.com"},
	{N_("Hong Kong Chinese"),   "zh_HK", "Abel Cheung", "abelindsay@gmail.com"},
	{N_("Hong Kong Chinese"),   "zh_HK", "Ambrose C. Li", "acli@ada.dhs.org"},
	{N_("Hong Kong Chinese"),   "zh_HK", "Paladin R. Liu", "paladin@ms1.hinet.net"},
	{N_("Traditional Chinese"), "zh_TW", "Ambrose C. Li", "acli@ada.dhs.org"},
	{N_("Traditional Chinese"), "zh_TW", "Paladin R. Liu", "paladin@ms1.hinet.net"},
	{NULL, NULL, NULL, NULL}
};


static const struct translator past_translators[] = {
	{N_("Amharic"),             "am", "Daniel Yacob", NULL},
	{N_("Arabic"),              "ar", "Mohamed Magdy", "alnokta@yahoo.com"},
	{N_("Bulgarian"),           "bg", "Hristo Todorov", NULL},
	{N_("Bengali"),             "bn", "INDRANIL DAS GUPTA", "indradg@l2c2.org"},
	{N_("Bengali"),             "bn", "Tisa Nafisa", "tisa_nafisa@yahoo.com"},
	{N_("Catalan"),             "ca", "JM Pérez Cáncer", NULL},
	{N_("Catalan"),             "ca", "Robert Millan", NULL},
	{N_("Czech"),               "cs", "Honza Král", NULL},
	{N_("Czech"),               "cs", "Miloslav Trmac", "mitr@volny.cz"},
	{N_("German"),              "de", "Daniel Seifert, Karsten Weiss", NULL},
	{N_("Spanish"),             "es", "JM Pérez Cáncer", NULL},
	{N_("Spanish"),             "es", "Nicolás Lichtmaier", NULL},
	{N_("Spanish"),             "es", "Amaya Rodrigo", NULL},
	{N_("Spanish"),             "es", "Alejandro G Villar", NULL},
	{N_("Basque"),              "eu", "Iñaki Larrañaga Murgoitio", "dooteo@zundan.com"},
	{N_("Basque"),              "eu", "Hizkuntza Politikarako Sailburuordetza", "hizkpol@ej-gv.es"},
	{N_("Finnish"),             "fi", "Arto Alakulju", NULL},
	{N_("Finnish"),             "fi", "Tero Kuusela", NULL},
	{N_("French"),              "fr", "Sébastien François", NULL},
	{N_("French"),              "fr", "Stéphane Pontier", NULL},
	{N_("French"),              "fr", "Stéphane Wirtel", NULL},
	{N_("French"),              "fr", "Loïc Jeannin", NULL},
	{N_("Galician"),            "gl", "Ignacio Casal Quinteiro", NULL},
	{N_("Hebrew"),              "he", "Pavel Bibergal", NULL},
	{N_("Hindi"),               "hi", "Ravishankar Shrivastava", NULL},
	{N_("Hungarian"),           "hu", "Zoltan Sutto", NULL},
	{N_("Italian"),             "it", "Salvatore di Maggio", NULL},
	{N_("Japanese"),            "ja", "Ryosuke Kutsuna", NULL},
	{N_("Japanese"),            "ja", "Taku Yasui", NULL},
	{N_("Japanese"),            "ja", "Junichi Uekawa", NULL},
	{N_("Georgian"),            "ka", "Temuri Doghonadze", NULL},
	{N_("Korean"),              "ko", "Sang-hyun S, A Ho-seok Lee", NULL},
	{N_("Korean"),              "ko", "Kyeong-uk Son", NULL},
	{N_("Lithuanian"),          "lt", "Laurynas Biveinis", "laurynas.biveinis@gmail.com"},
	{N_("Lithuanian"),          "lt", "Gediminas Čičinskas", NULL},
	{N_("Lithuanian"),          "lt", "Andrius Štikonas", NULL},
	{N_("Macedonian"),          "mk", "Tomislav Markovski", NULL},
	{N_("Bokmål Norwegian"),    "nb", "Hallvard Glad", "hallvard.glad@gmail.com"},
	{N_("Bokmål Norwegian"),    "nb", "Petter Johan Olsen", NULL},
	{N_("Bokmål Norwegian"),    "nb", "Espen Stefansen", "espenas@gmail.com"},
	{N_("Polish"),              "pl", "Emil Nowak", "emil5@go2.pl"},
	{N_("Polish"),              "pl", "Paweł Godlewski", "pawel@bajk.pl"},
	{N_("Polish"),              "pl", "Krzysztof Foltman", "krzysztof@foltman.com"},
	{N_("Polish"),              "pl", "Przemysław Sułek", NULL},
	{N_("Portuguese-Brazil"),   "pt_BR", "Maurício de Lemos Rodrigues Collares Neto", "mauricioc@gmail.com"},
	{N_("Russian"),             "ru", "Dmitry Beloglazov", "dmaa@users.sf.net"},
	{N_("Russian"),             "ru", "Alexandre Prokoudine", NULL},
	{N_("Russian"),             "ru", "Sergey Volozhanin", NULL},
	{N_("Slovak"),              "sk", "Daniel Režný", NULL},
	{N_("Slovak"),              "sk", "helix84", NULL},
	{N_("Slovak"),              "sk", "Richard Golier", NULL},
	{N_("Slovenian"),           "sl", "Matjaz Horvat", NULL},
	{N_("Serbian"),             "sr", "Danilo Šegan", "dsegan@gmx.net"},
	{N_("Serbian"),             "sr", "Aleksandar Urosevic", "urke@users.sourceforge.net"},
	{N_("Swedish"),             "sv", "Tore Lundqvist", NULL},
	{N_("Swedish"),             "sv", "Christian Rose", NULL},
	{N_("Turkish"),             "tr", "Ahmet Alp BALKAN", NULL},
	{N_("Simplified Chinese"),  "zh_CN", "Hashao, Rocky S. Lee", NULL},
	{N_("Simplified Chinese"),  "zh_CN", "Funda Wang", "fundawang@linux.net.cn"},
	{N_("Traditional Chinese"), "zh_TW", "Hashao, Rocky S. Lee", NULL},
	{NULL, NULL, NULL, NULL}
};

static void
add_developers(GString *str, const struct developer *list)
{
	for (; list->name != NULL; list++) {
		if (list->email != NULL) {
			g_string_append_printf(str, "  <a href=\"mailto:%s\">%s</a>%s%s%s<br/>",
			                       list->email, _(list->name),
			                       list->role ? " (" : "",
			                       list->role ? _(list->role) : "",
			                       list->role ? ")" : "");
		} else {
			g_string_append_printf(str, "  %s%s%s%s<br/>",
			                       _(list->name),
			                       list->role ? " (" : "",
			                       list->role ? _(list->role) : "",
			                       list->role ? ")" : "");
		}
	}
}

static void
add_translators(GString *str, const struct translator *list)
{
	for (; list->language != NULL; list++) {
		if (list->email != NULL) {
			g_string_append_printf(str, "  <b>%s (%s)</b> - <a href=\"mailto:%s\">%s</a><br/>",
			                       _(list->language),
			                       list->abbr,
			                       list->email,
			                       _(list->name));
		} else {
			g_string_append_printf(str, "  <b>%s (%s)</b> - %s<br/>",
			                       _(list->language),
			                       list->abbr,
			                       _(list->name));
		}
	}
}

void
pidgin_dialogs_destroy_all()
{
	while (dialogwindows) {
		gtk_widget_destroy(dialogwindows->data);
		dialogwindows = g_list_remove(dialogwindows, dialogwindows->data);
	}
}

static void destroy_win(GtkWidget *button, GtkWidget *win)
{
	gtk_widget_destroy(win);
}

#if 0
/* This function puts the version number onto the pixmap we use in the 'about'
 * screen in Pidgin. */
static void
pidgin_logo_versionize(GdkPixbuf **original, GtkWidget *widget) {
	GdkPixmap *pixmap;
	GtkStyle *style;
	PangoContext *context;
	PangoLayout *layout;
	gchar *markup;
	gint width, height;
	gint lwidth = 0, lheight = 0;

	style = gtk_widget_get_style(widget);

	gdk_pixbuf_render_pixmap_and_mask(*original, &pixmap, NULL, 255);
	width = gdk_pixbuf_get_width(*original);
	height = gdk_pixbuf_get_height(*original);
	g_object_unref(G_OBJECT(*original));

	context = gtk_widget_get_pango_context(widget);
	layout = pango_layout_new(context);

	markup = g_strdup_printf("<span foreground=\"#000000\">%s</span>", DISPLAY_VERSION);
	pango_layout_set_font_description(layout, style->font_desc);
	pango_layout_set_markup(layout, markup, strlen(markup));
	g_free(markup);

	pango_layout_get_pixel_size(layout, &lwidth, &lheight);
	gdk_draw_layout(GDK_DRAWABLE(pixmap), style->bg_gc[GTK_STATE_NORMAL],
					width - (lwidth + 3), 1, layout);
	g_object_unref(G_OBJECT(layout));

	*original = gdk_pixbuf_get_from_drawable(NULL, pixmap, NULL,
											 0, 0, 0, 0,
											 width, height);
	g_object_unref(G_OBJECT(pixmap));
}
#endif

/* Note: Frees 'string' */
static GtkWidget *
pidgin_build_help_dialog(const char *title, const char *role, GString *string)
{
	GtkWidget *win, *vbox, *frame, *logo, *imhtml, *button;
	GdkPixbuf *pixbuf;
	GtkTextIter iter;
	AtkObject *obj;
	char *filename, *tmp;

	win = pidgin_create_dialog(title, PIDGIN_HIG_BORDER, role, TRUE);
	vbox = pidgin_dialog_get_vbox_with_properties(GTK_DIALOG(win), FALSE, PIDGIN_HIG_BORDER);
	gtk_window_set_default_size(GTK_WINDOW(win), 450, 450);

	/* Generate a logo with a version number */
	filename = g_build_filename(DATADIR, "pixmaps", "pidgin", "logo.png", NULL);
	pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
	g_free(filename);

#if 0  /* Don't versionize the logo when the logo has the version in it */
	pidgin_logo_versionize(&pixbuf, logo);
#endif

	/* Insert the logo */
	logo = gtk_image_new_from_pixbuf(pixbuf);
	g_object_unref(G_OBJECT(pixbuf));
	obj = gtk_widget_get_accessible(logo);
	tmp = g_strconcat(PIDGIN_NAME, " " DISPLAY_VERSION, NULL);
	atk_object_set_description(obj, tmp);
	g_free(tmp);
	gtk_box_pack_start(GTK_BOX(vbox), logo, FALSE, FALSE, 0);

	frame = pidgin_create_imhtml(FALSE, &imhtml, NULL, NULL);
	gtk_imhtml_set_format_functions(GTK_IMHTML(imhtml), GTK_IMHTML_ALL ^ GTK_IMHTML_SMILEY);
	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);

	gtk_imhtml_append_text(GTK_IMHTML(imhtml), string->str, GTK_IMHTML_NO_SCROLL);
	gtk_text_buffer_get_start_iter(gtk_text_view_get_buffer(GTK_TEXT_VIEW(imhtml)), &iter);
	gtk_text_buffer_place_cursor(gtk_text_view_get_buffer(GTK_TEXT_VIEW(imhtml)), &iter);

	button = pidgin_dialog_add_button(GTK_DIALOG(win), GTK_STOCK_CLOSE,
	                G_CALLBACK(destroy_win), win);

	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_widget_grab_default(button);

	gtk_widget_show_all(win);
	gtk_window_present(GTK_WINDOW(win));

	g_string_free(string, TRUE);

	return win;
}

void pidgin_dialogs_about(void)
{
	GString *str;
	char *tmp;
	static GtkWidget *about = NULL;

	if (about != NULL) {
		gtk_window_present(GTK_WINDOW(about));
		return;
	}

	str = g_string_sized_new(4096);

	g_string_append_printf(str,
		"<CENTER><FONT SIZE=\"4\"><B>%s %s</B></FONT></CENTER> (libpurple %s)"
		"<BR>%s<BR><BR>", PIDGIN_NAME, DISPLAY_VERSION,
		purple_core_get_version(), REVISION);

	g_string_append_printf(str,
		_("%s is a messaging client based on libpurple which is capable of "
		  "connecting to multiple messaging services at once.  %s is written "
		  "in C using GTK+.  %s is released, and may be modified and "
		  "redistributed,  under the terms of the GPL version 2 (or later).  "
		  "A copy of the GPL is distributed with %s.  %s is copyrighted by "
		  "its contributors, a list of whom is also distributed with %s.  "
		  "There is no warranty for %s.<BR><BR>"), PIDGIN_NAME, PIDGIN_NAME,
		PIDGIN_NAME, PIDGIN_NAME, PIDGIN_NAME, PIDGIN_NAME, PIDGIN_NAME);

	g_string_append_printf(str,
			_("<FONT SIZE=\"4\"><B>Helpful Resources</B></FONT><BR>\t<A "
			  "HREF=\"%s\">Website</A><BR>\t<A HREF=\"%s\">Frequently Asked "
			  "Questions</A><BR>\tIRC Channel: #pidgin on irc.freenode.net<BR>"
			  "\tXMPP MUC: devel@conference.pidgin.im<BR><BR>"), PURPLE_WEBSITE,
			"http://developer.pidgin.im/wiki/FAQ");

	g_string_append_printf(str,
			_("<font size=\"4\"><b>Help from other Pidgin users</b></font> is "
			  "available by e-mailing <a "
			  "href=\"mailto:support@pidgin.im\">support@pidgin.im</a><br/>"
			  "This is a <b>public</b> mailing list! "
			  "(<a href=\"http://pidgin.im/pipermail/support/\">archive</a>)<br/>"
			  "We can't help with third-party protocols or plugins!<br/>"
			  "This list's primary language is <b>English</b>.  You are "
			  "welcome to post in another language, but the responses may "
			  "be less helpful.<br/>"));

	tmp = g_strdup_printf(_("About %s"), PIDGIN_NAME);
	about = pidgin_build_help_dialog(tmp, "about", str);
	g_signal_connect(G_OBJECT(about), "destroy", G_CALLBACK(gtk_widget_destroyed), &about);
	g_free(tmp);
}

void pidgin_dialogs_buildinfo(void)
{
	GString *str;
	char *tmp;
	static GtkWidget *buildinfo = NULL;

	if (buildinfo != NULL) {
		gtk_window_present(GTK_WINDOW(buildinfo));
		return;
	}

	str = g_string_sized_new(4096);

	g_string_append_printf(str,
		"<FONT SIZE=\"4\"><B>%s %s</B></FONT> (libpurple %s)<BR>%s<BR><BR>", PIDGIN_NAME, DISPLAY_VERSION, purple_core_get_version(), REVISION);

	g_string_append_printf(str, "<FONT SIZE=\"4\"><B>%s</B></FONT><br/>", _("Build Information"));

	/* The following primarly intented for user/developer interaction and thus
	   ought not be translated */

#ifdef CONFIG_ARGS /* win32 build doesn't use configure */
	g_string_append(str, "  <b>Arguments to <i>./configure</i>:</b>  " CONFIG_ARGS "<br/>");
#endif

#ifndef _WIN32
#ifdef DEBUG
	g_string_append(str, "  <b>Print debugging messages:</b> Yes<br/>");
#else
	g_string_append(str, "  <b>Print debugging messages:</b> No<br/>");
#endif
#endif

#ifdef PURPLE_PLUGINS
	g_string_append(str, "  <b>Plugins:</b> Enabled<br/>");
#else
	g_string_append(str, "  <b>Plugins:</b> Disabled<br/>");
#endif

#ifdef HAVE_SSL
	g_string_append(str, "  <b>SSL:</b> SSL support is present.<br/>");
#else
	g_string_append(str, "  <b>SSL:</b> SSL support was <b><i>NOT</i></b> compiled!<br/>");
#endif

/* This might be useful elsewhere too, but it is particularly useful for
 * debugging stuff known to be GTK+/Glib bugs on Windows */
#ifdef _WIN32
	g_string_append_printf(str, "  <b>GTK+ Runtime:</b> %u.%u.%u<br/>"
		"  <b>Glib Runtime:</b> %u.%u.%u<br/>",
		gtk_major_version, gtk_minor_version, gtk_micro_version,
		glib_major_version, glib_minor_version, glib_micro_version);
#endif

g_string_append(str, "<br/>  <b>Library Support</b><br/>");

#ifdef HAVE_CYRUS_SASL
	g_string_append_printf(str, "    <b>Cyrus SASL:</b> Enabled<br/>");
#else
	g_string_append_printf(str, "    <b>Cyrus SASL:</b> Disabled<br/>");
#endif

#ifndef _WIN32
#ifdef HAVE_DBUS
	g_string_append_printf(str, "    <b>D-Bus:</b> Enabled<br/>");
#else
	g_string_append_printf(str, "    <b>D-Bus:</b> Disabled<br/>");
#endif

#ifdef HAVE_EVOLUTION_ADDRESSBOOK
	g_string_append_printf(str, "    <b>Evolution Addressbook:</b> Enabled<br/>");
#else
	g_string_append_printf(str, "    <b>Evolution Addressbook:</b> Disabled<br/>");
#endif
#endif

#if defined(_WIN32) || defined(USE_INTERNAL_LIBGADU)
	g_string_append(str, "    <b>Gadu-Gadu library (libgadu):</b> Internal<br/>");
#else
#ifdef HAVE_LIBGADU
	g_string_append(str, "    <b>Gadu-Gadu library (libgadu):</b> Enabled<br/>");
#else
	g_string_append(str, "    <b>Gadu-Gadu library (libgadu):</b> Disabled<br/>");
#endif
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

#ifndef _WIN32
#ifdef USE_GSTREAMER
	g_string_append(str, "    <b>GStreamer:</b> Enabled<br/>");
#else
	g_string_append(str, "    <b>GStreamer:</b> Disabled<br/>");
#endif
#endif

#ifndef _WIN32
#ifdef ENABLE_MONO
	g_string_append(str, "    <b>Mono:</b> Enabled<br/>");
#else
	g_string_append(str, "    <b>Mono:</b> Disabled<br/>");
#endif
#endif

#ifndef _WIN32
#ifdef HAVE_NETWORKMANAGER
	g_string_append(str, "    <b>NetworkManager:</b> Enabled<br/>");
#else
	g_string_append(str, "    <b>NetworkManager:</b> Disabled<br/>");
#endif
#endif

#ifdef HAVE_NSS
	g_string_append(str, "    <b>Network Security Services (NSS):</b> Enabled<br/>");
#else
	g_string_append(str, "    <b>Network Security Services (NSS):</b> Disabled<br/>");
#endif

if (purple_plugins_find_with_id("core-perl") != NULL)
	g_string_append(str, "    <b>Perl:</b> Enabled<br/>");
else
	g_string_append(str, "    <b>Perl:</b> Disabled<br/>");

#ifndef _WIN32
#ifdef HAVE_STARTUP_NOTIFICATION
	g_string_append(str, "    <b>Startup Notification:</b> Enabled<br/>");
#else
	g_string_append(str, "    <b>Startup Notification:</b> Disabled<br/>");
#endif
#endif

if (purple_plugins_find_with_id("core-tcl") != NULL) {
	g_string_append(str, "    <b>Tcl:</b> Enabled<br/>");
#ifdef HAVE_TK
	g_string_append(str, "    <b>Tk:</b> Enabled<br/>");
#else
	g_string_append(str, "    <b>Tk:</b> Disabled<br/>");
#endif
} else {
	g_string_append(str, "    <b>Tcl:</b> Disabled<br/>");
	g_string_append(str, "    <b>Tk:</b> Disabled<br/>");
}

#ifdef USE_IDN
	g_string_append(str, "    <b>UTF-8 DNS (IDN):</b> Enabled<br/>");
#else
	g_string_append(str, "    <b>UTF-8 DNS (IDN):</b> Disabled<br/>");
#endif

#ifdef USE_VV
	g_string_append(str, "    <b>Voice and Video:</b> Enabled<br/>");
#else
	g_string_append(str, "    <b>Voice and Video:</b> Disabled<br/>");
#endif

#ifndef _WIN32
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
	g_string_append(str, "    <b>Zephyr library (libzephyr):</b> Internal<br/>");
#endif

#ifdef ZEPHYR_USES_KERBEROS
	g_string_append(str, "    <b>Zephyr uses Kerberos:</b> Yes<br/>");
#else
	g_string_append(str, "    <b>Zephyr uses Kerberos:</b> No<br/>");
#endif
#endif

	/* End of not to be translated section */

	tmp = g_strdup_printf(_("%s Build Information"), PIDGIN_NAME);
	buildinfo = pidgin_build_help_dialog(tmp, "buildinfo", str);
	g_signal_connect(G_OBJECT(buildinfo), "destroy", G_CALLBACK(gtk_widget_destroyed), &buildinfo);
	g_free(tmp);
}

void pidgin_dialogs_developers(void)
{
	GString *str;
	char *tmp;
	static GtkWidget *developer_info = NULL;

	if (developer_info != NULL) {
		gtk_window_present(GTK_WINDOW(developer_info));
		return;
	}

	str = g_string_sized_new(4096);

	/* Current Developers */
	g_string_append_printf(str, "<FONT SIZE=\"4\"><B>%s:</B></FONT><BR/>",
						   _("Current Developers"));
	add_developers(str, developers);
	g_string_append(str, "<BR/>");

	/* Crazy Patch Writers */
	g_string_append_printf(str, "<FONT SIZE=\"4\"><B>%s:</B></FONT><BR/>",
						   _("Crazy Patch Writers"));
	add_developers(str, patch_writers);
	g_string_append(str, "<BR/>");

	/* Retired Developers */
	g_string_append_printf(str, "<FONT SIZE=\"4\"><B>%s:</B></FONT><BR/>",
						   _("Retired Developers"));
	add_developers(str, retired_developers);
	g_string_append(str, "<BR/>");

	/* Retired Crazy Patch Writers */
	g_string_append_printf(str, "<FONT SIZE=\"4\"><B>%s:</B></FONT><BR/>",
						   _("Retired Crazy Patch Writers"));
	add_developers(str, retired_patch_writers);

	tmp = g_strdup_printf(_("%s Developer Information"), PIDGIN_NAME);
	developer_info = pidgin_build_help_dialog(tmp, "developer_info", str);
	g_signal_connect(G_OBJECT(developer_info), "destroy", G_CALLBACK(gtk_widget_destroyed), &developer_info);
	g_free(tmp);
}

void pidgin_dialogs_translators(void)
{
	GString *str;
	char *tmp;
	static GtkWidget *translator_info = NULL;

	if (translator_info != NULL) {
		gtk_window_present(GTK_WINDOW(translator_info));
		return;
	}

	str = g_string_sized_new(4096);

	/* Current Translators */
	g_string_append_printf(str, "<FONT SIZE=\"4\">%s:</FONT><BR/>",
						   _("Current Translators"));
	add_translators(str, translators);
	g_string_append(str, "<BR/>");

	/* Past Translators */
	g_string_append_printf(str, "<FONT SIZE=\"4\">%s:</FONT><BR/>",
						   _("Past Translators"));
	add_translators(str, past_translators);

	tmp = g_strdup_printf(_("%s Translator Information"), PIDGIN_NAME);
	translator_info = pidgin_build_help_dialog(tmp, "translator_info", str);
	g_signal_connect(G_OBJECT(translator_info), "destroy", G_CALLBACK(gtk_widget_destroyed), &translator_info);
	g_free(tmp);
}

static void
pidgin_dialogs_im_cb(gpointer data, PurpleRequestFields *fields)
{
	PurpleAccount *account;
	const char *username;

	account  = purple_request_fields_get_account(fields, "account");
	username = purple_request_fields_get_string(fields,  "screenname");

	pidgin_dialogs_im_with_user(account, username);
}

void
pidgin_dialogs_im(void)
{
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;

	fields = purple_request_fields_new();

	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);

	field = purple_request_field_string_new("screenname", _("_Name"), NULL, FALSE);
	purple_request_field_set_type_hint(field, "screenname");
	purple_request_field_set_required(field, TRUE);
	purple_request_field_group_add_field(group, field);

	field = purple_request_field_account_new("account", _("_Account"), NULL);
	purple_request_field_set_type_hint(field, "account");
	purple_request_field_set_visible(field,
		(purple_connections_get_all() != NULL &&
		 purple_connections_get_all()->next != NULL));
	purple_request_field_set_required(field, TRUE);
	purple_request_field_group_add_field(group, field);

	purple_request_fields(purple_get_blist(), _("New Instant Message"),
						NULL,
						_("Please enter the username or alias of the person "
						  "you would like to IM."),
						fields,
						_("OK"), G_CALLBACK(pidgin_dialogs_im_cb),
						_("Cancel"), NULL,
						NULL, NULL, NULL,
						NULL);
}

void
pidgin_dialogs_im_with_user(PurpleAccount *account, const char *username)
{
	PurpleConversation *conv;

	g_return_if_fail(account != NULL);
	g_return_if_fail(username != NULL);

	conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, username, account);

	if (conv == NULL)
		conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, account, username);

	pidgin_conv_attach_to_conversation(conv);
	purple_conversation_present(conv);
}

static gboolean
pidgin_dialogs_ee(const char *ee)
{
	GtkWidget *window;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *img;
	gchar *norm = purple_strreplace(ee, "rocksmyworld", "");

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

	window = gtk_dialog_new_with_buttons(PIDGIN_ALERT_TITLE, NULL, 0, GTK_STOCK_CLOSE, GTK_RESPONSE_OK, NULL);
	gtk_dialog_set_default_response (GTK_DIALOG(window), GTK_RESPONSE_OK);
	g_signal_connect(G_OBJECT(window), "response", G_CALLBACK(gtk_widget_destroy), NULL);

	gtk_container_set_border_width (GTK_CONTAINER(window), PIDGIN_HIG_BOX_SPACE);
	gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
	gtk_dialog_set_has_separator(GTK_DIALOG(window), FALSE);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(window)->vbox), PIDGIN_HIG_BORDER);
	gtk_container_set_border_width (GTK_CONTAINER(GTK_DIALOG(window)->vbox), PIDGIN_HIG_BOX_SPACE);

	hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BORDER);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(window)->vbox), hbox);
	img = gtk_image_new_from_stock(PIDGIN_STOCK_DIALOG_COOL, gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_HUGE));
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);

	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	gtk_widget_show_all(window);
	return TRUE;
}

static void
pidgin_dialogs_info_cb(gpointer data, PurpleRequestFields *fields)
{
	char *username;
	gboolean found = FALSE;
	PurpleAccount *account;

	account  = purple_request_fields_get_account(fields, "account");

	username = g_strdup(purple_normalize(account,
		purple_request_fields_get_string(fields,  "screenname")));

	if (username != NULL && purple_str_has_suffix(username, "rocksmyworld"))
		found = pidgin_dialogs_ee(username);

	if (!found && username != NULL && *username != '\0' && account != NULL)
		pidgin_retrieve_user_info(purple_account_get_connection(account), username);

	g_free(username);
}

void
pidgin_dialogs_info(void)
{
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;

	fields = purple_request_fields_new();

	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);

	field = purple_request_field_string_new("screenname", _("_Name"), NULL, FALSE);
	purple_request_field_set_type_hint(field, "screenname");
	purple_request_field_set_required(field, TRUE);
	purple_request_field_group_add_field(group, field);

	field = purple_request_field_account_new("account", _("_Account"), NULL);
	purple_request_field_set_type_hint(field, "account");
	purple_request_field_set_visible(field,
		(purple_connections_get_all() != NULL &&
		 purple_connections_get_all()->next != NULL));
	purple_request_field_set_required(field, TRUE);
	purple_request_field_group_add_field(group, field);

	purple_request_fields(purple_get_blist(), _("Get User Info"),
						NULL,
						_("Please enter the username or alias of the person "
						  "whose info you would like to view."),
						fields,
						_("OK"), G_CALLBACK(pidgin_dialogs_info_cb),
						_("Cancel"), NULL,
						NULL, NULL, NULL,
						NULL);
}

static void
pidgin_dialogs_log_cb(gpointer data, PurpleRequestFields *fields)
{
	char *username;
	PurpleAccount *account;
	GSList *cur;

	account  = purple_request_fields_get_account(fields, "account");

	username = g_strdup(purple_normalize(account,
		purple_request_fields_get_string(fields,  "screenname")));

	if (username != NULL && *username != '\0' && account != NULL)
	{
		PidginBuddyList *gtkblist = pidgin_blist_get_default_gtk_blist();
		GSList *buddies;

		pidgin_set_cursor(gtkblist->window, GDK_WATCH);

		buddies = purple_find_buddies(account, username);
		for (cur = buddies; cur != NULL; cur = cur->next)
		{
			PurpleBlistNode *node = cur->data;
			if ((node != NULL) && ((node->prev != NULL) || (node->next != NULL)))
			{
				pidgin_log_show_contact((PurpleContact *)node->parent);
				g_slist_free(buddies);
				pidgin_clear_cursor(gtkblist->window);
				g_free(username);
				return;
			}
		}
		g_slist_free(buddies);

		pidgin_log_show(PURPLE_LOG_IM, username, account);

		pidgin_clear_cursor(gtkblist->window);
	}

	g_free(username);
}

/*
 * TODO - This needs to deal with logs of all types, not just IM logs.
 */
void
pidgin_dialogs_log(void)
{
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;

	fields = purple_request_fields_new();

	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);

	field = purple_request_field_string_new("screenname", _("_Name"), NULL, FALSE);
	purple_request_field_set_type_hint(field, "screenname-all");
	purple_request_field_set_required(field, TRUE);
	purple_request_field_group_add_field(group, field);

	field = purple_request_field_account_new("account", _("_Account"), NULL);

	/* purple_request_field_account_new() only sets a default value if you're
	 * connected, and it sets it from the list of connected accounts.  Since
	 * we're going to set show_all here, it makes sense to use the first
	 * account, not the first connected account. */
	if (purple_accounts_get_all() != NULL) {
		purple_request_field_account_set_default_value(field, purple_accounts_get_all()->data);
		purple_request_field_account_set_value(field, purple_accounts_get_all()->data);
	}

	purple_request_field_set_type_hint(field, "account");
	purple_request_field_account_set_show_all(field, TRUE);
	purple_request_field_set_visible(field,
		(purple_accounts_get_all() != NULL &&
		 purple_accounts_get_all()->next != NULL));
	purple_request_field_set_required(field, TRUE);
	purple_request_field_group_add_field(group, field);

	purple_request_fields(purple_get_blist(), _("View User Log"),
						NULL,
						_("Please enter the username or alias of the person "
						  "whose log you would like to view."),
						fields,
						_("OK"), G_CALLBACK(pidgin_dialogs_log_cb),
						_("Cancel"), NULL,
						NULL, NULL, NULL,
						NULL);
}

static void
pidgin_dialogs_alias_contact_cb(PurpleContact *contact, const char *new_alias)
{
	purple_blist_alias_contact(contact, new_alias);
}

void
pidgin_dialogs_alias_contact(PurpleContact *contact)
{
	g_return_if_fail(contact != NULL);

	purple_request_input(NULL, _("Alias Contact"), NULL,
					   _("Enter an alias for this contact."),
					   contact->alias, FALSE, FALSE, NULL,
					   _("Alias"), G_CALLBACK(pidgin_dialogs_alias_contact_cb),
					   _("Cancel"), NULL,
					   NULL, purple_contact_get_alias(contact), NULL,
					   contact);
}

static void
pidgin_dialogs_alias_buddy_cb(PurpleBuddy *buddy, const char *new_alias)
{
	purple_blist_alias_buddy(buddy, new_alias);
	serv_alias_buddy(buddy);
}

void
pidgin_dialogs_alias_buddy(PurpleBuddy *buddy)
{
	gchar *secondary;

	g_return_if_fail(buddy != NULL);

	secondary = g_strdup_printf(_("Enter an alias for %s."), buddy->name);

	purple_request_input(NULL, _("Alias Buddy"), NULL,
					   secondary, buddy->alias, FALSE, FALSE, NULL,
					   _("Alias"), G_CALLBACK(pidgin_dialogs_alias_buddy_cb),
					   _("Cancel"), NULL,
					   purple_buddy_get_account(buddy), purple_buddy_get_name(buddy), NULL,
					   buddy);

	g_free(secondary);
}

static void
pidgin_dialogs_alias_chat_cb(PurpleChat *chat, const char *new_alias)
{
	purple_blist_alias_chat(chat, new_alias);
}

void
pidgin_dialogs_alias_chat(PurpleChat *chat)
{
	g_return_if_fail(chat != NULL);

	purple_request_input(NULL, _("Alias Chat"), NULL,
					   _("Enter an alias for this chat."),
					   chat->alias, FALSE, FALSE, NULL,
					   _("Alias"), G_CALLBACK(pidgin_dialogs_alias_chat_cb),
					   _("Cancel"), NULL,
					   chat->account, NULL, NULL,
					   chat);
}

static void
pidgin_dialogs_remove_contact_cb(PurpleContact *contact)
{
	PurpleBlistNode *bnode, *cnode;
	PurpleGroup *group;

	cnode = (PurpleBlistNode *)contact;
	group = (PurpleGroup*)cnode->parent;
	for (bnode = cnode->child; bnode; bnode = bnode->next) {
		PurpleBuddy *buddy = (PurpleBuddy*)bnode;
		if (purple_account_is_connected(buddy->account))
			purple_account_remove_buddy(buddy->account, buddy, group);
	}
	purple_blist_remove_contact(contact);
}

void
pidgin_dialogs_remove_contact(PurpleContact *contact)
{
	PurpleBuddy *buddy = purple_contact_get_priority_buddy(contact);

	g_return_if_fail(contact != NULL);
	g_return_if_fail(buddy != NULL);

	if (PURPLE_BLIST_NODE(contact)->child == PURPLE_BLIST_NODE(buddy) &&
	    PURPLE_BLIST_NODE(buddy)->next == NULL) {
		pidgin_dialogs_remove_buddy(buddy);
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

		purple_request_action(contact, NULL, _("Remove Contact"), text, 0,
				NULL, purple_contact_get_alias(contact), NULL,
				contact, 2,
				_("_Remove Contact"), G_CALLBACK(pidgin_dialogs_remove_contact_cb),
				_("Cancel"),
				NULL);

		g_free(text);
	}
}

static void free_ggmo(struct _PidginGroupMergeObject *ggp)
{
	g_free(ggp->new_name);
	g_free(ggp);
}

static void
pidgin_dialogs_merge_groups_cb(struct _PidginGroupMergeObject *GGP)
{
	purple_blist_rename_group(GGP->parent, GGP->new_name);
	free_ggmo(GGP);
}

void
pidgin_dialogs_merge_groups(PurpleGroup *source, const char *new_name)
{
	gchar *text;
	struct _PidginGroupMergeObject *ggp;

	g_return_if_fail(source != NULL);
	g_return_if_fail(new_name != NULL);

	text = g_strdup_printf(
				_("You are about to merge the group called %s into the group "
				"called %s. Do you want to continue?"), source->name, new_name);

	ggp = g_new(struct _PidginGroupMergeObject, 1);
	ggp->parent = source;
	ggp->new_name = g_strdup(new_name);

	purple_request_action(source, NULL, _("Merge Groups"), text, 0,
			NULL, NULL, NULL,
			ggp, 2,
			_("_Merge Groups"), G_CALLBACK(pidgin_dialogs_merge_groups_cb),
			_("Cancel"), G_CALLBACK(free_ggmo));

	g_free(text);
}

static void
pidgin_dialogs_remove_group_cb(PurpleGroup *group)
{
	PurpleBlistNode *cnode, *bnode;

	cnode = ((PurpleBlistNode*)group)->child;

	while (cnode) {
		if (PURPLE_BLIST_NODE_IS_CONTACT(cnode)) {
			bnode = cnode->child;
			cnode = cnode->next;
			while (bnode) {
				PurpleBuddy *buddy;
				if (PURPLE_BLIST_NODE_IS_BUDDY(bnode)) {
					buddy = (PurpleBuddy*)bnode;
					bnode = bnode->next;
					if (purple_account_is_connected(buddy->account)) {
						purple_account_remove_buddy(buddy->account, buddy, group);
						purple_blist_remove_buddy(buddy);
					}
				} else {
					bnode = bnode->next;
				}
			}
		} else if (PURPLE_BLIST_NODE_IS_CHAT(cnode)) {
			PurpleChat *chat = (PurpleChat *)cnode;
			cnode = cnode->next;
			if (purple_account_is_connected(chat->account))
				purple_blist_remove_chat(chat);
		} else {
			cnode = cnode->next;
		}
	}

	purple_blist_remove_group(group);
}

void
pidgin_dialogs_remove_group(PurpleGroup *group)
{
	gchar *text;

	g_return_if_fail(group != NULL);

	text = g_strdup_printf(_("You are about to remove the group %s and all its members from your buddy list.  Do you want to continue?"),
						   group->name);

	purple_request_action(group, NULL, _("Remove Group"), text, 0,
						NULL, NULL, NULL,
						group, 2,
						_("_Remove Group"), G_CALLBACK(pidgin_dialogs_remove_group_cb),
						_("Cancel"), NULL);

	g_free(text);
}

/* XXX - Some of this should be moved into the core, methinks. */
static void
pidgin_dialogs_remove_buddy_cb(PurpleBuddy *buddy)
{
	PurpleGroup *group;
	gchar *name;
	PurpleAccount *account;

	group = purple_buddy_get_group(buddy);
	name = g_strdup(buddy->name); /* b->name is a crasher after remove_buddy */
	account = buddy->account;

	purple_debug_info("blist", "Removing '%s' from buddy list.\n", buddy->name);
	/* TODO - Should remove from blist first... then call purple_account_remove_buddy()? */
	purple_account_remove_buddy(buddy->account, buddy, group);
	purple_blist_remove_buddy(buddy);

	g_free(name);
}

void
pidgin_dialogs_remove_buddy(PurpleBuddy *buddy)
{
	gchar *text;

	g_return_if_fail(buddy != NULL);

	text = g_strdup_printf(_("You are about to remove %s from your buddy list.  Do you want to continue?"),
						   buddy->name);

	purple_request_action(buddy, NULL, _("Remove Buddy"), text, 0,
						purple_buddy_get_account(buddy), purple_buddy_get_name(buddy), NULL,
						buddy, 2,
						_("_Remove Buddy"), G_CALLBACK(pidgin_dialogs_remove_buddy_cb),
						_("Cancel"), NULL);

	g_free(text);
}

static void
pidgin_dialogs_remove_chat_cb(PurpleChat *chat)
{
	purple_blist_remove_chat(chat);
}

void
pidgin_dialogs_remove_chat(PurpleChat *chat)
{
	const gchar *name;
	gchar *text;

	g_return_if_fail(chat != NULL);

	name = purple_chat_get_name(chat);
	text = g_strdup_printf(_("You are about to remove the chat %s from your buddy list.  Do you want to continue?"),
			name ? name : "");

	purple_request_action(chat, NULL, _("Remove Chat"), text, 0,
						chat->account, NULL, NULL,
						chat, 2,
						_("_Remove Chat"), G_CALLBACK(pidgin_dialogs_remove_chat_cb),
						_("Cancel"), NULL);

	g_free(text);
}
