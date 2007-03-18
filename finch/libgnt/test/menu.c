#include "gnt.h"
#include "gntbox.h"
#include "gntlabel.h"
#include "gntmenu.h"
#include "gntmenuitem.h"
#include "gntwindow.h"

void dothis(GntMenuItem *item, gpointer null)
{
	GntWidget *w = gnt_vbox_new(FALSE);
	gnt_box_set_toplevel(GNT_BOX(w), TRUE);
	gnt_box_add_widget(GNT_BOX(w),
			gnt_label_new("Callback to a menuitem"));
	gnt_widget_show(w);
}

int main()
{
	freopen(".error", "w", stderr);
	gnt_init();

	GntWidget *menu = gnt_menu_new(GNT_MENU_TOPLEVEL);
	GObject *item = gnt_menuitem_new("File");

	gnt_menu_add_item(GNT_MENU(menu), GNT_MENUITEM(item));

	item = gnt_menuitem_new("Edit");
	gnt_menu_add_item(GNT_MENU(menu), GNT_MENUITEM(item));

	item = gnt_menuitem_new("Help");
	gnt_menu_add_item(GNT_MENU(menu), GNT_MENUITEM(item));

	GntWidget *sub = gnt_menu_new(GNT_MENU_POPUP);
	gnt_menuitem_set_submenu(GNT_MENUITEM(item), GNT_MENU(sub));

	item = gnt_menuitem_new("Online Help");
	gnt_menu_add_item(GNT_MENU(sub), GNT_MENUITEM(item));

	item = gnt_menuitem_new("About");
	gnt_menu_add_item(GNT_MENU(sub), GNT_MENUITEM(item));

	sub = gnt_menu_new(GNT_MENU_POPUP);
	gnt_menuitem_set_submenu(GNT_MENUITEM(item), GNT_MENU(sub));

	item = gnt_menuitem_new("Online Help");
	gnt_menu_add_item(GNT_MENU(sub), GNT_MENUITEM(item));
	gnt_menuitem_set_callback(GNT_MENUITEM(item), dothis, NULL);

	gnt_screen_menu_show(menu);

	GntWidget *win = gnt_window_new();
	gnt_box_add_widget(GNT_BOX(win),
		gnt_label_new("..."));
	gnt_box_set_title(GNT_BOX(win), "Title");
	gnt_window_set_menu(GNT_WINDOW(win), GNT_MENU(menu));
	gnt_widget_show(win);

	gnt_main();

	gnt_quit();

	return  0;
}

