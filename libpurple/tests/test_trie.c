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

#include "../trie.h"

static gint find_sum;

static gboolean
test_trie_replace_cb(GString *out, const gchar *word, gpointer word_data,
	gpointer user_data)
{
	/* the "test" word for the test_trie_replace test */
	if ((gintptr)word_data == 0x1001)
		return FALSE;

	g_string_append_printf(out, "[%d:%x]",
		(int)(gintptr)user_data, (int)(gintptr)word_data);

	return TRUE;
}

static gboolean
test_trie_find_cb(const gchar *word, gpointer word_data,
	gpointer user_data)
{
	if ((gintptr)word_data == 0x7004)
		return FALSE;

	find_sum += (gintptr)word_data;
	find_sum -= (gintptr)user_data * 0x1000;

	return TRUE;
}

static void
test_trie_replace_normal(void) {
	PurpleTrie *trie;
	const gchar *in;
	gchar *out;

	trie = purple_trie_new();
	purple_trie_set_reset_on_match(trie, FALSE);

	purple_trie_add(trie, "test", (gpointer)0x1001);
	purple_trie_add(trie, "testing", (gpointer)0x1002);
	purple_trie_add(trie, "overtested", (gpointer)0x1003);
	purple_trie_add(trie, "trie", (gpointer)0x1004);
	purple_trie_add(trie, "tree", (gpointer)0x1005);
	purple_trie_add(trie, "implement", (gpointer)0x1006);
	purple_trie_add(trie, "implementation", (gpointer)0x1007);

	in = "Alice is testing her trie implementation, "
		"but she's far away from making test tree overtested";

	out = purple_trie_replace(trie, in, test_trie_replace_cb, (gpointer)1);

	g_assert_cmpstr(
		"Alice is [1:1002] her [1:1004] [1:1006]ation,"
		" but she's far away from making test [1:1005] [1:1003]",
		==,
		out
	);

	g_object_unref(trie);
	g_free(out);
}

static void
test_trie_replace_whole(void) {
	PurpleTrie *trie;
	const gchar *in;
	gchar *out;

	trie = purple_trie_new();

	purple_trie_add(trie, "test", (gpointer)0x2002);

	in = "test";

	out = purple_trie_replace(trie, in, test_trie_replace_cb, (gpointer)2);

	g_assert_cmpstr("[2:2002]", ==, out);

	g_object_unref(trie);
	g_free(out);
}

static void
test_trie_replace_inner(void) {
	PurpleTrie *trie;
	const gchar *in;
	gchar *out;

	trie = purple_trie_new();

	purple_trie_add(trie, "est", (gpointer)0x3001);
	purple_trie_add(trie, "tester", (gpointer)0x3002);

	in = "the test!";

	out = purple_trie_replace(trie, in, test_trie_replace_cb, (gpointer)3);

	g_assert_cmpstr("the t[3:3001]!", ==, out);

	g_object_unref(trie);
	g_free(out);
}


static void
test_trie_replace_empty(void) {
	PurpleTrie *trie;
	const gchar *in;
	gchar *out;

	trie = purple_trie_new();

	purple_trie_add(trie, "test", (gpointer)0x4001);

	in = "";

	out = purple_trie_replace(trie, in, test_trie_replace_cb, (gpointer)4);

	g_assert_cmpstr("", ==, out);

	g_object_unref(trie);
	g_free(out);
}

static void
test_trie_multi_replace(void) {
	PurpleTrie *trie1, *trie2, *trie3;
	GSList *tries = NULL;
	const gchar *in;
	gchar *out;

	trie1 = purple_trie_new();
	trie2 = purple_trie_new();
	trie3 = purple_trie_new();

	/* appending is not efficient, but we have only 3 elements */
	tries = g_slist_append(tries, trie1);
	tries = g_slist_append(tries, trie2);
	tries = g_slist_append(tries, trie3);

	purple_trie_add(trie1, "test", (gpointer)0x5011);
	purple_trie_add(trie1, "trie1", (gpointer)0x5012);
	purple_trie_add(trie1, "Alice", (gpointer)0x5013);

	purple_trie_add(trie2, "test", (gpointer)0x5021);
	purple_trie_add(trie2, "trie2", (gpointer)0x5022);
	purple_trie_add(trie2, "example", (gpointer)0x5023);
	purple_trie_add(trie2, "Ali", (gpointer)0x5024);

	/* "tester" without last (accepting) letter of "test" */
	purple_trie_add(trie3, "teser", (gpointer)0x5031);
	purple_trie_add(trie3, "trie3", (gpointer)0x5032);
	purple_trie_add(trie3, "tester", (gpointer)0x5033);
	purple_trie_add(trie3, "example", (gpointer)0x5034);
	purple_trie_add(trie3, "Al", (gpointer)0x5035);

	in = "test tester trie trie1 trie2 trie3 example Alice";

	out = purple_trie_multi_replace(tries, in,
		test_trie_replace_cb, (gpointer)5);

	g_assert_cmpstr(
		"[5:5011] [5:5011]er trie [5:5012] [5:5022] "
		"[5:5032] [5:5023] [5:5035]ice",
		==,
		out
	);

	g_slist_free_full(tries, g_object_unref);
	g_free(out);
}

