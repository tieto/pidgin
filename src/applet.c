/**************************************************************
**
** GaimGnomeAppletMgr
** Author - Quinticent (John Palmieri: johnp@martianrock.com)
**
** Purpose - Takes over the task of managing the GNOME applet
**           code and provides a centralized codebase for
**	     GNOME integration for Gaim.
**
**
** gaim
**
** Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef USE_APPLET
#include <string.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libart_lgpl/art_affine.h>
#include <libart_lgpl/art_rgb_affine.h>
#include <libart_lgpl/art_rgb_rgba_affine.h>
#include "gaim.h"
#include "applet.h"
#include "pixmaps/aimicon.xpm"

static int connecting = 0;

gboolean applet_buddy_show = FALSE;

GtkWidget *applet;
static GtkWidget *icon;

static GtkAllocation get_applet_pos(gboolean);
static gint sizehint = 48;

static GdkPixmap *get_applet_icon(const char *name)
{
	GdkPixmap *cache;
	GdkGC *gc;
	char *path;
	GdkPixbuf *pb, *scale;
	guchar *dst, *p;
	double affine[6];
	int r,g,b,i;

	cache = gdk_pixmap_new(applet->window, sizehint, sizehint,
			gtk_widget_get_visual(applet)->depth); 
	gc = gdk_gc_new(cache);
	gdk_gc_copy(gc, applet->style->bg_gc[GTK_WIDGET_STATE(applet)]);

	path = gnome_pixmap_file(name);
	scale = gdk_pixbuf_new_from_file(path);
	g_free(path);
	if (!scale)
		return NULL;
	pb = gdk_pixbuf_scale_simple(scale, sizehint, sizehint, GDK_INTERP_HYPER);

	dst = g_new0(guchar, sizehint*sizehint*3);
	r = applet->style->bg[GTK_WIDGET_STATE(applet)].red>>8;
	g = applet->style->bg[GTK_WIDGET_STATE(applet)].green>>8;
	b = applet->style->bg[GTK_WIDGET_STATE(applet)].blue>>8;
	p = dst;
	for (i = 0; i < sizehint * sizehint; i++) {
		*p++ = r;
		*p++ = g;
		*p++ = b;
	}

	art_affine_identity(affine);
	art_rgb_rgba_affine(dst, 0, 0, sizehint, sizehint, sizehint * 3,
			gdk_pixbuf_get_pixels(pb),
			gdk_pixbuf_get_width(pb), gdk_pixbuf_get_height(pb),
			gdk_pixbuf_get_rowstride(pb), affine, ART_FILTER_NEAREST, NULL);

	gdk_pixbuf_unref(pb);
	gdk_draw_rgb_image(cache, gc, 0, 0, sizehint, sizehint,
			GDK_RGB_DITHER_NORMAL, dst, sizehint * 3);
	g_free(dst);

	gdk_gc_unref(gc);
	return cache;
}

static gboolean update_applet()
{
	char buf[BUF_LONG];
	GSList *c = connections;
	GdkPixmap *newpix;

	if (connecting) {
		newpix = get_applet_icon(GAIM_GNOME_CONNECT_ICON);
		applet_set_tooltips(_("Attempting to sign on...."));
	} else if (!connections) {
		newpix = get_applet_icon(GAIM_GNOME_OFFLINE_ICON);
		applet_set_tooltips(_("Offline. Click to bring up login box."));
	} else if (awaymessage) {
		int dsr = 0;

		if ((away_options & OPT_AWAY_QUEUE) && message_queue) {
			GSList *m = message_queue;
			int dsr = 0;
			while (m) {
				struct queued_message *qm = m->data;
				if (qm->flags & WFLAG_RECV)
					dsr++;
				m = m->next;
			}
		}

		if (dsr) {
			newpix = get_applet_icon(GAIM_GNOME_MSG_PENDING_ICON);
			g_snprintf(buf, sizeof(buf), _("Away: %d pending."), dsr);
		} else {
			newpix = get_applet_icon(GAIM_GNOME_AWAY_ICON);
			g_snprintf(buf, sizeof(buf), _("Away."));
		}

		applet_set_tooltips(buf);
	} else {
		newpix = get_applet_icon(GAIM_GNOME_ONLINE_ICON);
		g_snprintf(buf, sizeof buf, "Online: ");
		while (c) {
			strcat(buf, ((struct gaim_connection *)c->data)->username);
			c = g_slist_next(c);
			if (c)
				strcat(buf, ", ");
		}
		applet_set_tooltips(buf);
	}

	if (newpix) {
		gtk_pixmap_set(GTK_PIXMAP(icon), newpix, NULL);
		gdk_pixmap_unref(newpix);
	}

	return TRUE;
}

#ifdef HAVE_PANEL_PIXEL_SIZE
static void applet_change_pixel_size(GtkWidget *w, int size, gpointer data)
{
	sizehint = size;
	gtk_widget_set_usize(icon, sizehint, sizehint);
	update_applet();
}
#endif

extern GtkWidget *mainwindow;
void applet_show_login(AppletWidget *widget, gpointer data)
{
	show_login();
	if (blist_options & OPT_BLIST_NEAR_APPLET) {
		GtkAllocation a = get_applet_pos(FALSE);
		gtk_widget_set_uposition(mainwindow, a.x, a.y);
	}
}

void applet_do_signon(AppletWidget *widget, gpointer data)
{
	applet_show_login(NULL, 0);
}

void insert_applet_away()
{
	GSList *awy = away_messages;
	struct away_message *a;
	char *awayname;

	applet_widget_register_callback_dir(APPLET_WIDGET(applet), "away/", _("Away"));
	applet_widget_register_callback(APPLET_WIDGET(applet),
					"away/new",
					_("New Away Message"),
					(AppletCallbackFunc)create_away_mess, NULL);

	while (awy) {
		a = (struct away_message *)awy->data;

		awayname = g_malloc(sizeof *awayname * (6 + strlen(a->name)));
		awayname[0] = '\0';
		strcat(awayname, "away/");
		strcat(awayname, a->name);
		applet_widget_register_callback(APPLET_WIDGET(applet),
						awayname,
						a->name, (AppletCallbackFunc)do_away_message, a);

		awy = g_slist_next(awy);
		g_free(awayname);
	}
}

void remove_applet_away()
{
	GSList *awy = away_messages;
	struct away_message *a;
	char *awayname;

	if (!applet)
		return;

	applet_widget_unregister_callback(APPLET_WIDGET(applet), "away/new");

	while (awy) {
		a = (struct away_message *)awy->data;

		awayname = g_malloc(sizeof *awayname * (6 + strlen(a->name)));
		awayname[0] = '\0';
		strcat(awayname, "away/");
		strcat(awayname, a->name);
		applet_widget_unregister_callback(APPLET_WIDGET(applet), awayname);

		awy = g_slist_next(awy);
		free(awayname);
	}
	applet_widget_unregister_callback_dir(APPLET_WIDGET(applet), "away/");
	applet_widget_unregister_callback(APPLET_WIDGET(applet), "away");
}

static GtkAllocation get_applet_pos(gboolean for_blist)
{
	gint x, y, pad;
	GtkRequisition buddy_req, applet_req;
	GtkAllocation result;
	GNOME_Panel_OrientType orient = applet_widget_get_panel_orient(APPLET_WIDGET(applet));
	pad = 5;

	gdk_window_get_position(gtk_widget_get_parent_window(icon), &x, &y);
	if (for_blist) {
		if (blist_options & OPT_BLIST_SAVED_WINDOWS) {
			buddy_req.width = blist_pos.width;
			buddy_req.height = blist_pos.height;
		} else {
			buddy_req = blist->requisition;
		}
	} else {
		buddy_req = mainwindow->requisition;
	}
	applet_req = icon->requisition;

	/* FIXME : we need to be smarter here */
	switch (orient) {
	case ORIENT_UP:
		result.x = x;
		result.y = y - (buddy_req.height + pad);
		break;
	case ORIENT_DOWN:
		result.x = x;
		result.y = y + applet_req.height + pad;
		break;
	case ORIENT_LEFT:
		result.x = x - (buddy_req.width + pad);
		result.y = y;
		break;
	case ORIENT_RIGHT:
		result.x = x + applet_req.width + pad;
		result.y = y;
		break;
	}
	return result;
}

