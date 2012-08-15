/*
 * @file gtkstatusbox.c GTK+ Status Selection Widget
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

/*
 * The status box is made up of two main pieces:
 *   - The box that displays the current status, which is made
 *     of a GtkListStore ("status_box->store") and GtkCellView
 *     ("status_box->cell_view").  There is always exactly 1 row
 *     in this list store.  Only the TYPE_ICON and TYPE_TEXT
 *     columns are used in this list store.
 *   - The dropdown menu that lets users select a status, which
 *     is made of a GtkComboBox ("status_box") and GtkListStore
 *     ("status_box->dropdown_store").  This dropdown is shown
 *     when the user clicks on the box that displays the current
 *     status.  This list store contains one row for Available,
 *     one row for Away, etc., a few rows for popular statuses,
 *     and the "New..." and "Saved..." options.
 */

#include <gdk/gdkkeysyms.h>

#include "internal.h"

#include "account.h"
#include "buddyicon.h"
#include "core.h"
#include "imgstore.h"
#include "network.h"
#include "request.h"
#include "savedstatuses.h"
#include "status.h"
#include "debug.h"

#include "pidgin.h"
#include "gtksavedstatuses.h"
#include "pidginstock.h"
#include "gtkstatusbox.h"
#include "gtkutils.h"

#ifdef USE_GTKSPELL
#  include <gtkspell/gtkspell.h>
#  ifdef _WIN32
#    include "wspell.h"
#  endif
#endif

#include "gtk3compat.h"

/* Timeout for typing notifications in seconds */
#define TYPING_TIMEOUT 4

static void webview_changed_cb(GtkWebView *webview, void *data);
static void webview_format_changed_cb(GtkWebView *webview, GtkWebViewButtons buttons, void *data);
static void remove_typing_cb(PidginStatusBox *box);
static void update_size (PidginStatusBox *box);
static gint get_statusbox_index(PidginStatusBox *box, PurpleSavedStatus *saved_status);
static PurpleAccount* check_active_accounts_for_identical_statuses(void);

static void pidgin_status_box_pulse_typing(PidginStatusBox *status_box);
static void pidgin_status_box_refresh(PidginStatusBox *status_box);
static void status_menu_refresh_iter(PidginStatusBox *status_box, gboolean status_changed);
static void pidgin_status_box_regenerate(PidginStatusBox *status_box, gboolean status_changed);
static void pidgin_status_box_changed(PidginStatusBox *box);
#if GTK_CHECK_VERSION(3,0,0)
static void pidgin_status_box_get_preferred_height (GtkWidget *widget,
	gint *minimum_height, gint *natural_height);
static gboolean pidgin_status_box_draw (GtkWidget *widget, cairo_t *cr);
#else
static void pidgin_status_box_size_request (GtkWidget *widget, GtkRequisition *requisition);
static gboolean pidgin_status_box_expose_event (GtkWidget *widget, GdkEventExpose *event);
#endif
static void pidgin_status_box_size_allocate (GtkWidget *widget, GtkAllocation *allocation);
static void pidgin_status_box_redisplay_buddy_icon(PidginStatusBox *status_box);
static void pidgin_status_box_forall (GtkContainer *container, gboolean include_internals, GtkCallback callback, gpointer callback_data);
static void pidgin_status_box_popup(PidginStatusBox *box, GdkEvent *event);
static void pidgin_status_box_popdown(PidginStatusBox *box, GdkEvent *event);

static void do_colorshift (GdkPixbuf *dest, GdkPixbuf *src, int shift);
static void icon_choose_cb(const char *filename, gpointer data);
static void remove_buddy_icon_cb(GtkWidget *w, PidginStatusBox *box);
static void choose_buddy_icon_cb(GtkWidget *w, PidginStatusBox *box);

enum {
	/** A PidginStatusBoxItemType */
	TYPE_COLUMN,

	/** This is the stock-id for the icon. */
	ICON_STOCK_COLUMN,

	/**
	 * This is a GdkPixbuf (the other columns are strings).
	 * This column is visible.
	 */
	ICON_COLUMN,

	/** The text displayed on the status box.  This column is visible. */
	TEXT_COLUMN,

	/** The plain-English title of this item */
	TITLE_COLUMN,

	/** A plain-English description of this item */
	DESC_COLUMN,

	/**
	 * This value depends on TYPE_COLUMN.  For POPULAR types,
	 * this is the creation time.  For PRIMITIVE types,
	 * this is the PurpleStatusPrimitive.
	 */
	DATA_COLUMN,

	/**
 	 * This column stores the GdkPixbuf for the status emblem. Currently only 'saved' is stored.
	 * In the GtkTreeModel for the dropdown, this is the stock-id (gchararray), and for the
	 * GtkTreeModel for the cell_view (for the account-specific statusbox), this is the prpl-icon
	 * (GdkPixbuf) of the account.
 	 */
	EMBLEM_COLUMN,

	/**
 	* This column stores whether to show the emblem.
 	*/
	EMBLEM_VISIBLE_COLUMN,

	NUM_COLUMNS
};

enum {
	PROP_0,
	PROP_ACCOUNT,
	PROP_ICON_SEL,
};

static char *typing_stock_ids[7] = {
	PIDGIN_STOCK_ANIMATION_TYPING0,
	PIDGIN_STOCK_ANIMATION_TYPING1,
	PIDGIN_STOCK_ANIMATION_TYPING2,
	PIDGIN_STOCK_ANIMATION_TYPING3,
	PIDGIN_STOCK_ANIMATION_TYPING4,
	NULL
};

static char *connecting_stock_ids[] = {
	PIDGIN_STOCK_ANIMATION_CONNECT0,
	PIDGIN_STOCK_ANIMATION_CONNECT1,
	PIDGIN_STOCK_ANIMATION_CONNECT2,
	PIDGIN_STOCK_ANIMATION_CONNECT3,
	PIDGIN_STOCK_ANIMATION_CONNECT4,
	PIDGIN_STOCK_ANIMATION_CONNECT5,
	PIDGIN_STOCK_ANIMATION_CONNECT6,
	PIDGIN_STOCK_ANIMATION_CONNECT7,
	PIDGIN_STOCK_ANIMATION_CONNECT8,
	PIDGIN_STOCK_ANIMATION_CONNECT9,
	PIDGIN_STOCK_ANIMATION_CONNECT10,
	PIDGIN_STOCK_ANIMATION_CONNECT11,
	PIDGIN_STOCK_ANIMATION_CONNECT12,
	PIDGIN_STOCK_ANIMATION_CONNECT13,
	PIDGIN_STOCK_ANIMATION_CONNECT14,
	PIDGIN_STOCK_ANIMATION_CONNECT15,
	PIDGIN_STOCK_ANIMATION_CONNECT16,
	PIDGIN_STOCK_ANIMATION_CONNECT17,
	PIDGIN_STOCK_ANIMATION_CONNECT18,
	PIDGIN_STOCK_ANIMATION_CONNECT19,
	PIDGIN_STOCK_ANIMATION_CONNECT20,
	PIDGIN_STOCK_ANIMATION_CONNECT21,
	PIDGIN_STOCK_ANIMATION_CONNECT22,
	PIDGIN_STOCK_ANIMATION_CONNECT23,
	PIDGIN_STOCK_ANIMATION_CONNECT24,
	PIDGIN_STOCK_ANIMATION_CONNECT25,
	PIDGIN_STOCK_ANIMATION_CONNECT26,
	PIDGIN_STOCK_ANIMATION_CONNECT27,
	PIDGIN_STOCK_ANIMATION_CONNECT28,
	PIDGIN_STOCK_ANIMATION_CONNECT29,
	PIDGIN_STOCK_ANIMATION_CONNECT30,
	NULL
};

static GtkContainerClass *parent_class = NULL;

static void pidgin_status_box_class_init (PidginStatusBoxClass *klass);
static void pidgin_status_box_init (PidginStatusBox *status_box);

GType
pidgin_status_box_get_type (void)
{
	static GType status_box_type = 0;

	if (!status_box_type)
	{
		static const GTypeInfo status_box_info =
		{
			sizeof (PidginStatusBoxClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) pidgin_status_box_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (PidginStatusBox),
			0,
			(GInstanceInitFunc) pidgin_status_box_init,
			NULL  /* value_table */
		};

		status_box_type = g_type_register_static(GTK_TYPE_CONTAINER,
												 "PidginStatusBox",
												 &status_box_info,
												 0);
	}

	return status_box_type;
}

static void
pidgin_status_box_get_property(GObject *object, guint param_id,
                                 GValue *value, GParamSpec *psec)
{
	PidginStatusBox *statusbox = PIDGIN_STATUS_BOX(object);

	switch (param_id) {
	case PROP_ACCOUNT:
		g_value_set_pointer(value, statusbox->account);
		break;
	case PROP_ICON_SEL:
		g_value_set_boolean(value, statusbox->icon_box != NULL);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, psec);
		break;
	}
}

static void
update_to_reflect_account_status(PidginStatusBox *status_box, PurpleAccount *account, PurpleStatus *newstatus)
{
	GList *l;
	int status_no = -1;
	const PurpleStatusType *statustype = NULL;
	const char *message;

	statustype = purple_status_type_find_with_id((GList *)purple_account_get_status_types(account),
	                                           (char *)purple_status_type_get_id(purple_status_get_type(newstatus)));

	for (l = purple_account_get_status_types(account); l != NULL; l = l->next) {
		PurpleStatusType *status_type = (PurpleStatusType *)l->data;

		if (!purple_status_type_is_user_settable(status_type) ||
				purple_status_type_is_independent(status_type))
			continue;
		status_no++;
		if (statustype == status_type)
			break;
	}

#if 0
	/* TODO WebKit: Doesn't do this? */
	gtk_webview_set_populate_primary_clipboard(
		GTK_WEBVIEW(status_box->webview), TRUE);
#endif

	if (status_no != -1) {
		GtkTreePath *path;
		gtk_widget_set_sensitive(GTK_WIDGET(status_box), FALSE);
		path = gtk_tree_path_new_from_indices(status_no, -1);
		if (status_box->active_row)
			gtk_tree_row_reference_free(status_box->active_row);
		status_box->active_row = gtk_tree_row_reference_new(GTK_TREE_MODEL(status_box->dropdown_store), path);
		gtk_tree_path_free(path);

		message = purple_status_get_attr_string(newstatus, "message");

		if (!message || !*message)
		{
			gtk_widget_hide(status_box->vbox);
			status_box->webview_visible = FALSE;
		}
		else
		{
			gtk_widget_show_all(status_box->vbox);
			status_box->webview_visible = TRUE;
			gtk_webview_load_html_string(GTK_WEBVIEW(status_box->webview), message);
		}
		gtk_widget_set_sensitive(GTK_WIDGET(status_box), TRUE);
		pidgin_status_box_refresh(status_box);
	}
}

static void
account_status_changed_cb(PurpleAccount *account, PurpleStatus *oldstatus, PurpleStatus *newstatus, PidginStatusBox *status_box)
{
	if (status_box->account == account)
		update_to_reflect_account_status(status_box, account, newstatus);
	else if (status_box->token_status_account == account)
		status_menu_refresh_iter(status_box, TRUE);
}

static gboolean
icon_box_press_cb(GtkWidget *widget, GdkEventButton *event, PidginStatusBox *box)
{
	if (event->button == 3) {
		GtkWidget *menu_item;
		const char *path;

		if (box->icon_box_menu)
			gtk_widget_destroy(box->icon_box_menu);

		box->icon_box_menu = gtk_menu_new();

		menu_item = pidgin_new_item_from_stock(box->icon_box_menu, _("Select Buddy Icon"), GTK_STOCK_ADD,
						     G_CALLBACK(choose_buddy_icon_cb),
						     box, 0, 0, NULL);

		menu_item = pidgin_new_item_from_stock(box->icon_box_menu, _("Remove"), GTK_STOCK_REMOVE,
						     G_CALLBACK(remove_buddy_icon_cb),
						     box, 0, 0, NULL);
		if (!(path = purple_prefs_get_path(PIDGIN_PREFS_ROOT "/accounts/buddyicon"))
				|| !*path)
			gtk_widget_set_sensitive(menu_item, FALSE);

		gtk_menu_popup(GTK_MENU(box->icon_box_menu), NULL, NULL, NULL, NULL,
			       event->button, event->time);

	} else {
		choose_buddy_icon_cb(widget, box);
	}
	return FALSE;
}

static void
icon_box_dnd_cb(GtkWidget *widget, GdkDragContext *dc, gint x, gint y,
		GtkSelectionData *sd, guint info, guint t, PidginStatusBox *box)
{
	gchar *name = (gchar *) gtk_selection_data_get_data(sd);

	if ((gtk_selection_data_get_length(sd) >= 0)
	  && (gtk_selection_data_get_format(sd) == 8)) {
		/* Well, it looks like the drag event was cool.
		 * Let's do something with it */
		if (!g_ascii_strncasecmp(name, "file://", 7)) {
			GError *converr = NULL;
			gchar *tmp, *rtmp;

			if(!(tmp = g_filename_from_uri(name, NULL, &converr))) {
				purple_debug(PURPLE_DEBUG_ERROR, "buddyicon", "%s\n",
					   (converr ? converr->message :
					    "g_filename_from_uri error"));
				return;
			}
			if ((rtmp = strchr(tmp, '\r')) || (rtmp = strchr(tmp, '\n')))
				*rtmp = '\0';
			icon_choose_cb(tmp, box);
			g_free(tmp);
		}
		gtk_drag_finish(dc, TRUE, FALSE, t);
	}
	gtk_drag_finish(dc, FALSE, FALSE, t);
}

