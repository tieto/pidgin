/*
 * @file gtkstatusbox.c GTK+ Status Selection Widget
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

#include "account.h"
#include "internal.h"
#include "savedstatuses.h"
#include "status.h"
#include "debug.h"

#include "gtkgaim.h"
#include "gtksavedstatuses.h"
#include "gtkstock.h"
#include "gtkstatusbox.h"
#include "gtkutils.h"

#ifdef USE_GTKSPELL
#  include <gtkspell/gtkspell.h>
#  ifdef _WIN32
#    include "wspell.h"
#  endif
#endif

#define TYPING_TIMEOUT 4000

static void imhtml_changed_cb(GtkTextBuffer *buffer, void *data);
static void imhtml_format_changed_cb(GtkIMHtml *imhtml, GtkIMHtmlButtons buttons, void *data);
static void remove_typing_cb(GtkGaimStatusBox *box);
static void update_size (GtkGaimStatusBox *box);
static gint get_statusbox_index(GtkGaimStatusBox *box, GaimSavedStatus *saved_status);

static void gtk_gaim_status_box_pulse_typing(GtkGaimStatusBox *status_box);
static void gtk_gaim_status_box_refresh(GtkGaimStatusBox *status_box);
static void gtk_gaim_status_box_regenerate(GtkGaimStatusBox *status_box);
static void gtk_gaim_status_box_changed(GtkComboBox *box);
static void gtk_gaim_status_box_size_request (GtkWidget *widget, GtkRequisition *requisition);
static void gtk_gaim_status_box_size_allocate (GtkWidget *widget, GtkAllocation *allocation);
static gboolean gtk_gaim_status_box_expose_event (GtkWidget *widget, GdkEventExpose *event);
static void gtk_gaim_status_box_forall (GtkContainer *container, gboolean include_internals, GtkCallback callback, gpointer callback_data);

static void do_colorshift (GdkPixbuf *dest, GdkPixbuf *src, int shift);
static void icon_choose_cb(const char *filename, gpointer data);

static void (*combo_box_size_request)(GtkWidget *widget, GtkRequisition *requisition);
static void (*combo_box_size_allocate)(GtkWidget *widget, GtkAllocation *allocation);
static void (*combo_box_forall) (GtkContainer *container, gboolean include_internals, GtkCallback callback, gpointer callback_data);

enum {
	/** A GtkGaimStatusBoxItemType */
	TYPE_COLUMN,

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

	/*
	 * This value depends on TYPE_COLUMN.  For POPULAR types,
	 * this is the creation time.  For PRIMITIVE types,
	 * this is the GaimStatusPrimitive.
	 */
	DATA_COLUMN,

	NUM_COLUMNS
};

enum {
	PROP_0,
	PROP_ACCOUNT
};

GtkComboBoxClass *parent_class = NULL;

static void gtk_gaim_status_box_class_init (GtkGaimStatusBoxClass *klass);
static void gtk_gaim_status_box_init (GtkGaimStatusBox *status_box);

GType
gtk_gaim_status_box_get_type (void)
{
	static GType status_box_type = 0;

	if (!status_box_type)
	{
		static const GTypeInfo status_box_info =
		{
			sizeof (GtkGaimStatusBoxClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) gtk_gaim_status_box_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GtkGaimStatusBox),
			0,
			(GInstanceInitFunc) gtk_gaim_status_box_init,
			NULL  /* value_table */
		};

		status_box_type = g_type_register_static(GTK_TYPE_COMBO_BOX,
												 "GtkGaimStatusBox",
												 &status_box_info,
												 0);
	}

	return status_box_type;
}

static void
gtk_gaim_status_box_get_property(GObject *object, guint param_id,
                                 GValue *value, GParamSpec *psec)
{
	GtkGaimStatusBox *statusbox = GTK_GAIM_STATUS_BOX(object);

	switch (param_id) {
	case PROP_ACCOUNT:
		g_value_set_pointer(value, statusbox->account);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, psec);
		break;
	}
}

static void
update_to_reflect_account_status(GtkGaimStatusBox *status_box, GaimAccount *account, GaimStatus *newstatus)
{
	const GList *l;
	int status_no = -1;
	const GaimStatusType *statustype = NULL;
	const char *message;

	statustype = gaim_status_type_find_with_id((GList *)gaim_account_get_status_types(account),
	                                           (char *)gaim_status_type_get_id(gaim_status_get_type(newstatus)));

	for (l = gaim_account_get_status_types(account); l != NULL; l = l->next) {
		GaimStatusType *status_type = (GaimStatusType *)l->data;

		if (!gaim_status_type_is_user_settable(status_type))
			continue;
		status_no++;
		if (statustype == status_type)
			break;
	}

	if (status_no != -1) {
		gtk_widget_set_sensitive(GTK_WIDGET(status_box), FALSE);
		gtk_combo_box_set_active(GTK_COMBO_BOX(status_box), status_no);

		message = gaim_status_get_attr_string(newstatus, "message");

		if (!message || !*message)
		{
			gtk_widget_hide_all(status_box->vbox);
			status_box->imhtml_visible = FALSE;
		}
		else
		{
			gtk_widget_show_all(status_box->vbox);
			status_box->imhtml_visible = TRUE;
			gtk_imhtml_clear(GTK_IMHTML(status_box->imhtml));
			gtk_imhtml_clear_formatting(GTK_IMHTML(status_box->imhtml));
			gtk_imhtml_append_text(GTK_IMHTML(status_box->imhtml), message, 0);
		}
		gtk_widget_set_sensitive(GTK_WIDGET(status_box), TRUE);
		gtk_gaim_status_box_refresh(status_box);
	}
}

static void
account_status_changed_cb(GaimAccount *account, GaimStatus *oldstatus, GaimStatus *newstatus, GtkGaimStatusBox *status_box)
{
	if (status_box->account == account)
		update_to_reflect_account_status(status_box, account, newstatus);
}

static gboolean
icon_box_press_cb(GtkWidget *widget, GdkEventButton *event, GtkGaimStatusBox *box)
{
	if (box->buddy_icon_sel) {
		gtk_window_present(GTK_WINDOW(box->buddy_icon_sel));
		return FALSE;
	}

	box->buddy_icon_sel = gaim_gtk_buddy_icon_chooser_new(NULL, icon_choose_cb, box);
	gtk_widget_show_all(box->buddy_icon_sel);
	return FALSE;
}

static gboolean
icon_box_enter_cb(GtkWidget *widget, GdkEventCrossing *event, GtkGaimStatusBox *box)
{
	gdk_window_set_cursor(widget->window, box->hand_cursor);
	gtk_image_set_from_pixbuf(GTK_IMAGE(box->icon), box->buddy_icon_hover);
	return FALSE;
}

static gboolean
icon_box_leave_cb(GtkWidget *widget, GdkEventCrossing *event, GtkGaimStatusBox *box)
{
	gdk_window_set_cursor(widget->window, box->arrow_cursor);
	gtk_image_set_from_pixbuf(GTK_IMAGE(box->icon), box->buddy_icon) ;
	return FALSE;
}

static void
setup_icon_box(GtkGaimStatusBox *status_box)
{
	if (status_box->account &&
		!gaim_account_get_ui_bool(status_box->account, GAIM_GTK_UI, "use-global-buddyicon", TRUE))
	{
		char *string = gaim_buddy_icons_get_full_path(gaim_account_get_buddy_icon(status_box->account));
		gtk_gaim_status_box_set_buddy_icon(status_box, string);
		g_free(string);
	}
	else
	{
		gtk_gaim_status_box_set_buddy_icon(status_box, gaim_prefs_get_string("/gaim/gtk/accounts/buddyicon"));
	}
	status_box->icon = gtk_image_new_from_pixbuf(status_box->buddy_icon);
	status_box->icon_box = gtk_event_box_new();
	status_box->hand_cursor = gdk_cursor_new (GDK_HAND2);
	status_box->arrow_cursor = gdk_cursor_new (GDK_LEFT_PTR);

	g_signal_connect(G_OBJECT(status_box->icon_box), "enter-notify-event", G_CALLBACK(icon_box_enter_cb), status_box);
	g_signal_connect(G_OBJECT(status_box->icon_box), "leave-notify-event", G_CALLBACK(icon_box_leave_cb), status_box);
	g_signal_connect(G_OBJECT(status_box->icon_box), "button-press-event", G_CALLBACK(icon_box_press_cb), status_box);

	gtk_container_add(GTK_CONTAINER(status_box->icon_box), status_box->icon);

	gtk_widget_show_all(status_box->icon_box);
	gtk_widget_set_parent(status_box->icon_box, GTK_WIDGET(status_box));
}

