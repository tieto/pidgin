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

#include "gtkblist.h"
#include "gtkdialogs.h"
#include "gtkimhtml.h"
#include "gtkimhtmltoolbar.h"
#include "gtklog.h"
#include "gtkutils.h"
#include "gaimstock.h"

static GList *dialogwindows = NULL;

static GtkWidget *about = NULL;

struct _GaimGroupMergeObject {
	GaimGroup* parent;
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

/* Order: Lead Developer, then Alphabetical by Last Name */
static struct developer developers[] = {
	{"Sean Egan",					N_("lead developer"), "sean.egan@gmail.com"},
	{"Daniel 'datallah' Atallah",	N_("developer"), NULL},
	{"Ethan 'Paco-Paco' Blanton",	N_("developer"), NULL},
	{"Thomas Butter",				N_("developer"), NULL},
	{"Sadrul Habib Chowdhury",		N_("developer"), NULL},
	{"Mark 'KingAnt' Doliner",		N_("developer"), NULL},
	{"Christian 'ChipX86' Hammond",	N_("developer & webmaster"), NULL},
	{"Gary 'grim' Kramlich",		N_("developer"), NULL},
	{"Richard 'rlaager' Laager",	N_("developer"), NULL},
	{"Christopher 'siege' O'Brien", N_("developer"), "taliesein@users.sf.net"},
	{"Bartosz Oler",		N_("developer"), NULL},
	{"Etan 'deryni' Reisner",       N_("developer"), NULL},
	{"Tim 'marv' Ringenbach",		N_("developer"), NULL},
	{"Luke 'LSchiere' Schierer",	N_("support"), "lschiere@users.sf.net"},
	{"Megan 'Cae' Schneider",       N_("support/QA"), NULL},
	{"Evan Schoenberg",		N_("developer"), NULL},
	{"Stu 'nosnilmot' Tomlinson",	N_("developer"), NULL},
	{"Nathan 'faceprint' Walp",		N_("developer"), NULL},
	{NULL, NULL, NULL}
};

/* Order: Alphabetical by Last Name */
static struct developer patch_writers[] = {
	{"Ka-Hing 'javabsp' Cheung",	NULL,	NULL},
	{"Felipe 'shx' Contreras",		NULL,	NULL},
	{"Decklin Foster",				NULL,	NULL},
	{"Casey Harkins",               NULL,   NULL},
	{"Peter 'Bleeter' Lawler",      NULL,   NULL},
	{"Robert 'Robot101' McQueen",	NULL,	NULL},
	{"Benjamin Miller",				NULL,	NULL},
	{"Kevin 'SimGuy' Stange",		NULL,	NULL},
	{NULL, NULL, NULL}
};

/* Order: Alphabetical by Last Name */
static struct developer retired_developers[] = {
	{"Herman Bloggs",		N_("win32 port"), "herman@bluedigits.com"},
	{"Jim Duchek",			N_("maintainer"), "jim@linuxpimps.com"},
	{"Rob Flynn",			N_("maintainer"), "gaim@robflynn.com"},
	{"Adam Fritzler",		N_("libfaim maintainer"), NULL},
	/* If "lazy bum" translates literally into a serious insult, use something else or omit it. */
	{"Syd Logan",			N_("hacker and designated driver [lazy bum]"), NULL},
	{"Jim Seymour",			N_("Jabber developer"), NULL},
	{"Mark Spencer",		N_("original author"), "markster@marko.net"},
	{"Eric Warmenhoven",	N_("lead developer"), "warmenhoven@yahoo.com"},
	{NULL, NULL, NULL}
};

/* Order: Code, then Alphabetical by Last Name */
static struct translator current_translators[] = {
	{N_("Arabic"),              "ar", "Mohamed Magdy", "alnokta@yahoo.com"},
	{N_("Bulgarian"),           "bg", "Vladimira Girginova", "missing@here.is"},
	{N_("Bulgarian"),           "bg", "Vladimir (Kaladan) Petkov", "vpetkov@i-space.org"},
	{N_("Bengali"),             "bn", "INDRANIL DAS GUPTA", "indradg@l2c2.org"},
	{N_("Bengali"),             "bn", "Samia Nimatullah", "mailsamia2001@yahoo.com"},
	{N_("Bosnian"),             "bs", "Lejla Hadzialic", "lejlah@gmail.com"},
	{N_("Catalan"),             "ca", "Josep Puigdemont", "tradgnome@softcatala.org"},
	{N_("Valencian-Catalan"),   "ca@valencia", "Toni Hermoso", "toniher@softcatala.org"},
	{N_("Valencian-Catalan"),   "ca@valencia", "Josep Puigdemont", "tradgnome@softcatala.org"},
	{N_("Czech"),               "cs", "Miloslav Trmac", "mitr@volny.cz"},
	{N_("Danish"),              "da", "Morten Brix Pedersen", "morten@wtf.dk"},
	{N_("German"),              "de", "Björn Voigt", "bjoern@cs.tu-berlin.de"},
	{N_("German"),              "de", "Jochen Kemnade", "kemnade@gmail.com"},
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
	{N_("Euskera(Basque)"),     "eu", "Hizkuntza Politikarako Sailburuordetza", "hizkpol@ej-gv.es"},
	{N_("Euskera(Basque)"),     "eu", "Iñaki Larrañaga Murgoitio", "dooteo@zundan.com"},
	{N_("Persian"),             "fa", "Elnaz Sarbar", "elnaz@farsiweb.info"},
	{N_("Persian"),             "fa", "Meelad Zakaria", "meelad@farsiweb.info"},
	{N_("Persian"),             "fa", "Roozbeh Pournader ", "roozbeh@farsiweb.info"},
	{N_("Finnish"),             "fi", "Timo Jyrinki", "timo.jyrinki@iki.fi"},
	{N_("French"),              "fr", "Éric Boumaour", "zongo_fr@users.sourceforge.net"},
	{N_("Galician"),            "gl", "Ignacio Casal Quinteiro", "nacho.resa@gmail.com"},
	{N_("Gujarati"),            "gu", "Ankit Patel", "ankit_patel@users.sf.net"},
	{N_("Gujarati"),            "gu", "Gujarati Language Team", "indianoss-gujarati@lists.sourceforge.net"},
	{N_("Hebrew"),              "he", "Shalom Craimer", "scraimer@gmail.com"},
	{N_("Hindi"),               "hi", "Ravishankar Shrivastava", "raviratlami@yahoo.com"},
	{N_("Hungarian"),           "hu", "Zoltan Sutto", "sutto.zoltan@rutinsoft.hu"},
	{N_("Italian"),             "it", "Claudio Satriano", "satriano@na.infn.it"},
	{N_("Japanese"),            "ja", "Takashi Aihana", "aihana@gnome.gr.jp"},
	{N_("Georgian"),            "ka", "Ubuntu Georgian Translators", "alexander.didebulidze@stusta.mhn.de"},
	{N_("Korean"),              "ko", "Kyung-uk Son", "vvs740@chol.com"},
	{N_("Kurdish"),             "ku", "Erdal Ronahi", "erdal.ronahi@gmail.com"},
	{N_("Kurdish"),             "ku", "Amed Ç. Jiyan", "amedcj@hotmail.com"},
	{N_("Kurdish"),             "ku", "Rizoyê Xerzî", "rizoxerzi@hotmail.com"},
	{N_("Lithuanian"),          "lt", "Laurynas Biveinis", "laurynas.biveinis@gmail.com"},
	{N_("Macedonian"),          "mk", "Tomislav Markovski", "herrera@users.sf.net"},
	{N_("Nepali"),              "ne", "Shyam Krishna Bal", "shyamkrishna_bal@yahoo.com"},
	{N_("Dutch, Flemish"),      "nl", "Vincent van Adrighem", "V.vanAdrighem@dirck.mine.nu"},
	{N_("Norwegian"),           "no", "Petter Johan Olsen", "petter.olsen@cc.uit.no"},
	{N_("Polish"),              "pl", "Emil Nowak", "emil5@go2.pl"},
	{N_("Polish"),              "pl", "Krzysztof Foltman", "krzysztof@foltman.com"},
	{N_("Portuguese"),          "pt", "Duarte Henriques", "duarte_henriques@myrealbox.com"},
	{N_("Portuguese-Brazil"),   "pt_BR", "Maurício de Lemos Rodrigues Collares Neto", "mauricioc@gmail.com"},
	{N_("Romanian"),            "ro", "Mişu Moldovan", "dumol@gnome.ro"},
	{N_("Russian"),             "ru", "Dmitry Beloglazov", "dmaa@users.sf.net"},
	{N_("Serbian"),             "sr", "Danilo Šegan", "dsegan@gmx.net"},
	{N_("Serbian"),             "sr", "Aleksandar Urosevic", "urke@users.sourceforge.net"},
	{N_("Slovak"),              "sk", "Richard Golier", "golierr@gmail.com"},
	{N_("Slovenian"),           "sl", "Martin Srebotnjak", "miles @ filmsi . net"},
	{N_("Albanian"),            "sq", "Besnik Bleta", "besnik@programeshqip.org"},
	{N_("Swedish"),             "sv", "Tore Lundqvist", "tlt@mima.x.se"},
	{N_("Tamil"),               "ta", "Viveka Nathan K", "vivekanathan@users.sourceforge.net"},
	{N_("Telugu"),              "te", "Mr. Subbaramaih", "info.gist@cdac.in"},
	{N_("Thai"),                "th", "Isriya Paireepairit", "markpeak@gmail.com"},
	{N_("Turkish"),             "tr", "Ahmet Alp BALKAN", "prf_q@users.sf.net"},
	{N_("Vietnamese"),          "vi", N_("T.M.Thanh and the Gnome-Vi Team"), "gnomevi-list@lists.sf.net"},
	{N_("Simplified Chinese"),  "zh_CN", "Funda Wang", "fundawang@linux.net.cn"},
	{N_("Traditional Chinese"), "zh_TW", "Ambrose C. Li", "acli@ada.dhs.org"},
	{N_("Traditional Chinese"), "zh_TW", "Paladin R. Liu", "paladin@ms1.hinet.net"},
	{NULL, NULL, NULL, NULL}
};


static struct translator past_translators[] = {
	{N_("Amharic"),             "am", "Daniel Yacob", NULL},
	{N_("Bulgarian"),           "bg", "Hristo Todorov", NULL},
	{N_("Catalan"),             "ca", "JM Pérez Cáncer", NULL},
	{N_("Catalan"),             "ca", "Robert Millan", NULL},
	{N_("Czech"),               "cs", "Honza Král", NULL},
	{N_("German"),              "de", "Daniel Seifert, Karsten Weiss", NULL},
	{N_("Spanish"),             "es", "JM Pérez Cáncer", NULL},
	{N_("Spanish"),             "es", "Nicolás Lichtmaier", NULL},
	{N_("Spanish"),             "es", "Amaya Rodrigo", NULL},
	{N_("Spanish"),				"es", "Alejandro G Villar", NULL},
	{N_("Finnish"),             "fi", "Arto Alakulju", NULL},
	{N_("Finnish"),             "fi", "Tero Kuusela", NULL},
	{N_("French"),              "fr", "Sébastien François", NULL},
	{N_("French"),              "fr", "Stéphane Pontier", NULL},
	{N_("French"),              "fr", "Stéphane Wirtel", NULL},
	{N_("French"),              "fr", "Loïc Jeannin", NULL},
	{N_("Hebrew"),              "he", "Pavel Bibergal", NULL},
	{N_("Italian"),				"it", "Salvatore di Maggio", NULL},
	{N_("Japanese"),            "ja", "Ryosuke Kutsuna", NULL},
	{N_("Japanese"),            "ja", "Taku Yasui", NULL},
	{N_("Japanese"),            "ja", "Junichi Uekawa", NULL},
	{N_("Georgian"),            "ka", "Temuri Doghonadze", NULL},
	{N_("Korean"),              "ko", "Sang-hyun S, A Ho-seok Lee", NULL},
	{N_("Lithuanian"),          "lt", "Andrius Štikonas", NULL},
	{N_("Lithuanian"),          "lt", "Gediminas Čičinskas", NULL},
	{N_("Polish"),              "pl", "Przemysław Sułek", NULL},
	{N_("Russian"),             "ru", "Alexandre Prokoudine", NULL},
	{N_("Russian"),             "ru", "Sergey Volozhanin", NULL},
	{N_("Slovak"),              "sk", "Daniel Režný", NULL},
	{N_("Slovenian"),           "sl", "Matjaz Horvat", NULL},
	{N_("Swedish"),             "sv", "Christian Rose", NULL},
	{N_("Simplified Chinese"),  "zh_CN", "Hashao, Rocky S. Lee", NULL},
	{N_("Traditional Chinese"), "zh_TW", "Hashao, Rocky S. Lee", NULL},
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

/* This function puts the version number onto the pixmap we use in the 'about' 
 * screen in Gaim. */
static void
gaim_gtk_logo_versionize(GdkPixbuf **original, GtkWidget *widget) {
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

	markup = g_strdup_printf("<span foreground=\"#FFFFFF\" size=\"larger\">%s</span>", VERSION);
	pango_layout_set_font_description(layout, style->font_desc);
	pango_layout_set_markup(layout, markup, strlen(markup));
	g_free(markup);

	pango_layout_get_pixel_size(layout, &lwidth, &lheight);
	gdk_draw_layout(GDK_DRAWABLE(pixmap), style->bg_gc[GTK_STATE_NORMAL],
					width - (lwidth + 3), height - (lheight + 1), layout);
	g_object_unref(G_OBJECT(layout));

	*original = gdk_pixbuf_get_from_drawable(NULL, pixmap, NULL,
											 0, 0, 0, 0,
											 width, height);
	g_object_unref(G_OBJECT(pixmap));
}

void gaim_gtkdialogs_about()
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
	char* filename;
	GdkPixbuf *pixbuf;

