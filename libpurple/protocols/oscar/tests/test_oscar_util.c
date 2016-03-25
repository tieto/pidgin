#include <glib.h>

#include "../oscar.h"

static void
test_oscar_util_name_compare(void) {
	gsize i;
	const gchar *good[] = {
		"test",
		"TEST",
		"Test",
		"teSt",
		" TesT",
		"test ",
		"  T E   s T  "
	};
	const gchar *bad[] = {
		"toast",
		"test@example.com",
		"test@aim.com"
	};

	for(i = 0; i < G_N_ELEMENTS(good); i++) {
		g_assert_cmpint(0, ==, oscar_util_name_compare("test", good[i]));
		g_assert_cmpint(0, ==, oscar_util_name_compare(good[i], "test"));
	}

	for(i = 0; i < G_N_ELEMENTS(bad); i++) {
		g_assert_cmpint(0, !=, oscar_util_name_compare("test", bad[i]));
		g_assert_cmpint(0, !=, oscar_util_name_compare(bad[i], "test"));
	}
}

gint
main(gint argc, gchar **argv) {
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/oscar/util/name compare",
	                test_oscar_util_name_compare);

	return g_test_run();
}