static void
gtk_gaim_status_box_set_property(GObject *object, guint param_id,
                                 const GValue *value, GParamSpec *pspec)
{
	GtkGaimStatusBox *statusbox = GTK_GAIM_STATUS_BOX(object);

	switch (param_id) {
	case PROP_ACCOUNT:
		statusbox->account = g_value_get_pointer(value);

		if (statusbox->status_changed_signal) {
			gaim_signal_disconnect(gaim_accounts_get_handle(), "account-status-changed",
			                        statusbox, GAIM_CALLBACK(account_status_changed_cb));
			statusbox->status_changed_signal = 0;
		}

		if (statusbox->account) {
			GaimPlugin *plug = gaim_plugins_find_with_id(gaim_account_get_protocol_id(statusbox->account));
			GaimPluginProtocolInfo *prplinfo = GAIM_PLUGIN_PROTOCOL_INFO(plug);
			if (prplinfo && prplinfo->icon_spec.format != NULL) {
				setup_icon_box(statusbox);
			}
			statusbox->status_changed_signal = gaim_signal_connect(gaim_accounts_get_handle(), "account-status-changed",
			                                                       statusbox, GAIM_CALLBACK(account_status_changed_cb),
			                                                       statusbox);
		} else {
			setup_icon_box(statusbox);
		}
		gtk_gaim_status_box_regenerate(statusbox);

		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, pspec);
		break;
	}
}

static void
gtk_gaim_status_box_finalize(GObject *obj)
{
	GtkGaimStatusBox *statusbox = GTK_GAIM_STATUS_BOX(obj);

	if (statusbox->status_changed_signal) {
		gaim_signal_disconnect(gaim_accounts_get_handle(), "account-status-changed",
								statusbox, GAIM_CALLBACK(account_status_changed_cb));
		statusbox->status_changed_signal = 0;
	}
	gaim_signals_disconnect_by_handle(statusbox);
	gaim_prefs_disconnect_by_handle(statusbox);

	gdk_cursor_unref(statusbox->hand_cursor);
	gdk_cursor_unref(statusbox->arrow_cursor);

	g_object_unref(G_OBJECT(statusbox->buddy_icon));
	g_object_unref(G_OBJECT(statusbox->buddy_icon_hover));

	if (statusbox->buddy_icon_sel)
		gtk_widget_destroy(statusbox->buddy_icon_sel);

	g_free(statusbox->buddy_icon_path);

	G_OBJECT_CLASS(parent_class)->finalize(obj);
}

