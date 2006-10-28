#include "gntmenu.h"
#include "gntmenuitemcheck.h"

#include <string.h>

enum
{
	SIGS = 1,
};

static GntTreeClass *parent_class = NULL;

static void (*org_draw)(GntWidget *wid);
static void (*org_destroy)(GntWidget *wid);
static void (*org_map)(GntWidget *wid);
static gboolean (*org_key_pressed)(GntWidget *w, const char *t);

static void
gnt_menu_draw(GntWidget *widget)
{
	GntMenu *menu = GNT_MENU(widget);
	GList *iter;
	chtype type;
	int i;

	if (menu->type == GNT_MENU_TOPLEVEL) {
		wbkgdset(widget->window, '\0' | COLOR_PAIR(GNT_COLOR_HIGHLIGHT));
		werase(widget->window);

		for (i = 0, iter = menu->list; iter; iter = iter->next, i++) {
			GntMenuItem *item = GNT_MENUITEM(iter->data);
			type = ' ' | COLOR_PAIR(GNT_COLOR_HIGHLIGHT);
			if (i == menu->selected)
				type |= A_REVERSE;
			item->priv.x = getcurx(widget->window) + widget->priv.x;
			item->priv.y = getcury(widget->window) + widget->priv.y + 1;
			wbkgdset(widget->window, type);
			wprintw(widget->window, " %s   ", item->text);
		}
	} else {
		org_draw(widget);
	}

	GNTDEBUG;
}

static void
gnt_menu_size_request(GntWidget *widget)
{
	GntMenu *menu = GNT_MENU(widget);

	if (menu->type == GNT_MENU_TOPLEVEL) {
		widget->priv.height = 1;
		widget->priv.width = getmaxx(stdscr);
	} else {
		widget->priv.height = g_list_length(menu->list) + 2;
		widget->priv.width = 25;  /* XXX: */
	}
}

static void
menu_tree_add(GntMenu *menu, GntMenuItem *item, GntMenuItem *parent)
{
	if (GNT_IS_MENUITEM_CHECK(item)) {
		gnt_tree_add_choice(GNT_TREE(menu), item,
			gnt_tree_create_row(GNT_TREE(menu), item->text, " "), parent, NULL);
		gnt_tree_set_choice(GNT_TREE(menu), item, gnt_menuitem_check_get_checked(GNT_MENUITEM_CHECK(item)));
	} else
		gnt_tree_add_row_last(GNT_TREE(menu), item,
			gnt_tree_create_row(GNT_TREE(menu), item->text, item->submenu ? ">" : " "), parent);

	if (0 && item->submenu) {
		GntMenu *sub = GNT_MENU(item->submenu);
		GList *iter;
		for (iter = sub->list; iter; iter = iter->next) {
			GntMenuItem *it = GNT_MENUITEM(iter->data);
			menu_tree_add(menu, it, item);
		}
	}
}

static void
gnt_menu_map(GntWidget *widget)
{
	GntMenu *menu = GNT_MENU(widget);

	if (menu->type == GNT_MENU_TOPLEVEL) {
		gnt_widget_size_request(widget);
	} else {
		/* Populate the tree */
		GList *iter;
		gnt_tree_remove_all(GNT_TREE(widget));
		for (iter = menu->list; iter; iter = iter->next) {
			GntMenuItem *item = GNT_MENUITEM(iter->data);
			menu_tree_add(menu, item, NULL);
		}
		org_map(widget);
		gnt_tree_adjust_columns(GNT_TREE(widget));
	}
	GNTDEBUG;
}

static void
menuitem_activate(GntMenu *menu, GntMenuItem *item)
{
	if (item) {
		if (item->submenu) {
			GntMenu *sub = GNT_MENU(item->submenu);
			menu->submenu = sub;
			sub->type = GNT_MENU_POPUP;	/* Submenus are *never* toplevel */
			sub->parentmenu = menu;
			if (menu->type != GNT_MENU_TOPLEVEL) {
				GntWidget *widget = GNT_WIDGET(menu);
				item->priv.x = widget->priv.x + widget->priv.width - 1;
				item->priv.y = widget->priv.y + gnt_tree_get_selection_visible_line(GNT_TREE(menu));
			}
			gnt_widget_set_position(GNT_WIDGET(sub), item->priv.x, item->priv.y);
			gnt_widget_draw(GNT_WIDGET(sub));
		} else if (item->callback) {
			item->callback(item, item->callbackdata);
			while (menu) {
				gnt_widget_hide(GNT_WIDGET(menu));
				menu = menu->parentmenu;
			}
		}
	}
}