	if (about != NULL) {
		gtk_window_present(GTK_WINDOW(about));
		return;
	}

	GAIM_DIALOG(about);
	gtk_window_set_title(GTK_WINDOW(about), _("About " PIDGIN_NAME));
	gtk_window_set_role(GTK_WINDOW(about), "about");
	gtk_window_set_resizable(GTK_WINDOW(about), TRUE);
	gtk_window_set_default_size(GTK_WINDOW(about), 340, 450);

	hbox = gtk_hbox_new(FALSE, GAIM_HIG_BORDER);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), GAIM_HIG_BORDER);
	gtk_container_add(GTK_CONTAINER(about), hbox);

	vbox = gtk_vbox_new(FALSE, GAIM_HIG_BORDER);
	gtk_container_add(GTK_CONTAINER(hbox), vbox);

	/* Generate a logo with a version number */
	logo = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_realize(logo);
	filename = g_build_filename(DATADIR, "pixmaps", "gaim", "logo.png", NULL);
	pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
	g_free(filename);
	gaim_gtk_logo_versionize(&pixbuf, logo);
	gtk_widget_destroy(logo);
	logo = gtk_image_new_from_pixbuf(pixbuf);
	gdk_pixbuf_unref(pixbuf);
	/* Insert the logo */
	obj = gtk_widget_get_accessible(logo);
	atk_object_set_description(obj, PIDGIN_NAME " " VERSION);
	gtk_box_pack_start(GTK_BOX(vbox), logo, FALSE, FALSE, 0);

	frame = gaim_gtk_create_imhtml(FALSE, &text, NULL, NULL);
	gtk_imhtml_set_format_functions(GTK_IMHTML(text), GTK_IMHTML_ALL ^ GTK_IMHTML_SMILEY);
	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);

	str = g_string_sized_new(4096);

	g_string_append(str,
		_(PIDGIN_NAME " is a modular messaging client capable of using "
		  "AIM, MSN, Yahoo!, Jabber, ICQ, IRC, SILC, SIP/SIMPLE, "
		  "Novell GroupWise, Lotus Sametime, Bonjour, Zephyr, "
		  "Gadu-Gadu, and QQ all at once.  "
		  "It is written using GTK+.<BR><BR>"
		  "You may modify and redistribute the program under "
		  "the terms of the GPL (version 2 or later).  A copy of the GPL is "
		  "contained in the 'COPYING' file distributed with Gaim.  "
		  PIDGIN_NAME " is copyrighted by its contributors.  See the 'COPYRIGHT' "
		  "file for the complete list of contributors.  We provide no "
		  "warranty for this program.<BR><BR>"));

	g_string_append(str, "<FONT SIZE=\"4\">URL:</FONT> <A HREF=\""
					GAIM_WEBSITE "\">" GAIM_WEBSITE "</A><BR/><BR/>");
