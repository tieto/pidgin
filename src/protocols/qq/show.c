/**
* The QQ2003C protocol plugin
 *
 * for gaim
 *
 * Copyright (C) 2004 Puzzlebird
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
 * This code is based on qq_show.c, kindly contributed by herryouly@linuxsir
 */

// START OF FILE
/*****************************************************************************/
#include <sys/types.h>		// mkdir
#include <sys/stat.h>

#include "internal.h"		// _("get_text")
#include "debug.h"		// gaim_debug
#include "notify.h"		// gaim_notify_error
#include "util.h"		// gaim_url_fetch

#include "show.h"

#define QQ_SHOW_SERVER          "http://qqshow-user.tencent.com"
#define QQ_SHOW_IMAGE           "10/00/00.gif"

#define QQ_SHOW_CACHE_DIR       "qqshow"
#define QQ_SHOW_DEFAULT_IMAGE   "qqshow_default.gif"

#define QQ_SHOW_DEST_X          0
#define QQ_SHOW_DEST_Y          0
#define QQ_SHOW_DEST_WIDTH      120
#define QQ_SHOW_DEST_HEIGHT     180
#define QQ_SHOW_OFFSET_X        -10
#define QQ_SHOW_OFFSET_Y        -35
#define QQ_SHOW_SCALE_X         0.68
#define QQ_SHOW_SCALE_Y         0.68

enum {
	QQ_SHOW_READ,
	QQ_SHOW_WRITE,
};

/*****************************************************************************/
// return the local cached QQ show file name for uid
static gchar *_qq_show_get_cache_name(guint32 uid, gint io)
{
	gchar *path, *file, *file_fullname;

	g_return_val_if_fail(uid > 0, NULL);

	path = g_build_filename(gaim_user_dir(), QQ_SHOW_CACHE_DIR, NULL);
	if (!g_file_test(path, G_FILE_TEST_EXISTS))
		g_mkdir(path, 0700);

	file = g_strdup_printf("%d.gif", uid);
	file_fullname = g_build_filename(path, file, NULL);

	if (io == QQ_SHOW_READ) {
		if (!g_file_test(file_fullname, G_FILE_TEST_EXISTS)) {
			gaim_debug(GAIM_DEBUG_WARNING, "QQ", "No cached QQ show image for buddy %d\n", uid);
			g_free(file_fullname);
			file_fullname =
			    g_build_filename(gaim_prefs_get_string
					     ("/plugins/prpl/qq/datadir"),
					     "pixmaps", "gaim", "status", "default", QQ_SHOW_DEFAULT_IMAGE, NULL);
		}		// if g_file_test
	}			// if io

	g_free(path);
	g_free(file);
	return file_fullname;

}				// _qq_show_get_cache_name

/*****************************************************************************/
// scale the image to suit my window
static GdkPixbuf *_qq_show_scale_image(GdkPixbuf * pixbuf_src)
{
	GdkPixbuf *pixbuf_dst;

	pixbuf_dst = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8,
				    QQ_SHOW_DEST_WIDTH * QQ_SHOW_SCALE_X, QQ_SHOW_DEST_HEIGHT * QQ_SHOW_SCALE_Y);

	gdk_pixbuf_scale(pixbuf_src, pixbuf_dst,
			 QQ_SHOW_DEST_X * QQ_SHOW_SCALE_X,
			 QQ_SHOW_DEST_Y * QQ_SHOW_SCALE_Y,
			 QQ_SHOW_DEST_WIDTH * QQ_SHOW_SCALE_X,
			 QQ_SHOW_DEST_HEIGHT * QQ_SHOW_SCALE_Y,
			 QQ_SHOW_OFFSET_X * QQ_SHOW_SCALE_X,
			 QQ_SHOW_OFFSET_Y * QQ_SHOW_SCALE_Y, QQ_SHOW_SCALE_X, QQ_SHOW_SCALE_Y, GDK_INTERP_BILINEAR);

	g_object_unref(G_OBJECT(pixbuf_src));
	return pixbuf_dst;
}				// qq_show_scale_image