static void
statusbox_got_url(PurpleUtilFetchUrlData *url_data, gpointer user_data,
                  const gchar *themedata, size_t len, const gchar *error_message)
{
	FILE *f;
	gchar *path;
	size_t wc;

	if ((error_message != NULL) || (len == 0))
		return;

	f = purple_mkstemp(&path, TRUE);
	wc = fwrite(themedata, len, 1, f);
	if (wc != 1) {
		purple_debug_warning("theme_got_url", "Unable to write theme data.\n");
		fclose(f);
		g_unlink(path);
		g_free(path);
		return;
	}
	fclose(f);

	icon_choose_cb(path, user_data);

	g_unlink(path);
	g_free(path);
}


static gboolean
statusbox_uri_handler(const char *proto, const char *cmd, GHashTable *params, void *data)
{
	const char *src;

	if (g_ascii_strcasecmp(proto, "aim"))
		return FALSE;

	if (g_ascii_strcasecmp(cmd, "buddyicon"))
		return FALSE;

	src = g_hash_table_lookup(params, "account");
	if (src == NULL)
		return FALSE;

	purple_util_fetch_url(src, TRUE, NULL, FALSE, -1, statusbox_got_url, data);
	return TRUE;
}

static gboolean
icon_box_enter_cb(GtkWidget *widget, GdkEventCrossing *event, PidginStatusBox *box)
{
	gdk_window_set_cursor(gtk_widget_get_window(widget), box->hand_cursor);
	gtk_image_set_from_pixbuf(GTK_IMAGE(box->icon), box->buddy_icon_hover);
	return FALSE;
}

static gboolean
icon_box_leave_cb(GtkWidget *widget, GdkEventCrossing *event, PidginStatusBox *box)
{
	gdk_window_set_cursor(gtk_widget_get_window(widget), box->arrow_cursor);
	gtk_image_set_from_pixbuf(GTK_IMAGE(box->icon), box->buddy_icon) ;
	return FALSE;
}


static const GtkTargetEntry dnd_targets[] = {
	{"text/plain", 0, 0},
	{"text/uri-list", 0, 1},
	{"STRING", 0, 2}
};

static void
setup_icon_box(PidginStatusBox *status_box)
{
	if (status_box->icon_box != NULL)
		return;

	status_box->icon = gtk_image_new();
	status_box->icon_box = gtk_event_box_new();
	gtk_widget_set_parent(status_box->icon_box, GTK_WIDGET(status_box));
	gtk_widget_show(status_box->icon_box);

#if GTK_CHECK_VERSION(2,12,0)
	gtk_widget_set_tooltip_text(status_box->icon_box,
			status_box->account ? _("Click to change your buddyicon for this account.") :
				_("Click to change your buddyicon for all accounts."));
#endif

	if (status_box->account &&
		!purple_account_get_bool(status_box->account, "use-global-buddyicon", TRUE))
	{
		PurpleStoredImage *img = purple_buddy_icons_find_account_icon(status_box->account);
		pidgin_status_box_set_buddy_icon(status_box, img);
		purple_imgstore_unref(img);
	}
	else
	{
		const char *filename = purple_prefs_get_path(PIDGIN_PREFS_ROOT "/accounts/buddyicon");
		PurpleStoredImage *img = NULL;

		if (filename && *filename)
			img = purple_imgstore_new_from_file(filename);

		pidgin_status_box_set_buddy_icon(status_box, img);
		if (img)
			/*
			 * purple_imgstore_new gives us a reference and
			 * pidgin_status_box_set_buddy_icon also takes one.
			 */
			purple_imgstore_unref(img);
	}

	status_box->hand_cursor = gdk_cursor_new (GDK_HAND2);
	status_box->arrow_cursor = gdk_cursor_new (GDK_LEFT_PTR);

	/* Set up DND */
	gtk_drag_dest_set(status_box->icon_box,
			  GTK_DEST_DEFAULT_MOTION |
			  GTK_DEST_DEFAULT_DROP,
			  dnd_targets,
			  sizeof(dnd_targets) / sizeof(GtkTargetEntry),
			  GDK_ACTION_COPY);

	g_signal_connect(G_OBJECT(status_box->icon_box), "drag_data_received", G_CALLBACK(icon_box_dnd_cb), status_box);
	g_signal_connect(G_OBJECT(status_box->icon_box), "enter-notify-event", G_CALLBACK(icon_box_enter_cb), status_box);
	g_signal_connect(G_OBJECT(status_box->icon_box), "leave-notify-event", G_CALLBACK(icon_box_leave_cb), status_box);
	g_signal_connect(G_OBJECT(status_box->icon_box), "button-press-event", G_CALLBACK(icon_box_press_cb), status_box);

	gtk_container_add(GTK_CONTAINER(status_box->icon_box), status_box->icon);
	gtk_widget_show(status_box->icon);
}

static void
destroy_icon_box(PidginStatusBox *statusbox)
{
	if (statusbox->icon_box == NULL)
		return;

	gtk_widget_destroy(statusbox->icon_box);
	gdk_cursor_unref(statusbox->hand_cursor);
	gdk_cursor_unref(statusbox->arrow_cursor);

	purple_imgstore_unref(statusbox->buddy_icon_img);

	g_object_unref(G_OBJECT(statusbox->buddy_icon));
	g_object_unref(G_OBJECT(statusbox->buddy_icon_hover));

	if (statusbox->buddy_icon_sel)
		gtk_widget_destroy(statusbox->buddy_icon_sel);

	if (statusbox->icon_box_menu)
		gtk_widget_destroy(statusbox->icon_box_menu);

	statusbox->icon = NULL;
	statusbox->icon_box = NULL;
	statusbox->icon_box_menu = NULL;
	statusbox->buddy_icon_img = NULL;
	statusbox->buddy_icon = NULL;
	statusbox->buddy_icon_hover = NULL;
	statusbox->hand_cursor = NULL;
	statusbox->arrow_cursor = NULL;
}

static void
pidgin_status_box_set_property(GObject *object, guint param_id,
                               const GValue *value, GParamSpec *pspec)
{
	PidginStatusBox *statusbox = PIDGIN_STATUS_BOX(object);

	switch (param_id) {
	case PROP_ICON_SEL:
		if (g_value_get_boolean(value)) {
			if (statusbox->account) {
				PurplePlugin *plug = purple_plugins_find_with_id(purple_account_get_protocol_id(statusbox->account));
				if (plug) {
					PurplePluginProtocolInfo *prplinfo = PURPLE_PLUGIN_PROTOCOL_INFO(plug);
					if (prplinfo && prplinfo->icon_spec.format != NULL)
						setup_icon_box(statusbox);
				}
			} else {
				setup_icon_box(statusbox);
			}
		} else {
			destroy_icon_box(statusbox);
		}
		break;
	case PROP_ACCOUNT:
		statusbox->account = g_value_get_pointer(value);
		if (statusbox->account)
			statusbox->token_status_account = NULL;
		else
			statusbox->token_status_account = check_active_accounts_for_identical_statuses();

		pidgin_status_box_regenerate(statusbox, TRUE);

		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, pspec);
		break;
	}
}

static void
pidgin_status_box_finalize(GObject *obj)
{
	PidginStatusBox *statusbox = PIDGIN_STATUS_BOX(obj);
	int i;

	purple_signals_disconnect_by_handle(statusbox);
	purple_prefs_disconnect_by_handle(statusbox);

	destroy_icon_box(statusbox);

	if (statusbox->active_row)
		gtk_tree_row_reference_free(statusbox->active_row);

	for (i = 0; i < G_N_ELEMENTS(statusbox->connecting_pixbufs); i++) {
		if (statusbox->connecting_pixbufs[i] != NULL)
			g_object_unref(G_OBJECT(statusbox->connecting_pixbufs[i]));
	}

	for (i = 0; i < G_N_ELEMENTS(statusbox->typing_pixbufs); i++) {
		if (statusbox->typing_pixbufs[i] != NULL)
			g_object_unref(G_OBJECT(statusbox->typing_pixbufs[i]));
	}

	g_object_unref(G_OBJECT(statusbox->store));
	g_object_unref(G_OBJECT(statusbox->dropdown_store));
	G_OBJECT_CLASS(parent_class)->finalize(obj);
}

static GType
pidgin_status_box_child_type (GtkContainer *container)
{
	return GTK_TYPE_WIDGET;
}

static void
pidgin_status_box_class_init (PidginStatusBoxClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkContainerClass *container_class = (GtkContainerClass*)klass;

	parent_class = g_type_class_peek_parent(klass);

	widget_class = (GtkWidgetClass*)klass;
#if GTK_CHECK_VERSION(3,0,0)
	widget_class->get_preferred_height = pidgin_status_box_get_preferred_height;
	widget_class->draw = pidgin_status_box_draw;
#else
	widget_class->size_request = pidgin_status_box_size_request;
	widget_class->expose_event = pidgin_status_box_expose_event;
#endif
	widget_class->size_allocate = pidgin_status_box_size_allocate;

	container_class->child_type = pidgin_status_box_child_type;
	container_class->forall = pidgin_status_box_forall;
	container_class->remove = NULL;

	object_class = (GObjectClass *)klass;

	object_class->finalize = pidgin_status_box_finalize;

	object_class->get_property = pidgin_status_box_get_property;
	object_class->set_property = pidgin_status_box_set_property;

	g_object_class_install_property(object_class,
	                                PROP_ACCOUNT,
	                                g_param_spec_pointer("account",
	                                                     "Account",
	                                                     "The account, or NULL for all accounts",
	                                                      G_PARAM_READWRITE
	                                                     )
	                               );
	g_object_class_install_property(object_class,
	                                PROP_ICON_SEL,
	                                g_param_spec_boolean("iconsel",
	                                                     "Icon Selector",
	                                                     "Whether the icon selector should be displayed or not.",
														 FALSE,
	                                                      G_PARAM_READWRITE
	                                                     )
	                               );
}

/**
 * This updates the text displayed on the status box so that it shows
 * the current status.  This is the only function in this file that
 * should modify status_box->store
 */
static void
pidgin_status_box_refresh(PidginStatusBox *status_box)
{
	GtkStyle *style;
	char aa_color[8];
	PurpleSavedStatus *saved_status;
	char *primary, *secondary, *text;
	const char *stock = NULL;
	GdkPixbuf *emblem = NULL;
	GtkTreePath *path;
	gboolean account_status = FALSE;
	PurpleAccount *acct = (status_box->account) ? status_box->account : status_box->token_status_account;

	style = gtk_widget_get_style(GTK_WIDGET(status_box));
	snprintf(aa_color, sizeof(aa_color), "#%02x%02x%02x",
		 style->text_aa[GTK_STATE_NORMAL].red >> 8,
		 style->text_aa[GTK_STATE_NORMAL].green >> 8,
		 style->text_aa[GTK_STATE_NORMAL].blue >> 8);

	saved_status = purple_savedstatus_get_current();

	if (status_box->account || (status_box->token_status_account
			&& purple_savedstatus_is_transient(saved_status)))
		account_status = TRUE;

	/* Primary */
	if (status_box->typing != 0)
	{
		GtkTreeIter iter;
		PidginStatusBoxItemType type;
		gpointer data;

		/* Primary (get the status selected in the dropdown) */
		path = gtk_tree_row_reference_get_path(status_box->active_row);
		if (!gtk_tree_model_get_iter (GTK_TREE_MODEL(status_box->dropdown_store), &iter, path))
			return;
		gtk_tree_path_free(path);

		gtk_tree_model_get(GTK_TREE_MODEL(status_box->dropdown_store), &iter,
						   TYPE_COLUMN, &type,
						   DATA_COLUMN, &data,
						   -1);
		if (type == PIDGIN_STATUS_BOX_TYPE_PRIMITIVE)
			primary = g_strdup(purple_primitive_get_name_from_type(GPOINTER_TO_INT(data)));
		else
			/* This should never happen, but just in case... */
			primary = g_strdup("New status");
	}
	else if (account_status)
		primary = g_strdup(purple_status_get_name(purple_account_get_active_status(acct)));
	else if (purple_savedstatus_is_transient(saved_status))
		primary = g_strdup(purple_primitive_get_name_from_type(purple_savedstatus_get_type(saved_status)));
	else
		primary = g_markup_escape_text(purple_savedstatus_get_title(saved_status), -1);

	/* Secondary */
	if (status_box->typing != 0)
		secondary = g_strdup(_("Typing"));
	else if (status_box->connecting)
		secondary = g_strdup(_("Connecting"));
	else if (!status_box->network_available)
		secondary = g_strdup(_("Waiting for network connection"));
	else if (purple_savedstatus_is_transient(saved_status))
		secondary = NULL;
	else
	{
		const char *message;
		char *tmp;
		message = purple_savedstatus_get_message(saved_status);
		if (message != NULL)
		{
			tmp = purple_markup_strip_html(message);
			purple_util_chrreplace(tmp, '\n', ' ');
			secondary = g_markup_escape_text(tmp, -1);
			g_free(tmp);
		}
		else
			secondary = NULL;
	}

	/* Pixbuf */
	if (status_box->typing != 0)
		stock = typing_stock_ids[status_box->typing_index];
	else if (status_box->connecting)
		stock = connecting_stock_ids[status_box->connecting_index];
	else
	  {
	    PurpleStatusType *status_type;
	    PurpleStatusPrimitive prim;
	    if (account_status) {
			status_type = purple_status_get_type(purple_account_get_active_status(acct));
	        prim = purple_status_type_get_primitive(status_type);
	    } else {
			prim = purple_savedstatus_get_type(saved_status);
	    }

		stock = pidgin_stock_id_from_status_primitive(prim);
	}

	if (status_box->account != NULL) {
		text = g_strdup_printf("%s - <span size=\"smaller\" color=\"%s\">%s</span>",
				       purple_account_get_username(status_box->account),
				       aa_color, secondary ? secondary : primary);
		emblem = pidgin_create_prpl_icon(status_box->account, PIDGIN_PRPL_ICON_SMALL);
	} else if (secondary != NULL) {
		text = g_strdup_printf("%s<span size=\"smaller\" color=\"%s\"> - %s</span>",
				       primary, aa_color, secondary);
	} else {
		text = g_strdup(primary);
	}
	g_free(primary);
	g_free(secondary);

	/*
	 * Only two columns are used in this list store (does it
	 * really need to be a list store?)
	 */
	gtk_list_store_set(status_box->store, &(status_box->iter),
			   ICON_STOCK_COLUMN, (gpointer)stock,
			   TEXT_COLUMN, text,
			   EMBLEM_COLUMN, emblem,
			   EMBLEM_VISIBLE_COLUMN, (emblem != NULL),
			   -1);
	g_free(text);
	if (emblem)
		g_object_unref(emblem);

	/* Make sure to activate the only row in the tree view */
	path = gtk_tree_path_new_from_string("0");
	gtk_cell_view_set_displayed_row(GTK_CELL_VIEW(status_box->cell_view), path);
	gtk_tree_path_free(path);

	update_size(status_box);
}

