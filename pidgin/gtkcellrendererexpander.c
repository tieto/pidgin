/*
 * @file gtkcellrendererexpander.c GTK+ Cell Renderer Expander
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
 *
 */

/* This is taken largely from GtkCellRenderer[Text|Pixbuf|Toggle] by
 * Jonathon Blandford <jrb@redhat.com> for RedHat, Inc.
 */

#include <gtk/gtk.h>
#include "gtkcellrendererexpander.h"

static void pidgin_cell_renderer_expander_get_property  (GObject                    *object,
						      guint                       param_id,
						      GValue                     *value,
						      GParamSpec                 *pspec);
static void pidgin_cell_renderer_expander_set_property  (GObject                    *object,
						      guint                       param_id,
						      const GValue               *value,
						      GParamSpec                 *pspec);
static void pidgin_cell_renderer_expander_init       (PidginCellRendererExpander      *cellexpander);
static void pidgin_cell_renderer_expander_class_init (PidginCellRendererExpanderClass *class);
static void pidgin_cell_renderer_expander_get_size   (GtkCellRenderer            *cell,
						   GtkWidget                  *widget,
						   const GdkRectangle         *cell_area,
						   gint                       *x_offset,
						   gint                       *y_offset,
						   gint                       *width,
						   gint                       *height);
static void pidgin_cell_renderer_expander_render     (GtkCellRenderer            *cell,
						   cairo_t                  *cr,
						   GtkWidget                  *widget,
						   const GdkRectangle               *background_area,
						   const GdkRectangle               *cell_area,
						   GtkCellRendererState             flags);
static gboolean pidgin_cell_renderer_expander_activate  (GtkCellRenderer            *r,
						      GdkEvent                   *event,
						      GtkWidget                  *widget,
						      const gchar                *p,
						      const GdkRectangle               *bg,
						      const GdkRectangle               *cell,
						      GtkCellRendererState        flags);
static void  pidgin_cell_renderer_expander_finalize (GObject *gobject);

enum {
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_IS_EXPANDER
};

static gpointer parent_class;
/* static guint expander_cell_renderer_signals [LAST_SIGNAL]; */

GType  pidgin_cell_renderer_expander_get_type (void)
{
	static GType cell_expander_type = 0;

	if (!cell_expander_type)
		{
			static const GTypeInfo cell_expander_info =
				{
					sizeof (PidginCellRendererExpanderClass),
					NULL,           /* base_init */
					NULL,           /* base_finalize */
					(GClassInitFunc) pidgin_cell_renderer_expander_class_init,
					NULL,           /* class_finalize */
					NULL,           /* class_data */
					sizeof (PidginCellRendererExpander),
					0,              /* n_preallocs */
					(GInstanceInitFunc) pidgin_cell_renderer_expander_init,
					NULL		/* value_table */
				};

			cell_expander_type =
				g_type_register_static (GTK_TYPE_CELL_RENDERER,
										"PidginCellRendererExpander",
										&cell_expander_info, 0);
		}

	return cell_expander_type;
}

static void pidgin_cell_renderer_expander_init (PidginCellRendererExpander *cellexpander)
{
  g_object_set(G_OBJECT(cellexpander), "mode",
               GTK_CELL_RENDERER_MODE_ACTIVATABLE, NULL);
	/*GTK_CELL_RENDERER(cellexpander)->mode = GTK_CELL_RENDERER_MODE_ACTIVATABLE;*/
  gtk_cell_renderer_set_padding(GTK_CELL_RENDERER(cellexpander), 0, 2);
}

static void pidgin_cell_renderer_expander_class_init (PidginCellRendererExpanderClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS(class);
	GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS(class);

	parent_class = g_type_class_peek_parent (class);
	object_class->finalize = pidgin_cell_renderer_expander_finalize;

	object_class->get_property = pidgin_cell_renderer_expander_get_property;
	object_class->set_property = pidgin_cell_renderer_expander_set_property;

	cell_class->get_size = pidgin_cell_renderer_expander_get_size;
	cell_class->render   = pidgin_cell_renderer_expander_render;
	cell_class->activate = pidgin_cell_renderer_expander_activate;

	g_object_class_install_property (object_class,
					 PROP_IS_EXPANDER,
					 g_param_spec_boolean ("expander-visible",
							      "Is Expander",
							      "True if the renderer should draw an expander",
							      FALSE,
							      G_PARAM_READWRITE));
}

static void pidgin_cell_renderer_expander_finalize (GObject *object)
{
/*
	PidginCellRendererExpander *cellexpander = PIDGIN_CELL_RENDERER_EXPANDER(object);
*/

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void pidgin_cell_renderer_expander_get_property (GObject    *object,
						     guint      param_id,
						     GValue     *value,
						     GParamSpec *psec)
{
	PidginCellRendererExpander *cellexpander = PIDGIN_CELL_RENDERER_EXPANDER(object);

	switch (param_id)
		{
		case PROP_IS_EXPANDER:
			g_value_set_boolean(value, cellexpander->is_expander);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, psec);
			break;

		}
}