static void
gtk_gaim_status_box_class_init (GtkGaimStatusBoxClass *klass)
{
	GObjectClass *object_class;
	GtkComboBoxClass *combo_class;
	GtkWidgetClass *widget_class;
	GtkContainerClass *container_class = (GtkContainerClass*)klass;

	parent_class = g_type_class_peek_parent(klass);

	combo_class = (GtkComboBoxClass*)klass;
	combo_class->changed = gtk_gaim_status_box_changed;

	widget_class = (GtkWidgetClass*)klass;
	combo_box_size_request = widget_class->size_request;
	widget_class->size_request = gtk_gaim_status_box_size_request;
	combo_box_size_allocate = widget_class->size_allocate;
	widget_class->size_allocate = gtk_gaim_status_box_size_allocate;
	widget_class->expose_event = gtk_gaim_status_box_expose_event;

	combo_box_forall = container_class->forall;
	container_class->forall = gtk_gaim_status_box_forall;

	object_class = (GObjectClass *)klass;

	object_class->finalize = gtk_gaim_status_box_finalize;

	object_class->get_property = gtk_gaim_status_box_get_property;
	object_class->set_property = gtk_gaim_status_box_set_property;

	g_object_class_install_property(object_class,
	                                PROP_ACCOUNT,
	                                g_param_spec_pointer("account",
	                                                     "Account",
	                                                     "The account, or NULL for all accounts",
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
gtk_gaim_status_box_refresh(GtkGaimStatusBox *status_box)
{
	gboolean show_buddy_icons;
	GtkIconSize icon_size;
	GtkStyle *style;
	char aa_color[8];
	GaimSavedStatus *saved_status;
	char *primary, *secondary, *text;
	GdkPixbuf *pixbuf;
	GtkTreePath *path;

	show_buddy_icons = gaim_prefs_get_bool("/gaim/gtk/blist/show_buddy_icons");
	if (show_buddy_icons)
		icon_size = gtk_icon_size_from_name(GAIM_ICON_SIZE_STATUS);
	else
		icon_size = gtk_icon_size_from_name(GAIM_ICON_SIZE_STATUS_SMALL);

	style = gtk_widget_get_style(GTK_WIDGET(status_box));
	snprintf(aa_color, sizeof(aa_color), "#%02x%02x%02x",
		 style->text_aa[GTK_STATE_NORMAL].red >> 8,
		 style->text_aa[GTK_STATE_NORMAL].green >> 8,
		 style->text_aa[GTK_STATE_NORMAL].blue >> 8);

	saved_status = gaim_savedstatus_get_current();

	/* Primary */
	if (status_box->typing != 0)
	{
		GtkTreeIter iter;
		GtkGaimStatusBoxItemType type;
		gpointer data;

		/* Primary (get the status selected in the dropdown) */
		gtk_combo_box_get_active_iter(GTK_COMBO_BOX(status_box), &iter);
		gtk_tree_model_get(GTK_TREE_MODEL(status_box->dropdown_store), &iter,
						   TYPE_COLUMN, &type,
						   DATA_COLUMN, &data,
						   -1);
		if (type == GTK_GAIM_STATUS_BOX_TYPE_PRIMITIVE)
			primary = g_strdup(gaim_primitive_get_name_from_type(GPOINTER_TO_INT(data)));
		else
			/* This should never happen, but just in case... */
			primary = g_strdup("New status");
	}
	else if (status_box->account != NULL)
	{
		primary = g_strdup(gaim_status_get_name(gaim_account_get_active_status(status_box->account)));
	}
	else if (gaim_savedstatus_is_transient(saved_status))
		primary = g_strdup(gaim_primitive_get_name_from_type(gaim_savedstatus_get_type(saved_status)));
	else
		primary = g_markup_escape_text(gaim_savedstatus_get_title(saved_status), -1);

	/* Secondary */
	if (status_box->typing != 0)
		secondary = g_strdup(_("Typing"));
	else if (status_box->connecting)
		secondary = g_strdup(_("Connecting"));
	else if (gaim_savedstatus_is_transient(saved_status))
		secondary = NULL;
	else
	{
		const char *message;
		char *tmp;
		message = gaim_savedstatus_get_message(saved_status);
		if (message != NULL)
		{
			tmp = gaim_markup_strip_html(message);
			gaim_util_chrreplace(tmp, '\n', ' ');
			secondary = g_markup_escape_text(tmp, -1);
			g_free(tmp);
		}
		else
			secondary = NULL;
	}

	/* Pixbuf */
	if (status_box->typing != 0)
		pixbuf = status_box->typing_pixbufs[status_box->typing_index];
	else if (status_box->connecting)
		pixbuf = status_box->connecting_pixbufs[status_box->connecting_index];
	else
	{
		if (status_box->account != NULL)
			pixbuf = gaim_gtk_create_prpl_icon_with_status(status_box->account,
						gaim_status_get_type(gaim_account_get_active_status(status_box->account)),
						show_buddy_icons ? 1.0 : 0.5);
		else
			pixbuf = gaim_gtk_create_gaim_icon_with_status(
						gaim_savedstatus_get_type(saved_status),
						show_buddy_icons ? 1.0 : 0.5);

		if (!gaim_savedstatus_is_transient(saved_status))
		{
			GdkPixbuf *emblem;

			/* Overlay a disk in the bottom left corner */
			emblem = gtk_widget_render_icon(GTK_WIDGET(status_box->vbox),
						GTK_STOCK_SAVE, icon_size, "GtkGaimStatusBox");
			if (emblem != NULL)
			{
				int width, height;
				width = gdk_pixbuf_get_width(pixbuf) / 2;
				height = gdk_pixbuf_get_height(pixbuf) / 2;
				gdk_pixbuf_composite(emblem, pixbuf, 0, height,
							width, height, 0, height,
							0.5, 0.5, GDK_INTERP_BILINEAR, 255);
				g_object_unref(G_OBJECT(emblem));
			}
		}
	}

	if (status_box->account != NULL) {
		text = g_strdup_printf("%s%s<span size=\"smaller\" color=\"%s\">%s</span>",
						gaim_account_get_username(status_box->account),
						show_buddy_icons ? "\n" : " - ", aa_color,
						secondary ? secondary : primary);
	} else if (secondary != NULL) {
		char *separator;
		separator = show_buddy_icons ? "\n" : " - ";
		text = g_strdup_printf("%s<span size=\"smaller\" color=\"%s\">%s%s</span>",
							  primary, aa_color, separator, secondary);
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
			   ICON_COLUMN, pixbuf,
			   TEXT_COLUMN, text,
			   -1);
	if ((status_box->typing == 0) && (!status_box->connecting))
		g_object_unref(pixbuf);
	g_free(text);

	/* Make sure to activate the only row in the tree view */
	path = gtk_tree_path_new_from_string("0");
	gtk_cell_view_set_displayed_row(GTK_CELL_VIEW(status_box->cell_view), path);
	gtk_tree_path_free(path);

	update_size(status_box);
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
status_menu_refresh_iter(GtkGaimStatusBox *status_box)
{
	GaimSavedStatus *saved_status;
	GaimStatusPrimitive primitive;
	gint index;
	const char *message;

	/* this function is inappropriate for ones with accounts */
	if (status_box->account)
		return;

	saved_status = gaim_savedstatus_get_current();

	/*
	 * Suppress the "changed" signal because the status
	 * was changed programmatically.
	 */
	gtk_widget_set_sensitive(GTK_WIDGET(status_box), FALSE);

	/*
	 * If the saved_status is transient, is Available, Away, Invisible
	 * or Offline, and it does not have an substatuses, then select
	 * the primitive in the dropdown menu.  Otherwise select the
	 * popular status in the dropdown menu.
	 */
	primitive = gaim_savedstatus_get_type(saved_status);
	if (gaim_savedstatus_is_transient(saved_status) &&
		((primitive == GAIM_STATUS_AVAILABLE) || (primitive == GAIM_STATUS_AWAY) ||
		 (primitive == GAIM_STATUS_INVISIBLE) || (primitive == GAIM_STATUS_OFFLINE)) &&
		(!gaim_savedstatus_has_substatuses(saved_status)))
	{
		index = get_statusbox_index(status_box, saved_status);
		gtk_combo_box_set_active(GTK_COMBO_BOX(status_box), index);
	}
	else
	{
		GtkTreeIter iter;
		GtkGaimStatusBoxItemType type;
		gpointer data;

		/* Unset the active item */
		gtk_combo_box_set_active(GTK_COMBO_BOX(status_box), -1);

		/* If this saved status is in the list store, then set it as the active item */
		if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(status_box->dropdown_store), &iter))
		{
			do
			{
				gtk_tree_model_get(GTK_TREE_MODEL(status_box->dropdown_store), &iter,
							TYPE_COLUMN, &type,
							DATA_COLUMN, &data,
							-1);
				if ((type == GTK_GAIM_STATUS_BOX_TYPE_POPULAR) &&
					(GPOINTER_TO_INT(data) == gaim_savedstatus_get_creation_time(saved_status)))
				{
					/* Found! */
					gtk_combo_box_set_active_iter(GTK_COMBO_BOX(status_box), &iter);
					break;
				}
			}
			while (gtk_tree_model_iter_next(GTK_TREE_MODEL(status_box->dropdown_store), &iter));
		}
	}

	message = gaim_savedstatus_get_message(saved_status);
	if (!gaim_savedstatus_is_transient(saved_status) || !message || !*message)
	{
		status_box->imhtml_visible = FALSE;
		gtk_widget_hide_all(status_box->vbox);
	}
	else
	{
		status_box->imhtml_visible = TRUE;
		gtk_widget_show_all(status_box->vbox);

		/*
		 * Suppress the "changed" signal because the status
		 * was changed programmatically.
		 */
		gtk_widget_set_sensitive(GTK_WIDGET(status_box->imhtml), FALSE);

		gtk_imhtml_clear(GTK_IMHTML(status_box->imhtml));
		gtk_imhtml_clear_formatting(GTK_IMHTML(status_box->imhtml));
		gtk_imhtml_append_text(GTK_IMHTML(status_box->imhtml), message, 0);
		gtk_widget_set_sensitive(GTK_WIDGET(status_box->imhtml), TRUE);
	}

	update_size(status_box);

	/* Stop suppressing the "changed" signal. */
	gtk_widget_set_sensitive(GTK_WIDGET(status_box), TRUE);
}

static void
add_popular_statuses(GtkGaimStatusBox *statusbox)
{
	gboolean show_buddy_icons;
	GtkIconSize icon_size;
	GList *list, *cur;
	GdkPixbuf *pixbuf, *emblem;
	int width, height;

	list = gaim_savedstatuses_get_popular(6);
	if (list == NULL)
		/* Odd... oh well, nothing we can do about it. */
		return;

	show_buddy_icons = gaim_prefs_get_bool("/gaim/gtk/blist/show_buddy_icons");
	if (show_buddy_icons)
		icon_size = gtk_icon_size_from_name(GAIM_ICON_SIZE_STATUS);
	else
		icon_size = gtk_icon_size_from_name(GAIM_ICON_SIZE_STATUS_SMALL);

	gtk_gaim_status_box_add_separator(statusbox);

	for (cur = list; cur != NULL; cur = cur->next)
	{
		GaimSavedStatus *saved = cur->data;
		const gchar *message;
		gchar *stripped = NULL;

		/* Get an appropriate status icon */
		pixbuf = gaim_gtk_create_gaim_icon_with_status(
					gaim_savedstatus_get_type(saved),
					show_buddy_icons ? 1.0 : 0.5);

		if (gaim_savedstatus_is_transient(saved))
		{
			/*
			 * Transient statuses do not have a title, so the savedstatus
			 * API returns the message when gaim_savedstatus_get_title() is
			 * called, so we don't need to get the message a second time.
			 */
		}
		else
		{
			message = gaim_savedstatus_get_message(saved);
			if (message != NULL)
			{
				stripped = gaim_markup_strip_html(message);
				gaim_util_chrreplace(stripped, '\n', ' ');
			}

			/* Overlay a disk in the bottom left corner */
			emblem = gtk_widget_render_icon(GTK_WIDGET(statusbox->vbox),
						GTK_STOCK_SAVE, icon_size, "GtkGaimStatusBox");
			if (emblem != NULL)
			{
				width = gdk_pixbuf_get_width(pixbuf) / 2;
				height = gdk_pixbuf_get_height(pixbuf) / 2;
				gdk_pixbuf_composite(emblem, pixbuf, 0, height,
							width, height, 0, height,
							0.5, 0.5, GDK_INTERP_BILINEAR, 255);
				g_object_unref(G_OBJECT(emblem));
			}
		}

		gtk_gaim_status_box_add(statusbox, GTK_GAIM_STATUS_BOX_TYPE_POPULAR,
				pixbuf, gaim_savedstatus_get_title(saved), stripped,
				GINT_TO_POINTER(gaim_savedstatus_get_creation_time(saved)));
		g_free(stripped);
		if (pixbuf != NULL)
			g_object_unref(G_OBJECT(pixbuf));
	}

	g_list_free(list);
}

static void
gtk_gaim_status_box_regenerate(GtkGaimStatusBox *status_box)
{
	gboolean show_buddy_icons;
	GaimAccount *account;
	GdkPixbuf *pixbuf, *pixbuf2, *pixbuf3, *pixbuf4, *tmp;
	GtkIconSize icon_size;

	show_buddy_icons = gaim_prefs_get_bool("/gaim/gtk/blist/show_buddy_icons");
	if (show_buddy_icons)
		icon_size = gtk_icon_size_from_name(GAIM_ICON_SIZE_STATUS);
	else
		icon_size = gtk_icon_size_from_name(GAIM_ICON_SIZE_STATUS_SMALL);

	/* Unset the model while clearing it */
	gtk_combo_box_set_model(GTK_COMBO_BOX(status_box), NULL);
	gtk_list_store_clear(status_box->dropdown_store);
	/* Don't set the model until the new statuses have been added to the box.
	 * What is presumably a bug in Gtk < 2.4 causes things to get all confused
	 * if we do this here. */
	/* gtk_combo_box_set_model(GTK_COMBO_BOX(status_box), GTK_TREE_MODEL(status_box->dropdown_store)); */

	account = GTK_GAIM_STATUS_BOX(status_box)->account;
	if (account == NULL)
	{
		/* Global */
		pixbuf = gtk_widget_render_icon (GTK_WIDGET(status_box->vbox), GAIM_STOCK_STATUS_ONLINE,
		                                 icon_size, "GtkGaimStatusBox");
		pixbuf2 = gtk_widget_render_icon (GTK_WIDGET(status_box->vbox), GAIM_STOCK_STATUS_AWAY,
		                                  icon_size, "GtkGaimStatusBox");
		pixbuf3 = gtk_widget_render_icon (GTK_WIDGET(status_box->vbox), GAIM_STOCK_STATUS_OFFLINE,
		                                  icon_size, "GtkGaimStatusBox");
		pixbuf4 = gtk_widget_render_icon (GTK_WIDGET(status_box->vbox), GAIM_STOCK_STATUS_INVISIBLE,
		                                  icon_size, "GtkGaimStatusBox");

		gtk_gaim_status_box_add(GTK_GAIM_STATUS_BOX(status_box), GTK_GAIM_STATUS_BOX_TYPE_PRIMITIVE, pixbuf, _("Available"), NULL, GINT_TO_POINTER(GAIM_STATUS_AVAILABLE));
		gtk_gaim_status_box_add(GTK_GAIM_STATUS_BOX(status_box), GTK_GAIM_STATUS_BOX_TYPE_PRIMITIVE, pixbuf2, _("Away"), NULL, GINT_TO_POINTER(GAIM_STATUS_AWAY));
		gtk_gaim_status_box_add(GTK_GAIM_STATUS_BOX(status_box), GTK_GAIM_STATUS_BOX_TYPE_PRIMITIVE, pixbuf4, _("Invisible"), NULL, GINT_TO_POINTER(GAIM_STATUS_INVISIBLE));
		gtk_gaim_status_box_add(GTK_GAIM_STATUS_BOX(status_box), GTK_GAIM_STATUS_BOX_TYPE_PRIMITIVE, pixbuf3, _("Offline"), NULL, GINT_TO_POINTER(GAIM_STATUS_OFFLINE));

		add_popular_statuses(status_box);

		gtk_gaim_status_box_add_separator(GTK_GAIM_STATUS_BOX(status_box));
		gtk_gaim_status_box_add(GTK_GAIM_STATUS_BOX(status_box), GTK_GAIM_STATUS_BOX_TYPE_CUSTOM, pixbuf, _("New..."), NULL, NULL);
		gtk_gaim_status_box_add(GTK_GAIM_STATUS_BOX(status_box), GTK_GAIM_STATUS_BOX_TYPE_SAVED, pixbuf, _("Saved..."), NULL, NULL);

		gtk_combo_box_set_model(GTK_COMBO_BOX(status_box), GTK_TREE_MODEL(status_box->dropdown_store));
		status_menu_refresh_iter(status_box);

	} else {
		/* Per-account */
		const GList *l;

		for (l = gaim_account_get_status_types(account); l != NULL; l = l->next)
		{
			GaimStatusType *status_type = (GaimStatusType *)l->data;

			if (!gaim_status_type_is_user_settable(status_type))
				continue;

			tmp = gaim_gtk_create_prpl_icon_with_status(account, status_type,
					show_buddy_icons ? 1.0 : 0.5);
			gtk_gaim_status_box_add(GTK_GAIM_STATUS_BOX(status_box),
									GTK_GAIM_STATUS_BOX_TYPE_PRIMITIVE, tmp,
									gaim_status_type_get_name(status_type),
									NULL,
									GINT_TO_POINTER(gaim_status_type_get_primitive(status_type)));
			if (tmp != NULL)
				g_object_unref(tmp);
		}
		gtk_combo_box_set_model(GTK_COMBO_BOX(status_box), GTK_TREE_MODEL(status_box->dropdown_store));
		update_to_reflect_account_status(status_box, account, gaim_account_get_active_status(account));
	}
}

static gboolean combo_box_scroll_event_cb(GtkWidget *w, GdkEventScroll *event, GtkIMHtml *imhtml)
{
	gtk_combo_box_popup(GTK_COMBO_BOX(w));
	return TRUE;
}

static gboolean imhtml_scroll_event_cb(GtkWidget *w, GdkEventScroll *event, GtkIMHtml *imhtml)
{
	if (event->direction == GDK_SCROLL_UP)
		gtk_imhtml_page_up(imhtml);
	else if (event->direction == GDK_SCROLL_DOWN)
		gtk_imhtml_page_down(imhtml);
	return TRUE;
}

static int imhtml_remove_focus(GtkWidget *w, GdkEventKey *event, GtkGaimStatusBox *status_box)
{
	if (event->keyval == GDK_Tab || event->keyval == GDK_KP_Tab)
	{
		/* If last inserted character is a tab, then remove the focus from here */
		GtkWidget *top = gtk_widget_get_toplevel(w);
		g_signal_emit_by_name(G_OBJECT(top), "move_focus",
				(event->state & GDK_SHIFT_MASK) ?
				                  GTK_DIR_TAB_BACKWARD: GTK_DIR_TAB_FORWARD);
		return TRUE;
	}
	if (!status_box->typing != 0)
		return FALSE;

	/* Reset the status if Escape was pressed */
	if (event->keyval == GDK_Escape)
	{
		g_source_remove(status_box->typing);
		status_box->typing = 0;
		if (status_box->account != NULL)
			update_to_reflect_account_status(status_box, status_box->account,
							gaim_account_get_active_status(status_box->account));
		else
			status_menu_refresh_iter(status_box);
		return TRUE;
	}

	gtk_gaim_status_box_pulse_typing(status_box);
	g_source_remove(status_box->typing);
	status_box->typing = g_timeout_add(TYPING_TIMEOUT, (GSourceFunc)remove_typing_cb, status_box);

	return FALSE;
}

#if GTK_CHECK_VERSION(2,6,0)
static gboolean
dropdown_store_row_separator_func(GtkTreeModel *model,
								  GtkTreeIter *iter, gpointer data)
{
	GtkGaimStatusBoxItemType type;

	gtk_tree_model_get(model, iter, TYPE_COLUMN, &type, -1);

	if (type == GTK_GAIM_STATUS_BOX_TYPE_SEPARATOR)
		return TRUE;

	return FALSE;
}
#endif

static void
cache_pixbufs(GtkGaimStatusBox *status_box)
{
	GtkIconSize icon_size;

	if (gaim_prefs_get_bool("/gaim/gtk/blist/show_buddy_icons"))
	{
		g_object_set(G_OBJECT(status_box->icon_rend), "xpad", 6, NULL);
		icon_size = gtk_icon_size_from_name(GAIM_ICON_SIZE_STATUS_TWO_LINE);
	} else {
		g_object_set(G_OBJECT(status_box->icon_rend), "xpad", 3, NULL);
		icon_size = gtk_icon_size_from_name(GAIM_ICON_SIZE_STATUS_SMALL_TWO_LINE);
	}

	if (status_box->connecting_pixbufs[0] != NULL)
		gdk_pixbuf_unref(status_box->connecting_pixbufs[0]);
	if (status_box->connecting_pixbufs[1] != NULL)
		gdk_pixbuf_unref(status_box->connecting_pixbufs[1]);
	if (status_box->connecting_pixbufs[2] != NULL)
		gdk_pixbuf_unref(status_box->connecting_pixbufs[2]);
	if (status_box->connecting_pixbufs[3] != NULL)
		gdk_pixbuf_unref(status_box->connecting_pixbufs[3]);

	status_box->connecting_index = 0;
	status_box->connecting_pixbufs[0] = gtk_widget_render_icon (GTK_WIDGET(status_box->vbox), GAIM_STOCK_STATUS_CONNECT0,
								     icon_size, "GtkGaimStatusBox");
	status_box->connecting_pixbufs[1] = gtk_widget_render_icon (GTK_WIDGET(status_box->vbox), GAIM_STOCK_STATUS_CONNECT1,
								     icon_size, "GtkGaimStatusBox");
	status_box->connecting_pixbufs[2] = gtk_widget_render_icon (GTK_WIDGET(status_box->vbox), GAIM_STOCK_STATUS_CONNECT2,
								     icon_size, "GtkGaimStatusBox");
	status_box->connecting_pixbufs[3] = gtk_widget_render_icon (GTK_WIDGET(status_box->vbox), GAIM_STOCK_STATUS_CONNECT3,
								     icon_size, "GtkGaimStatusBox");

	if (status_box->typing_pixbufs[0] != NULL)
		gdk_pixbuf_unref(status_box->typing_pixbufs[0]);
	if (status_box->typing_pixbufs[1] != NULL)
		gdk_pixbuf_unref(status_box->typing_pixbufs[1]);
	if (status_box->typing_pixbufs[2] != NULL)
		gdk_pixbuf_unref(status_box->typing_pixbufs[2]);
	if (status_box->typing_pixbufs[3] != NULL)
		gdk_pixbuf_unref(status_box->typing_pixbufs[3]);

	status_box->typing_index = 0;
	status_box->typing_pixbufs[0] =  gtk_widget_render_icon (GTK_WIDGET(status_box->vbox), GAIM_STOCK_STATUS_TYPING0,
								     icon_size, "GtkGaimStatusBox");
	status_box->typing_pixbufs[1] =  gtk_widget_render_icon (GTK_WIDGET(status_box->vbox), GAIM_STOCK_STATUS_TYPING1,
								     icon_size, "GtkGaimStatusBox");
	status_box->typing_pixbufs[2] =  gtk_widget_render_icon (GTK_WIDGET(status_box->vbox), GAIM_STOCK_STATUS_TYPING2,
								     icon_size, "GtkGaimStatusBox");
	status_box->typing_pixbufs[3] =  gtk_widget_render_icon (GTK_WIDGET(status_box->vbox), GAIM_STOCK_STATUS_TYPING3,
								     icon_size, "GtkGaimStatusBox");
}

static void
current_savedstatus_changed_cb(GaimSavedStatus *now, GaimSavedStatus *old, gpointer data)
{
	GtkGaimStatusBox *status_box = data;

	/* Make sure our current status is added to the list of popular statuses */
	gtk_gaim_status_box_regenerate(status_box);

	if (status_box->account != NULL)
		update_to_reflect_account_status(status_box, status_box->account,
						gaim_account_get_active_status(status_box->account));
	else
		status_menu_refresh_iter(status_box);

	gtk_gaim_status_box_refresh(status_box);
}

static void
buddy_list_details_pref_changed_cb(const char *name, GaimPrefType type,
								   gconstpointer val, gpointer data)
{
	GtkGaimStatusBox *status_box = (GtkGaimStatusBox *)data;

	cache_pixbufs(status_box);
	gtk_gaim_status_box_regenerate(status_box);
	gtk_gaim_status_box_refresh(status_box);
}

static void
spellcheck_prefs_cb(const char *name, GaimPrefType type,
					gconstpointer value, gpointer data)
{
#ifdef USE_GTKSPELL
	GtkGaimStatusBox *status_box = (GtkGaimStatusBox *)data;

	if (value)
		gaim_gtk_setup_gtkspell(GTK_TEXT_VIEW(status_box->imhtml));
	else
	{
		GtkSpell *spell;
		spell = gtkspell_get_from_text_view(GTK_TEXT_VIEW(status_box->imhtml));
		gtkspell_detach(spell);
	}
#endif
}

#if 0
static gboolean button_released_cb(GtkWidget *widget, GdkEventButton *event, GtkGaimStatusBox *box)
{

	if (event->button != 1)
		return FALSE;
	gtk_combo_box_popdown(GTK_COMBO_BOX(box));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(box->toggle_button), FALSE);
	if (!box->imhtml_visible)
		g_signal_emit_by_name(G_OBJECT(box), "changed", NULL, NULL);
	return TRUE;
}

static gboolean button_pressed_cb(GtkWidget *widget, GdkEventButton *event, GtkGaimStatusBox *box)
{
	if (event->button != 1)
		return FALSE;
	gtk_combo_box_popup(GTK_COMBO_BOX(box));
	// Disabled until button_released_cb works
	// gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(box->toggle_button), TRUE);
	return TRUE;
}
#endif

static void
toggled_cb(GtkWidget *widget, GtkGaimStatusBox *box)
{
	gtk_combo_box_popup(GTK_COMBO_BOX(box));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(box->toggle_button), FALSE);
}