#ifdef _WIN32
	g_string_append_printf(str, _("<FONT SIZE=\"4\">IRC:</FONT> "
						   "#wingaim on irc.freenode.net<BR><BR>"));
#else
	g_string_append_printf(str, _("<FONT SIZE=\"4\">IRC:</FONT> "
						   "#gaim on irc.freenode.net<BR><BR>"));
#endif

	/* Current Developers */
	g_string_append_printf(str, "<FONT SIZE=\"4\">%s:</FONT><BR/>",
						   _("Current Developers"));
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

#ifndef _WIN32
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
#endif

#ifdef GAIM_PLUGINS
	g_string_append(str, "  <b>Plugins:</b> Enabled<br/>");
#else
	g_string_append(str, "  <b>Plugins:</b> Disabled<br/>");
#endif

#ifdef HAVE_SSL
	g_string_append(str, "  <b>SSL:</b> " PIDGIN_NAME " was compiled with SSL support.<br/>");
#else
	g_string_append(str, "  <b>SSL:</b> " PIDGIN_NAME " was <b><i>NOT</i></b> compiled with any SSL support!<br/>");
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
	g_string_append_printf(str, "    <b>D-BUS:</b> Enabled<br/>");
#else
	g_string_append_printf(str, "    <b>D-BUS:</b> Disabled<br/>");
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
#ifdef HAVE_LIBNM
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

