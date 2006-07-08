#include "gntbox.h"
#include "gntcombobox.h"
#include "gnttree.h"
#include "gntmarshal.h"

#include <string.h>

enum
{
	SIG_SELECTION_CHANGED,
	SIGS,
};

static GntWidgetClass *parent_class = NULL;
static guint signals[SIGS] = { 0 };
static void (*widget_lost_focus)(GntWidget *widget);

static void
set_selection(GntComboBox *box, gpointer key)
{
	if (box->selected != key)
	{
		gpointer old = box->selected;
		box->selected = key;
		g_signal_emit(box, signals[SIG_SELECTION_CHANGED], 0, old, key);
		gnt_widget_draw(GNT_WIDGET(box));
	}
}

static void
gnt_combo_box_draw(GntWidget *widget)
{
	GntComboBox *box = GNT_COMBO_BOX(widget);
	const char *text = NULL;
	GntColorType type;
	
	if (box->dropdown)
	{
		text = gnt_tree_get_selection_text(GNT_TREE(box->dropdown));
		box->selected = gnt_tree_get_selection_data(GNT_TREE(box->dropdown));
	}

	if (text == NULL)
		text = "";

	if (gnt_widget_has_focus(widget))
		type = GNT_COLOR_HIGHLIGHT;
	else
		type = GNT_COLOR_NORMAL;

	wbkgdset(widget->window, '\0' | COLOR_PAIR(type));
	mvwprintw(widget->window, 1, 1, text);

	DEBUG;
}

static void
gnt_combo_box_size_request(GntWidget *widget)
{
	widget->priv.height = 3;   /* For now, a combobox will have border */
	widget->priv.width = 15;
}

static void
gnt_combo_box_map(GntWidget *widget)
{
	if (widget->priv.width == 0 || widget->priv.height == 0)
		gnt_widget_size_request(widget);
	DEBUG;
}

static gboolean
gnt_combo_box_key_pressed(GntWidget *widget, const char *text)
{
	GntComboBox *box = GNT_COMBO_BOX(widget);
	if (GNT_WIDGET_IS_FLAG_SET(box->dropdown->parent, GNT_WIDGET_MAPPED))
	{
		if (text[1] == 0)
		{
			switch (text[0])
			{
				case '\r':
				case '\t':
					/* XXX: Get the selction */
					set_selection(box, gnt_tree_get_selection_data(GNT_TREE(box->dropdown)));
				case 27:
					gnt_widget_hide(box->dropdown->parent);
					return TRUE;
					break;
			}
		}
		if (gnt_widget_key_pressed(box->dropdown, text))
			return TRUE;
	}
	else
	{
		if (text[0] == 27)
		{
			if (strcmp(text + 1, GNT_KEY_UP) == 0 ||
					strcmp(text + 1, GNT_KEY_DOWN) == 0)
			{
				gnt_widget_set_size(box->dropdown, widget->priv.width, 9);
				gnt_widget_set_position(box->dropdown->parent,
						widget->priv.x, widget->priv.y + widget->priv.height - 1);
				gnt_widget_draw(box->dropdown->parent);
				return TRUE;
			}
		}
	}

	return FALSE;
}

static void
gnt_combo_box_destroy(GntWidget *widget)
{
	gnt_widget_destroy(GNT_COMBO_BOX(widget)->dropdown->parent);
}

static void
gnt_combo_box_lost_focus(GntWidget *widget)
{
	GntComboBox *combo = GNT_COMBO_BOX(widget);
	if (GNT_WIDGET_IS_FLAG_SET(combo->dropdown->parent, GNT_WIDGET_MAPPED))
		gnt_widget_hide(GNT_COMBO_BOX(widget)->dropdown->parent);
	widget_lost_focus(widget);
}

static void
gnt_combo_box_class_init(GntComboBoxClass *klass)
{
	parent_class = GNT_WIDGET_CLASS(klass);

	parent_class->destroy = gnt_combo_box_destroy;
	parent_class->draw = gnt_combo_box_draw;
	parent_class->map = gnt_combo_box_map;
	parent_class->size_request = gnt_combo_box_size_request;
	parent_class->key_pressed = gnt_combo_box_key_pressed;

	widget_lost_focus = parent_class->lost_focus;
	parent_class->lost_focus = gnt_combo_box_lost_focus;

	signals[SIG_SELECTION_CHANGED] = 
		g_signal_new("selection-changed",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 0,
					 NULL, NULL,
					 gnt_closure_marshal_VOID__POINTER_POINTER,
					 G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_POINTER);

	DEBUG;
}

static void
gnt_combo_box_init(GTypeInstance *instance, gpointer class)
{
	GntWidget *box;
	GntComboBox *combo = GNT_COMBO_BOX(instance);

	GNT_WIDGET_SET_FLAGS(GNT_WIDGET(instance),
			GNT_WIDGET_GROW_X | GNT_WIDGET_CAN_TAKE_FOCUS | GNT_WIDGET_NO_SHADOW);
	combo->dropdown = gnt_tree_new();

	box = gnt_box_new(FALSE, FALSE);
	GNT_WIDGET_SET_FLAGS(box, GNT_WIDGET_NO_SHADOW | GNT_WIDGET_NO_BORDER);
	gnt_box_add_widget(GNT_BOX(box), combo->dropdown);
	
	DEBUG;
}

/******************************************************************************
 * GntComboBox API
 *****************************************************************************/
GType
gnt_combo_box_get_gtype(void)
{
	static GType type = 0;

	if(type == 0)
	{
		static const GTypeInfo info = {
			sizeof(GntComboBoxClass),
			NULL,					/* base_init		*/
			NULL,					/* base_finalize	*/
			(GClassInitFunc)gnt_combo_box_class_init,
			NULL,					/* class_finalize	*/
			NULL,					/* class_data		*/
			sizeof(GntComboBox),
			0,						/* n_preallocs		*/
			gnt_combo_box_init,			/* instance_init	*/
		};

		type = g_type_register_static(GNT_TYPE_WIDGET,
									  "GntComboBox",
									  &info, 0);
	}

	return type;
}

GntWidget *gnt_combo_box_new()
{
	GntWidget *widget = g_object_new(GNT_TYPE_COMBO_BOX, NULL);

	return widget;
}

void gnt_combo_box_add_data(GntComboBox *box, gpointer key, const char *text)
{
	gnt_tree_add_row_after(GNT_TREE(box->dropdown), key, text, NULL, NULL);
	if (box->selected == NULL)
		set_selection(box, key);
}

gpointer gnt_combo_box_get_selected_data(GntComboBox *box)
{
	return box->selected;
}