static PurpleStatusType *
find_status_type_by_index(const PurpleAccount *account, gint active)
{
	GList *l = purple_account_get_status_types(account);
	gint i;

	for (i = 0; l; l = l->next) {
		PurpleStatusType *status_type = l->data;
		if (!purple_status_type_is_user_settable(status_type) ||
				purple_status_type_is_independent(status_type))
			continue;

		if (active == i)
			return status_type;
		i++;
	}

	return NULL;
}

/**
 * This updates the GtkTreeView so that it correctly shows the state
 * we are currently using.  It is used when the current state is
 * updated from somewhere other than the GtkStatusBox (from a plugin,
 * or when signing on with the "-n" option, for example).  It is
 * also used when the user selects the "New..." option.
 *
 * Maybe we could accomplish this by triggering off the mouse and
 * keyboard signals instead of the changed signal?
 */
static void
status_menu_refresh_iter(PidginStatusBox *status_box, gboolean status_changed)
{
	PurpleSavedStatus *saved_status;
	PurpleStatusPrimitive primitive;
	gint index;
	const char *message;
	GtkTreePath *path = NULL;

	/* this function is inappropriate for ones with accounts */
	if (status_box->account)
		return;

	saved_status = purple_savedstatus_get_current();

	/*
	 * Suppress the "changed" signal because the status
	 * was changed programmatically.
	 */
	gtk_widget_set_sensitive(GTK_WIDGET(status_box), FALSE);

	/*
	 * If there is a token-account, then select the primitive from the
	 * dropdown using a loop. Otherwise select from the default list.
	 */
	primitive = purple_savedstatus_get_type(saved_status);
	if (!status_box->token_status_account && purple_savedstatus_is_transient(saved_status) &&
		((primitive == PURPLE_STATUS_AVAILABLE) || (primitive == PURPLE_STATUS_AWAY) ||
		 (primitive == PURPLE_STATUS_INVISIBLE) || (primitive == PURPLE_STATUS_OFFLINE) ||
		 (primitive == PURPLE_STATUS_UNAVAILABLE)) &&
		(!purple_savedstatus_has_substatuses(saved_status)))
	{
		index = get_statusbox_index(status_box, saved_status);
		path = gtk_tree_path_new_from_indices(index, -1);
	}
	else
	{
		GtkTreeIter iter;
		PidginStatusBoxItemType type;
		gpointer data;

		/* If this saved status is in the list store, then set it as the active item */
		if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(status_box->dropdown_store), &iter))
		{
			do
			{
				gtk_tree_model_get(GTK_TREE_MODEL(status_box->dropdown_store), &iter,
							TYPE_COLUMN, &type,
							DATA_COLUMN, &data,
							-1);

				/* This is a special case because Primitives for the token_status_account are actually
				 * saved statuses with substatuses for the enabled accounts */
				if (status_box->token_status_account && purple_savedstatus_is_transient(saved_status)
					&& type == PIDGIN_STATUS_BOX_TYPE_PRIMITIVE && primitive == GPOINTER_TO_INT(data))
				{
					char *name;
					const char *acct_status_name = purple_status_get_name(
						purple_account_get_active_status(status_box->token_status_account));

					gtk_tree_model_get(GTK_TREE_MODEL(status_box->dropdown_store), &iter,
							TEXT_COLUMN, &name, -1);

					if (!purple_savedstatus_has_substatuses(saved_status)
						|| !strcmp(name, acct_status_name))
					{
						/* Found! */
						path = gtk_tree_model_get_path(GTK_TREE_MODEL(status_box->dropdown_store), &iter);
						g_free(name);
						break;
					}
					g_free(name);

				} else if ((type == PIDGIN_STATUS_BOX_TYPE_POPULAR) &&
						(GPOINTER_TO_INT(data) == purple_savedstatus_get_creation_time(saved_status)))
				{
					/* Found! */
					path = gtk_tree_model_get_path(GTK_TREE_MODEL(status_box->dropdown_store), &iter);
					break;
				}
			} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(status_box->dropdown_store), &iter));
		}
	}

	if (status_box->active_row)
		gtk_tree_row_reference_free(status_box->active_row);
	if (path) {   /* path should never be NULL */
		status_box->active_row = gtk_tree_row_reference_new(GTK_TREE_MODEL(status_box->dropdown_store), path);
		gtk_tree_path_free(path);
	} else
		status_box->active_row = NULL;

	if (status_changed) {
		message = purple_savedstatus_get_message(saved_status);

		/*
		 * If we are going to hide the webview, don't retain the
		 * message because showing the old message later is
		 * confusing. If we are going to set the message to a pre-set,
		 * then we need to do this anyway
		 *
		 * Suppress the "changed" signal because the status
		 * was changed programmatically.
		 */
		gtk_widget_set_sensitive(GTK_WIDGET(status_box->webview), FALSE);

		gtk_webview_load_html_string(GTK_WEBVIEW(status_box->webview), "");
		gtk_webview_clear_formatting(GTK_WEBVIEW(status_box->webview));

		if (!purple_savedstatus_is_transient(saved_status) || !message || !*message)
		{
			status_box->webview_visible = FALSE;
			gtk_widget_hide(status_box->vbox);
		}
		else
		{
			status_box->webview_visible = TRUE;
			gtk_widget_show_all(status_box->vbox);

			gtk_webview_load_html_string(GTK_WEBVIEW(status_box->webview), message);
		}

		gtk_widget_set_sensitive(GTK_WIDGET(status_box->webview), TRUE);
		update_size(status_box);
	}

	/* Stop suppressing the "changed" signal. */
	gtk_widget_set_sensitive(GTK_WIDGET(status_box), TRUE);
}

static void
add_popular_statuses(PidginStatusBox *statusbox)
{
	GList *list, *cur;

	list = purple_savedstatuses_get_popular(6);
	if (list == NULL)
		/* Odd... oh well, nothing we can do about it. */
		return;

	pidgin_status_box_add_separator(statusbox);

	for (cur = list; cur != NULL; cur = cur->next)
	{
		PurpleSavedStatus *saved = cur->data;
		const gchar *message;
		gchar *stripped = NULL;
		PidginStatusBoxItemType type;

		if (purple_savedstatus_is_transient(saved))
		{
			/*
			 * Transient statuses do not have a title, so the savedstatus
			 * API returns the message when purple_savedstatus_get_title() is
			 * called, so we don't need to get the message a second time.
			 */
			type = PIDGIN_STATUS_BOX_TYPE_POPULAR;
		}
		else
		{
			type = PIDGIN_STATUS_BOX_TYPE_SAVED_POPULAR;

			message = purple_savedstatus_get_message(saved);
			if (message != NULL)
			{
				stripped = purple_markup_strip_html(message);
				purple_util_chrreplace(stripped, '\n', ' ');
			}
		}

		pidgin_status_box_add(statusbox, type,
				NULL, purple_savedstatus_get_title(saved), stripped,
				GINT_TO_POINTER(purple_savedstatus_get_creation_time(saved)));
		g_free(stripped);
	}

	g_list_free(list);
}

/* This returns NULL if the active accounts don't have identical
 * statuses and a token account if they do */
static PurpleAccount* check_active_accounts_for_identical_statuses(void)
{
	GList *iter, *active_accts = purple_accounts_get_all_active();
	PurpleAccount *acct1 = NULL;
	const char *prpl1 = NULL;

	if (active_accts) {
		acct1 = active_accts->data;
		prpl1 = purple_account_get_protocol_id(acct1);
	} else {
		/* there's no enabled account */
		return NULL;
	}

	/* start at the second account */
	for (iter = active_accts->next; iter; iter = iter->next) {
		PurpleAccount *acct2 = iter->data;
		GList *s1, *s2;

		if (!g_str_equal(prpl1, purple_account_get_protocol_id(acct2))) {
			acct1 = NULL;
			break;
		}

		for (s1 = purple_account_get_status_types(acct1),
				 s2 = purple_account_get_status_types(acct2); s1 && s2;
			 s1 = s1->next, s2 = s2->next) {
			PurpleStatusType *st1 = s1->data, *st2 = s2->data;
			/* TODO: Are these enough to consider the statuses identical? */
			if (purple_status_type_get_primitive(st1) != purple_status_type_get_primitive(st2)
				|| strcmp(purple_status_type_get_id(st1), purple_status_type_get_id(st2))
				|| strcmp(purple_status_type_get_name(st1), purple_status_type_get_name(st2))) {
				acct1 = NULL;
				break;
			}
		}

		if (s1 != s2) {/* Will both be NULL if matched */
			acct1 = NULL;
			break;
		}
	}

	g_list_free(active_accts);

	return acct1;
}

static void
add_account_statuses(PidginStatusBox *status_box, PurpleAccount *account)
{
	/* Per-account */
	GList *l;

	for (l = purple_account_get_status_types(account); l != NULL; l = l->next)
	{
		PurpleStatusType *status_type = (PurpleStatusType *)l->data;
		PurpleStatusPrimitive prim;

		if (!purple_status_type_is_user_settable(status_type) ||
				purple_status_type_is_independent(status_type))
			continue;

		prim = purple_status_type_get_primitive(status_type);

		pidgin_status_box_add(PIDGIN_STATUS_BOX(status_box),
					PIDGIN_STATUS_BOX_TYPE_PRIMITIVE, NULL,
					purple_status_type_get_name(status_type),
					NULL,
					GINT_TO_POINTER(prim));
	}
}

static void
pidgin_status_box_regenerate(PidginStatusBox *status_box, gboolean status_changed)
{
	/* Unset the model while clearing it */
	gtk_tree_view_set_model(GTK_TREE_VIEW(status_box->tree_view), NULL);
	gtk_list_store_clear(status_box->dropdown_store);
	/* Don't set the model until the new statuses have been added to the box.
	 * What is presumably a bug in Gtk < 2.4 causes things to get all confused
	 * if we do this here. */
	/* gtk_combo_box_set_model(GTK_COMBO_BOX(status_box), GTK_TREE_MODEL(status_box->dropdown_store)); */

	if (status_box->account == NULL)
	{
		/* Do all the currently enabled accounts have the same statuses?
		 * If so, display them instead of our global list.
		 */
		if (status_box->token_status_account) {
			add_account_statuses(status_box, status_box->token_status_account);
		} else {
			/* Global */
			pidgin_status_box_add(PIDGIN_STATUS_BOX(status_box), PIDGIN_STATUS_BOX_TYPE_PRIMITIVE, NULL, _("Available"), NULL, GINT_TO_POINTER(PURPLE_STATUS_AVAILABLE));
			pidgin_status_box_add(PIDGIN_STATUS_BOX(status_box), PIDGIN_STATUS_BOX_TYPE_PRIMITIVE, NULL, _("Away"), NULL, GINT_TO_POINTER(PURPLE_STATUS_AWAY));
			pidgin_status_box_add(PIDGIN_STATUS_BOX(status_box), PIDGIN_STATUS_BOX_TYPE_PRIMITIVE, NULL, _("Do not disturb"), NULL, GINT_TO_POINTER(PURPLE_STATUS_UNAVAILABLE));
			pidgin_status_box_add(PIDGIN_STATUS_BOX(status_box), PIDGIN_STATUS_BOX_TYPE_PRIMITIVE, NULL, _("Invisible"), NULL, GINT_TO_POINTER(PURPLE_STATUS_INVISIBLE));
			pidgin_status_box_add(PIDGIN_STATUS_BOX(status_box), PIDGIN_STATUS_BOX_TYPE_PRIMITIVE, NULL, _("Offline"), NULL, GINT_TO_POINTER(PURPLE_STATUS_OFFLINE));
		}

		add_popular_statuses(status_box);

		pidgin_status_box_add_separator(PIDGIN_STATUS_BOX(status_box));
		pidgin_status_box_add(PIDGIN_STATUS_BOX(status_box), PIDGIN_STATUS_BOX_TYPE_CUSTOM, NULL, _("New status..."), NULL, NULL);
		pidgin_status_box_add(PIDGIN_STATUS_BOX(status_box), PIDGIN_STATUS_BOX_TYPE_SAVED, NULL, _("Saved statuses..."), NULL, NULL);

		status_menu_refresh_iter(status_box, status_changed);
		pidgin_status_box_refresh(status_box);

	} else {
		add_account_statuses(status_box, status_box->account);
		update_to_reflect_account_status(status_box, status_box->account,
			purple_account_get_active_status(status_box->account));
	}
	gtk_tree_view_set_model(GTK_TREE_VIEW(status_box->tree_view), GTK_TREE_MODEL(status_box->dropdown_store));
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(status_box->tree_view), TEXT_COLUMN);
}