static void pidgin_cell_renderer_expander_set_property (GObject      *object,
						     guint        param_id,
						     const GValue *value,
						     GParamSpec   *pspec)
{
	PidginCellRendererExpander *cellexpander = PIDGIN_CELL_RENDERER_EXPANDER (object);

	switch (param_id)
		{
		case PROP_IS_EXPANDER:
			cellexpander->is_expander = g_value_get_boolean(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, pspec);
			break;
		}
}

GtkCellRenderer *pidgin_cell_renderer_expander_new(void)
{
	return g_object_new(PIDGIN_TYPE_GTK_CELL_RENDERER_EXPANDER, NULL);
}

static void pidgin_cell_renderer_expander_get_size (GtkCellRenderer *cell,
						 GtkWidget       *widget,
						 const GdkRectangle    *cell_area,
						 gint            *x_offset,
						 gint            *y_offset,
						 gint            *width,
						 gint            *height)
{
	gint calc_width;
	gint calc_height;
	gint expander_size;
  gint xpad;
  gint ypad;
  gfloat xalign;
  gfloat yalign;
    
	gtk_widget_style_get(widget, "expander-size", &expander_size, NULL);

  gtk_cell_renderer_get_padding(cell, &xpad, &ypad);
  gtk_cell_renderer_get_alignment(cell, &xalign, &yalign); 
	calc_width = (gint) xpad * 2 + expander_size;
	calc_height = (gint) ypad * 2 + expander_size;

	if (width)
		*width = calc_width;

	if (height)
		*height = calc_height;

	if (cell_area)
		{
			if (x_offset)
				{
					*x_offset = xalign * (cell_area->width - calc_width);
					*x_offset = MAX (*x_offset, 0);
				}
			if (y_offset)
				{
					*y_offset = yalign * (cell_area->height - calc_height);
					*y_offset = MAX (*y_offset, 0);
				}
		}
}


static void pidgin_cell_renderer_expander_render(GtkCellRenderer *cell,
					       cairo_t         *cr,
					       GtkWidget       *widget,
					       const GdkRectangle    *background_area,
					       const GdkRectangle    *cell_area,
					       GtkCellRendererState  flags)
{
	PidginCellRendererExpander *cellexpander = (PidginCellRendererExpander *) cell;
	gboolean set;
	gint width, height;
	GtkStateType state;
  gint xpad;
  gint ypad;
  gboolean is_expanded;
  GtkAllocation allocation;
    
  gtk_cell_renderer_get_padding(cell, &xpad, &ypad);
  g_object_get(G_OBJECT(cell), "is-expanded", &is_expanded, NULL);
    
	if (!cellexpander->is_expander)
		return;

	width = cell_area->width;
	height = cell_area->height;

	if (!gtk_widget_get_sensitive(GTK_WIDGET(cell)))
		state = GTK_STATE_INSENSITIVE;
	else if (flags & GTK_CELL_RENDERER_PRELIT)
		state = GTK_STATE_PRELIGHT;
	else if (gtk_widget_has_focus(widget) && flags & GTK_CELL_RENDERER_SELECTED)
		state = GTK_STATE_ACTIVE;
	else
		state = GTK_STATE_NORMAL;

	width -= xpad*2;
	height -= ypad*2;

	gtk_paint_expander (gtk_widget_get_style(widget),
			    cr, state,
			    widget, "treeview",
			    cell_area->x + xpad + (width / 2),
			    cell_area->y + ypad + (height / 2),
			    is_expanded ? GTK_EXPANDER_EXPANDED : GTK_EXPANDER_COLLAPSED);

	/* only draw the line if the color isn't set - this prevents a bug where the hline appears only under the expander */
	g_object_get(cellexpander, "cell-background-set", &set, NULL);
  gtk_widget_get_allocation(widget, &allocation);

  if (is_expanded && !set)
		gtk_paint_hline (gtk_widget_get_style(widget), cr, state, widget, NULL, 0,
				 allocation.width, cell_area->y + cell_area->height);
}

static gboolean pidgin_cell_renderer_expander_activate(GtkCellRenderer *r,
						     GdkEvent *event,
						     GtkWidget *widget,
						     const gchar *p,
						     const GdkRectangle *bg,
						     const GdkRectangle *cell,
						     GtkCellRendererState flags)
{
	GtkTreePath *path = gtk_tree_path_new_from_string(p);
	if (gtk_tree_view_row_expanded(GTK_TREE_VIEW(widget), path))
		gtk_tree_view_collapse_row(GTK_TREE_VIEW(widget), path);
	else
		gtk_tree_view_expand_row(GTK_TREE_VIEW(widget),path,FALSE);
	gtk_tree_path_free(path);
	return FALSE;
}
