#include <glib.h>

#include "gnt.h"
#include "gntwidget.h"

void gnt_util_get_text_bound(const char *text, int *width, int *height);

int gnt_util_onscreen_width(const char *start, const char *end);

char *gnt_util_onscreen_width_to_pointer(const char *str, int len, int *w);
