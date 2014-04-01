/*
 * PidginWebViewToolbar
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * under the terms of the GNU General Public License as published by
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
 *
 */

#ifndef _PIDGINWEBVIEWTOOLBAR_H_
#define _PIDGINWEBVIEWTOOLBAR_H_
/**
 * SECTION:gtkwebviewtoolbar
 * @section_id: pidgin-gtkwebviewtoolbar
 * @short_description: <filename>gtkwebviewtoolbar.h</filename>
 * @title: WebView Toolbar
 */

#include <gtk/gtk.h>
#include "gtkwebview.h"

#define PIDGIN_DEFAULT_FONT_FACE "Helvetica 12"

#define PIDGIN_TYPE_WEBVIEWTOOLBAR            (pidgin_webviewtoolbar_get_type())
#define PIDGIN_WEBVIEWTOOLBAR(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), PIDGIN_TYPE_WEBVIEWTOOLBAR, PidginWebViewToolbar))
#define PIDGIN_WEBVIEWTOOLBAR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PIDGIN_TYPE_WEBVIEWTOOLBAR, PidginWebViewToolbarClass))
#define PIDGIN_IS_WEBVIEWTOOLBAR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), PIDGIN_TYPE_WEBVIEWTOOLBAR))
#define PIDGIN_IS_WEBVIEWTOOLBAR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PIDGIN_TYPE_WEBVIEWTOOLBAR))
#define PIDGIN_WEBVIEWTOOLBAR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), PIDGIN_TYPE_WEBVIEWTOOLBAR, PidginWebViewToolbarClass))

typedef struct _PidginWebViewToolbar PidginWebViewToolbar;
typedef struct _PidginWebViewToolbarClass PidginWebViewToolbarClass;

struct _PidginWebViewToolbar {
	GtkHBox box;

	GtkWidget *webview;
};

struct _PidginWebViewToolbarClass {
	GtkHBoxClass parent_class;
};

G_BEGIN_DECLS

/**
 * pidgin_webviewtoolbar_get_type:
 *
 * Returns the GType for a PidginWebViewToolbar widget
 *
 * Returns: The GType for PidginWebViewToolbar widget
 */
GType pidgin_webviewtoolbar_get_type(void);

/**
 * pidgin_webviewtoolbar_new:
 *
 * Create a new PidginWebViewToolbar object
 *
 * Returns: A GtkWidget corresponding to the PidginWebViewToolbar object
 */
GtkWidget *pidgin_webviewtoolbar_new(void);

/**
 * pidgin_webviewtoolbar_attach:
 * @toolbar: The PidginWebViewToolbar object
 * @webview: The PidginWebView object
 *
 * Attach a PidginWebViewToolbar object to a PidginWebView
 */
void pidgin_webviewtoolbar_attach(PidginWebViewToolbar *toolbar, GtkWidget *webview);

/**
 * pidgin_webviewtoolbar_switch_active_conversation:
 * @toolbar: The PidginWebViewToolbar object
 * @conv:    The new conversation
 *
 * Switch the active conversation for a PidginWebViewToolbar object
 */
void pidgin_webviewtoolbar_switch_active_conversation(PidginWebViewToolbar *toolbar,
                                                   PurpleConversation *conv);

/**
 * pidgin_webviewtoolbar_activate:
 * @toolbar: The PidginWebViewToolbar object
 * @action:  The PidginWebViewAction
 *
 * Activate a PidginWebViewToolbar action
 */
void pidgin_webviewtoolbar_activate(PidginWebViewToolbar *toolbar,
                                 PidginWebViewAction action);

G_END_DECLS

#endif /* _PIDGINWEBVIEWTOOLBAR_H_ */