static void
icon_choose_cb(const char *filename, gpointer data)
{
	GtkGaimStatusBox *box;

	box = data;

	if (filename) {
		GList *accounts;

		if (box->account) {
			GaimPlugin *plug = gaim_find_prpl(gaim_account_get_protocol_id(box->account));
			if (plug) {
				GaimPluginProtocolInfo *prplinfo = GAIM_PLUGIN_PROTOCOL_INFO(plug);
				if (prplinfo && prplinfo->icon_spec.format) {
					char *icon = gaim_gtk_convert_buddy_icon(plug, filename);
					gaim_account_set_buddy_icon(box->account, icon);
					g_free(icon);
					gaim_account_set_ui_bool(box->account, GAIM_GTK_UI, "use-global-buddyicon", FALSE);
				}
			}
		} else {
			for (accounts = gaim_accounts_get_all(); accounts != NULL; accounts = accounts->next) {
				GaimAccount *account = accounts->data;
				GaimPlugin *plug = gaim_find_prpl(gaim_account_get_protocol_id(account));
				if (plug) {
					GaimPluginProtocolInfo *prplinfo = GAIM_PLUGIN_PROTOCOL_INFO(plug);
					if (prplinfo != NULL &&
					    gaim_account_get_ui_bool(account, GAIM_GTK_UI, "use-global-buddyicon", TRUE) &&
					    prplinfo->icon_spec.format) {
						char *icon = gaim_gtk_convert_buddy_icon(plug, filename);
						gaim_account_set_buddy_icon(account, icon);
						g_free(icon);
					}
				}
			}
		}
		gtk_gaim_status_box_set_buddy_icon(box, filename);
	}

	box->buddy_icon_sel = NULL;
}

