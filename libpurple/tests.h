/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */
#ifndef PURPLE_TESTS_H
#define PURPLE_TESTS_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct {
	const gchar *input;
	const gchar *output;
} PurpleTestStringData;

typedef const gchar *(*PurpleTestStringFunc)(const gchar *);
typedef gchar *(*PurpleTestStringFreeFunc)(const gchar *);

inline void
purple_test_string_compare(PurpleTestStringFunc func,
                           PurpleTestStringData data[])
{
	gint i;

	for(i = 0; data[i].input; i++)
		g_assert_cmpstr(data[i].output, ==, func(data[i].input));
}

inline void
purple_test_string_compare_free(PurpleTestStringFreeFunc func,
                                PurpleTestStringData data[])
{
	gint i;

	for(i = 0; data[i].input; i++) {
		gchar *got = func(data[i].input);

		g_assert_cmpstr(data[i].output, ==, got);

		g_free(got);
	}
}


G_END_DECLS

#endif /* PURPLE_TESTS_H */
