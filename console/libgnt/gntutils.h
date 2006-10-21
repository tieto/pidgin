#include <glib.h>

#include "gnt.h"
#include "gntwidget.h"

void gnt_util_get_text_bound(const char *text, int *width, int *height);

/* excluding *end */
int gnt_util_onscreen_width(const char *start, const char *end);

const char *gnt_util_onscreen_width_to_pointer(const char *str, int len, int *w);

/* Inserts newlines in 'string' where necessary so that its onscreen width is
 * no more than 'maxw'.
 * 'maxw' can be <= 0, in which case the maximum screen width is considered.
 *
 * Returns a newly allocated string.
 */
char *gnt_util_onscreen_fit_string(const char *string, int maxw);

GHashTable *g_hash_table_duplicate(GHashTable *src, GHashFunc hash,
		GEqualFunc equal, GDestroyNotify key_d, GDestroyNotify value_d);