static gboolean
gnt_menu_key_pressed(GntWidget *widget, const char *text)
{
	GntMenu *menu = GNT_MENU(widget);
	int current = menu->selected;

	if (menu->submenu) {
		return (gnt_widget_key_pressed(GNT_WIDGET(menu->submenu), text));
	}

	if (text[0] == 27 && text[1] == 0) {
		/* Escape closes menu */
		GntMenu *par = menu->parentmenu;
		if (par != NULL) {
			par->submenu = NULL;
			gnt_widget_hide(widget);
		} else
			gnt_widget_hide(widget);
		return TRUE;
	}

	if (menu->type == GNT_MENU_TOPLEVEL) {
		if (text[0] == 27) {
			if (strcmp(text + 1, GNT_KEY_LEFT) == 0) {
				menu->selected--;
				if (menu->selected < 0)
					menu->selected = g_list_length(menu->list) - 1;
			} else if (strcmp(text + 1, GNT_KEY_RIGHT) == 0) {
				menu->selected++;
				if (menu->selected >= g_list_length(menu->list))
					menu->selected = 0;
			}
		} else if (text[0] == '\r' && text[1] == 0) {
			gnt_widget_activate(widget);
		}

		if (current != menu->selected) {
			gnt_widget_draw(widget);
			return TRUE;
		}
	} else {
		return org_key_pressed(widget, text);
	}

	return FALSE;
}

static void
gnt_menu_destroy(GntWidget *widget)
{
	GntMenu *menu = GNT_MENU(widget);
	g_list_foreach(menu->list, (GFunc)g_object_run_dispose, NULL);
	g_list_free(menu->list);
	org_destroy(widget);
}

static void
gnt_menu_toggled(GntTree *tree, gpointer key)
{
	GntMenuItem *item = GNT_MENUITEM(key);
	GntMenu *menu = GNT_MENU(tree);
	gboolean check = gnt_menuitem_check_get_checked(GNT_MENUITEM_CHECK(item));
	gnt_menuitem_check_set_checked(GNT_MENUITEM_CHECK(item), !check);
	if (item->callback)
		item->callback(item, item->callbackdata);
	while (menu) {
		gnt_widget_hide(GNT_WIDGET(menu));
		menu = menu->parentmenu;
	}
}

static void
gnt_menu_activate(GntWidget *widget)
{
	GntMenu *menu = GNT_MENU(widget);
	GntMenuItem *item;

	if (menu->type == GNT_MENU_TOPLEVEL) {
		item = g_list_nth_data(menu->list, menu->selected);
	} else {
		item = gnt_tree_get_selection_data(GNT_TREE(menu));
	}

	if (item) {
		if (GNT_MENUITEM_CHECK(item))
			gnt_menu_toggled(GNT_TREE(widget), item);
		else
			menuitem_activate(menu, item);
	}
}

static void
gnt_menu_hide(GntWidget *widget)
{
	GntMenu *menu = GNT_MENU(widget);
	if (menu->parentmenu)
		menu->parentmenu->submenu = NULL;
}

static void
gnt_menu_class_init(GntMenuClass *klass)
{
	GntWidgetClass *wid_class = GNT_WIDGET_CLASS(klass);
	parent_class = GNT_TREE_CLASS(klass);

	org_destroy = wid_class->destroy;
	org_map = wid_class->map;
	org_draw = wid_class->draw;
	org_key_pressed = wid_class->key_pressed;

	wid_class->destroy = gnt_menu_destroy;
	wid_class->draw = gnt_menu_draw;
	wid_class->map = gnt_menu_map;
	wid_class->size_request = gnt_menu_size_request;
	wid_class->key_pressed = gnt_menu_key_pressed;
	wid_class->activate = gnt_menu_activate;
	wid_class->hide = gnt_menu_hide;

	parent_class->toggled = gnt_menu_toggled;

	GNTDEBUG;
}

static void
gnt_menu_init(GTypeInstance *instance, gpointer class)
{
	GntWidget *widget = GNT_WIDGET(instance);
	GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_NO_SHADOW | GNT_WIDGET_NO_BORDER |
			GNT_WIDGET_CAN_TAKE_FOCUS);
	GNTDEBUG;
}

/******************************************************************************
 * GntMenu API
 *****************************************************************************/
GType
gnt_menu_get_gtype(void)
{
	static GType type = 0;

	if(type == 0)
	{
		static const GTypeInfo info = {
			sizeof(GntMenuClass),
			NULL,					/* base_init		*/
			NULL,					/* base_finalize	*/
			(GClassInitFunc)gnt_menu_class_init,
			NULL,					/* class_finalize	*/
			NULL,					/* class_data		*/
			sizeof(GntMenu),
			0,						/* n_preallocs		*/
			gnt_menu_init,			/* instance_init	*/
		};

		type = g_type_register_static(GNT_TYPE_TREE,
									  "GntMenu",
									  &info, 0);
	}

	return type;
}

GntWidget *gnt_menu_new(GntMenuType type)
{
	GntWidget *widget = g_object_new(GNT_TYPE_MENU, NULL);
	GntMenu *menu = GNT_MENU(widget);
	menu->list = NULL;
	menu->selected = 0;
	menu->type = type;

	if (type == GNT_MENU_TOPLEVEL) {
		widget->priv.x = 0;
		widget->priv.y = 0;
	} else {
		GNT_TREE(widget)->show_separator = FALSE;
		_gnt_tree_init_internals(GNT_TREE(widget), 2);
		gnt_tree_set_col_width(GNT_TREE(widget), 1, 1);  /* The second column is to indicate that it has a submenu */
		GNT_WIDGET_UNSET_FLAGS(widget, GNT_WIDGET_NO_BORDER);
	}

	return widget;
}

void gnt_menu_add_item(GntMenu *menu, GntMenuItem *item)
{
	menu->list = g_list_append(menu->list, item);
}