static void
gtk_gaim_status_box_init (GtkGaimStatusBox *status_box)
{
	GtkCellRenderer *text_rend;
	GtkCellRenderer *icon_rend;
	GtkTextBuffer *buffer;

	status_box->imhtml_visible = FALSE;
	status_box->connecting = FALSE;
	status_box->typing = 0;
	status_box->toggle_button = gtk_toggle_button_new();
	status_box->hbox = gtk_hbox_new(FALSE, 6);
	status_box->cell_view = gtk_cell_view_new();
	status_box->vsep = gtk_vseparator_new();
	status_box->arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);

	status_box->store = gtk_list_store_new(NUM_COLUMNS, G_TYPE_INT, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
	status_box->dropdown_store = gtk_list_store_new(NUM_COLUMNS, G_TYPE_INT, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
	gtk_combo_box_set_model(GTK_COMBO_BOX(status_box), GTK_TREE_MODEL(status_box->dropdown_store));
	gtk_cell_view_set_model(GTK_CELL_VIEW(status_box->cell_view), GTK_TREE_MODEL(status_box->store));
	gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(status_box), 0);
	gtk_list_store_append(status_box->store, &(status_box->iter));

	gtk_container_add(GTK_CONTAINER(status_box->toggle_button), status_box->hbox);
	gtk_box_pack_start(GTK_BOX(status_box->hbox), status_box->cell_view, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(status_box->hbox), status_box->vsep, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(status_box->hbox), status_box->arrow, FALSE, FALSE, 0);
	gtk_widget_show_all(status_box->toggle_button);
#if GTK_CHECK_VERSION(2,4,0)
	gtk_button_set_focus_on_click(GTK_BUTTON(status_box->toggle_button), FALSE);
#endif

	text_rend = gtk_cell_renderer_text_new();
	icon_rend = gtk_cell_renderer_pixbuf_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(status_box), icon_rend, FALSE);
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(status_box), text_rend, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(status_box), icon_rend, "pixbuf", ICON_COLUMN, NULL);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(status_box), text_rend, "markup", TEXT_COLUMN, NULL);
#if GTK_CHECK_VERSION(2, 6, 0)
	g_object_set(text_rend, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
#endif

	status_box->icon_rend = gtk_cell_renderer_pixbuf_new();
	status_box->text_rend = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(status_box->cell_view), status_box->icon_rend, FALSE);
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(status_box->cell_view), status_box->text_rend, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(status_box->cell_view), status_box->icon_rend, "pixbuf", ICON_COLUMN, NULL);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(status_box->cell_view), status_box->text_rend, "markup", TEXT_COLUMN, NULL);
#if GTK_CHECK_VERSION(2, 6, 0)
	g_object_set(status_box->text_rend, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
#endif

	status_box->vbox = gtk_vbox_new(0, FALSE);
	status_box->sw = gaim_gtk_create_imhtml(FALSE, &status_box->imhtml, NULL, NULL);
	gtk_imhtml_set_editable(GTK_IMHTML(status_box->imhtml), TRUE);

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(status_box->imhtml));
#if 0
	g_signal_connect(G_OBJECT(status_box->toggle_button), "button-press-event",
			 G_CALLBACK(button_pressed_cb), status_box);
	g_signal_connect(G_OBJECT(status_box->toggle_button), "button-release-event",
			 G_CALLBACK(button_released_cb), status_box);
