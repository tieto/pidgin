/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * @file gtkgaim-disclosure.c GTK+ Gaim Disclosure
 * @ingroup gtkui
 *
 * gaim
 *
 *  Gaim is the legal property of its developers, whose names are too numerous
 *  to list here.  Please refer to the COPYRIGHT file distributed with this
 *  source distribution.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtktogglebutton.h>

#include "gtkgaim-disclosure.h"

#ifdef ENABLE_NLS
#  include <libintl.h>
#  define _(x) gettext(x)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define N_(String) (String)
#  define _(x) (x)
#endif

static GtkCheckButtonClass *parent_class = NULL;

struct _GaimDisclosurePrivate {
	GtkWidget *container;
	char *shown;
	char *hidden;
	
	guint32 expand_id;
	GtkExpanderStyle style;

	int expander_size;
	int direction;
};

static void
finalize (GObject *object)
{
	GaimDisclosure *disclosure;

	disclosure = GAIM_DISCLOSURE (object);
	if (disclosure->priv == NULL) {
		return;
	}

	g_free (disclosure->priv->hidden);
	g_free (disclosure->priv->shown);

	if (disclosure->priv->container != NULL) {
		g_object_unref (G_OBJECT (disclosure->priv->container));
	}
	
	g_free (disclosure->priv);
	disclosure->priv = NULL;

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
get_x_y (GaimDisclosure *disclosure,
	 int *x,
	 int *y,
	 GtkStateType *state_type)
{
	GtkCheckButton *check_button;
	int indicator_size = 0;
	int focus_width;
	int focus_pad;
	gboolean interior_focus;
	GtkWidget *widget = GTK_WIDGET (disclosure);
	GtkBin *bin = GTK_BIN (disclosure);
	int width;
	
	if (GTK_WIDGET_VISIBLE (disclosure) &&
	    GTK_WIDGET_MAPPED (disclosure)) {
		check_button = GTK_CHECK_BUTTON (disclosure);
		
		gtk_widget_style_get (widget,
				      "interior_focus", &interior_focus,
				      "focus-line-width", &focus_width,
				      "focus-padding", &focus_pad,
				      NULL);
		
		*state_type = GTK_WIDGET_STATE (widget);
		if ((*state_type != GTK_STATE_NORMAL) &&
		    (*state_type != GTK_STATE_PRELIGHT)) {
			*state_type = GTK_STATE_NORMAL;
		}

		if (bin->child) {
			width = bin->child->allocation.x - widget->allocation.x - (2 * GTK_CONTAINER (widget)->border_width);
		} else {
			width = widget->allocation.width;
		}
		
		*x = widget->allocation.x + (width) / 2;
		*y = widget->allocation.y + widget->allocation.height / 2;

		if (interior_focus == FALSE) {
			*x += focus_width + focus_pad;
		}

		*state_type = GTK_WIDGET_STATE (widget) == GTK_STATE_ACTIVE ? GTK_STATE_NORMAL : GTK_WIDGET_STATE (widget);

		if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) {
			*x = widget->allocation.x + widget->allocation.width - (indicator_size + *x - widget->allocation.x);
		}
	} else {
		*x = 0;
		*y = 0;
		*state_type = GTK_STATE_NORMAL;
	}
}

