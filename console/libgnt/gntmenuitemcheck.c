#include "gntmenuitemcheck.h"

static GntMenuItemClass *parent_class = NULL;

static void
gnt_menuitem_check_class_init(GntMenuItemCheckClass *klass)
{
	parent_class = GNT_MENUITEM_CLASS(klass);

	GNTDEBUG;
}

static void
gnt_menuitem_check_init(GTypeInstance *instance, gpointer class)
{
	GNTDEBUG;
}

/******************************************************************************
 * GntMenuItemCheck API
 *****************************************************************************/
GType
gnt_menuitem_check_get_gtype(void)
{
	static GType type = 0;

	if(type == 0)
	{
		static const GTypeInfo info = {
			sizeof(GntMenuItemCheckClass),
			NULL,					/* base_init		*/
			NULL,					/* base_finalize	*/
			(GClassInitFunc)gnt_menuitem_check_class_init,
			NULL,					/* class_finalize	*/
			NULL,					/* class_data		*/
			sizeof(GntMenuItemCheck),
			0,						/* n_preallocs		*/
			gnt_menuitem_check_init,			/* instance_init	*/
			NULL					/* value_table		*/
		};

		type = g_type_register_static(GNT_TYPE_MENUITEM,
									  "GntMenuItemCheck",
									  &info, 0);
	}

	return type;
}

GntMenuItem *gnt_menuitem_check_new(const char *text)
{
	GntMenuItem *item = g_object_new(GNT_TYPE_MENUITEM_CHECK, NULL);
	GntMenuItem *menuitem = GNT_MENUITEM(item);

	menuitem->text = g_strdup(text);
	return item;
}

gboolean gnt_menuitem_check_get_checked(GntMenuItemCheck *item)
{
		return item->checked;
}

void gnt_menuitem_check_set_checked(GntMenuItemCheck *item, gboolean set)
{
		item->checked = set;
}