static gboolean
combo_box_scroll_event_cb(GtkWidget *w, GdkEventScroll *event, GtkWebView *webview)
{
	pidgin_status_box_popup(PIDGIN_STATUS_BOX(w), (GdkEvent *)event);
	return TRUE;
}

static gboolean
webview_scroll_event_cb(GtkWidget *w, GdkEventScroll *event, GtkWebView *webview)
{
	if (event->direction == GDK_SCROLL_UP)
		gtk_webview_page_up(webview);
	else if (event->direction == GDK_SCROLL_DOWN)
		gtk_webview_page_down(webview);
	return TRUE;
}

static gboolean
webview_remove_focus(GtkWidget *w, GdkEventKey *event, PidginStatusBox *status_box)
{
	if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
		remove_typing_cb(status_box);
		return TRUE;
	}
	else if (event->keyval == GDK_KEY_Tab || event->keyval == GDK_KEY_KP_Tab || event->keyval == GDK_KEY_ISO_Left_Tab)
	{
		/* If last inserted character is a tab, then remove the focus from here */
		GtkWidget *top = gtk_widget_get_toplevel(w);
		g_signal_emit_by_name(G_OBJECT(top), "move-focus",
				(event->state & GDK_SHIFT_MASK) ?
				                  GTK_DIR_TAB_BACKWARD: GTK_DIR_TAB_FORWARD);
		return TRUE;
	}
	if (status_box->typing == 0)
		return FALSE;

	/* Reset the status if Escape was pressed */
	if (event->keyval == GDK_KEY_Escape)
	{
		purple_timeout_remove(status_box->typing);
		status_box->typing = 0;
#if 0
	/* TODO WebKit: Doesn't do this? */
		gtk_webview_set_populate_primary_clipboard(
			GTK_WEBVIEW(status_box->webview), TRUE);
#endif
		if (status_box->account != NULL)
			update_to_reflect_account_status(status_box, status_box->account,
							purple_account_get_active_status(status_box->account));
		else {
			status_menu_refresh_iter(status_box, TRUE);
			pidgin_status_box_refresh(status_box);
		}
		return TRUE;
	}

	pidgin_status_box_pulse_typing(status_box);
	purple_timeout_remove(status_box->typing);
	status_box->typing = purple_timeout_add_seconds(TYPING_TIMEOUT, (GSourceFunc)remove_typing_cb, status_box);

	return FALSE;
}

static gboolean
dropdown_store_row_separator_func(GtkTreeModel *model,
								  GtkTreeIter *iter, gpointer data)
{
	PidginStatusBoxItemType type;

	gtk_tree_model_get(model, iter, TYPE_COLUMN, &type, -1);

	if (type == PIDGIN_STATUS_BOX_TYPE_SEPARATOR)
		return TRUE;

	return FALSE;
}

static void
cache_pixbufs(PidginStatusBox *status_box)
{
	GtkIconSize icon_size;
	int i;

	g_object_set(G_OBJECT(status_box->icon_rend), "xpad", 3, NULL);
	icon_size = gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL);

	for (i = 0; i < G_N_ELEMENTS(status_box->connecting_pixbufs); i++) {
		if (status_box->connecting_pixbufs[i] != NULL)
			g_object_unref(G_OBJECT(status_box->connecting_pixbufs[i]));
		if (connecting_stock_ids[i])
			status_box->connecting_pixbufs[i] = gtk_widget_render_icon (GTK_WIDGET(status_box->vbox),
					connecting_stock_ids[i], icon_size, "PidginStatusBox");
		else
			status_box->connecting_pixbufs[i] = NULL;
	}
	status_box->connecting_index = 0;


	for (i = 0; i < G_N_ELEMENTS(status_box->typing_pixbufs); i++) {
		if (status_box->typing_pixbufs[i] != NULL)
			g_object_unref(G_OBJECT(status_box->typing_pixbufs[i]));
		if (typing_stock_ids[i])
			status_box->typing_pixbufs[i] =  gtk_widget_render_icon (GTK_WIDGET(status_box->vbox),
					typing_stock_ids[i], icon_size, "PidginStatusBox");
		else
			status_box->typing_pixbufs[i] = NULL;
	}
	status_box->typing_index = 0;
}

static void account_enabled_cb(PurpleAccount *acct, PidginStatusBox *status_box)
{
	PurpleAccount *initial_token_acct = status_box->token_status_account;

	if (status_box->account)
		return;

	status_box->token_status_account = check_active_accounts_for_identical_statuses();

	/* Regenerate the list if it has changed */
	if (initial_token_acct != status_box->token_status_account) {
		pidgin_status_box_regenerate(status_box, TRUE);
	}

}

static void
current_savedstatus_changed_cb(PurpleSavedStatus *now, PurpleSavedStatus *old, PidginStatusBox *status_box)
{
	/* Make sure our current status is added to the list of popular statuses */
	pidgin_status_box_regenerate(status_box, TRUE);
}

static void
saved_status_updated_cb(PurpleSavedStatus *status, PidginStatusBox *status_box)
{
	pidgin_status_box_regenerate(status_box,
		purple_savedstatus_get_current() == status);
}

static void
spellcheck_prefs_cb(const char *name, PurplePrefType type,
					gconstpointer value, gpointer data)
{
	PidginStatusBox *status_box = (PidginStatusBox *)data;

	pidgin_webview_set_spellcheck(GTK_WEBVIEW(status_box->webview),
	                              (gboolean)GPOINTER_TO_INT(value));
}

#if 0
static gboolean button_released_cb(GtkWidget *widget, GdkEventButton *event, PidginStatusBox *box)
{

	if (event->button != 1)
		return FALSE;
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(box->toggle_button), FALSE);
	if (!box->webview_visible)
		g_signal_emit_by_name(G_OBJECT(box), "changed", NULL, NULL);
	return TRUE;
}

static gboolean button_pressed_cb(GtkWidget *widget, GdkEventButton *event, PidginStatusBox *box)
{
	if (event->button != 1)
		return FALSE;
	gtk_combo_box_popup(GTK_COMBO_BOX(box));
	/* Disabled until button_released_cb works */
#if 0
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(box->toggle_button), TRUE);
#endif
	return TRUE;
}
#endif

static void
pidgin_status_box_list_position (PidginStatusBox *status_box, int *x, int *y, int *width, int *height)
{
	GdkScreen *screen;
	gint monitor_num;
	GdkRectangle monitor;
	GtkRequisition popup_req;
	GtkPolicyType hpolicy, vpolicy;
	GtkAllocation allocation;

	gtk_widget_get_allocation(GTK_WIDGET(status_box), &allocation);
	gdk_window_get_origin(gtk_widget_get_window(GTK_WIDGET(status_box)), x, y);

	*x += allocation.x;
	*y += allocation.y;

	*width = allocation.width;

	hpolicy = vpolicy = GTK_POLICY_NEVER;
	g_object_set(G_OBJECT(status_box->scrolled_window),
	             "hscrollbar-policy", hpolicy,
	             "vscrollbar-policy", vpolicy,
	             NULL);
	gtk_widget_size_request(status_box->popup_frame, &popup_req);

	if (popup_req.width > *width) {
		hpolicy = GTK_POLICY_ALWAYS;
		g_object_set(G_OBJECT(status_box->scrolled_window),
		             "hscrollbar-policy", hpolicy,
		             "vscrollbar-policy", vpolicy,
		             NULL);
		gtk_widget_size_request(status_box->popup_frame, &popup_req);
	}

	*height = popup_req.height;

	screen = gtk_widget_get_screen(GTK_WIDGET(status_box));
	monitor_num = gdk_screen_get_monitor_at_window(screen,
							gtk_widget_get_window(GTK_WIDGET(status_box)));
	gdk_screen_get_monitor_geometry(screen, monitor_num, &monitor);

	if (*x < monitor.x)
		*x = monitor.x;
	else if (*x + *width > monitor.x + monitor.width)
		*x = monitor.x + monitor.width - *width;

	if (*y + allocation.height + *height <= monitor.y + monitor.height)
		*y += allocation.height;
	else if (*y - *height >= monitor.y)
		*y -= *height;
	else if (monitor.y + monitor.height - (*y + allocation.height) > *y - monitor.y)
	{
		*y += allocation.height;
		*height = monitor.y + monitor.height - *y;
	}
	else
	{
		*height = *y - monitor.y;
		*y = monitor.y;
	}

	if (popup_req.height > *height)
	{
		vpolicy = GTK_POLICY_ALWAYS;

		g_object_set(G_OBJECT(status_box->scrolled_window),
		             "hscrollbar-policy", hpolicy,
		             "vscrollbar-policy", vpolicy,
		             NULL);
	}
}

static gboolean
popup_grab_on_window(GdkWindow *window, GdkEvent *event)
{
	guint32 activate_time = gdk_event_get_time(event);
#if GTK_CHECK_VERSION(3,0,0)
	GdkDevice *device = gdk_event_get_device(event);
	GdkGrabStatus status;

	status = gdk_device_grab(device, window, GDK_OWNERSHIP_WINDOW, TRUE,
	                         GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
	                         GDK_POINTER_MOTION_MASK | GDK_KEY_PRESS_MASK |
	                         GDK_KEY_RELEASE_MASK, NULL, activate_time);
	if (status == GDK_GRAB_SUCCESS) {
		status = gdk_device_grab(gdk_device_get_associated_device(device),
		                         window, GDK_OWNERSHIP_WINDOW, TRUE,
		                         GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
		                         GDK_POINTER_MOTION_MASK | GDK_KEY_PRESS_MASK |
		                         GDK_KEY_RELEASE_MASK, NULL, activate_time);
		if (status == GDK_GRAB_SUCCESS)
			return TRUE;
		else
			gdk_device_ungrab(device, activate_time);
	}

	return FALSE;
#else
	if ((gdk_pointer_grab(window, TRUE,
	                      GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
	                      GDK_POINTER_MOTION_MASK,
	                      NULL, NULL, activate_time) == 0))
	{
		if (gdk_keyboard_grab(window, TRUE, activate_time) == 0)
			return TRUE;
		else {
			gdk_display_pointer_ungrab(gdk_window_get_display(window),
			                           activate_time);
			return FALSE;
		}
	}

	return FALSE;
#endif
}


static void
pidgin_status_box_popup(PidginStatusBox *box, GdkEvent *event)
{
	int width, height, x, y;
	pidgin_status_box_list_position (box, &x, &y, &width, &height);

	gtk_widget_set_size_request (box->popup_window, width, height);
	gtk_window_move (GTK_WINDOW (box->popup_window), x, y);
	gtk_widget_show(box->popup_window);
	gtk_widget_grab_focus (box->tree_view);
	if (!popup_grab_on_window(gtk_widget_get_window(box->popup_window), event)) {
		gtk_widget_hide (box->popup_window);
		return;
	}
	gtk_grab_add (box->popup_window);
	/*box->popup_in_progress = TRUE;*/
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (box->toggle_button),
				      TRUE);

	if (box->active_row) {
		GtkTreePath *path = gtk_tree_row_reference_get_path(box->active_row);
		gtk_tree_view_set_cursor(GTK_TREE_VIEW(box->tree_view), path, NULL, FALSE);
		gtk_tree_path_free(path);
	}
}

static void
pidgin_status_box_popdown(PidginStatusBox *box, GdkEvent *event)
{
	guint32 time;
#if GTK_CHECK_VERSION(3,0,0)
	GdkDevice *device;
#endif
	gtk_widget_hide(box->popup_window);
	box->popup_in_progress = FALSE;
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(box->toggle_button), FALSE);
	gtk_grab_remove(box->popup_window);
	time = gdk_event_get_time(event);
#if GTK_CHECK_VERSION(3,0,0)
	device = gdk_event_get_device(event);
	gdk_device_ungrab(device, time);
	gdk_device_ungrab(gdk_device_get_associated_device(device), time);
#else
	gdk_pointer_ungrab(time);
	gdk_keyboard_ungrab(time);
#endif
}