#endif
	g_signal_connect(G_OBJECT(status_box->toggle_button), "toggled",
	                 G_CALLBACK(toggled_cb), status_box);
	g_signal_connect(G_OBJECT(buffer), "changed", G_CALLBACK(imhtml_changed_cb), status_box);
	g_signal_connect(G_OBJECT(status_box->imhtml), "format_function_toggle",
			 G_CALLBACK(imhtml_format_changed_cb), status_box);
	g_signal_connect(G_OBJECT(status_box->imhtml), "key_press_event",
			 G_CALLBACK(imhtml_remove_focus), status_box);
	g_signal_connect_swapped(G_OBJECT(status_box->imhtml), "message_send", G_CALLBACK(remove_typing_cb), status_box);
	gtk_imhtml_set_editable(GTK_IMHTML(status_box->imhtml), TRUE);
#ifdef USE_GTKSPELL
	if (gaim_prefs_get_bool("/gaim/gtk/conversations/spellcheck"))
		gaim_gtk_setup_gtkspell(GTK_TEXT_VIEW(status_box->imhtml));
#endif
	gtk_widget_set_parent(status_box->vbox, GTK_WIDGET(status_box));
	gtk_widget_set_parent(status_box->toggle_button, GTK_WIDGET(status_box));
	GTK_BIN(status_box)->child = status_box->toggle_button;

	gtk_box_pack_start(GTK_BOX(status_box->vbox), status_box->sw, TRUE, TRUE, 0);

	g_signal_connect(G_OBJECT(status_box), "scroll_event", G_CALLBACK(combo_box_scroll_event_cb), NULL);
	g_signal_connect(G_OBJECT(status_box->imhtml), "scroll_event",
					G_CALLBACK(imhtml_scroll_event_cb), status_box->imhtml);

#if GTK_CHECK_VERSION(2,6,0)
	gtk_combo_box_set_row_separator_func(GTK_COMBO_BOX(status_box), dropdown_store_row_separator_func, NULL, NULL);
#endif

	cache_pixbufs(status_box);
	gtk_gaim_status_box_regenerate(status_box);
	gtk_gaim_status_box_refresh(status_box);

	gaim_signal_connect(gaim_savedstatuses_get_handle(), "savedstatus-changed",
						status_box,
						GAIM_CALLBACK(current_savedstatus_changed_cb),
						status_box);
	gaim_prefs_connect_callback(status_box, "/gaim/gtk/blist/show_buddy_icons",
								buddy_list_details_pref_changed_cb, status_box);
	gaim_prefs_connect_callback(status_box, "/gaim/gtk/conversations/spellcheck",
								spellcheck_prefs_cb, status_box);
}

static void
gtk_gaim_status_box_size_request(GtkWidget *widget,
								 GtkRequisition *requisition)
{
	GtkRequisition box_req;
	combo_box_size_request(widget, requisition);
	requisition->height += 3;

	/* If the gtkimhtml is visible, then add some additional padding */
	gtk_widget_size_request(GTK_GAIM_STATUS_BOX(widget)->vbox, &box_req);
	if (box_req.height > 1)
		requisition->height += box_req.height + 3;

	requisition->width = 1;


}

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
gtk_gaim_status_box_size_allocate(GtkWidget *widget,
				  GtkAllocation *allocation)
{
	GtkGaimStatusBox *status_box = GTK_GAIM_STATUS_BOX(widget);
	GtkRequisition req = {0,0};
	GtkAllocation parent_alc, box_alc, icon_alc;
	GdkPixbuf *scaled;

	combo_box_size_request(widget, &req);

	box_alc = *allocation;
	box_alc.height = MAX(1, (allocation->height - req.height - 6));
	box_alc.y += req.height + 6;
	gtk_widget_size_allocate((GTK_GAIM_STATUS_BOX(widget))->vbox, &box_alc);

	parent_alc = *allocation;
	parent_alc.height = MAX(1,req.height);
	parent_alc.y += 3;

	if (status_box->icon_box)
	{
		parent_alc.width -= (parent_alc.height + 3);
		icon_alc = *allocation;
		icon_alc.height = MAX(1,req.height);
		icon_alc.width = icon_alc.height;
		icon_alc.x = allocation->width - icon_alc.width;
		icon_alc.y += 3;

		if (status_box->icon_size != icon_alc.height)
		{
			if ((status_box->buddy_icon_path != NULL) &&
				(*status_box->buddy_icon_path != '\0'))
			{
				scaled = gdk_pixbuf_new_from_file_at_scale(status_box->buddy_icon_path,
									   icon_alc.height, icon_alc.width, FALSE, NULL);
				g_object_unref(status_box->buddy_icon_hover);
				status_box->buddy_icon_hover = gdk_pixbuf_copy(scaled);
				do_colorshift(status_box->buddy_icon_hover, status_box->buddy_icon_hover, 30);
				g_object_unref(status_box->buddy_icon);
				status_box->buddy_icon = scaled;
				gtk_image_set_from_pixbuf(GTK_IMAGE(status_box->icon), status_box->buddy_icon);
			}
			status_box->icon_size = icon_alc.height;
		}
		gtk_widget_size_allocate(status_box->icon_box, &icon_alc);
	}

	combo_box_size_allocate(widget, &parent_alc);
	gtk_widget_size_allocate(status_box->toggle_button, &parent_alc);
	widget->allocation = *allocation;
}

static gboolean
gtk_gaim_status_box_expose_event(GtkWidget *widget,
				 GdkEventExpose *event)
{
	GtkGaimStatusBox *status_box = GTK_GAIM_STATUS_BOX(widget);
	gtk_container_propagate_expose(GTK_CONTAINER(widget), status_box->vbox, event);
	gtk_container_propagate_expose(GTK_CONTAINER(widget), status_box->toggle_button, event);
	gtk_container_propagate_expose(GTK_CONTAINER(widget), status_box->icon_box, event);
	return FALSE;
}

