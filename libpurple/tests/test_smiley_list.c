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

#include <purple.h>

/******************************************************************************
 * Test
 *****************************************************************************/
static void
test_smiley_list_new(void) {
	PurpleSmileyList *list = NULL;

	list = purple_smiley_list_new();

	g_assert(purple_smiley_list_is_empty(list));
	g_assert(purple_smiley_list_get_unique(list) == NULL);
	g_assert(purple_smiley_list_get_all(list) == NULL);

	g_object_unref(G_OBJECT(list));
}

static void
test_smiley_list_add_remove(void) {
	PurpleSmileyList *list = NULL;
	PurpleSmiley *smiley = NULL, *smiley2 = NULL;
	PurpleTrie *trie = NULL;
	gboolean added = FALSE;

	list = purple_smiley_list_new();

	g_assert(purple_smiley_list_is_empty(list));

	/* create a smiley */
	smiley = purple_smiley_new_from_data("testing", NULL, 0);

	/* add the smiley to the list */
	added = purple_smiley_list_add(list, smiley);
	g_assert(added);
	g_assert(!purple_smiley_list_is_empty(list));

	/* make sure we can get it back out */
	smiley2 = purple_smiley_list_get_by_shortcut(list, "testing");
	g_assert(smiley == smiley2);

	/* make sure it returns a valid trie */
	trie = purple_smiley_list_get_trie(list);
	g_assert(PURPLE_IS_TRIE(trie));
	/* don't free the trie, as it's ownership is not transfered to us */

	/* add it again (should fail) */
	added = purple_smiley_list_add(list, smiley);
	g_assert(!added);

	/* now remove it and make sure the list is empty */
	purple_smiley_list_remove(list, smiley);
	g_assert(purple_smiley_list_is_empty(list));

	g_object_unref(G_OBJECT(smiley));
	g_object_unref(G_OBJECT(list));
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

	g_test_add_func("/smiley_list/new", test_smiley_list_new);
	g_test_add_func("/smiley_list/add-remove", test_smiley_list_add_remove);

	return g_test_run();
}