static gboolean
expand_collapse_timeout (gpointer data)
{
	GtkWidget *widget = data;
	GaimDisclosure *disclosure = data;
	GtkStateType state_type;
	int x, y;
	
	gdk_window_invalidate_rect (widget->window, &widget->allocation, TRUE);
	get_x_y (disclosure, &x, &y, &state_type);
	
	gtk_paint_expander (widget->style,
			    widget->window,
			    state_type,
			    &widget->allocation,
			    widget,
			    "disclosure",
			    x, y,
			    disclosure->priv->style);

	disclosure->priv->style += disclosure->priv->direction;
	if ((int) disclosure->priv->style > (int) GTK_EXPANDER_EXPANDED) {
		disclosure->priv->style = GTK_EXPANDER_EXPANDED;

		if (disclosure->priv->container != NULL) {
			gtk_widget_show (disclosure->priv->container);
		}

		g_object_set (G_OBJECT (disclosure),
			      "label", disclosure->priv->hidden,
			      NULL);
		return FALSE;
	} else if ((int) disclosure->priv->style < (int) GTK_EXPANDER_COLLAPSED) {
		disclosure->priv->style = GTK_EXPANDER_COLLAPSED;

		if (disclosure->priv->container != NULL) {
			gtk_widget_hide (disclosure->priv->container);
		}

		g_object_set (G_OBJECT (disclosure),
			      "label", disclosure->priv->shown,
			      NULL);

		return FALSE;
	} else {
		return TRUE;
	}
}

static void
do_animation (GaimDisclosure *disclosure,
	      gboolean opening)
{
	if (disclosure->priv->expand_id > 0) {
		gaim_timeout_remove(disclosure->priv->expand_id);
	}

	disclosure->priv->direction = opening ? 1 : -1;
	disclosure->priv->expand_id = gaim_timeout_add (50, expand_collapse_timeout, disclosure);
}

static void
toggled (GtkToggleButton *tb)
{
	GaimDisclosure *disclosure;

	disclosure = GAIM_DISCLOSURE (tb);
	do_animation (disclosure, gtk_toggle_button_get_active (tb));

	if (disclosure->priv->container == NULL) {
		return;
	}
}

static void
draw_indicator (GtkCheckButton *check,
		GdkRectangle *area)
{
	GtkWidget *widget = GTK_WIDGET (check);
	GaimDisclosure *disclosure = GAIM_DISCLOSURE (check);
	GtkStateType state_type;
	int x, y;

	get_x_y (disclosure, &x, &y, &state_type);
	gtk_paint_expander (widget->style,
			    widget->window,
			    state_type,
			    area,
			    widget,
			    "treeview",
			    x, y,
			    disclosure->priv->style);
}

static void
class_init (GaimDisclosureClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkCheckButtonClass *button_class;
	GtkToggleButtonClass *toggle_class;
	
	object_class = G_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);
	button_class = GTK_CHECK_BUTTON_CLASS (klass);
	toggle_class = GTK_TOGGLE_BUTTON_CLASS (klass);
	
	toggle_class->toggled = toggled;
	button_class->draw_indicator = draw_indicator;

	object_class->finalize = finalize;

	parent_class = g_type_class_peek_parent (klass);

	gtk_widget_class_install_style_property (widget_class,
						 g_param_spec_int ("expander_size",
								   _("Expander Size"),
								   _("Size of the expander arrow"),
								   0, G_MAXINT,
								   10, G_PARAM_READABLE));
}

static void
init (GaimDisclosure *disclosure)
{
	disclosure->priv = g_new0 (GaimDisclosurePrivate, 1);
	disclosure->priv->expander_size = 10;
}

GType
gaim_disclosure_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		GTypeInfo info = {
			sizeof (GaimDisclosureClass),
			NULL, NULL, (GClassInitFunc) class_init, NULL, NULL,
			sizeof (GaimDisclosure), 0, (GInstanceInitFunc) init
		};

		type = g_type_register_static (GTK_TYPE_CHECK_BUTTON, "GaimDisclosure", &info, 0);
	}

	return type;
}

GtkWidget *
gaim_disclosure_new (const char *shown,
		     const char *hidden)
{
	GaimDisclosure *disclosure;

	disclosure = g_object_new (gaim_disclosure_get_type (), "label", shown, NULL);

	disclosure->priv->shown = g_strdup (shown);
	disclosure->priv->hidden = g_strdup (hidden);
	return GTK_WIDGET (disclosure);
}

void
gaim_disclosure_set_container (GaimDisclosure *disclosure,
			       GtkWidget *container)
{
	g_object_ref (G_OBJECT (container));
	disclosure->priv->container = container;
}