static void
gtk_gaim_status_box_forall(GtkContainer *container,
						   gboolean include_internals,
						   GtkCallback callback,
						   gpointer callback_data)
{
	GtkGaimStatusBox *status_box = GTK_GAIM_STATUS_BOX (container);

	if (include_internals)
	{
		(* callback) (status_box->vbox, callback_data);
		(* callback) (status_box->toggle_button, callback_data);
		(* callback) (status_box->arrow, callback_data);
		if (status_box->icon_box)
			(* callback) (status_box->icon_box, callback_data);
	}

	combo_box_forall(container, include_internals, callback, callback_data);
}

GtkWidget *
gtk_gaim_status_box_new()
{
	return g_object_new(GTK_GAIM_TYPE_STATUS_BOX, "account", NULL, NULL);
}

GtkWidget *
gtk_gaim_status_box_new_with_account(GaimAccount *account)
{
	return g_object_new(GTK_GAIM_TYPE_STATUS_BOX, "account", account, NULL);
}

/**
 * Add a row to the dropdown menu.
 *
 * @param status_box The status box itself.
 * @param type       A GtkGaimStatusBoxItemType.
 * @param pixbuf     The icon to associate with this row in the menu.
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
 *                   GaimStatusPrimitive.  For saved statuses this is the
 *                   creation timestamp.
 */
void
gtk_gaim_status_box_add(GtkGaimStatusBox *status_box, GtkGaimStatusBoxItemType type, GdkPixbuf *pixbuf, const char *title, const char *desc, gpointer data)
{
	GtkTreeIter iter;
	char *text;

	if (desc == NULL)
	{
		text = g_markup_escape_text(title, -1);
	}
	else
	{
		gboolean show_buddy_icons;
		GtkStyle *style;
		char aa_color[8];
		gchar *escaped_title, *escaped_desc;

		show_buddy_icons = gaim_prefs_get_bool("/gaim/gtk/blist/show_buddy_icons");
		style = gtk_widget_get_style(GTK_WIDGET(status_box));
		snprintf(aa_color, sizeof(aa_color), "#%02x%02x%02x",
			 style->text_aa[GTK_STATE_NORMAL].red >> 8,
			 style->text_aa[GTK_STATE_NORMAL].green >> 8,
			 style->text_aa[GTK_STATE_NORMAL].blue >> 8);

		escaped_title = g_markup_escape_text(title, -1);
		escaped_desc = g_markup_escape_text(desc, -1);
		text = g_strdup_printf("%s%s<span color=\"%s\" size=\"smaller\">%s</span>",
					escaped_title,
					show_buddy_icons ? "\n" : " - ",
					aa_color, escaped_desc);
		g_free(escaped_title);
		g_free(escaped_desc);
	}

	gtk_list_store_append(status_box->dropdown_store, &iter);
	gtk_list_store_set(status_box->dropdown_store, &iter,
			TYPE_COLUMN, type,
			ICON_COLUMN, pixbuf,
			TEXT_COLUMN, text,
			TITLE_COLUMN, title,
			DESC_COLUMN, desc,
			DATA_COLUMN, data,
			-1);
	g_free(text);
}

void
gtk_gaim_status_box_add_separator(GtkGaimStatusBox *status_box)
{
	/* Don't do anything unless GTK actually supports
	 * gtk_combo_box_set_row_separator_func */
#if GTK_CHECK_VERSION(2,6,0)
	GtkTreeIter iter;

	gtk_list_store_append(status_box->dropdown_store, &iter);
	gtk_list_store_set(status_box->dropdown_store, &iter,
			   TYPE_COLUMN, GTK_GAIM_STATUS_BOX_TYPE_SEPARATOR,
			   -1);
#endif
}

void
gtk_gaim_status_box_set_connecting(GtkGaimStatusBox *status_box, gboolean connecting)
{
	if (!status_box)
		return;
	status_box->connecting = connecting;
	gtk_gaim_status_box_refresh(status_box);
}

void
gtk_gaim_status_box_set_buddy_icon(GtkGaimStatusBox *box, const char *filename)
{
	GdkPixbuf *scaled;
	g_free(box->buddy_icon_path);
	box->buddy_icon_path = g_strdup(filename);

	if ((filename != NULL) && (*filename != '\0'))
	{
		if (box->buddy_icon != NULL)
			g_object_unref(box->buddy_icon);
		scaled = gdk_pixbuf_new_from_file_at_scale(filename,
							   box->icon_size, box->icon_size, FALSE, NULL);
		if (scaled != NULL)
		{
			box->buddy_icon_hover = gdk_pixbuf_copy(scaled);
			do_colorshift(box->buddy_icon_hover, box->buddy_icon_hover, 30);
			box->buddy_icon = scaled;
			gtk_image_set_from_pixbuf(GTK_IMAGE(box->icon), box->buddy_icon);
		}
	}

	if (box->account == NULL)
		gaim_prefs_set_string("/gaim/gtk/accounts/buddyicon", filename);
}

const char*
gtk_gaim_status_box_get_buddy_icon(GtkGaimStatusBox *box)
{
	return box->buddy_icon_path;
}

void
gtk_gaim_status_box_pulse_connecting(GtkGaimStatusBox *status_box)
{
	if (!status_box)
		return;
	if (status_box->connecting_index == 3)
		status_box->connecting_index = 0;
	else
		status_box->connecting_index++;
	gtk_gaim_status_box_refresh(status_box);
}

static void
gtk_gaim_status_box_pulse_typing(GtkGaimStatusBox *status_box)
{
	if (status_box->typing_index == 3)
		status_box->typing_index = 0;
	else
		status_box->typing_index++;
	gtk_gaim_status_box_refresh(status_box);
}

static GaimStatusType *
find_status_type_by_index(const GaimAccount *account, gint active)
{
	const GList *l = gaim_account_get_status_types(account);
	gint i;

	for (i = 0; l; l = l->next) {
		GaimStatusType *status_type = l->data;
		if (!gaim_status_type_is_user_settable(status_type))
			continue;

		if (active == i)
			return status_type;
		i++;
	}

	return NULL;
}

static gboolean
message_changed(const char *one, const char *two)
{
	if (one == NULL && two == NULL)
		return FALSE;

	if (one == NULL || two == NULL)
		return TRUE;

	return (g_utf8_collate(one, two) != 0);
}

static void
activate_currently_selected_status(GtkGaimStatusBox *status_box)
{
	GtkGaimStatusBoxItemType type;
	gpointer data;
	gchar *title;
	GtkTreeIter iter;
	char *message;
	GaimSavedStatus *saved_status;
	gboolean changed = TRUE;

	if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(status_box), &iter))
		return;

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
	if (type != GTK_GAIM_STATUS_BOX_TYPE_PRIMITIVE)
		return;

	gtk_tree_model_get(GTK_TREE_MODEL(status_box->dropdown_store), &iter,
					   TITLE_COLUMN, &title, -1);

	message = gtk_gaim_status_box_get_message(status_box);
	if (!message || !*message)
	{
		gtk_widget_hide_all(status_box->vbox);
		status_box->imhtml_visible = FALSE;
		if (message != NULL)
		{
			g_free(message);
			message = NULL;
		}
	}

	if (status_box->account == NULL) {
		/* Global */
		/* Save the newly selected status to prefs.xml and status.xml */

		/* Has the status really been changed? */
		saved_status = gaim_savedstatus_get_current();
		if (gaim_savedstatus_get_type(saved_status) == GPOINTER_TO_INT(data) &&
		    !gaim_savedstatus_has_substatuses(saved_status))
		{
			if (!message_changed(gaim_savedstatus_get_message(saved_status), message))
				changed = FALSE;
		}

		if (changed)
		{
			/* If we've used this type+message before, lookup the transient status */
			saved_status = gaim_savedstatus_find_transient_by_type_and_message(
										GPOINTER_TO_INT(data), message);

			/* If this type+message is unique then create a new transient saved status */
			if (saved_status == NULL)
			{
				saved_status = gaim_savedstatus_new(NULL, GPOINTER_TO_INT(data));
				gaim_savedstatus_set_message(saved_status, message);
			}

			/* Set the status for each account */
			gaim_savedstatus_activate(saved_status);
		}
	} else {
		/* Per-account */
		gint active;
		GaimStatusType *status_type;
		GaimStatus *status;
		const char *id = NULL;

		status = gaim_account_get_active_status(status_box->account);

		g_object_get(G_OBJECT(status_box), "active", &active, NULL);

		status_type = find_status_type_by_index(status_box->account, active);
		id = gaim_status_type_get_id(status_type);

		if (strncmp(id, gaim_status_get_id(status), strlen(id)) == 0)
		{
			/* Selected status and previous status is the same */
			if (!message_changed(message, gaim_status_get_attr_string(status, "message")))
				changed = FALSE;
		}

		if (changed)
		{
			if (message)
				gaim_account_set_status(status_box->account, id,
										TRUE, "message", message, NULL);
			else
				gaim_account_set_status(status_box->account, id,
										TRUE, NULL);
		}
	}

	g_free(title);
	g_free(message);
}

