/**
 * GNT - The GLib Ncurses Toolkit
 *
 * GNT is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <glib.h>

#include "gnt.h"
#include "gntwidget.h"

typedef gpointer (*GDupFunc)(gconstpointer data);

/**
 * 
 * @param text
 * @param width
 * @param height
 */
void gnt_util_get_text_bound(const char *text, int *width, int *height);

/* excluding *end */
/**
 * 
 * @param start
 * @param end
 *
 * @return
 */
int gnt_util_onscreen_width(const char *start, const char *end);

const char *gnt_util_onscreen_width_to_pointer(const char *str, int len, int *w);

/* Inserts newlines in 'string' where necessary so that its onscreen width is
 * no more than 'maxw'.
 * 'maxw' can be <= 0, in which case the maximum screen width is considered.
 *
 * Returns a newly allocated string.
 */
/**
 * 
 * @param string
 * @param maxw
 *
 * @return
 */
char * gnt_util_onscreen_fit_string(const char *string, int maxw);

/**
 * 
 * @param src
 * @param hash
 * @param equal
 * @param key_d
 * @param value_d
 * @param key_dup
 * @param value_dup
 *
 * @return
 */
GHashTable * g_hash_table_duplicate(GHashTable *src, GHashFunc hash, GEqualFunc equal, GDestroyNotify key_d, GDestroyNotify value_d, GDupFunc key_dup, GDupFunc value_dup);

/**
 * To be used with g_signal_new. Look in the key_pressed signal-definition in
 * gntwidget.c for usage.
 */
/**
 * 
 * @param ihint
 * @param return_accu
 * @param handler_return
 * @param dummy
 *
 * @return
 */
gboolean gnt_boolean_handled_accumulator(GSignalInvocationHint *ihint, GValue *return_accu, const GValue *handler_return, gpointer dummy);

/**
 * Returns a GntTree populated with "key" -> "binding" for the widget.
 */
/**
 * 
 * @param widget
 *
 * @return
 */
GntWidget * gnt_widget_bindings_view(GntWidget *widget);

/**
 * Parse widgets from 'string'.
 */
/**
 * 
 * @param string
 * @param num
 */
void gnt_util_parse_widgets(const char *string, int num, ...);