static gboolean
toggle_key_press_cb(GtkWidget *widget, GdkEventKey *event, PidginStatusBox *box)
{
	switch (event->keyval) {
		case GDK_KEY_Return:
		case GDK_KEY_KP_Enter:
		case GDK_KEY_KP_Space:
		case GDK_KEY_space:
			if (!box->popup_in_progress) {
				pidgin_status_box_popup(box, (GdkEvent *)event);
				box->popup_in_progress = TRUE;
			} else {
				pidgin_status_box_popdown(box, (GdkEvent *)event);
			}
			return TRUE;
		default:
			return FALSE;
	}
}

static gboolean
toggled_cb(GtkWidget *widget, GdkEventButton *event, PidginStatusBox *box)
{
	if (!box->popup_in_progress)
		pidgin_status_box_popup(box, (GdkEvent *)event);
	else
		pidgin_status_box_popdown(box, (GdkEvent *)event);
	return TRUE;
}

static void
buddy_icon_set_cb(const char *filename, PidginStatusBox *box)
{
	PurpleStoredImage *img = NULL;

	if (box->account) {
		PurplePlugin *plug = purple_find_prpl(purple_account_get_protocol_id(box->account));
		if (plug) {
			PurplePluginProtocolInfo *prplinfo = PURPLE_PLUGIN_PROTOCOL_INFO(plug);
			if (prplinfo && prplinfo->icon_spec.format) {
				gpointer data = NULL;
				size_t len = 0;
				if (filename)
					data = pidgin_convert_buddy_icon(plug, filename, &len);
				img = purple_buddy_icons_set_account_icon(box->account, data, len);
				if (img)
					/*
					 * set_account_icon doesn't give us a reference, but we
					 * unref one below (for the other code path)
					 */
					purple_imgstore_ref(img);

				purple_account_set_buddy_icon_path(box->account, filename);

				purple_account_set_bool(box->account, "use-global-buddyicon", (filename != NULL));
			}
		}
	} else {
		GList *accounts;
		for (accounts = purple_accounts_get_all(); accounts != NULL; accounts = accounts->next) {
			PurpleAccount *account = accounts->data;
			PurplePlugin *plug = purple_find_prpl(purple_account_get_protocol_id(account));
			if (plug) {
				PurplePluginProtocolInfo *prplinfo = PURPLE_PLUGIN_PROTOCOL_INFO(plug);
				if (prplinfo != NULL &&
				    purple_account_get_bool(account, "use-global-buddyicon", TRUE) &&
				    prplinfo->icon_spec.format) {
					gpointer data = NULL;
					size_t len = 0;
					if (filename)
						data = pidgin_convert_buddy_icon(plug, filename, &len);
					purple_buddy_icons_set_account_icon(account, data, len);
					purple_account_set_buddy_icon_path(account, filename);
				}
			}
		}

		/* Even if no accounts were processed, load the icon that was set. */
		if (filename != NULL)
			img = purple_imgstore_new_from_file(filename);
	}

	pidgin_status_box_set_buddy_icon(box, img);
	if (img)
		purple_imgstore_unref(img);
}

static void
remove_buddy_icon_cb(GtkWidget *w, PidginStatusBox *box)
{
	if (box->account == NULL)
		/* The pref-connect callback does the actual work */
		purple_prefs_set_path(PIDGIN_PREFS_ROOT "/accounts/buddyicon", NULL);
	else
		buddy_icon_set_cb(NULL, box);

	gtk_widget_destroy(box->icon_box_menu);
	box->icon_box_menu = NULL;
}

static void
choose_buddy_icon_cb(GtkWidget *w, PidginStatusBox *box)
{
	if (box->buddy_icon_sel) {
		gtk_window_present(GTK_WINDOW(box->buddy_icon_sel));
	} else {
		box->buddy_icon_sel = pidgin_buddy_icon_chooser_new(GTK_WINDOW(gtk_widget_get_toplevel(w)), icon_choose_cb, box);
		gtk_widget_show_all(box->buddy_icon_sel);
	}
}

static void
icon_choose_cb(const char *filename, gpointer data)
{
	PidginStatusBox *box = data;
	if (filename) {
		if (box->account == NULL)
			/* The pref-connect callback does the actual work */
			purple_prefs_set_path(PIDGIN_PREFS_ROOT "/accounts/buddyicon", filename);
		else
			buddy_icon_set_cb(filename, box);
	}

	box->buddy_icon_sel = NULL;
}

static void
update_buddyicon_cb(const char *name, PurplePrefType type,
		    gconstpointer value, gpointer data)
{
	buddy_icon_set_cb(value, (PidginStatusBox*) data);
}

static void
treeview_activate_current_selection(PidginStatusBox *status_box, GtkTreePath *path, GdkEvent *event)
{
	if (status_box->active_row)
		gtk_tree_row_reference_free(status_box->active_row);

	status_box->active_row = gtk_tree_row_reference_new(GTK_TREE_MODEL(status_box->dropdown_store), path);
	pidgin_status_box_popdown(status_box, event);
	pidgin_status_box_changed(status_box);
}

static void tree_view_delete_current_selection_cb(gpointer data)
{
	PurpleSavedStatus *saved;

	saved = purple_savedstatus_find_by_creation_time(GPOINTER_TO_INT(data));
	g_return_if_fail(saved != NULL);

	if (purple_savedstatus_get_current() != saved)
		purple_savedstatus_delete_by_status(saved);
}

static void
tree_view_delete_current_selection(PidginStatusBox *status_box, GtkTreePath *path, GdkEvent *event)
{
	GtkTreeIter iter;
	gpointer data;
	PurpleSavedStatus *saved;
	gchar *msg;

	if (status_box->active_row) {
		/* don't delete active status */
		if (gtk_tree_path_compare(path, gtk_tree_row_reference_get_path(status_box->active_row)) == 0)
			return;
	}

	if (!gtk_tree_model_get_iter (GTK_TREE_MODEL(status_box->dropdown_store), &iter, path))
		return;

	gtk_tree_model_get(GTK_TREE_MODEL(status_box->dropdown_store), &iter,
			   DATA_COLUMN, &data,
			   -1);

	saved = purple_savedstatus_find_by_creation_time(GPOINTER_TO_INT(data));
	g_return_if_fail(saved != NULL);
	if (saved == purple_savedstatus_get_current())
		return;

	msg = g_strdup_printf(_("Are you sure you want to delete %s?"), purple_savedstatus_get_title(saved));

	purple_request_action(saved, NULL, msg, NULL, 0,
		NULL, NULL, NULL,
		data, 2,
		_("Delete"), tree_view_delete_current_selection_cb,
		_("Cancel"), NULL);

	g_free(msg);

	pidgin_status_box_popdown(status_box, event);
}

static gboolean
treeview_button_release_cb(GtkWidget *widget, GdkEventButton *event, PidginStatusBox *status_box)
{
	GtkTreePath *path = NULL;
	int ret;
	GtkWidget *ewidget = gtk_get_event_widget ((GdkEvent *)event);

	if (ewidget != status_box->tree_view) {
		if (ewidget == status_box->toggle_button &&
		    status_box->popup_in_progress &&
		    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (status_box->toggle_button))) {
			pidgin_status_box_popdown(status_box, (GdkEvent *)event);
			return TRUE;
		} else if (ewidget == status_box->toggle_button) {
			status_box->popup_in_progress = TRUE;
		}

		/* released outside treeview */
		if (ewidget != status_box->toggle_button) {
				pidgin_status_box_popdown(status_box, (GdkEvent *)event);
				return TRUE;
		}

		return FALSE;
	}

	ret = gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (status_box->tree_view),
					     event->x, event->y,
					     &path,
					     NULL, NULL, NULL);

	if (!ret)
		return TRUE; /* clicked outside window? */

	treeview_activate_current_selection(status_box, path, (GdkEvent *)event);
	gtk_tree_path_free (path);

	return TRUE;
}

static gboolean
treeview_key_press_event(GtkWidget *widget,
			GdkEventKey *event, PidginStatusBox *box)
{
	if (box->popup_in_progress) {
		if (event->keyval == GDK_KEY_Escape) {
			pidgin_status_box_popdown(box, (GdkEvent *)event);
			return TRUE;
		} else {
			GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(box->tree_view));
			GtkTreeIter iter;
			GtkTreePath *path;

			if (gtk_tree_selection_get_selected(sel, NULL, &iter)) {
				gboolean ret = TRUE;
				path = gtk_tree_model_get_path(GTK_TREE_MODEL(box->dropdown_store), &iter);
				if (event->keyval == GDK_KEY_Return) {
					treeview_activate_current_selection(box, path, (GdkEvent *)event);
				} else if (event->keyval == GDK_KEY_Delete) {
					tree_view_delete_current_selection(box, path, (GdkEvent *)event);
				} else
					ret = FALSE;

				gtk_tree_path_free (path);
				return ret;
			}
		}
	}
	return FALSE;
}

static void
webview_cursor_moved_cb(gpointer data, GtkWebView *webview)
{
	/* Restart the typing timeout if arrow keys are pressed while editing the message */
	PidginStatusBox *status_box = data;
	if (status_box->typing == 0)
		return;
	webview_changed_cb(NULL, status_box);
}

static void
treeview_cursor_changed_cb(GtkTreeView *treeview, gpointer data)
{
	GtkTreeSelection *sel = gtk_tree_view_get_selection (treeview);
	GtkTreeModel *model = GTK_TREE_MODEL (data);
	GtkTreeIter iter;
	GtkTreePath *cursor;
	GtkTreePath *selection;
	gint cmp;

	if (gtk_tree_selection_get_selected (sel, NULL, &iter)) {
		if ((selection = gtk_tree_model_get_path (model, &iter)) == NULL) {
			/* Shouldn't happen, but ignore anyway */
			return;
		}
	} else {
		/* I don't think this can happen, but we'll just ignore it */
		return;
	}

	gtk_tree_view_get_cursor (treeview, &cursor, NULL);
	if (cursor == NULL) {
		/* Probably won't happen in a 'cursor-changed' event? */
		gtk_tree_path_free (selection);
		return;
	}

	cmp = gtk_tree_path_compare (cursor, selection);
	if (cmp < 0) {
		/* The cursor moved up without moving the selection, so move it up again */
		gtk_tree_path_prev (cursor);
		gtk_tree_view_set_cursor (treeview, cursor, NULL, FALSE);
	} else if (cmp > 0) {
		/* The cursor moved down without moving the selection, so move it down again */
		gtk_tree_path_next (cursor);
		gtk_tree_view_set_cursor (treeview, cursor, NULL, FALSE);
	}

	gtk_tree_path_free (selection);
	gtk_tree_path_free (cursor);
}