void createOnlinePopup()
{
	GtkAllocation al;
	if (blist)
		gtk_widget_show(blist);
	al = get_applet_pos(TRUE);
	if (blist_options & OPT_BLIST_NEAR_APPLET)
		gtk_widget_set_uposition(blist, al.x, al.y);
	else if (blist_options & OPT_BLIST_SAVED_WINDOWS)
		gtk_widget_set_uposition(blist, blist_pos.x - blist_pos.xoff,
					 blist_pos.y - blist_pos.yoff);
}

void AppletClicked(GtkWidget *sender, GdkEventButton *ev, gpointer data)
{
	if (!ev || ev->button != 1 || ev->type != GDK_BUTTON_PRESS)
		return;

	if (applet_buddy_show) {
		applet_buddy_show = FALSE;
		if (!connections && mainwindow)
			gtk_widget_hide(mainwindow);
		else
			gtk_widget_hide(blist);
	} else {
		applet_buddy_show = TRUE;
		if (!connections)
			applet_show_login(APPLET_WIDGET(applet), NULL);
		else
			createOnlinePopup();
	}
}


/***************************************************************
**
** Initialize GNOME stuff
**
****************************************************************/

gint init_applet_mgr(int argc, char *argv[])
{
	GdkPixmap *pm;

	applet_widget_init("GAIM", VERSION, argc, argv, NULL, 0, NULL);

	applet = applet_widget_new("gaim_applet");
	if (!applet)
		g_error(_("Can't create GAIM applet!"));
	gtk_widget_set_events(applet, gtk_widget_get_events(applet) | GDK_BUTTON_PRESS_MASK);
	gtk_widget_realize(applet);

	pm = get_applet_icon(GAIM_GNOME_OFFLINE_ICON);
	if (!pm)
		pm = gdk_pixmap_create_from_xpm_d(applet->window, NULL,
				&applet->style->bg[GTK_WIDGET_STATE(applet)], aimicon_xpm);
	icon = gtk_pixmap_new(pm, NULL);
	gtk_widget_set_usize(icon, sizehint, sizehint);
	gdk_pixmap_unref(pm);
	applet_widget_add(APPLET_WIDGET(applet), icon);

	applet_widget_register_stock_callback(APPLET_WIDGET(applet),
					      "about",
					      GNOME_STOCK_MENU_ABOUT,
					      _("About..."), (AppletCallbackFunc)show_about, NULL);

	gtk_signal_connect(GTK_OBJECT(applet), "button_press_event", GTK_SIGNAL_FUNC(AppletClicked),
			   NULL);

	gtk_signal_connect(GTK_OBJECT(applet), "destroy", GTK_SIGNAL_FUNC(do_quit), NULL);

#ifdef HAVE_PANEL_PIXEL_SIZE
	gtk_signal_connect(GTK_OBJECT(applet), "change_pixel_size",
			   GTK_SIGNAL_FUNC(applet_change_pixel_size), NULL);
#endif

	gtk_widget_show(icon);
	gtk_widget_show(applet);
	return 0;
}

void set_user_state(enum gaim_user_states state)
{
	if (state == signing_on)
		connecting++;
	else if ((state == away || state == online) && connecting > 0)
		connecting--;
	update_applet();
}

void applet_set_tooltips(char *msg)
{
	if (!applet)
		return;
	applet_widget_set_tooltip(APPLET_WIDGET(applet), msg);
}

#endif /*USE_APPLET */