static void update_size(GtkGaimStatusBox *status_box)
{
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	int wrapped_lines;
	int lines;
	GdkRectangle oneline;
	int height;
	int pad_top, pad_inside, pad_bottom;

	if (!status_box->imhtml_visible)
	{
		if (status_box->vbox != NULL)
			gtk_widget_set_size_request(status_box->vbox, -1, -1);
		return;
	}

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(status_box->imhtml));

	wrapped_lines = 1;
	gtk_text_buffer_get_start_iter(buffer, &iter);
	while (gtk_text_view_forward_display_line(GTK_TEXT_VIEW(status_box->imhtml), &iter))
		wrapped_lines++;

	lines = gtk_text_buffer_get_line_count(buffer);

	/* Show a maximum of 4 lines */
	lines = MIN(lines, 4);
	wrapped_lines = MIN(wrapped_lines, 4);

	gtk_text_buffer_get_start_iter(buffer, &iter);
	gtk_text_view_get_iter_location(GTK_TEXT_VIEW(status_box->imhtml), &iter, &oneline);

	pad_top = gtk_text_view_get_pixels_above_lines(GTK_TEXT_VIEW(status_box->imhtml));
	pad_bottom = gtk_text_view_get_pixels_below_lines(GTK_TEXT_VIEW(status_box->imhtml));
	pad_inside = gtk_text_view_get_pixels_inside_wrap(GTK_TEXT_VIEW(status_box->imhtml));

	height = (oneline.height + pad_top + pad_bottom) * lines;
	height += (oneline.height + pad_inside) * (wrapped_lines - lines);

	gtk_widget_set_size_request(status_box->vbox, -1, height + GAIM_HIG_BOX_SPACE);
}

static void remove_typing_cb(GtkGaimStatusBox *status_box)
{
	if (status_box->typing == 0)
	{
		/* Nothing has changed, so we don't need to do anything */
		status_menu_refresh_iter(status_box);
		return;
	}

	g_source_remove(status_box->typing);
	status_box->typing = 0;

	activate_currently_selected_status(status_box);
	gtk_gaim_status_box_refresh(status_box);
}

static void gtk_gaim_status_box_changed(GtkComboBox *box)
{
	GtkGaimStatusBox *status_box;
	GtkTreeIter iter;
	GtkGaimStatusBoxItemType type;
	gpointer data;
	GList *accounts = NULL, *node;

	status_box = GTK_GAIM_STATUS_BOX(box);

	if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(status_box), &iter))
		return;
	gtk_tree_model_get(GTK_TREE_MODEL(status_box->dropdown_store), &iter,
			   TYPE_COLUMN, &type,
			   DATA_COLUMN, &data,
			   -1);
	if (status_box->typing != 0)
		g_source_remove(status_box->typing);
	status_box->typing = 0;

	if (GTK_WIDGET_IS_SENSITIVE(GTK_WIDGET(status_box)))
	{
		if (type == GTK_GAIM_STATUS_BOX_TYPE_POPULAR)
		{
			GaimSavedStatus *saved;
			saved = gaim_savedstatus_find_by_creation_time(GPOINTER_TO_INT(data));
			g_return_if_fail(saved != NULL);
			gaim_savedstatus_activate(saved);
			return;
		}

		if (type == GTK_GAIM_STATUS_BOX_TYPE_CUSTOM)
		{
			GaimSavedStatus *saved_status;
			saved_status = gaim_savedstatus_get_current();
			gaim_gtk_status_editor_show(FALSE,
				gaim_savedstatus_is_transient(saved_status)
					? saved_status : NULL);
			status_menu_refresh_iter(status_box);
			return;
		}

		if (type == GTK_GAIM_STATUS_BOX_TYPE_SAVED)
		{
			gaim_gtk_status_window_show();
			status_menu_refresh_iter(status_box);
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
		accounts = gaim_accounts_get_all_active();
	status_box->imhtml_visible = FALSE;
	for (node = accounts; node != NULL; node = node->next)
	{
		GaimAccount *account;
		GaimStatusType *status_type;

		account = node->data;
		status_type = gaim_account_get_status_type_with_primitive(account, GPOINTER_TO_INT(data));
		if ((status_type != NULL) &&
			(gaim_status_type_get_attr(status_type, "message") != NULL))
		{
			status_box->imhtml_visible = TRUE;
			break;
		}
	}
	g_list_free(accounts);

	if (GTK_WIDGET_IS_SENSITIVE(GTK_WIDGET(status_box)))
	{
		if (status_box->imhtml_visible)
		{
			gtk_widget_show_all(status_box->vbox);
			if (GTK_WIDGET_IS_SENSITIVE(GTK_WIDGET(status_box))) {
				status_box->typing = g_timeout_add(TYPING_TIMEOUT, (GSourceFunc)remove_typing_cb, status_box);
			}
			gtk_widget_grab_focus(status_box->imhtml);
			gtk_imhtml_clear(GTK_IMHTML(status_box->imhtml));
		}
		else
		{
			gtk_widget_hide_all(status_box->vbox);
			if (GTK_WIDGET_IS_SENSITIVE(GTK_WIDGET(status_box)))
				activate_currently_selected_status(status_box); /* This is where we actually set the status */
		}
	}
	gtk_gaim_status_box_refresh(status_box);
}

static gint
get_statusbox_index(GtkGaimStatusBox *box, GaimSavedStatus *saved_status)
{
	gint index;

	switch (gaim_savedstatus_get_type(saved_status))
	{
		case GAIM_STATUS_AVAILABLE:
			index = 0;
			break;
		case GAIM_STATUS_AWAY:
			index = 1;
			break;
		case GAIM_STATUS_INVISIBLE:
			index = 2;
			break;
		case GAIM_STATUS_OFFLINE:
			index = 3;
			break;
		default:
			index = -1;
			break;
	}

	return index;
}

static void imhtml_changed_cb(GtkTextBuffer *buffer, void *data)
{
	GtkGaimStatusBox *status_box = (GtkGaimStatusBox*)data;
	if (GTK_WIDGET_IS_SENSITIVE(GTK_WIDGET(status_box)))
	{
		if (status_box->typing != 0) {
			gtk_gaim_status_box_pulse_typing(status_box);
			g_source_remove(status_box->typing);
		}
		status_box->typing = g_timeout_add(TYPING_TIMEOUT, (GSourceFunc)remove_typing_cb, status_box);
	}
	gtk_gaim_status_box_refresh(status_box);
}

static void imhtml_format_changed_cb(GtkIMHtml *imhtml, GtkIMHtmlButtons buttons, void *data)
{
	imhtml_changed_cb(NULL, data);
}

char *gtk_gaim_status_box_get_message(GtkGaimStatusBox *status_box)
{
	if (status_box->imhtml_visible)
		return gtk_imhtml_get_markup(GTK_IMHTML(status_box->imhtml));
	else
		return NULL;
}