static void
pidgin_status_box_init (PidginStatusBox *status_box)
{
	GtkCellRenderer *text_rend;
	GtkCellRenderer *icon_rend;
	GtkCellRenderer *emblem_rend;
	GtkWidget *toplevel;
	GtkTreeSelection *sel;

	gtk_widget_set_has_window(GTK_WIDGET(status_box), FALSE);
	status_box->webview_visible = FALSE;
	status_box->network_available = purple_network_is_available();
	status_box->connecting = FALSE;
	status_box->typing = 0;
	status_box->toggle_button = gtk_toggle_button_new();
	status_box->hbox = gtk_hbox_new(FALSE, 6);
	status_box->cell_view = gtk_cell_view_new();
	status_box->vsep = gtk_vseparator_new();
	status_box->arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);

	status_box->store = gtk_list_store_new(NUM_COLUMNS, G_TYPE_INT, G_TYPE_STRING, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING,
					       G_TYPE_STRING, G_TYPE_POINTER, GDK_TYPE_PIXBUF, G_TYPE_BOOLEAN);
	status_box->dropdown_store = gtk_list_store_new(NUM_COLUMNS, G_TYPE_INT, G_TYPE_STRING, GDK_TYPE_PIXBUF, G_TYPE_STRING,
							G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_STRING, G_TYPE_BOOLEAN);

	gtk_cell_view_set_model(GTK_CELL_VIEW(status_box->cell_view), GTK_TREE_MODEL(status_box->store));
	gtk_list_store_append(status_box->store, &(status_box->iter));

	atk_object_set_name(gtk_widget_get_accessible(status_box->toggle_button), _("Status Selector"));

	gtk_container_add(GTK_CONTAINER(status_box->toggle_button), status_box->hbox);
	gtk_box_pack_start(GTK_BOX(status_box->hbox), status_box->cell_view, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(status_box->hbox), status_box->vsep, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(status_box->hbox), status_box->arrow, FALSE, FALSE, 0);
	gtk_widget_show_all(status_box->toggle_button);
	gtk_button_set_focus_on_click(GTK_BUTTON(status_box->toggle_button), FALSE);

	text_rend = gtk_cell_renderer_text_new();
	icon_rend = gtk_cell_renderer_pixbuf_new();
	emblem_rend = gtk_cell_renderer_pixbuf_new();
	status_box->popup_window = gtk_window_new (GTK_WINDOW_POPUP);

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (status_box));
	if (GTK_IS_WINDOW (toplevel))  {
		gtk_window_set_transient_for (GTK_WINDOW (status_box->popup_window),
				GTK_WINDOW (toplevel));
	}

	gtk_window_set_resizable (GTK_WINDOW (status_box->popup_window), FALSE);
	gtk_window_set_type_hint (GTK_WINDOW (status_box->popup_window),
			GDK_WINDOW_TYPE_HINT_POPUP_MENU);
	gtk_window_set_screen (GTK_WINDOW (status_box->popup_window),
			gtk_widget_get_screen (GTK_WIDGET (status_box)));
	status_box->popup_frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (status_box->popup_frame),
			GTK_SHADOW_ETCHED_IN);
	gtk_container_add (GTK_CONTAINER (status_box->popup_window),
			status_box->popup_frame);

	gtk_widget_show (status_box->popup_frame);

	status_box->tree_view = gtk_tree_view_new ();
	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (status_box->tree_view));
	gtk_tree_selection_set_mode (sel, GTK_SELECTION_BROWSE);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (status_box->tree_view),
			FALSE);
	gtk_tree_view_set_hover_selection (GTK_TREE_VIEW (status_box->tree_view),
			TRUE);
	gtk_tree_view_set_model (GTK_TREE_VIEW (status_box->tree_view),
			GTK_TREE_MODEL(status_box->dropdown_store));
	status_box->column = gtk_tree_view_column_new ();
	gtk_tree_view_append_column (GTK_TREE_VIEW (status_box->tree_view),
			status_box->column);
	gtk_tree_view_column_pack_start(status_box->column, icon_rend, FALSE);
	gtk_tree_view_column_pack_start(status_box->column, text_rend, TRUE);
	gtk_tree_view_column_pack_start(status_box->column, emblem_rend, FALSE);
	gtk_tree_view_column_set_attributes(status_box->column, icon_rend, "stock-id", ICON_STOCK_COLUMN, NULL);
	gtk_tree_view_column_set_attributes(status_box->column, text_rend, "markup", TEXT_COLUMN, NULL);
	gtk_tree_view_column_set_attributes(status_box->column, emblem_rend, "stock-id", EMBLEM_COLUMN, "visible", EMBLEM_VISIBLE_COLUMN, NULL);

	status_box->scrolled_window = pidgin_make_scrollable(status_box->tree_view, GTK_POLICY_NEVER, GTK_POLICY_NEVER, GTK_SHADOW_NONE, -1, -1);
	gtk_container_add (GTK_CONTAINER (status_box->popup_frame),
			status_box->scrolled_window);

	gtk_widget_show(status_box->tree_view);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(status_box->tree_view), TEXT_COLUMN);
	gtk_tree_view_set_search_equal_func(GTK_TREE_VIEW(status_box->tree_view),
				pidgin_tree_view_search_equal_func, NULL, NULL);

	g_object_set(text_rend, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

	status_box->icon_rend = gtk_cell_renderer_pixbuf_new();
	status_box->text_rend = gtk_cell_renderer_text_new();
	emblem_rend = gtk_cell_renderer_pixbuf_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(status_box->cell_view), status_box->icon_rend, FALSE);
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(status_box->cell_view), status_box->text_rend, TRUE);
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(status_box->cell_view), emblem_rend, FALSE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(status_box->cell_view), status_box->icon_rend, "stock-id", ICON_STOCK_COLUMN, NULL);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(status_box->cell_view), status_box->text_rend, "markup", TEXT_COLUMN, NULL);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(status_box->cell_view), emblem_rend, "pixbuf", EMBLEM_COLUMN, "visible", EMBLEM_VISIBLE_COLUMN, NULL);
	g_object_set(status_box->text_rend, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

	status_box->vbox = gtk_vbox_new(0, FALSE);
	status_box->sw = pidgin_create_webview(FALSE, &status_box->webview, NULL, NULL);
	gtk_webview_set_editable(GTK_WEBVIEW(status_box->webview), TRUE);

#if 0
	g_signal_connect(G_OBJECT(status_box->toggle_button), "button-press-event",
			 G_CALLBACK(button_pressed_cb), status_box);
	g_signal_connect(G_OBJECT(status_box->toggle_button), "button-release-event",
			 G_CALLBACK(button_released_cb), status_box);
#endif
	g_signal_connect(G_OBJECT(status_box->toggle_button), "key-press-event",
	                 G_CALLBACK(toggle_key_press_cb), status_box);
	g_signal_connect(G_OBJECT(status_box->toggle_button), "button-press-event",
	                 G_CALLBACK(toggled_cb), status_box);
	g_signal_connect(G_OBJECT(status_box->webview), "changed",
	                 G_CALLBACK(webview_changed_cb), status_box);
	g_signal_connect(G_OBJECT(status_box->webview), "format-toggled",
	                 G_CALLBACK(webview_format_changed_cb), status_box);
	g_signal_connect_swapped(G_OBJECT(status_box->webview), "selection-changed",
	                         G_CALLBACK(webview_cursor_moved_cb), status_box);
	g_signal_connect(G_OBJECT(status_box->webview), "key-press-event",
	                 G_CALLBACK(webview_remove_focus), status_box);

	if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/spellcheck"))
		pidgin_webview_set_spellcheck(GTK_WEBVIEW(status_box->webview), TRUE);
	gtk_widget_set_parent(status_box->vbox, GTK_WIDGET(status_box));
	gtk_widget_show_all(status_box->vbox);

	gtk_widget_set_parent(status_box->toggle_button, GTK_WIDGET(status_box));

	gtk_box_pack_start(GTK_BOX(status_box->vbox), status_box->sw, TRUE, TRUE, 0);

	g_signal_connect(G_OBJECT(status_box), "scroll-event", G_CALLBACK(combo_box_scroll_event_cb), NULL);
	g_signal_connect(G_OBJECT(status_box->webview), "scroll-event",
	                 G_CALLBACK(webview_scroll_event_cb), status_box->webview);
	g_signal_connect(G_OBJECT(status_box->popup_window), "button_release_event", G_CALLBACK(treeview_button_release_cb), status_box);
	g_signal_connect(G_OBJECT(status_box->popup_window), "key_press_event", G_CALLBACK(treeview_key_press_event), status_box);
	g_signal_connect(G_OBJECT(status_box->tree_view), "cursor-changed",
					 G_CALLBACK(treeview_cursor_changed_cb), status_box->dropdown_store);

	gtk_tree_view_set_row_separator_func(GTK_TREE_VIEW(status_box->tree_view), dropdown_store_row_separator_func, NULL, NULL);

	status_box->token_status_account = check_active_accounts_for_identical_statuses();

	cache_pixbufs(status_box);
	pidgin_status_box_regenerate(status_box, TRUE);

	purple_signal_connect(purple_savedstatuses_get_handle(), "savedstatus-changed",
						status_box,
						PURPLE_CALLBACK(current_savedstatus_changed_cb),
						status_box);
	purple_signal_connect(purple_savedstatuses_get_handle(),
			"savedstatus-added", status_box,
			PURPLE_CALLBACK(saved_status_updated_cb), status_box);
	purple_signal_connect(purple_savedstatuses_get_handle(),
			"savedstatus-deleted", status_box,
			PURPLE_CALLBACK(saved_status_updated_cb), status_box);
	purple_signal_connect(purple_savedstatuses_get_handle(),
			"savedstatus-modified", status_box,
			PURPLE_CALLBACK(saved_status_updated_cb), status_box);
	purple_signal_connect(purple_accounts_get_handle(), "account-enabled", status_box,
						PURPLE_CALLBACK(account_enabled_cb),
						status_box);
	purple_signal_connect(purple_accounts_get_handle(), "account-disabled", status_box,
						PURPLE_CALLBACK(account_enabled_cb),
						status_box);
	purple_signal_connect(purple_accounts_get_handle(), "account-status-changed", status_box,
						PURPLE_CALLBACK(account_status_changed_cb),
						status_box);

	purple_prefs_connect_callback(status_box, PIDGIN_PREFS_ROOT "/conversations/spellcheck",
								spellcheck_prefs_cb, status_box);
	purple_prefs_connect_callback(status_box, PIDGIN_PREFS_ROOT "/accounts/buddyicon",
	                            update_buddyicon_cb, status_box);
	purple_signal_connect(purple_get_core(), "uri-handler", status_box,
					PURPLE_CALLBACK(statusbox_uri_handler), status_box);

}

#if GTK_CHECK_VERSION(3,0,0)
static void
pidgin_status_box_get_preferred_height(GtkWidget *widget, gint *minimum_height,
                                       gint *natural_height)
{
	gint box_min_height, box_nat_height;
	gint border_width = gtk_container_get_border_width(GTK_CONTAINER (widget));

	gtk_widget_get_preferred_height(PIDGIN_STATUS_BOX(widget)->toggle_button,
		minimum_height, natural_height);

	*minimum_height = MAX(*minimum_height, 34) + border_width * 2;
	*natural_height = MAX(*natural_height, 34) + border_width * 2;

	/* If the gtkwebview is visible, then add some additional padding */
	if (PIDGIN_STATUS_BOX(widget)->webview_visible) {
		gtk_widget_get_preferred_height(PIDGIN_STATUS_BOX(widget)->vbox,
			&box_min_height, &box_nat_height);

		if (box_min_height > 1)
			*minimum_height += box_min_height + border_width * 2;

		if (box_nat_height > 1)
			*natural_height += box_nat_height + border_width * 2;
	}
}
#else
static void
pidgin_status_box_size_request(GtkWidget *widget,
								 GtkRequisition *requisition)
{
	GtkRequisition box_req;
	gint border_width = GTK_CONTAINER (widget)->border_width;

	gtk_widget_size_request(PIDGIN_STATUS_BOX(widget)->toggle_button, requisition);

	/* Make this icon the same size as other buddy icons in the list; unless it already wants to be bigger */
	requisition->height = MAX(requisition->height, 34);
	requisition->height += border_width * 2;

	/* If the gtkwebview is visible, then add some additional padding */
	if (PIDGIN_STATUS_BOX(widget)->webview_visible) {
		gtk_widget_size_request(PIDGIN_STATUS_BOX(widget)->vbox, &box_req);
		if (box_req.height > 1)
			requisition->height += box_req.height + border_width * 2;
	}

	requisition->width = 1;
}
#endif

/* From gnome-panel */
static void
do_colorshift (GdkPixbuf *dest, GdkPixbuf *src, int shift)
{
	gint i, j;
	gint width, height, has_alpha, srcrowstride, destrowstride;
	guchar *target_pixels;
	guchar *original_pixels;
	guchar *pixsrc;
	guchar *pixdest;
	int val;
	guchar r,g,b;

	has_alpha = gdk_pixbuf_get_has_alpha (src);
	width = gdk_pixbuf_get_width (src);
	height = gdk_pixbuf_get_height (src);
	srcrowstride = gdk_pixbuf_get_rowstride (src);
	destrowstride = gdk_pixbuf_get_rowstride (dest);
	target_pixels = gdk_pixbuf_get_pixels (dest);
	original_pixels = gdk_pixbuf_get_pixels (src);

	for (i = 0; i < height; i++) {
		pixdest = target_pixels + i*destrowstride;
		pixsrc = original_pixels + i*srcrowstride;
		for (j = 0; j < width; j++) {
			r = *(pixsrc++);
			g = *(pixsrc++);
			b = *(pixsrc++);
			val = r + shift;
			*(pixdest++) = CLAMP(val, 0, 255);
			val = g + shift;
			*(pixdest++) = CLAMP(val, 0, 255);
			val = b + shift;
			*(pixdest++) = CLAMP(val, 0, 255);
			if (has_alpha)
				*(pixdest++) = *(pixsrc++);
		}
	}
}

static void
pidgin_status_box_size_allocate(GtkWidget *widget,
				  GtkAllocation *allocation)
{
	PidginStatusBox *status_box = PIDGIN_STATUS_BOX(widget);
	GtkRequisition req = {0,0};
	GtkAllocation parent_alc, box_alc, icon_alc;
	gint border_width = gtk_container_get_border_width(GTK_CONTAINER (widget));

	gtk_widget_size_request(status_box->toggle_button, &req);
	/* Make this icon the same size as other buddy icons in the list; unless it already wants to be bigger */

	req.height = MAX(req.height, 34);
	req.height += border_width * 2;

	box_alc = *allocation;

	box_alc.width -= (border_width * 2);
	box_alc.height = MAX(1, ((allocation->height - req.height) - (border_width*2)));
	box_alc.x += border_width;
	box_alc.y += req.height + border_width;
	gtk_widget_size_allocate(status_box->vbox, &box_alc);

	parent_alc = *allocation;
	parent_alc.height = MAX(1,req.height - (border_width *2));
	parent_alc.width -= (border_width * 2);
	parent_alc.x += border_width;
	parent_alc.y += border_width;

	if (status_box->icon_box)
	{
		parent_alc.width -= (parent_alc.height + border_width);
		icon_alc = parent_alc;
		icon_alc.height = MAX(1, icon_alc.height) - 2;
		icon_alc.width = icon_alc.height;
		icon_alc.x = allocation->width - (icon_alc.width + border_width + 1);
		icon_alc.y += 1;

		if (status_box->icon_size != icon_alc.height)
		{
			status_box->icon_size = icon_alc.height;
			pidgin_status_box_redisplay_buddy_icon(status_box);
		}
		gtk_widget_size_allocate(status_box->icon_box, &icon_alc);
	}
	gtk_widget_size_allocate(status_box->toggle_button, &parent_alc);
	gtk_widget_set_allocation(GTK_WIDGET(status_box), allocation);
}

