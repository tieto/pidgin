#include <glib.h>

#include "gnt.h"
#include "gntwidget.h"

typedef gpointer (*GDupFunc)(gconstpointer data);

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
		GEqualFunc equal, GDestroyNotify key_d, GDestroyNotify value_d,
		GDupFunc key_dup, GDupFunc value_dup);


/**
 * To be used with g_signal_new. Look in the key_pressed signal-definition in
 * gntwidget.c for usage.
 */
gboolean gnt_boolean_handled_accumulator(GSignalInvocationHint *ihint,
				  GValue                *return_accu,
				  const GValue          *handler_return,
				  gpointer               dummy);

