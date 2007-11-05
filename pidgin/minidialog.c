/**
 * @file minidialog.c Implementation of the #PidginMiniDialog Gtk widget.
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

#include <gtk/gtkhbox.h>
#include <gtk/gtkbutton.h>

#include "libpurple/prefs.h"

#include "pidgin/minidialog.h"
#include "pidgin/pidgin.h"
#include "pidgin/pidginstock.h"

G_DEFINE_TYPE (PidginMiniDialog, pidgin_mini_dialog, GTK_TYPE_VBOX)

enum
{
	PROP_TITLE = 1,
	PROP_DESCRIPTION,
	PROP_ICON_NAME,

	LAST_PROPERTY
} HazeConnectionProperties;

typedef struct _PidginMiniDialogPrivate
{
	GtkImage *icon;
	GtkBox *title_box;
	GtkLabel *title;
	GtkLabel *desc;
	GtkBox *buttons;
} PidginMiniDialogPrivate;

#define PIDGIN_MINI_DIALOG_GET_PRIVATE(dialog) \
	((PidginMiniDialogPrivate *) ((dialog)->priv))

PidginMiniDialog *
pidgin_mini_dialog_new(const gchar *title,
                       const gchar *description,
                       const gchar *icon_name)
{
	PidginMiniDialog *mini_dialog = g_object_new(PIDGIN_TYPE_MINI_DIALOG,
		"title", title,
		"description", description,
		"icon-name", icon_name,
		NULL);

	return mini_dialog;
}

void
pidgin_mini_dialog_set_title(PidginMiniDialog *mini_dialog,
                             const char *title)
{
	g_object_set(G_OBJECT(mini_dialog), "title", title, NULL);
}

void
pidgin_mini_dialog_set_description(PidginMiniDialog *mini_dialog,
                                   const char *description)
{
	g_object_set(G_OBJECT(mini_dialog), "description", description, NULL);
}

void
pidgin_mini_dialog_set_icon_name(PidginMiniDialog *mini_dialog,
                                 const char *icon_name)
{
	g_object_set(G_OBJECT(mini_dialog), "icon_name", icon_name, NULL);
}

struct _mini_dialog_button_clicked_cb_data
{
	PidginMiniDialog *mini_dialog;
	PidginMiniDialogCallback callback;
	gpointer user_data;
};

guint
pidgin_mini_dialog_get_num_children(PidginMiniDialog *mini_dialog)
{
	return g_list_length(mini_dialog->contents->children);
}

static gboolean
idle_destroy_cb(GtkWidget *mini_dialog)
{
	gtk_widget_destroy(mini_dialog);
	return FALSE;
}

static void
mini_dialog_button_clicked_cb(GtkButton *button,
                              gpointer user_data)
{
	struct _mini_dialog_button_clicked_cb_data *data = user_data;

	data->callback(data->mini_dialog, button, data->user_data);

	g_idle_add((GSourceFunc) idle_destroy_cb, data->mini_dialog);
}

static void
mini_dialog_button_destroy_cb(GtkButton *button,
                              gpointer user_data)
{
	struct _mini_dialog_button_clicked_cb_data *data = user_data;
	g_free(data);
}

void
pidgin_mini_dialog_add_button(PidginMiniDialog *self,
                              const char *text,
                              PidginMiniDialogCallback clicked_cb,
                              gpointer user_data)
{
	PidginMiniDialogPrivate *priv = PIDGIN_MINI_DIALOG_GET_PRIVATE(self);
	struct _mini_dialog_button_clicked_cb_data *callback_data
		= g_new0(struct _mini_dialog_button_clicked_cb_data, 1);
	GtkWidget *button = gtk_button_new();
	GtkWidget *label = gtk_label_new(NULL);
	char *button_text =
		g_strdup_printf("<span size=\"smaller\">%s</span>", text);

	gtk_label_set_markup_with_mnemonic(GTK_LABEL(label), button_text);
	g_free(button_text);

	callback_data->mini_dialog = self;
	callback_data->callback = clicked_cb;
	callback_data->user_data = user_data;
	g_signal_connect(G_OBJECT(button), "clicked",
		(GCallback) mini_dialog_button_clicked_cb, callback_data);
	g_signal_connect(G_OBJECT(button), "destroy",
		(GCallback) mini_dialog_button_destroy_cb, callback_data);

	gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0.5);
	gtk_container_add(GTK_CONTAINER(button), label);

	gtk_box_pack_end(GTK_BOX(priv->buttons), button, FALSE, FALSE,
		0);
	gtk_widget_show_all(GTK_WIDGET(button));
}

static void
pidgin_mini_dialog_get_property(GObject *object,
                                guint property_id,
                                GValue *value,
                                GParamSpec *pspec)
{
	PidginMiniDialog *self = PIDGIN_MINI_DIALOG(object);
	PidginMiniDialogPrivate *priv = PIDGIN_MINI_DIALOG_GET_PRIVATE(self);

	switch (property_id) {
		case PROP_TITLE:
			g_value_set_string(value, gtk_label_get_text(priv->title));
			break;
		case PROP_DESCRIPTION:
			g_value_set_string(value, gtk_label_get_text(priv->desc));
			break;
		case PROP_ICON_NAME:
		{
			gchar *icon_name = NULL;
			GtkIconSize size;
			gtk_image_get_stock(priv->icon, &icon_name, &size);
			g_value_set_string(value, icon_name);
			break;
		}
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
mini_dialog_set_title(PidginMiniDialog *self,
                      const char *title)
{
	PidginMiniDialogPrivate *priv = PIDGIN_MINI_DIALOG_GET_PRIVATE(self);

	char *title_esc = g_markup_escape_text(title, -1);
	char *title_markup = g_strdup_printf(
		"<span weight=\"bold\" size=\"smaller\">%s</span>",
		title_esc ? title_esc : "");

	gtk_label_set_markup(priv->title, title_markup);

	g_free(title_esc);
	g_free(title_markup);
}

static void
mini_dialog_set_description(PidginMiniDialog *self,
                            const char *description)
{
	PidginMiniDialogPrivate *priv = PIDGIN_MINI_DIALOG_GET_PRIVATE(self);
	if(description)
	{
		char *desc_esc = g_markup_escape_text(description, -1);
		char *desc_markup = g_strdup_printf(
			"<span size=\"smaller\">%s</span>", desc_esc);

		gtk_label_set_markup(priv->desc, desc_markup);

		g_free(desc_esc);
		g_free(desc_markup);

		gtk_widget_show(GTK_WIDGET(priv->desc));
	}
	else
	{
		gtk_label_set_text(priv->desc, NULL);
		gtk_widget_hide(GTK_WIDGET(priv->desc));
	}
}

static void
pidgin_mini_dialog_set_property(GObject *object,
                                guint property_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
	PidginMiniDialog *self = PIDGIN_MINI_DIALOG(object);
	PidginMiniDialogPrivate *priv = PIDGIN_MINI_DIALOG_GET_PRIVATE(self);

	switch (property_id) {
		case PROP_TITLE:
			mini_dialog_set_title(self, g_value_get_string(value));
			break;
		case PROP_DESCRIPTION:
			mini_dialog_set_description(self, g_value_get_string(value));
			break;
		case PROP_ICON_NAME:
			gtk_image_set_from_stock(priv->icon, g_value_get_string(value),
				gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
pidgin_mini_dialog_finalize(GObject *object)
{
	PidginMiniDialog *self = PIDGIN_MINI_DIALOG(object);
	PidginMiniDialogPrivate *priv = PIDGIN_MINI_DIALOG_GET_PRIVATE(self);

	g_free(priv);
	self->priv = NULL;

	G_OBJECT_CLASS (pidgin_mini_dialog_parent_class)->finalize (object);
}

static void
pidgin_mini_dialog_class_init(PidginMiniDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GParamSpec *param_spec;

	object_class->get_property = pidgin_mini_dialog_get_property;
	object_class->set_property = pidgin_mini_dialog_set_property;
	object_class->finalize = pidgin_mini_dialog_finalize;

	param_spec = g_param_spec_string("title", "title",
		"String specifying the mini-dialog's title", NULL,
#if GTK_CHECK_VERSION(2,8,0)
		G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB |
#endif
		G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_TITLE, param_spec);

	param_spec = g_param_spec_string("description", "description",
		"Description text for the mini-dialog, if desired", NULL,
#if GTK_CHECK_VERSION(2,8,0)
		G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB |
#endif
		G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_DESCRIPTION, param_spec);

	param_spec = g_param_spec_string("icon-name", "icon-name",
		"String specifying the Gtk stock name of the dialog's icon",
		NULL,
#if GTK_CHECK_VERSION(2,8,0)
		G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB |
#endif
		G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_ICON_NAME, param_spec);
}

/* 16 is the width of the icon, due to PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL */
#define BLIST_WIDTH_OTHER_THAN_LABEL \
	((PIDGIN_HIG_BOX_SPACE * 3) + 16)

