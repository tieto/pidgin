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
#include "gtkimhtml.h"
#include "gtkutils.h"
#include "stock.h"
#include "ui.h"

/* XXX For WEBSITE */
#include "gaim.h"

static GtkWidget *about = NULL;

static void destroy_about()
{
	if (about)
		gtk_widget_destroy(about);
	about = NULL;
}

void show_about(GtkWidget *w, void *data)
{
	GtkWidget *vbox;
	GtkWidget *frame;
	GtkWidget *fbox;
	GtkWidget *hbox;
	GtkWidget *button;
	GtkWidget *text;
	GtkWidget *sw;
	GtkWidget *logo;
	char abouttitle[45];

	if (!about) {

		GAIM_DIALOG(about);
		gtk_window_set_default_size(GTK_WINDOW(about), 450, -1);
		g_snprintf(abouttitle, sizeof(abouttitle), _("About Gaim v%s"), VERSION);
		gtk_window_set_title(GTK_WINDOW(about), abouttitle);
		gtk_window_set_role(GTK_WINDOW(about), "about");
		gtk_window_set_resizable(GTK_WINDOW(about), TRUE);
		gtk_widget_realize(about);

		vbox = gtk_vbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
		gtk_container_add(GTK_CONTAINER(about), vbox);

		frame = gtk_frame_new("Gaim v" VERSION);
		gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);

		fbox = gtk_vbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(fbox), 5);
		gtk_container_add(GTK_CONTAINER(frame), fbox);

		logo = gtk_image_new_from_stock(GAIM_STOCK_LOGO, gtk_icon_size_from_name(GAIM_ICON_SIZE_LOGO));
		gtk_box_pack_start(GTK_BOX(fbox), logo, FALSE, FALSE, 0);

		sw = gtk_scrolled_window_new(NULL, NULL);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
				GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_IN);
		gtk_box_pack_start(GTK_BOX(fbox), sw, TRUE, TRUE, 0);

		text = gtk_imhtml_new(NULL, NULL);
		gtk_container_add(GTK_CONTAINER(sw), text);
		gtk_widget_set_size_request(sw, -1, 350);
		gaim_setup_imhtml(text);

		gtk_imhtml_append_text(GTK_IMHTML(text),
				  _("Gaim is a modular Instant Messaging client capable of "
					  "using AIM, ICQ, Yahoo!, MSN, IRC, Jabber, Napster, "
					  "Zephyr, and Gadu-Gadu all at once.  It is written using "
					  "Gtk+ and is licensed under the GPL.<BR><BR>"), -1, GTK_IMHTML_NO_SCROLL);

		gtk_imhtml_append_text(GTK_IMHTML(text),
				"<FONT SIZE=\"3\">URL:</FONT> <A HREF=\"" GAIM_WEBSITE "\">"
				GAIM_WEBSITE "</A><BR><BR>", -1, GTK_IMHTML_NO_SCROLL);

		gtk_imhtml_append_text(GTK_IMHTML(text),
				_("<FONT SIZE=\"3\">IRC:</FONT> #gaim on irc.freenode.net"
				"<BR><BR>"), -1, GTK_IMHTML_NO_SCROLL);

		gtk_imhtml_append_text(GTK_IMHTML(text),
			_("<FONT SIZE=\"3\">Active Developers:</FONT><BR>"), -1, GTK_IMHTML_NO_SCROLL);
		gtk_imhtml_append_text(GTK_IMHTML(text),
			_("  Rob Flynn (maintainer) "
			"&lt;<A HREF=\"mailto:rob@marko.net\">rob@marko.net</A>&gt;<BR>"
			"  Sean Egan (lead developer) "
			"&lt;<A HREF=\"mailto:bj91704@binghamton.edu\">"
			"bj91704@binghamton.edu</A>&gt;<BR>"
			"  Christian 'ChipX86' Hammond (developer & webmaster)<BR>"
			"  Herman Bloggs (win32 port) "
			"&lt;<A HREF=\"mailto:hermanator12002@yahoo.com\">"
			"hermanator12002@yahoo.com</A>&gt;<BR>"
			"  Nathan 'faceprint' Walp (developer)<BR>"
			"  Mark 'KingAnt' Doliner (developer)<BR>"
			"  Luke 'LSchiere' Schierer (support)<BR>"
			"<BR>"), -1, GTK_IMHTML_NO_SCROLL);

		gtk_imhtml_append_text(GTK_IMHTML(text),
			_("<FONT SIZE=\"3\">Crazy Patch Writers:</FONT><BR>"), -1, GTK_IMHTML_NO_SCROLL);
		gtk_imhtml_append_text(GTK_IMHTML(text),
			"  Benjamin Miller<BR>"
			"  Decklin Foster<BR>"
			"  Etan 'deryni' Reisner<BR>"
			"  Ethan 'Paco-Paco' Blanton<br>"
			"  Robert 'Robot101' McQueen<BR>"
			"<BR>", -1, GTK_IMHTML_NO_SCROLL);

		gtk_imhtml_append_text(GTK_IMHTML(text),
				_("<FONT SIZE=\"3\">Retired Developers:</FONT><BR>"), -1, GTK_IMHTML_NO_SCROLL);
		gtk_imhtml_append_text(GTK_IMHTML(text),
				_("  Adam Fritzler (former libfaim maintainer)<BR>"
				"  Eric Warmenhoven (former lead developer) "
				"&lt;<A HREF=\"mailto:warmenhoven@yahoo.com\">"
				"warmenhoven@yahoo.com</A>&gt;<BR>"
				"  Jim Duchek (former maintainer)<BR>"
				"  Jim Seymour (former Jabber developer)<BR>"
				"  Mark Spencer (original author) "
				"&lt;<A HREF=\"mailto:markster@marko.net\">"
				"markster@marko.net</A>&gt;<BR>"
				"  Syd Logan (hacker and designated driver [lazy bum])<BR>"
				"<BR>"), -1, GTK_IMHTML_NO_SCROLL);

		gtk_imhtml_append_text(GTK_IMHTML(text),
				_("<FONT SIZE=\"3\">Current Translators:</FONT><BR>"), -1, GTK_IMHTML_NO_SCROLL);
		gtk_imhtml_append_text(GTK_IMHTML(text),
				_(""
				"  <b>Catalan (ca)</b> - Robert Millan &lt;<a href=\"mailto: zeratul2@wanadoo.es\">zeratul2@wanadoo.es</a>&gt;<br>"
				"  <b>Czech (cs)</b> - Miloslav Trmac &lt;<a href=\"mailto: mitr@volny.cz\">mitr@volny.cz</a>&gt;<br>"
				"  <b>Danish (da)</b> - Morten Brix Pedersen &lt;<a href=\"mailto: morten@wtf.dk\">morten@wtf.dk</a>&gt;<br>"
				"  <b>German (de)</b> - Björn Voigt &lt;<a href=\"mailto: bjoern@cs.tu-berlin.de\">bjoern@cs.tu-berlin.de</a>&gt;<br>"
				"  <b>Spanish (es)</b> - Javier Fernández-Sanguino Peña &lt;<a href=\"mailto: jfs@debian.org\">jfs@debian.org</a>&gt;<br>"
				"  <b>French (fr)</b> - E'ric Boumaour &lt;<a href=\"mailto: zongo_fr@users.sourceforge.net\">zongo_fr@users.sourceforge.net</a>&gt;<br>"
				"  <b>Hindi (hi)</b> - Guntupalli Karunakar &lt;<a href=\"mailto: karunakar@freedomink.org\">karunakar@freedomink.org</a>&gt;<br>"
				"  <b>Hungarian (hu)</b> - Zoltan Sutto &lt;<a href=\"mailto: suttozoltan@chello.hu\">suttozoltan@chello.hu</a>&gt;<br>"
				"  <b>Italian (it)</b> - Claudio Satriano &lt;<a href=\"mailto: satriano@na.infn.it\">satriano@na.infn.it</a>&gt;<br>"
				"  <b>Korean (ko)</b> - Kyung-uk Son &lt;<a href=\"mailto: vvs740@chol.com\">vvs740@chol.com</a>&gt;<br>"
				"  <b>Dutch; Flemish (nl)</b> - Vincent van Adrighem &lt;<a href=\"mailto: V.vanAdrighem@dirck.mine.nu\">V.vanAdrighem@dirck.mine.nu</a>&gt;<br>"
				"  <b>Portuguese-Brazil (pt_BR)</b> - Maurício de Lemos Rodrigues Collares Neto &lt;<a href=\"mailto: mauricioc@myrealbox.com\">mauricioc@myrealbox.com</a>&gt;<br>"
				"  <b>Romanian (ro)</b> - Mişu Moldovan &lt;<a href=\"mailto: dumol@go.ro\">dumol@go.ro</a>&gt;<br>"
				"  <b>Serbian (sr)</b> - Danilo Segan &lt;<a href=\"mailto: dsegan@gmx.net\">dsegan@gmx.net</a>&gt;<br>"
				"  <b>Swedish (sv)</b> - Tore Lundqvist &lt;<a href=\"mailto: tlt@mima.x.se\">tlt@mima.x.se</a>&gt;<br>"
				"  <b>Chinese-China (zh_CN)</b> - Funda Wang &lt;<a href=\"mailto: fundawang@linux.net.cn\">fundawang@linux.net.cn</a>&gt;<br>"
				"  <b>Chinese-Taiwan (zh_TW)</b> - Ambrose C. Li &lt;<a href=\"mailto: acli@ada.dhs.org\">acli@ada.dhs.org</a>&gt;<br>"
				"<BR>"), -1, GTK_IMHTML_NO_SCROLL);

		gtk_imhtml_append_text(GTK_IMHTML(text),
				_("<FONT SIZE=\"3\">Past Translators:</FONT><BR>"), -1, GTK_IMHTML_NO_SCROLL);
		gtk_imhtml_append_text(GTK_IMHTML(text),
				_(""
				"  <b>Amharic (am)</b> - Daniel Yacob<br>"
				"  <b>Bulgarian (bg)</b> - Hristo Todorov<br>"
				"  <b>Catalan (ca)</b> - JM Pérez Cáncer<br>"
				"  <b>Czech (cs)</b> - Honza Král<br>"
				"  <b>German (de)</b> - Daniel Seifert, Karsten Weiss<br>"
				"  <b>Spanish (es)</b> - Amaya Rodrigo, Alejandro G Villar, Nicola's Lichtmaier, JM Pérez Cáncer<br>"
				"  <b>French (fr)</b> - sebfrance, Stéphane Pontier, Stéphane Wirtel, Loïc Jeannin<br>"
				"  <b>Hebrew (he)</b> - Pavel Bibergal<br>"
				"  <b>Italian (it)</b> - Salvatore di Maggio<br>"
				"  <b>Japanese (ja)</b> - Ryosuke Kutsuna, Taku Yasui, Junichi Uekawa<br>"
				"  <b>Korean (ko)</b> - Sang-hyun S, A Ho-seok Lee<br>"
				"  <b>Norwegian (no)</b> - Petter Johan Olsen<br>"
				"  <b>Polish (pl)</b> - Przemysław Sułek<br>"
				"  <b>Russian (ru)</b> - Sergey Volozhanin<br>"
				"  <b>Slovak (sk)</b> - Daniel Režný<br>"
				"  <b>Swedish (sv)</b> - Christian Rose<br>"
				"  <b>Chinese-Taiwan (zh_TW)</b> - Paladin R. Liu<br>"
				"<BR>"), -1, GTK_IMHTML_NO_SCROLL);

		gtk_adjustment_set_value(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(sw)), 0);

		/* Close Button */

		hbox = gtk_hbox_new(FALSE, 5);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
		gtk_widget_show(hbox);

		button = gaim_pixbuf_button_from_stock(_("Close"), GTK_STOCK_CLOSE, GAIM_BUTTON_HORIZONTAL);
		gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);

		g_signal_connect_swapped(G_OBJECT(button), "clicked",
								 G_CALLBACK(destroy_about), G_OBJECT(about));
		g_signal_connect(G_OBJECT(about), "destroy",
						 G_CALLBACK(destroy_about), G_OBJECT(about));

		/* this makes the sizes not work. */
		/* GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT); */
		/* gtk_widget_grab_default(button); */

	}

	/* Let's give'em something to talk about -- woah woah woah */
	gtk_widget_show_all(about);
	gtk_window_present(GTK_WINDOW(about));
}