static void
test_trie_remove(void) {
	PurpleTrie *trie;
	const gchar *in;
	gchar *out;

	trie = purple_trie_new();

	purple_trie_add(trie, "alice", (gpointer)0x6001);
	purple_trie_add(trie, "bob", (gpointer)0x6002);
	purple_trie_add(trie, "cherry", (gpointer)0x6003);

	purple_trie_remove(trie, "bob");

	in = "alice bob cherry";

	out = purple_trie_replace(trie, in, test_trie_replace_cb, (gpointer)6);

	g_assert_cmpstr("[6:6001] bob [6:6003]", ==, out);

	g_object_unref(trie);
	g_free(out);
}

static void
test_trie_find_normal(void) {
	PurpleTrie *trie;
	const gchar *in;
	gint out;

	trie = purple_trie_new();

	purple_trie_add(trie, "alice", (gpointer)0x7001);
	purple_trie_add(trie, "bob", (gpointer)0x7002);
	purple_trie_add(trie, "cherry", (gpointer)0x7003);
	purple_trie_add(trie, "al", (gpointer)0x7004); /* not accepted */

	in = "test alice bob test cherry alice";

	find_sum = 0;
	out = purple_trie_find(trie, in, test_trie_find_cb, (gpointer)7);

	g_assert_cmpint(4, ==, out);
	g_assert_cmpint(2 * 1 + 2 + 3, ==, find_sum);

	g_object_unref(trie);
}

static void
test_trie_find_reset(void) {
	PurpleTrie *trie;
	const gchar *in;
	gint out;

	trie = purple_trie_new();
	purple_trie_set_reset_on_match(trie, TRUE);

	purple_trie_add(trie, "alice", (gpointer)0x8001);
	purple_trie_add(trie, "ali", (gpointer)0x8002);
	purple_trie_add(trie, "al", (gpointer)0x8003);

	in = "al ali alice";

	find_sum = 0;
	out = purple_trie_find(trie, in, test_trie_find_cb, (gpointer)8);

	g_assert_cmpint(3, ==, out);
	g_assert_cmpint(3 * 3, ==, find_sum);

	g_object_unref(trie);
}

static void
test_trie_find_noreset(void) {
	PurpleTrie *trie;
	const gchar *in;
	gint out;

	trie = purple_trie_new();
	purple_trie_set_reset_on_match(trie, FALSE);

	purple_trie_add(trie, "alice", (gpointer)0x9001);
	purple_trie_add(trie, "ali", (gpointer)0x9002);
	purple_trie_add(trie, "al", (gpointer)0x9003);

	in = "al ali alice";

	find_sum = 0;
	out = purple_trie_find(trie, in, test_trie_find_cb, (gpointer)9);

	g_assert_cmpint(6, ==, out);
	g_assert_cmpint(3 * 3 + 2 * 2 + 1, ==, find_sum);

	g_object_unref(trie);
}

static void
test_trie_multi_find(void) {
	PurpleTrie *trie1, *trie2, *trie3;
	GSList *tries = NULL;
	const gchar *in;
	int out;

	trie1 = purple_trie_new();
	trie2 = purple_trie_new();
	trie3 = purple_trie_new();
	purple_trie_set_reset_on_match(trie1, FALSE);
	purple_trie_set_reset_on_match(trie2, TRUE);
	purple_trie_set_reset_on_match(trie3, FALSE);

	/* appending is not efficient, but we have only 3 elements */
	tries = g_slist_append(tries, trie1);
	tries = g_slist_append(tries, trie2);
	tries = g_slist_append(tries, trie3);

	purple_trie_add(trie1, "test", (gpointer)0x10011);
	purple_trie_add(trie1, "trie1", (gpointer)0x10012);
	purple_trie_add(trie1, "Alice", (gpointer)0x10013);

	purple_trie_add(trie2, "test", (gpointer)0x10021);
	purple_trie_add(trie2, "trie2", (gpointer)0x10022);
	purple_trie_add(trie2, "example", (gpointer)0x10023);
	purple_trie_add(trie2, "Ali", (gpointer)0x10024);

	/* "tester" without last (accepting) letter of "test" */
	purple_trie_add(trie3, "teser", (gpointer)0x10031);
	purple_trie_add(trie3, "trie3", (gpointer)0x10032);
	purple_trie_add(trie3, "tester", (gpointer)0x10033);
	purple_trie_add(trie3, "example", (gpointer)0x10034);
	purple_trie_add(trie3, "Al", (gpointer)0x10035);

	in = "test tester trie trie1 trie2 trie3 example Alice";

	out = purple_trie_multi_find(tries, in,
		test_trie_find_cb, (gpointer)0x10);

	g_assert_cmpint(9, ==, out);
	g_assert_cmpint(2 * 0x11 + 0x33 + 0x12 + 0x22 +
		0x32 + 0x23 + 0x35 + 0x13, ==, find_sum);

	g_slist_free_full(tries, g_object_unref);
}

gint
main(gint argc, gchar **argv) {
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/trie/replace/normal",
	                test_trie_replace_normal);
	g_test_add_func("/trie/replace/whole",
	                test_trie_replace_whole);
	g_test_add_func("/trie/replace/inner",
	                test_trie_replace_inner);
	g_test_add_func("/trie/replace/empty",
	                test_trie_replace_empty);

	g_test_add_func("/trie/multi_replace",
	                test_trie_multi_replace);

	g_test_add_func("/trie/remove",
	                test_trie_remove);

	g_test_add_func("/trie/find/normal",
	                test_trie_find_normal);
	g_test_add_func("/trie/find/reset",
	                test_trie_find_reset);
	g_test_add_func("/trie/find/noreset",
	                test_trie_find_noreset);

	g_test_add_func("/trie/multi_find",
	                test_trie_multi_find);

	return g_test_run();
}
