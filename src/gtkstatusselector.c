/**
 * @file gtkstatusselector.c Status selector widget
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
#include "gtkimhtml.h"
#include "gtkstatusselector.h"
#include "gtkutils.h"

#include "account.h"
#include "debug.h"
#include "prefs.h"

struct _GaimGtkStatusSelectorPrivate
{
	GtkWidget *combo;
	GtkWidget *entry;
	GtkWidget *frame;

#if GTK_CHECK_VERSION(2,4,0)
	GtkListStore *model;
#endif

	guint entry_timer;
};

#if GTK_CHECK_VERSION(2,4,0)
enum
{
	COLUMN_STATUS_TYPE_ID,
	COLUMN_ICON,
	COLUMN_NAME,
	NUM_COLUMNS
};
#endif /* GTK >= 2.4.0 */

static void gaim_gtk_status_selector_class_init(GaimGtkStatusSelectorClass *klass);
static void gaim_gtk_status_selector_init(GaimGtkStatusSelector *selector);
static void gaim_gtk_status_selector_finalize(GObject *obj);
static void gaim_gtk_status_selector_destroy(GtkObject *obj);
static void status_switched_cb(GtkWidget *combo, GaimGtkStatusSelector *selector);
static gboolean key_press_cb(GtkWidget *entry, GdkEventKey *event, gpointer user_data);
static void signed_on_off_cb(GaimConnection *gc, GaimGtkStatusSelector *selector);
static void rebuild_list(GaimGtkStatusSelector *selector);

static GtkVBox *parent_class = NULL;

GType
gaim_gtk_status_selector_get_type(void)
{
	static GType type = 0;

	if (!type)
	{
		static const GTypeInfo info =
		{
			sizeof(GaimGtkStatusSelectorClass),
			NULL,
			NULL,
			(GClassInitFunc)gaim_gtk_status_selector_class_init,
			NULL,
			NULL,
			sizeof(GaimGtkStatusSelector),
			0,
			(GInstanceInitFunc)gaim_gtk_status_selector_init
		};

		type = g_type_register_static(GTK_TYPE_VBOX,
									  "GaimGtkStatusSelector", &info, 0);
	}

	return type;
}

static void
gaim_gtk_status_selector_class_init(GaimGtkStatusSelectorClass *klass)
{
	GObjectClass *gobject_class;
	GtkObjectClass *object_class;

	parent_class = g_type_class_peek_parent(klass);

	gobject_class = G_OBJECT_CLASS(klass);
	object_class  = GTK_OBJECT_CLASS(klass);

	gobject_class->finalize = gaim_gtk_status_selector_finalize;

	object_class->destroy = gaim_gtk_status_selector_destroy;
}

static void
gaim_gtk_status_selector_init(GaimGtkStatusSelector *selector)
{
	GtkWidget *combo;
	GtkWidget *entry;
	GtkWidget *toolbar;
	GtkWidget *frame;
#if GTK_CHECK_VERSION(2,4,0)
	GtkCellRenderer *renderer;
#endif

	selector->priv = g_new0(GaimGtkStatusSelectorPrivate, 1);

#if GTK_CHECK_VERSION(2,4,0)
	selector->priv->model = gtk_list_store_new(NUM_COLUMNS, G_TYPE_POINTER,
											   GDK_TYPE_PIXBUF, G_TYPE_STRING);

	combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(selector->priv->model));
	selector->priv->combo = combo;

	g_object_unref(G_OBJECT(selector->priv->model));

	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), renderer, FALSE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo), renderer,
								   "pixbuf", COLUMN_ICON,
								   NULL);

	renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), renderer, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo), renderer,
								   "text", COLUMN_NAME,
								   NULL);

	g_signal_connect(G_OBJECT(combo), "changed",
					 G_CALLBACK(status_switched_cb), selector);
#else /* GTK < 2.4.0 */

	/* TODO */

#endif /* GTK < 2.4.0 */

	gtk_widget_show(combo);
	gtk_box_pack_start(GTK_BOX(selector), combo, FALSE, FALSE, 0);

	frame = gaim_gtk_create_imhtml(TRUE, &entry, &toolbar);
	selector->priv->entry = entry;
	selector->priv->frame = frame;
	gtk_widget_set_name(entry, "gaim_gtkstatusselector_imhtml");
	gtk_box_pack_start(GTK_BOX(selector), frame, TRUE, TRUE, 0);
	gtk_widget_hide(toolbar);

	g_signal_connect(G_OBJECT(entry), "key_press_event",
					 G_CALLBACK(key_press_cb), selector);

	gaim_signal_connect(gaim_connections_get_handle(), "signed-on",
						selector, GAIM_CALLBACK(signed_on_off_cb),
						selector);
	gaim_signal_connect(gaim_connections_get_handle(), "signed-off",
						selector, GAIM_CALLBACK(signed_on_off_cb),
						selector);

	rebuild_list(selector);
}

