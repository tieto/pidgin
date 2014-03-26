#include <string.h>

#include "tests.h"
#include "../trie.h"

static gboolean
test_trie_replace_cb(GString *out, const gchar *word, gpointer word_data,
	gpointer user_data)
{
	/* the "test" word */
	if ((int)word_data == 0x1001)
		return FALSE;

	g_string_append_printf(out, "[%d:%x]", (int)user_data, (int)word_data);

	return TRUE;
}

START_TEST(test_trie_replace)
{
	PurpleTrie *trie;
	const gchar *in;
	gchar *out;

	trie = purple_trie_new();

	purple_trie_add(trie, "test", (gpointer)0x1001);
	purple_trie_add(trie, "testing", (gpointer)0x1002);
	purple_trie_add(trie, "overtested", (gpointer)0x1003);
	purple_trie_add(trie, "trie", (gpointer)0x1004);
	purple_trie_add(trie, "tree", (gpointer)0x1005);
	purple_trie_add(trie, "implement", (gpointer)0x1006);
	purple_trie_add(trie, "implementation", (gpointer)0x1007);

	in = "Alice is testing her trie implementation, "
		"but she's far away from making test tree overtested";

	out = purple_trie_replace(trie, in, test_trie_replace_cb, (gpointer)7);

	assert_string_equal("Alice is [7:1002] her [7:1004] [7:1006]ation,"
		" but she's far away from making test [7:1005] [7:1003]", out);

	g_object_unref(trie);
	g_free(out);
}
END_TEST

START_TEST(test_trie_replace_whole)
{
	PurpleTrie *trie;
	const gchar *in;
	gchar *out;

	trie = purple_trie_new();

	purple_trie_add(trie, "test", (gpointer)0x2002);

	in = "test";

	out = purple_trie_replace(trie, in, test_trie_replace_cb, (gpointer)5);

	assert_string_equal("[5:2002]", out);

	g_object_unref(trie);
	g_free(out);
}
END_TEST

START_TEST(test_trie_replace_inner)
{
	PurpleTrie *trie;
	const gchar *in;
	gchar *out;

	trie = purple_trie_new();

	purple_trie_add(trie, "est", (gpointer)0x3001);
	purple_trie_add(trie, "tester", (gpointer)0x3002);

	in = "the test!";

	out = purple_trie_replace(trie, in, test_trie_replace_cb, (gpointer)3);

	assert_string_equal("the t[3:3001]!", out);

	g_object_unref(trie);
	g_free(out);
}
END_TEST

START_TEST(test_trie_replace_empty)
{
	PurpleTrie *trie;
	const gchar *in;
	gchar *out;

	trie = purple_trie_new();

	purple_trie_add(trie, "test", (gpointer)0x4001);

	in = "";

	out = purple_trie_replace(trie, in, test_trie_replace_cb, (gpointer)2);

	assert_string_equal("", out);

	g_object_unref(trie);
	g_free(out);
}
END_TEST

Suite *
purple_trie_suite(void)
{
	Suite *s = suite_create("PurpleTrie class");

	TCase *tc = tcase_create("trie");
	tcase_add_test(tc, test_trie_replace);
	tcase_add_test(tc, test_trie_replace_whole);
	tcase_add_test(tc, test_trie_replace_inner);
	tcase_add_test(tc, test_trie_replace_empty);

	suite_add_tcase(s, tc);

	return s;
}