#if GTK_CHECK_VERSION(3,0,0)
static gboolean
pidgin_status_box_draw(GtkWidget *widget, cairo_t *cr)
{
	PidginStatusBox *status_box = PIDGIN_STATUS_BOX(widget);
	gtk_widget_draw(status_box->toggle_button, cr);

	if (status_box->icon_box && status_box->icon_opaque) {
		GtkAllocation allocation;
		GtkStyleContext *context;

		gtk_widget_get_allocation(status_box->icon_box, &allocation);
		context = gtk_widget_get_style_context(widget);
		gtk_style_context_add_class(context, GTK_STYLE_CLASS_BUTTON);
		gtk_render_frame(context, cr, allocation.x-1, allocation.y-1, 34, 34);
	}
	return FALSE;
}
#else
static gboolean
pidgin_status_box_expose_event(GtkWidget *widget,
				 GdkEventExpose *event)
{
	PidginStatusBox *status_box = PIDGIN_STATUS_BOX(widget);
	gtk_container_propagate_expose(GTK_CONTAINER(widget), status_box->vbox, event);
	gtk_container_propagate_expose(GTK_CONTAINER(widget), status_box->toggle_button, event);
	if (status_box->icon_box && status_box->icon_opaque) {
		gtk_paint_box(widget->style, widget->window, GTK_STATE_NORMAL, GTK_SHADOW_OUT, NULL,
				status_box->icon_box, "button", status_box->icon_box->allocation.x-1, status_box->icon_box->allocation.y-1,
				34, 34);
	}
	return FALSE;
}
#endif

static void
pidgin_status_box_forall(GtkContainer *container,
						   gboolean include_internals,
						   GtkCallback callback,
						   gpointer callback_data)
{
	PidginStatusBox *status_box = PIDGIN_STATUS_BOX (container);

	if (include_internals)
	{
	  	(* callback) (status_box->vbox, callback_data);
		(* callback) (status_box->toggle_button, callback_data);
		(* callback) (status_box->arrow, callback_data);
		if (status_box->icon_box)
			(* callback) (status_box->icon_box, callback_data);
	}
}

GtkWidget *
pidgin_status_box_new()
{
	return g_object_new(PIDGIN_TYPE_STATUS_BOX, "account", NULL,
	                    "iconsel", TRUE, NULL);
}

GtkWidget *
pidgin_status_box_new_with_account(PurpleAccount *account)
{
	return g_object_new(PIDGIN_TYPE_STATUS_BOX, "account", account,
	                    "iconsel", TRUE, NULL);
}

/**
 * Add a row to the dropdown menu.
 *
 * @param status_box The status box itself.
 * @param type       A PidginStatusBoxItemType.
 * @param pixbuf     The icon to associate with this row in the menu. The
 *                   function will try to decide a pixbuf if none is given.
 * @param title      The title of this item.  For the primitive entries,
 *                   this is something like "Available" or "Away."  For
 *                   the saved statuses, this is something like
 *                   "My favorite away message!"  This should be
 *                   plaintext (non-markedup) (this function escapes it).
 * @param desc       The secondary text for this item.  This will be
 *                   placed on the row below the title, in a dimmer
 *                   font (generally gray).  This text should be plaintext
 *                   (non-markedup) (this function escapes it).
 * @param data       Data to be associated with this row in the dropdown
 *                   menu.  For primitives this is the value of the
 *                   PurpleStatusPrimitive.  For saved statuses this is the
 *                   creation timestamp.
 */
void
pidgin_status_box_add(PidginStatusBox *status_box, PidginStatusBoxItemType type, GdkPixbuf *pixbuf,
		const char *title, const char *desc, gpointer data)
{
	GtkTreeIter iter;
	char *text;
	const char *stock = NULL;

	if (desc == NULL)
	{
		text = g_markup_escape_text(title, -1);
	}
	else
	{
		GtkStyle *style;
		char aa_color[8];
		gchar *escaped_title, *escaped_desc;

		style = gtk_widget_get_style(GTK_WIDGET(status_box));
		snprintf(aa_color, sizeof(aa_color), "#%02x%02x%02x",
			 style->text_aa[GTK_STATE_NORMAL].red >> 8,
			 style->text_aa[GTK_STATE_NORMAL].green >> 8,
			 style->text_aa[GTK_STATE_NORMAL].blue >> 8);

		escaped_title = g_markup_escape_text(title, -1);
		escaped_desc = g_markup_escape_text(desc, -1);
		text = g_strdup_printf("%s - <span color=\"%s\" size=\"smaller\">%s</span>",
					escaped_title,
				       aa_color, escaped_desc);
		g_free(escaped_title);
		g_free(escaped_desc);
	}

	if (!pixbuf) {
		PurpleStatusPrimitive prim = PURPLE_STATUS_UNSET;
		if (type == PIDGIN_STATUS_BOX_TYPE_PRIMITIVE) {
			prim = GPOINTER_TO_INT(data);
		} else if (type == PIDGIN_STATUS_BOX_TYPE_SAVED_POPULAR ||
				type == PIDGIN_STATUS_BOX_TYPE_POPULAR) {
			PurpleSavedStatus *saved = purple_savedstatus_find_by_creation_time(GPOINTER_TO_INT(data));
			if (saved) {
				prim = purple_savedstatus_get_type(saved);
			}
		}

		stock = pidgin_stock_id_from_status_primitive(prim);
	}

	gtk_list_store_append(status_box->dropdown_store, &iter);
	gtk_list_store_set(status_box->dropdown_store, &iter,
			TYPE_COLUMN, type,
			ICON_STOCK_COLUMN, stock,
			TEXT_COLUMN, text,
			TITLE_COLUMN, title,
			DESC_COLUMN, desc,
			DATA_COLUMN, data,
			EMBLEM_VISIBLE_COLUMN, type == PIDGIN_STATUS_BOX_TYPE_SAVED_POPULAR,
			EMBLEM_COLUMN, GTK_STOCK_SAVE,
			-1);
	g_free(text);
}

void
pidgin_status_box_add_separator(PidginStatusBox *status_box)
{
	/* Don't do anything unless GTK actually supports
	 * gtk_combo_box_set_row_separator_func */
	GtkTreeIter iter;

	gtk_list_store_append(status_box->dropdown_store, &iter);
	gtk_list_store_set(status_box->dropdown_store, &iter,
			   TYPE_COLUMN, PIDGIN_STATUS_BOX_TYPE_SEPARATOR,
			   -1);
}

void
pidgin_status_box_set_network_available(PidginStatusBox *status_box, gboolean available)
{
	if (!status_box)
		return;
	status_box->network_available = available;
	pidgin_status_box_refresh(status_box);
}

void
pidgin_status_box_set_connecting(PidginStatusBox *status_box, gboolean connecting)
{
	if (!status_box)
		return;
	status_box->connecting = connecting;
	pidgin_status_box_refresh(status_box);
}

static void
pixbuf_size_prepared_cb(GdkPixbufLoader *loader, int width, int height, gpointer data)
{
	int w, h;
	GtkIconSize icon_size = gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_MEDIUM);
	gtk_icon_size_lookup(icon_size, &w, &h);
	if (height > width)
		w = width * h  / height;
	else if (width > height)
		h = height * w / width;
	gdk_pixbuf_loader_set_size(loader, w, h);
}

static void
pidgin_status_box_redisplay_buddy_icon(PidginStatusBox *status_box)
{

	/* This is sometimes called before the box is shown, and we will not have a size */
	if (status_box->icon_size <= 0)
		return;

	if (status_box->buddy_icon)
		g_object_unref(status_box->buddy_icon);
	if (status_box->buddy_icon_hover)
		g_object_unref(status_box->buddy_icon_hover);
	status_box->buddy_icon = NULL;
	status_box->buddy_icon_hover = NULL;

	if (status_box->buddy_icon_img != NULL)
	{
		GdkPixbufLoader *loader;
		GError *error = NULL;

		loader = gdk_pixbuf_loader_new();

		g_signal_connect(G_OBJECT(loader), "size-prepared", G_CALLBACK(pixbuf_size_prepared_cb), NULL);
		if (!gdk_pixbuf_loader_write(loader,
				purple_imgstore_get_data(status_box->buddy_icon_img),
				purple_imgstore_get_size(status_box->buddy_icon_img),
				&error) || error)
		{
			purple_debug_warning("gtkstatusbox", "gdk_pixbuf_loader_write() "
					"failed with size=%zu: %s\n",
					purple_imgstore_get_size(status_box->buddy_icon_img),
					error ? error->message : "(no error message)");
			if (error)
				g_error_free(error);
		} else if (!gdk_pixbuf_loader_close(loader, &error) || error) {
			purple_debug_warning("gtkstatusbox", "gdk_pixbuf_loader_close() "
					"failed for image of size %zu: %s\n",
					purple_imgstore_get_size(status_box->buddy_icon_img),
					error ? error->message : "(no error message)");
			if (error)
				g_error_free(error);
		} else {
			GdkPixbuf *buf, *scale;
			int scale_width, scale_height;

			buf = gdk_pixbuf_loader_get_pixbuf(loader);
			scale_width = gdk_pixbuf_get_width(buf);
			scale_height = gdk_pixbuf_get_height(buf);
			scale = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, scale_width, scale_height);
			gdk_pixbuf_fill(scale, 0x00000000);
			gdk_pixbuf_copy_area(buf, 0, 0, scale_width, scale_height, scale, 0, 0);
			if (pidgin_gdk_pixbuf_is_opaque(scale))
				pidgin_gdk_pixbuf_make_round(scale);
			status_box->buddy_icon = scale;
		}

		g_object_unref(loader);
	}

	if (status_box->buddy_icon == NULL)
	{
		/* Show a placeholder icon */
		GtkIconSize icon_size = gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_SMALL);
		status_box->buddy_icon = gtk_widget_render_icon(GTK_WIDGET(status_box),
		                                                PIDGIN_STOCK_TOOLBAR_SELECT_AVATAR,
		                                                icon_size, "PidginStatusBox");
	}

	if (status_box->buddy_icon != NULL) {
		status_box->icon_opaque = pidgin_gdk_pixbuf_is_opaque(status_box->buddy_icon);
		gtk_image_set_from_pixbuf(GTK_IMAGE(status_box->icon), status_box->buddy_icon);
		status_box->buddy_icon_hover = gdk_pixbuf_copy(status_box->buddy_icon);
		do_colorshift(status_box->buddy_icon_hover, status_box->buddy_icon_hover, 32);
		gtk_widget_queue_resize(GTK_WIDGET(status_box));
	}
}

void
pidgin_status_box_set_buddy_icon(PidginStatusBox *status_box, PurpleStoredImage *img)
{
	purple_imgstore_unref(status_box->buddy_icon_img);
	status_box->buddy_icon_img = img;
	if (status_box->buddy_icon_img != NULL)
		purple_imgstore_ref(status_box->buddy_icon_img);

	pidgin_status_box_redisplay_buddy_icon(status_box);
}

void
pidgin_status_box_pulse_connecting(PidginStatusBox *status_box)
{
	if (!status_box)
		return;
	if (!connecting_stock_ids[++status_box->connecting_index])
		status_box->connecting_index = 0;
	pidgin_status_box_refresh(status_box);
}

static void
pidgin_status_box_pulse_typing(PidginStatusBox *status_box)
{
	if (!typing_stock_ids[++status_box->typing_index])
		status_box->typing_index = 0;
	pidgin_status_box_refresh(status_box);
}

