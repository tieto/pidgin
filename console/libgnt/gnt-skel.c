#include "gnt-skel.h"

enum
{
	SIGS = 1,
};

static GntWidgetClass *parent_class = NULL;
static guint signals[SIGS] = { 0 };

static void
gnt_skel_draw(GntWidget *widget)
{
	DEBUG;
}

static void
gnt_skel_size_request(GntWidget *widget)
{
}

static void
gnt_skel_map(GntWidget *widget)
{
	if (widget->priv.width == 0 || widget->priv.height == 0)
		gnt_widget_size_request(widget);
	DEBUG;
}

static gboolean
gnt_skel_key_pressed(GntWidget *widget, const char *text)
{
	return FALSE;
}

static void
gnt_skel_destroy(GntWidget *widget)
{
}

static void
gnt_skel_class_init(GntWidgetClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	parent_class = GNT_WIDGET_CLASS(klass);
	parent_class->destroy = gnt_skel_destroy;
	parent_class->draw = gnt_skel_draw;
	parent_class->map = gnt_skel_map;
	parent_class->size_request = gnt_skel_size_request;
	parent_class->key_pressed = gnt_skel_key_pressed;

	DEBUG;
}

static void
gnt_skel_init(GTypeInstance *instance, gpointer class)
{
	DEBUG;
}

/******************************************************************************
 * GntSkel API
 *****************************************************************************/
GType
gnt_skel_get_gtype(void)
{
	static GType type = 0;

	if(type == 0)
	{
		static const GTypeInfo info = {
			sizeof(GntSkelClass),
			NULL,					/* base_init		*/
			NULL,					/* base_finalize	*/
			(GClassInitFunc)gnt_skel_class_init,
			NULL,					/* class_finalize	*/
			NULL,					/* class_data		*/
			sizeof(GntSkel),
			0,						/* n_preallocs		*/
			gnt_skel_init,			/* instance_init	*/
		};

		type = g_type_register_static(GNT_TYPE_WIDGET,
									  "GntSkel",
									  &info, 0);
	}

	return type;
}

GntWidget *gnt_skel_new()
{
	GntWidget *widget = g_object_new(GNT_TYPE_SKEL, NULL);
	GntSkel *skel = GNT_SKEL(widget);

	return widget;
}