#ifdef HAVE_PERL
	g_string_append(str, "    <b>Perl:</b> Enabled<br/>");
#else
	g_string_append(str, "    <b>Perl:</b> Disabled<br/>");
#endif

#ifndef _WIN32
#ifdef HAVE_STARTUP_NOTIFICATION
	g_string_append(str, "    <b>Startup Notification:</b> Enabled<br/>");
#else
	g_string_append(str, "    <b>Startup Notification:</b> Disabled<br/>");
#endif
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
	g_string_append(str, "    <b>Zephyr library (libzephyr):</b> Not External<br/>");
#endif

#ifdef ZEPHYR_USES_KERBEROS
	g_string_append(str, "    <b>Zephyr uses Kerberos:</b> Yes<br/>");
#else
	g_string_append(str, "    <b>Zephyr uses Kerberos:</b> No<br/>");
#endif
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

	g_return_if_fail(account != NULL);
	g_return_if_fail(username != NULL);

	conv = gaim_find_conversation_with_account(GAIM_CONV_TYPE_IM, username, account);

	if (conv == NULL)
		conv = gaim_conversation_new(GAIM_CONV_TYPE_IM, account, username);

	gaim_gtkconv_present_conversation(conv);
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

	gtk_container_set_border_width (GTK_CONTAINER(window), GAIM_HIG_BOX_SPACE);
	gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
	gtk_dialog_set_has_separator(GTK_DIALOG(window), FALSE);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(window)->vbox), GAIM_HIG_BORDER);
	gtk_container_set_border_width (GTK_CONTAINER(GTK_DIALOG(window)->vbox), GAIM_HIG_BOX_SPACE);

	hbox = gtk_hbox_new(FALSE, GAIM_HIG_BORDER);
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
	GSList *cur;

	account  = gaim_request_fields_get_account(fields, "account");

	username = g_strdup(gaim_normalize(account,
		gaim_request_fields_get_string(fields,  "screenname")));

	if (username != NULL && *username != '\0' && account != NULL)
	{
		GaimGtkBuddyList *gtkblist = gaim_gtk_blist_get_default_gtk_blist();
		GSList *buddies;

		gaim_gtk_set_cursor(gtkblist->window, GDK_WATCH);

		buddies = gaim_find_buddies(account, username);
		for (cur = buddies; cur != NULL; cur = cur->next)
		{
			GaimBlistNode *node = cur->data;
			if ((node != NULL) && ((node->prev != NULL) || (node->next != NULL)))
			{
				gaim_gtk_log_show_contact((GaimContact *)node->parent);
				g_slist_free(buddies);
				gaim_gtk_clear_cursor(gtkblist->window);
				g_free(username);
				return;
			}
		}
		g_slist_free(buddies);

		gaim_gtk_log_show(GAIM_LOG_IM, username, account);

		gaim_gtk_clear_cursor(gtkblist->window);
	}

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
	gaim_request_field_set_type_hint(field, "screenname-all");
	gaim_request_field_set_required(field, TRUE);
	gaim_request_field_group_add_field(group, field);

	field = gaim_request_field_account_new("account", _("_Account"), NULL);

	/* gaim_request_field_account_new() only sets a default value if you're
	 * connected, and it sets it from the list of connected accounts.  Since
	 * we're going to set show_all here, it makes sense to use the first
	 * account, not the first connected account. */
	if (gaim_accounts_get_all() != NULL) {
		gaim_request_field_account_set_default_value(field, gaim_accounts_get_all()->data);
		gaim_request_field_account_set_value(field, gaim_accounts_get_all()->data);
	}

	gaim_request_field_set_type_hint(field, "account");
	gaim_request_field_account_set_show_all(field, TRUE);
	gaim_request_field_set_visible(field,
		(gaim_accounts_get_all() != NULL &&
		 gaim_accounts_get_all()->next != NULL));
	gaim_request_field_set_required(field, TRUE);
	gaim_request_field_group_add_field(group, field);

	gaim_request_fields(gaim_get_blist(), _("View User Log"),
						NULL,
						_("Please enter the screen name or alias of the person "
						  "whose log you would like to view."),
						fields,
						_("OK"), G_CALLBACK(gaim_gtkdialogs_log_cb),
						_("Cancel"), NULL,
						NULL);
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
			gaim_account_remove_buddy(buddy->account, buddy, group);
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
				_("_Remove Contact"), G_CALLBACK(gaim_gtkdialogs_remove_contact_cb),
				_("Cancel"), NULL);

		g_free(text);
	}
}