static void
activate_currently_selected_status(PidginStatusBox *status_box)
{
	PidginStatusBoxItemType type;
	gpointer data;
	gchar *title;
	GtkTreeIter iter;
	GtkTreePath *path;
	char *message;
	PurpleSavedStatus *saved_status = NULL;
	gboolean changed = TRUE;

	path = gtk_tree_row_reference_get_path(status_box->active_row);
	if (!gtk_tree_model_get_iter (GTK_TREE_MODEL(status_box->dropdown_store), &iter, path))
		return;
	gtk_tree_path_free(path);

	gtk_tree_model_get(GTK_TREE_MODEL(status_box->dropdown_store), &iter,
					   TYPE_COLUMN, &type,
					   DATA_COLUMN, &data,
					   -1);

	/*
	 * If the currently selected status is "New..." or
	 * "Saved..." or a popular status then do nothing.
	 * Popular statuses are
	 * activated elsewhere, and we update the status_box
	 * accordingly by connecting to the savedstatus-changed
	 * signal and then calling status_menu_refresh_iter()
	 */
	if (type != PIDGIN_STATUS_BOX_TYPE_PRIMITIVE)
		return;

	gtk_tree_model_get(GTK_TREE_MODEL(status_box->dropdown_store), &iter,
					   TITLE_COLUMN, &title, -1);

	message = pidgin_status_box_get_message(status_box);
	if (!message || !*message)
	{
		gtk_widget_hide(status_box->vbox);
		status_box->webview_visible = FALSE;
		if (message != NULL)
		{
			g_free(message);
			message = NULL;
		}
	}

	if (status_box->account == NULL) {
		PurpleStatusType *acct_status_type = NULL;
		const char *id = NULL; /* id of acct_status_type */
		PurpleStatusPrimitive primitive = GPOINTER_TO_INT(data);
		/* Global */
		/* Save the newly selected status to prefs.xml and status.xml */

		/* Has the status really been changed? */
		if (status_box->token_status_account) {
			gint active;
			PurpleStatus *status;
			GtkTreePath *path = gtk_tree_row_reference_get_path(status_box->active_row);
			active = gtk_tree_path_get_indices(path)[0];

			gtk_tree_path_free(path);

			status = purple_account_get_active_status(status_box->token_status_account);

			acct_status_type = find_status_type_by_index(status_box->token_status_account, active);
			id = purple_status_type_get_id(acct_status_type);

			if (g_str_equal(id, purple_status_get_id(status)) &&
				purple_strequal(message, purple_status_get_attr_string(status, "message")))
			{
				/* Selected status and previous status is the same */
				PurpleSavedStatus *ss = purple_savedstatus_get_current();
				/* Make sure that statusbox displays the correct thing.
				 * It can get messed up if the previous selection was a
				 * saved status that wasn't supported by this account */
				if ((purple_savedstatus_get_type(ss) == primitive)
					&& purple_savedstatus_is_transient(ss)
					&& purple_savedstatus_has_substatuses(ss))
					changed = FALSE;
			}
		} else {
			saved_status = purple_savedstatus_get_current();
			if (purple_savedstatus_get_type(saved_status) == primitive &&
			    !purple_savedstatus_has_substatuses(saved_status) &&
				purple_strequal(purple_savedstatus_get_message(saved_status), message))
			{
				changed = FALSE;
			}
		}

		if (changed)
		{
			/* Manually find the appropriate transient status */
			if (status_box->token_status_account) {
				GList *iter = purple_savedstatuses_get_all();
				GList *tmp, *active_accts = purple_accounts_get_all_active();

				for (; iter != NULL; iter = iter->next) {
					PurpleSavedStatus *ss = iter->data;
					const char *ss_msg = purple_savedstatus_get_message(ss);
					/* find a known transient status that is the same as the
					 * new selected one */
					if ((purple_savedstatus_get_type(ss) == primitive) && purple_savedstatus_is_transient(ss) &&
						purple_savedstatus_has_substatuses(ss) && /* Must have substatuses */
						purple_strequal(ss_msg, message))
					{
						gboolean found = FALSE;
						/* this status must have substatuses for all the active accts */
						for(tmp = active_accts; tmp != NULL; tmp = tmp->next) {
							PurpleAccount *acct = tmp->data;
							PurpleSavedStatusSub *sub = purple_savedstatus_get_substatus(ss, acct);
							if (sub) {
								const PurpleStatusType *sub_type = purple_savedstatus_substatus_get_type(sub);
								const char *subtype_status_id = purple_status_type_get_id(sub_type);
								if (purple_strequal(subtype_status_id, id)) {
									found = TRUE;
									break;
								}
							}
						}

						if (found) {
							saved_status = ss;
							break;
						}
					}
				}

				g_list_free(active_accts);

			} else {
				/* If we've used this type+message before, lookup the transient status */
				saved_status = purple_savedstatus_find_transient_by_type_and_message(primitive, message);
			}

			/* If this type+message is unique then create a new transient saved status */
			if (saved_status == NULL)
			{
				saved_status = purple_savedstatus_new(NULL, primitive);
				purple_savedstatus_set_message(saved_status, message);
				if (status_box->token_status_account) {
					GList *tmp, *active_accts = purple_accounts_get_all_active();
					for (tmp = active_accts; tmp != NULL; tmp = tmp->next) {
						purple_savedstatus_set_substatus(saved_status,
							(PurpleAccount*) tmp->data, acct_status_type, message);
					}
					g_list_free(active_accts);
				}
			}

			/* Set the status for each account */
			purple_savedstatus_activate(saved_status);
		}
	} else {
		/* Per-account */
		gint active;
		PurpleStatusType *status_type;
		PurpleStatus *status;
		const char *id = NULL;

		status = purple_account_get_active_status(status_box->account);

		active = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(status_box), "active"));

		status_type = find_status_type_by_index(status_box->account, active);
		id = purple_status_type_get_id(status_type);

		if (g_str_equal(id, purple_status_get_id(status)) &&
			purple_strequal(message, purple_status_get_attr_string(status, "message")))
		{
			/* Selected status and previous status is the same */
			changed = FALSE;
		}

		if (changed)
		{
			if (message)
				purple_account_set_status(status_box->account, id,
										TRUE, "message", message, NULL);
			else
				purple_account_set_status(status_box->account, id,
										TRUE, NULL);

			saved_status = purple_savedstatus_get_current();
			if (purple_savedstatus_is_transient(saved_status))
				purple_savedstatus_set_substatus(saved_status, status_box->account,
						status_type, message);
		}
	}

	g_free(title);
	g_free(message);
}

static void update_size(PidginStatusBox *status_box)
{
#if 0
	/* TODO WebKit Sizing */
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	int display_lines;
	int lines;
	GdkRectangle oneline;
	int height;
	int pad_top, pad_inside, pad_bottom;
	gboolean interior_focus;
	int focus_width;
#endif

	if (!status_box->webview_visible)
	{
		if (status_box->vbox != NULL)
			gtk_widget_set_size_request(status_box->vbox, -1, -1);
		return;
	}

#if 0
	/* TODO WebKit: Entry sizing */
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(status_box->webview));

	height = 0;
	display_lines = 1;
	gtk_text_buffer_get_start_iter(buffer, &iter);
	do {
		gtk_text_view_get_iter_location(GTK_TEXT_VIEW(status_box->webview), &iter, &oneline);
		height += oneline.height;
		display_lines++;
	} while (display_lines <= 4 &&
		gtk_text_view_forward_display_line(GTK_TEXT_VIEW(status_box->webview), &iter));

	/*
	 * This check fixes the case where the last character entered is a
	 * newline (shift+return).  For some reason the
	 * gtk_text_view_forward_display_line() function doesn't treat this
	 * like a new line, and so we think the input box only needs to be
	 * two lines instead of three, for example.  So we check if the
	 * last character was a newline and add some extra height if so.
	 */
	if (display_lines <= 4
		&& gtk_text_iter_backward_char(&iter)
		&& gtk_text_iter_get_char(&iter) == '\n')
	{
		gtk_text_view_get_iter_location(GTK_TEXT_VIEW(status_box->webview), &iter, &oneline);
		height += oneline.height;
		display_lines++;
	}

	lines = gtk_text_buffer_get_line_count(buffer);

	/* Show a maximum of 4 lines */
	lines = MIN(lines, 4);
	display_lines = MIN(display_lines, 4);

	pad_top = gtk_text_view_get_pixels_above_lines(GTK_TEXT_VIEW(status_box->webview));
	pad_bottom = gtk_text_view_get_pixels_below_lines(GTK_TEXT_VIEW(status_box->webview));
	pad_inside = gtk_text_view_get_pixels_inside_wrap(GTK_TEXT_VIEW(status_box->webview));

	height += (pad_top + pad_bottom) * lines;
	height += (pad_inside) * (display_lines - lines);

	gtk_widget_style_get(status_box->webview,
	                     "interior-focus", &interior_focus,
	                     "focus-line-width", &focus_width,
	                     NULL);
	if (!interior_focus)
		height += 2 * focus_width;

	gtk_widget_set_size_request(status_box->vbox, -1, height + PIDGIN_HIG_BOX_SPACE);
#endif
	gtk_widget_set_size_request(status_box->vbox, -1, -1);
}

static void remove_typing_cb(PidginStatusBox *status_box)
{
	if (status_box->typing == 0)
	{
		/* Nothing has changed, so we don't need to do anything */
		status_menu_refresh_iter(status_box, FALSE);
		return;
	}

#if 0
	/* TODO WebKit: Doesn't do this? */
	gtk_webview_set_populate_primary_clipboard(
		GTK_WEBVIEW(status_box->webview), TRUE);
#endif

	purple_timeout_remove(status_box->typing);
	status_box->typing = 0;

	activate_currently_selected_status(status_box);
	pidgin_status_box_refresh(status_box);
}

static void pidgin_status_box_changed(PidginStatusBox *status_box)
{
	GtkTreePath *path = gtk_tree_row_reference_get_path(status_box->active_row);
	GtkTreeIter iter;
	PidginStatusBoxItemType type;
	gpointer data;
	GList *accounts = NULL, *node;
	int active;
	gboolean wastyping = FALSE;


	if (!gtk_tree_model_get_iter (GTK_TREE_MODEL(status_box->dropdown_store), &iter, path))
		return;
	active = gtk_tree_path_get_indices(path)[0];
	gtk_tree_path_free(path);
	g_object_set_data(G_OBJECT(status_box), "active", GINT_TO_POINTER(active));

	gtk_tree_model_get(GTK_TREE_MODEL(status_box->dropdown_store), &iter,
			   TYPE_COLUMN, &type,
			   DATA_COLUMN, &data,
			   -1);
	if ((wastyping = (status_box->typing != 0)))
		purple_timeout_remove(status_box->typing);
	status_box->typing = 0;

	if (gtk_widget_get_sensitive(GTK_WIDGET(status_box)))
	{
		if (type == PIDGIN_STATUS_BOX_TYPE_POPULAR || type == PIDGIN_STATUS_BOX_TYPE_SAVED_POPULAR)
		{
			PurpleSavedStatus *saved;
			saved = purple_savedstatus_find_by_creation_time(GPOINTER_TO_INT(data));
			g_return_if_fail(saved != NULL);
			purple_savedstatus_activate(saved);
			return;
		}

		if (type == PIDGIN_STATUS_BOX_TYPE_CUSTOM)
		{
			PurpleSavedStatus *saved_status;
			saved_status = purple_savedstatus_get_current();
			if (purple_savedstatus_get_type(saved_status) == PURPLE_STATUS_AVAILABLE)
				saved_status = purple_savedstatus_new(NULL, PURPLE_STATUS_AWAY);
			pidgin_status_editor_show(FALSE,
				purple_savedstatus_is_transient(saved_status)
					? saved_status : NULL);
			status_menu_refresh_iter(status_box, wastyping);
			if (wastyping)
				pidgin_status_box_refresh(status_box);
			return;
		}

		if (type == PIDGIN_STATUS_BOX_TYPE_SAVED)
		{
			pidgin_status_window_show();
			status_menu_refresh_iter(status_box, wastyping);
			if (wastyping)
				pidgin_status_box_refresh(status_box);
			return;
		}
	}

	/*
	 * Show the message box whenever the primitive allows for a
	 * message attribute on any protocol that is enabled,
	 * or our protocol, if we have account set
	 */
	if (status_box->account)
		accounts = g_list_prepend(accounts, status_box->account);
	else
		accounts = purple_accounts_get_all_active();
	status_box->webview_visible = FALSE;
	for (node = accounts; node != NULL; node = node->next)
	{
		PurpleAccount *account;
		PurpleStatusType *status_type;

		account = node->data;
		status_type = purple_account_get_status_type_with_primitive(account, GPOINTER_TO_INT(data));
		if ((status_type != NULL) &&
			(purple_status_type_get_attr(status_type, "message") != NULL))
		{
			status_box->webview_visible = TRUE;
			break;
		}
	}
	g_list_free(accounts);

	if (gtk_widget_get_sensitive(GTK_WIDGET(status_box)))
	{
		if (status_box->webview_visible)
		{
			gtk_widget_show_all(status_box->vbox);
			status_box->typing = purple_timeout_add_seconds(TYPING_TIMEOUT, (GSourceFunc)remove_typing_cb, status_box);
			gtk_widget_grab_focus(status_box->webview);
#if 0
			/* TODO WebKit: Doesn't do this? */
			gtk_webview_set_populate_primary_clipboard(
				GTK_WEBVIEW(status_box->webview), FALSE);
#endif

			webkit_web_view_select_all(WEBKIT_WEB_VIEW(status_box->webview));
		}
		else
		{
			gtk_widget_hide(status_box->vbox);
			activate_currently_selected_status(status_box); /* This is where we actually set the status */
		}
	}
	pidgin_status_box_refresh(status_box);
}

static gint
get_statusbox_index(PidginStatusBox *box, PurpleSavedStatus *saved_status)
{
	gint index = -1;

	switch (purple_savedstatus_get_type(saved_status))
	{
		/* In reverse order */
		case PURPLE_STATUS_OFFLINE:
			index++;
		case PURPLE_STATUS_INVISIBLE:
			index++;
		case PURPLE_STATUS_UNAVAILABLE:
			index++;
		case PURPLE_STATUS_AWAY:
			index++;
		case PURPLE_STATUS_AVAILABLE:
			index++;
			break;
		default:
			break;
	}

	return index;
}

static void
webview_changed_cb(GtkWebView *webview, void *data)
{
	PidginStatusBox *status_box = (PidginStatusBox*)data;
	if (gtk_widget_get_sensitive(GTK_WIDGET(status_box)))
	{
		if (status_box->typing != 0) {
			pidgin_status_box_pulse_typing(status_box);
			purple_timeout_remove(status_box->typing);
		}
		status_box->typing = purple_timeout_add_seconds(TYPING_TIMEOUT, (GSourceFunc)remove_typing_cb, status_box);
	}
	pidgin_status_box_refresh(status_box);
}

static void
webview_format_changed_cb(GtkWebView *webview, GtkWebViewButtons buttons, void *data)
{
	webview_changed_cb(NULL, data);
}

char *pidgin_status_box_get_message(PidginStatusBox *status_box)
{
	if (status_box->webview_visible)
		return gtk_webview_get_body_html(GTK_WEBVIEW(status_box->webview));
	else
		return NULL;
}