static void
gaim_gtk_status_selector_finalize(GObject *obj)
{
	GaimGtkStatusSelector *selector;

	g_return_if_fail(obj != NULL);
	g_return_if_fail(GAIM_GTK_IS_STATUS_SELECTOR(obj));

	selector = GAIM_GTK_STATUS_SELECTOR(obj);

	g_free(selector->priv);

	if (G_OBJECT_CLASS(parent_class)->finalize)
		G_OBJECT_CLASS(parent_class)->finalize(obj);
}

static void
gaim_gtk_status_selector_destroy(GtkObject *obj)
{
	GaimGtkStatusSelector *selector;

	g_return_if_fail(obj != NULL);
	g_return_if_fail(GAIM_GTK_IS_STATUS_SELECTOR(obj));

	selector = GAIM_GTK_STATUS_SELECTOR(obj);

	gaim_signals_disconnect_by_handle(selector);

	if (GTK_OBJECT_CLASS(parent_class)->destroy)
		GTK_OBJECT_CLASS(parent_class)->destroy(obj);
}

static void
status_switched_cb(GtkWidget *combo, GaimGtkStatusSelector *selector)
{
	GtkTreeIter iter;
	const char *status_type_id;
	const char *text;

	if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(selector->priv->combo),
									  &iter))
	{
		return;
	}

	gtk_tree_model_get(GTK_TREE_MODEL(selector->priv->model), &iter,
					   COLUMN_NAME, &text,
					   COLUMN_STATUS_TYPE_ID, &status_type_id,
					   -1);

	if (status_type_id == NULL)
	{
		if (!strcmp(text, _("New Status")))
		{
			/* TODO */
		}
	}
	else
	{
		/*
		 * If the chosen status does not require a message, then set the
		 * status immediately.  Otherwise just register a timeout and the
		 * status will be set whenever the user stops typing the message.
		 */
		GList *l;
		GtkTextBuffer *buffer;
		gboolean allow_message = FALSE;

		buffer =
			gtk_text_view_get_buffer(GTK_TEXT_VIEW(selector->priv->entry));

		gtk_text_buffer_set_text(buffer, text, -1);

		for (l = gaim_connections_get_all(); l != NULL; l = l->next)
		{
			GaimConnection *gc = (GaimConnection *)l->data;
			GaimAccount *account = gaim_connection_get_account(gc);
			GaimStatusType *status_type;

			status_type = gaim_account_get_status_type(account,
													   status_type_id);

			if (status_type == NULL)
				continue;

			if (gaim_status_type_get_attr(status_type, "message") != NULL)
			{
				allow_message = TRUE;
			}
			else
			{
				gaim_account_set_status(account,
										status_type_id, TRUE,
										NULL);
			}
		}

		if (allow_message)
		{
			gtk_widget_show(selector->priv->frame);
			key_press_cb(NULL, NULL, selector);
		}
		else
			gtk_widget_hide(selector->priv->frame);
	}
}

static gboolean
insert_text_timeout_cb(gpointer data)
{
	GaimGtkStatusSelector *selector = (GaimGtkStatusSelector *)data;
	GtkTreeIter iter;
	const char *status_type_id;
	const char *text;

	if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(selector->priv->combo),
									  &iter))
	{
		return FALSE;
	}

	gtk_tree_model_get(GTK_TREE_MODEL(selector->priv->model), &iter,
					   COLUMN_NAME, &text,
					   COLUMN_STATUS_TYPE_ID, &status_type_id,
					   -1);

	if (status_type_id == NULL)
	{
		if (!strcmp(text, _("New Status")))
		{
			/* TODO */
		}
	}
	else
	{
		gchar *message;
		GList *l;

		message = gtk_imhtml_get_markup(GTK_IMHTML(selector->priv->entry));

		for (l = gaim_connections_get_all(); l != NULL; l = l->next)
		{
			GaimConnection *gc = (GaimConnection *)l->data;
			GaimAccount *account = gaim_connection_get_account(gc);
			GaimStatusType *status_type;

			status_type = gaim_account_get_status_type(account,
													   status_type_id);

			if (status_type == NULL)
				continue;

			if (gaim_status_type_get_attr(status_type, "message") != NULL)
			{
				gaim_account_set_status(account,
										status_type_id, TRUE,
										"message", message,
										NULL);
			}
		}
	}

	return FALSE;
}

/**
 * The user typed in the IMHTML entry widget.  If the user is finished
 * typing then we want to set the appropriate status message.  So let's
 * wait 3 seconds, and if they haven't typed anything else then set the
 * status message.
 */