static void free_ggmo(struct _GaimGroupMergeObject *ggp)
{
	g_free(ggp->new_name);
	g_free(ggp);
}

static void
gaim_gtkdialogs_merge_groups_cb(struct _GaimGroupMergeObject *GGP)
{
	gaim_blist_rename_group(GGP->parent, GGP->new_name);
	free_ggmo(GGP);
}

void
gaim_gtkdialogs_merge_groups(GaimGroup *source, const char *new_name)
{
	gchar *text;
	struct _GaimGroupMergeObject *ggp;

	g_return_if_fail(source != NULL);
	g_return_if_fail(new_name != NULL);

	text = g_strdup_printf(
				_("You are about to merge the group called %s into the group "
				"called %s. Do you want to continue?"), source->name, new_name);

	ggp = g_new(struct _GaimGroupMergeObject, 1);
	ggp->parent = source;
	ggp->new_name = g_strdup(new_name);
	
	gaim_request_action(source, NULL, _("Merge Groups"), text, 0, ggp, 2,
			_("_Merge Groups"), G_CALLBACK(gaim_gtkdialogs_merge_groups_cb),
			_("Cancel"), G_CALLBACK(free_ggmo));

	g_free(text);
}

static void
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
					buddy = (GaimBuddy*)bnode;
					bnode = bnode->next;
					if (gaim_account_is_connected(buddy->account)) {
						gaim_account_remove_buddy(buddy->account, buddy, group);
						gaim_blist_remove_buddy(buddy);
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
						_("_Remove Group"), G_CALLBACK(gaim_gtkdialogs_remove_group_cb),
						_("Cancel"), NULL);

	g_free(text);
}

