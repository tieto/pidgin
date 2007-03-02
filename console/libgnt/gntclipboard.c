#include "gntclipboard.h"

gchar *string;

enum {
	SIG_CLIPBOARD = 0,
	SIGS
};

static guint signals[SIGS] = { 0 };

static void
gnt_clipboard_class_init(GntClipboardClass *klass)
{
	signals[SIG_CLIPBOARD] = 
		g_signal_new("clipboard_changed",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 0,
					 NULL, NULL,
					 g_cclosure_marshal_VOID__POINTER,
					 G_TYPE_NONE, 1, G_TYPE_POINTER);

}

static GObjectClass *parent_class = NULL;
/******************************************************************************
 * GntClipboard API
 *****************************************************************************/

void
gnt_clipboard_set_string(GntClipboard *clipboard, gchar *string)
{
	g_free(clipboard->string);
	clipboard->string = g_strdup(string);
	g_signal_emit(clipboard, signals[SIG_CLIPBOARD], 0, clipboard->string);
}

gchar *
gnt_clipboard_get_string(GntClipboard *clipboard)
{
	return g_strdup(clipboard->string);
}

static void gnt_clipboard_init(GTypeInstance *instance, gpointer class) {
	GntClipboard *clipboard = GNT_CLIPBOARD(instance);
	clipboard->string = g_strdup("");
}

GType
gnt_clipboard_get_gtype(void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(GntClipboardClass),
			NULL,					/* base_init		*/
			NULL,					/* base_finalize	*/
			(GClassInitFunc)gnt_clipboard_class_init,
			NULL,
			NULL,					/* class_data		*/
			sizeof(GntClipboard),
			0,						/* n_preallocs		*/
			gnt_clipboard_init,		/* instance_init	*/
		};

		type = g_type_register_static(G_TYPE_OBJECT,
									  "GntClipboard",
									  &info, 0);
	}

	return type;
}