static gboolean
key_press_cb(GtkWidget *entry, GdkEventKey *event, gpointer user_data)
{
	GaimGtkStatusSelector *selector = (GaimGtkStatusSelector *)user_data;

	if (selector->priv->entry_timer != 0) {
		gaim_timeout_remove(selector->priv->entry_timer);
	}

	selector->priv->entry_timer = gaim_timeout_add(3000, insert_text_timeout_cb,
											 selector);

	return FALSE;
}

static void
signed_on_off_cb(GaimConnection *gc, GaimGtkStatusSelector *selector)
{
	rebuild_list(selector);
}

static GdkPixbuf *
load_icon(const char *basename)
{
	char *filename;
	GdkPixbuf *pixbuf, *scale = NULL;

	if (!strcmp(basename, "available.png"))
		basename = "online.png";
	else if (!strcmp(basename, "hidden.png"))
		basename = "invisible.png";

	filename = g_build_filename(DATADIR, "pixmaps", "gaim", "icons",
								basename, NULL);
	pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
	g_free(filename);

	if (pixbuf != NULL)
	{
		scale = gdk_pixbuf_scale_simple(pixbuf, 16, 16,
										GDK_INTERP_BILINEAR);

		g_object_unref(G_OBJECT(pixbuf));
	}
	else
	{
		filename = g_build_filename(DATADIR, "pixmaps", "gaim", "status",
									"default", basename, NULL);
		scale = gdk_pixbuf_new_from_file(filename, NULL);
		g_free(filename);
	}

	return scale;
}

static void
add_item(GaimGtkStatusSelector *selector, const char *status_type_id,
		 const char *text, GdkPixbuf *pixbuf)
{
	GtkTreeIter iter;

	gtk_list_store_append(selector->priv->model, &iter);
	gtk_list_store_set(selector->priv->model, &iter,
					   COLUMN_STATUS_TYPE_ID, status_type_id,
					   COLUMN_ICON, pixbuf,
					   COLUMN_NAME, text,
					   -1);

	if (pixbuf != NULL)
		g_object_unref(G_OBJECT(pixbuf));
}

static void
rebuild_list(GaimGtkStatusSelector *selector)
{
	gboolean single_prpl = TRUE;
	GaimAccount *first_account = NULL;
	const char *first_prpl_type = NULL;
	GList *l;

	g_return_if_fail(selector != NULL);
	g_return_if_fail(GAIM_GTK_IS_STATUS_SELECTOR(selector));

	gtk_list_store_clear(selector->priv->model);

	/*
	 * If the user only has one IM account or one type of IM account
	 * connected, they'll see all their statuses. This is ideal for those
	 * who use only one account, or one single protocol. Everyone else
	 * gets Available and Away and a list of saved statuses.
	 */
	for (l = gaim_connections_get_all(); l != NULL && single_prpl; l = l->next)
	{
		GaimConnection *gc = (GaimConnection *)l->data;
		GaimAccount *account = gaim_connection_get_account(gc);
		GaimPluginProtocolInfo *prpl_info;
		const char *basename;

		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);
		basename = prpl_info->list_icon(account, NULL);

		if (first_prpl_type == NULL)
		{
			first_prpl_type = basename;
			first_account = account;
		}
		else if (strcmp(first_prpl_type, basename))
			single_prpl = FALSE;
	}

	if (single_prpl)
	{
		const GList *l;

		for (l = gaim_account_get_status_types(first_account);
			 l != NULL;
			 l = l->next)
		{
			GaimStatusType *status_type = (GaimStatusType *)l->data;
			char filename[BUFSIZ];

			if (!gaim_status_type_is_user_settable(status_type))
				continue;

			/*
			 * TODO Find a way to fallback to the GaimStatusPrimitive
			 * if an icon for this id does not exist.
			 */
			g_snprintf(filename, sizeof(filename), "%s.png",
					   gaim_status_type_get_id(status_type));

			add_item(selector,
					 gaim_status_type_get_id(status_type),
					 gaim_status_type_get_name(status_type),
					 load_icon(filename));
		}
	}
	else
	{
		add_item(selector, "available", _("Available"),
				 load_icon("online.png"));
		add_item(selector, "away", _("Away"), load_icon("away.png"));
	}

	/* TODO: Add saved statuses here? */

	add_item(selector, NULL, _("New Status"),
			 gtk_widget_render_icon(GTK_WIDGET(selector), GTK_STOCK_NEW,
									GTK_ICON_SIZE_MENU, NULL));
}

GtkWidget *
gaim_gtk_status_selector_new(void)
{
	GaimGtkStatusSelector *selector;

	selector = g_object_new(GAIM_GTK_TYPE_STATUS_SELECTOR, NULL);

	return GTK_WIDGET(selector);
}