/*****************************************************************************/
// keep a local copy of QQ show image to speed up loading
static void _qq_show_cache_image(const gchar * url_ret, size_t len, guint32 uid)
{

	GError *err;
	GIOChannel *cache;
	gchar *file;

	g_return_if_fail(uid > 0);

	err = NULL;
	file = _qq_show_get_cache_name(uid, QQ_SHOW_WRITE);
	cache = g_io_channel_new_file(file, "w", &err);

	if (err != NULL) {	// file error
		gaim_debug(GAIM_DEBUG_WARNING, "QQ", "Error with QQ show file: %s\n", err->message);
		g_error_free(err);
		return;
	} else {		// OK, go ahead
		g_io_channel_set_encoding(cache, NULL, NULL);	// binary mode
		g_io_channel_write_chars(cache, url_ret, len, NULL, &err);
		if (err != NULL) {
			gaim_debug(GAIM_DEBUG_WARNING, "QQ", "Fail cache QQ show for %d: %s\n", uid, err->message);
			g_error_free(err);
		} else
			gaim_debug(GAIM_DEBUG_INFO, "QQ", "Cache QQ show for %d, done\n", uid);
		g_io_channel_shutdown(cache, TRUE, NULL);
	}			// if err

}				// _qq_show_cache_image

/*****************************************************************************/
// display the image for downloaded data
static void qq_show_render_image(void *data, const gchar * url_ret, size_t len)
{
	guint32 uid;
	GdkPixbufLoader *pixbuf_loader;
	GdkPixbuf *pixbuf_src, *pixbuf_dst;
	GtkWidget *qq_show;

	g_return_if_fail(data != NULL && url_ret != NULL && len > 0);

	qq_show = (GtkWidget *) data;

	pixbuf_loader = gdk_pixbuf_loader_new();
	gdk_pixbuf_loader_write(pixbuf_loader, url_ret, len, NULL);
	gdk_pixbuf_loader_close(pixbuf_loader, NULL);	// finish loading

	pixbuf_src = gdk_pixbuf_loader_get_pixbuf(pixbuf_loader);

	if (pixbuf_src != NULL) {
		uid = (guint32) g_object_get_data(G_OBJECT(qq_show), "user_data");
		_qq_show_cache_image(url_ret, len, uid);
		pixbuf_dst = _qq_show_scale_image(pixbuf_src);
		gtk_image_set_from_pixbuf(GTK_IMAGE(qq_show), pixbuf_dst);
	} else {
		gaim_notify_error(NULL, NULL, _("Fail getting QQ show image"), NULL);
	}			// if pixbuf_src

}				// qq_show_render_image

/*****************************************************************************/
// show the default image (either local cache or default image)
GtkWidget *qq_show_default(contact_info * info)
{
	guint32 uid;
	GdkPixbuf *pixbuf_src;
	GError *err;
	gchar *file;

	g_return_val_if_fail(info != NULL, NULL);

	uid = strtol(info->uid, NULL, 10);
	g_return_val_if_fail(uid != 0, NULL);

	file = _qq_show_get_cache_name(uid, QQ_SHOW_READ);
	gaim_debug(GAIM_DEBUG_INFO, "QQ", "Load QQ show image: %s\n", file);

	err = NULL;
	pixbuf_src = gdk_pixbuf_new_from_file(file, &err);

	if (err != NULL) {
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Fail loaing QQ show: %s\n", err->message);
		g_free(file);
		return NULL;
	}			// if err

	g_free(file);
	return gtk_image_new_from_pixbuf(_qq_show_scale_image(pixbuf_src));
}				// qq_show_default

/*****************************************************************************/
// refresh the image
void qq_show_get_image(GtkWidget * event_box, GdkEventButton * event, gpointer data)
{
	gchar *url;
	gint uid;
	GtkWidget *qq_show;

	qq_show = (GtkWidget *) data;
	g_return_if_fail(qq_show != NULL);

	uid = (gint) g_object_get_data(G_OBJECT(qq_show), "user_data");
	g_return_if_fail(uid != 0);

	url = g_strdup_printf("%s/%d/%s", QQ_SHOW_SERVER, uid, QQ_SHOW_IMAGE);
	gaim_url_fetch(url, FALSE, NULL, TRUE, qq_show_render_image, qq_show);

	g_free(url);
}				// qq_show_get_image

/*****************************************************************************/
