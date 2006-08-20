#include "gnt.h"

typedef enum
{
	GNT_STYLE_SHADOW = 0,
	GNT_STYLES
} GntStyle;

void gnt_style_read_configure_file(const char *filename);

/* Returned strings are all lowercase */
const char *gnt_style_get(GntStyle style);

gboolean gnt_style_get_bool(GntStyle style, gboolean def);

/* This should be called only once for the each type */
void gnt_styles_get_keyremaps(GType type, GHashTable *hash);

void gnt_init_styles();

void gnt_uninit_styles();

