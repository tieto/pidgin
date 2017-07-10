/*
 * Purple
 *
 * Purple is the legal property of its developers, whose names are too
 * numerous to list here. Please refer to the COPYRIGHT file distributed
 * with this source distribution
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

#include <glib.h>
#include <string.h>

#include <purple.h>

// generated via:
// $ cat test-image.png | hexdump -v -e '1 1 "0x%02x," " "' | xargs -n 8 echo
static const gsize test_image_data_len = 160;
static const guint8 test_image_data[] = {
	0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
	0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
	0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02,
	0x08, 0x02, 0x00, 0x00, 0x00, 0xfd, 0xd4, 0x9a,
	0x73, 0x00, 0x00, 0x00, 0x09, 0x70, 0x48, 0x59,
	0x73, 0x00, 0x00, 0x0b, 0x13, 0x00, 0x00, 0x0b,
	0x13, 0x01, 0x00, 0x9a, 0x9c, 0x18, 0x00, 0x00,
	0x00, 0x07, 0x74, 0x49, 0x4d, 0x45, 0x07, 0xe0,
	0x0a, 0x02, 0x16, 0x30, 0x22, 0x28, 0xa4, 0xc9,
	0xdd, 0x00, 0x00, 0x00, 0x1d, 0x69, 0x54, 0x58,
	0x74, 0x43, 0x6f, 0x6d, 0x6d, 0x65, 0x6e, 0x74,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x43, 0x72, 0x65,
	0x61, 0x74, 0x65, 0x64, 0x20, 0x77, 0x69, 0x74,
	0x68, 0x20, 0x47, 0x49, 0x4d, 0x50, 0x64, 0x2e,
	0x65, 0x07, 0x00, 0x00, 0x00, 0x16, 0x49, 0x44,
	0x41, 0x54, 0x08, 0xd7, 0x63, 0xf8, 0xff, 0xff,
	0x3f, 0x03, 0x03, 0x03, 0xe3, 0xb3, 0x4c, 0xb5,
	0x9b, 0x4e, 0x0b, 0x00, 0x2f, 0xa9, 0x06, 0x2f,
	0x8a, 0xd1, 0xc6, 0xb3, 0x00, 0x00, 0x00, 0x00,
	0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82,
};

/******************************************************************************
 * Helpers
 *****************************************************************************/
static void
_test_smiley(PurpleSmiley *smiley,
             const gchar *path,
             const guint8 *edata,
             gsize elen,
             const gchar *ext,
             const gchar *mimetype,
             const gchar *shortcut)
{
	GBytes *bytes = NULL;
	const guint8 *adata = NULL;
	gsize alen;

	g_assert(PURPLE_IS_SMILEY(PURPLE_IMAGE(smiley)));

	bytes = purple_image_get_contents(PURPLE_IMAGE(smiley));
	adata = g_bytes_get_data(bytes, &alen);
	g_assert_cmpmem(adata, alen, edata, elen);
	g_bytes_unref(bytes);

	g_assert_cmpstr(
		purple_image_get_extension(PURPLE_IMAGE(smiley)),
		==,
		ext
	);
	g_assert_cmpstr(
		purple_image_get_mimetype(PURPLE_IMAGE(smiley)),
		==,
		mimetype
	);
	g_assert_cmpstr(purple_smiley_get_shortcut(smiley), ==, shortcut);

	if(path)
		g_assert_cmpstr(purple_image_get_path(PURPLE_IMAGE(smiley)), ==, path);

	g_object_unref(G_OBJECT(smiley));
}

/******************************************************************************
 * Tests
 *****************************************************************************/
static void
test_smiley_new_from_data(void) {
	PurpleSmiley *smiley = purple_smiley_new_from_data(
		":-X",
		test_image_data,
		test_image_data_len
	);

	_test_smiley(
		smiley,
		NULL,
		test_image_data,
		test_image_data_len,
		"png",
		"image/png",
		":-X"
	);
}

static void
test_smiley_new_from_file(void) {
	PurpleSmiley *smiley = NULL;
	GError *error = NULL;
	gchar *path = NULL;
	gchar *edata = NULL;
	gsize elen;

	path = g_build_filename(TEST_DATA_DIR, "test-image.png", NULL);
	smiley = purple_smiley_new("^_^", path);
	g_assert_no_error(error);

	g_file_get_contents(path, &edata, &elen, &error);
	g_assert_no_error(error);

	_test_smiley(
		smiley,
		path,
		(guint8 *)edata,
		elen,
		"png",
		"image/png",
		"^_^"
	);

	g_free(path);
}

/******************************************************************************
 * Main
 *****************************************************************************/
gint
main(gint argc, gchar **argv) {
	g_test_init(&argc, &argv, NULL);

	#if GLIB_CHECK_VERSION(2, 38, 0)
	g_test_set_nonfatal_assertions();
	#endif /* GLIB_CHECK_VERSION(2, 38, 0) */

	g_test_add_func("/smiley/new-from-data", test_smiley_new_from_data);
	g_test_add_func("/smiley/new-from-file", test_smiley_new_from_file);

	return g_test_run();
}
