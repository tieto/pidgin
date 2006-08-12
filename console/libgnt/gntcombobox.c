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
		/* XXX: make sure the key actually does exist */
		gpointer old = box->selected;
		box->selected = key;
		if (GNT_WIDGET(box)->window)
			gnt_widget_draw(GNT_WIDGET(box));
		if (box->dropdown)
			gnt_tree_set_selected(GNT_TREE(box->dropdown), key);
		g_signal_emit(box, signals[SIG_SELECTION_CHANGED], 0, old, key);
	}
}

static void
gnt_combo_box_draw(GntWidget *widget)
{
	GntComboBox *box = GNT_COMBO_BOX(widget);
	char *text = NULL;
	GntColorType type;
	int len;
	
	if (box->dropdown)
	{
		text = gnt_tree_get_selection_text(GNT_TREE(box->dropdown));
		box->selected = gnt_tree_get_selection_data(GNT_TREE(box->dropdown));
	}

	if (text == NULL)
		text = g_strdup(text);

	if (gnt_widget_has_focus(widget))
		type = GNT_COLOR_HIGHLIGHT;
	else
		type = GNT_COLOR_NORMAL;

	wbkgdset(widget->window, '\0' | COLOR_PAIR(type));

	if ((len = g_utf8_strlen(text, -1)) > widget->priv.width - 4)
	{
		char *s = g_utf8_offset_to_pointer(text, widget->priv.width - 4);
		*s = '\0';
		len = widget->priv.width - 4;
	}

	mvwprintw(widget->window, 1, 1, text);
	whline(widget->window, ' ' | COLOR_PAIR(type), widget->priv.width - 4 - len);
	mvwaddch(widget->window, 1, widget->priv.width - 3, ACS_VLINE | COLOR_PAIR(GNT_COLOR_NORMAL));
	mvwaddch(widget->window, 1, widget->priv.width - 2, ACS_DARROW | COLOR_PAIR(GNT_COLOR_NORMAL));

	g_free(text);
	DEBUG;
}

static void
gnt_combo_box_size_request(GntWidget *widget)
{
	if (!GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_MAPPED))
	{
		GntWidget *dd = GNT_COMBO_BOX(widget)->dropdown;
		gnt_widget_size_request(dd);
		widget->priv.height = 3;   /* For now, a combobox will have border */
		widget->priv.width = MIN(10, dd->priv.width + 4);
	}
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
					set_selection(box, gnt_tree_get_selection_data(GNT_TREE(box->dropdown)));
				case 27:
					gnt_tree_set_selected(GNT_TREE(box->dropdown), box->selected);
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
				GntWidget *parent = box->dropdown->parent;
				int height = g_list_length(GNT_TREE(box->dropdown)->list);
				int y = widget->priv.y + widget->priv.height - 1;
				gnt_widget_set_size(box->dropdown, widget->priv.width, height + 2);

				if (y + height + 2 >= getmaxy(stdscr))
					y = widget->priv.y - height - 1;
				gnt_widget_set_position(parent, widget->priv.x, y);
				if (parent->window)
				{
					mvwin(parent->window, y, widget->priv.x);
				}

				gnt_widget_draw(parent);
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
	GntWidget *widget = GNT_WIDGET(instance);
	GntComboBox *combo = GNT_COMBO_BOX(instance);

	GNT_WIDGET_SET_FLAGS(GNT_WIDGET(instance),
			GNT_WIDGET_GROW_X | GNT_WIDGET_CAN_TAKE_FOCUS | GNT_WIDGET_NO_SHADOW);
	combo->dropdown = gnt_tree_new();

	box = gnt_box_new(FALSE, FALSE);
	GNT_WIDGET_SET_FLAGS(box, GNT_WIDGET_NO_SHADOW | GNT_WIDGET_NO_BORDER | GNT_WIDGET_TRANSIENT);
	gnt_box_set_pad(GNT_BOX(box), 0);
	gnt_box_add_widget(GNT_BOX(box), combo->dropdown);
	
	widget->priv.minw = 4;
	widget->priv.minh = 3;
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
	gnt_tree_add_row_after(GNT_TREE(box->dropdown), key,
			gnt_tree_create_row(GNT_TREE(box->dropdown), text), NULL, NULL);
	if (box->selected == NULL)
		set_selection(box, key);
}

gpointer gnt_combo_box_get_selected_data(GntComboBox *box)
{
	return box->selected;
}

void gnt_combo_box_set_selected(GntComboBox *box, gpointer key)
{
	set_selection(box, key);
}