/* XXX - Some of this should be moved into the core, methinks. */
static void
gaim_gtkdialogs_remove_buddy_cb(GaimBuddy *buddy)
{
	GaimGroup *group;
	gchar *name;
	GaimAccount *account;

	group = gaim_buddy_get_group(buddy);
	name = g_strdup(buddy->name); /* b->name is a crasher after remove_buddy */
	account = buddy->account;

	gaim_debug_info("blist", "Removing '%s' from buddy list.\n", buddy->name);
	/* TODO - Should remove from blist first... then call gaim_account_remove_buddy()? */
	gaim_account_remove_buddy(buddy->account, buddy, group);
	gaim_blist_remove_buddy(buddy);

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
						_("_Remove Buddy"), G_CALLBACK(gaim_gtkdialogs_remove_buddy_cb),
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
	const gchar *name;
	gchar *text;

	g_return_if_fail(chat != NULL);

	name = gaim_chat_get_name(chat);
	text = g_strdup_printf(_("You are about to remove the chat %s from your buddy list.  Do you want to continue?"),
			name ? name : "");

	gaim_request_action(chat, NULL, _("Remove Chat"), text, 0, chat, 2,
						_("_Remove Chat"), G_CALLBACK(gaim_gtkdialogs_remove_chat_cb),
						_("Cancel"), NULL);

	g_free(text);
}