static void
pidgin_mini_dialog_init(PidginMiniDialog *self)
{
	GtkBox *self_box = GTK_BOX(self);
	guint blist_width = purple_prefs_get_int(PIDGIN_PREFS_ROOT "/blist/width");
	guint label_width = blist_width - BLIST_WIDTH_OTHER_THAN_LABEL;

	PidginMiniDialogPrivate *priv = g_new0(PidginMiniDialogPrivate, 1);
	self->priv = priv;

	gtk_container_set_border_width(GTK_CONTAINER(self), PIDGIN_HIG_BOX_SPACE);

	priv->title_box = GTK_BOX(gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE));

	priv->icon = GTK_IMAGE(gtk_image_new());
	gtk_misc_set_alignment(GTK_MISC(priv->icon), 0, 0);

	priv->title = GTK_LABEL(gtk_label_new(NULL));
	/* TODO: update this request when /blist/width updates. */
	gtk_widget_set_size_request(GTK_WIDGET(priv->title), label_width, -1);
	gtk_label_set_line_wrap(priv->title, TRUE);
	gtk_misc_set_alignment(GTK_MISC(priv->title), 0, 0);

	gtk_box_pack_start(priv->title_box, GTK_WIDGET(priv->icon), FALSE, FALSE, 0);
	gtk_box_pack_start(priv->title_box, GTK_WIDGET(priv->title), TRUE, TRUE, 0);

	priv->desc = GTK_LABEL(gtk_label_new(NULL));
	/* TODO: update this request when /blist/width updates. */
	gtk_widget_set_size_request(GTK_WIDGET(priv->desc), label_width, -1);
	gtk_label_set_line_wrap(priv->desc, TRUE);
	gtk_misc_set_alignment(GTK_MISC(priv->desc), 0, 0);

	self->contents = GTK_BOX(gtk_vbox_new(FALSE, 0));

	priv->buttons = GTK_BOX(gtk_hbox_new(FALSE, 0));

	gtk_box_pack_start(self_box, GTK_WIDGET(priv->title_box), FALSE, FALSE, 0);
	gtk_box_pack_start(self_box, GTK_WIDGET(priv->desc), FALSE, FALSE, 0);
	gtk_box_pack_start(self_box, GTK_WIDGET(self->contents), TRUE, TRUE, 0);
	gtk_box_pack_start(self_box, GTK_WIDGET(priv->buttons), FALSE, FALSE, 0);

	gtk_widget_show_all(GTK_WIDGET(self));
	gtk_widget_hide(GTK_WIDGET(priv->desc));
}
