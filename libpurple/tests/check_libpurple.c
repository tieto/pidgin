#include <glib.h>
#include <stdlib.h>

#include "tests.h"

#include "../core.h"
#include "../eventloop.h"
#include "../util.h"


/******************************************************************************
 * libpurple goodies
 *****************************************************************************/
static guint
purple_check_input_add(gint fd, PurpleInputCondition condition,
                     PurpleInputFunction function, gpointer data)
{
	/* this is a no-op for now, feel free to implement it */
	return 0;
}

static PurpleEventLoopUiOps eventloop_ui_ops = {
	g_timeout_add,
	g_source_remove,
	purple_check_input_add,
	g_source_remove,
	NULL, /* input_get_error */
#if GLIB_CHECK_VERSION(2,14,0)
	g_timeout_add_seconds,
#else
	NULL,
#endif
	NULL,
	NULL,
	NULL
};

static void
purple_check_init(void) {
	gchar *home_dir;

	g_type_init();

	purple_eventloop_set_ui_ops(&eventloop_ui_ops);

	/* build our fake home directory */
	home_dir = g_build_path(BUILDDIR, "libpurple", "tests", "home", NULL);
	purple_util_set_user_dir(home_dir);
	g_free(home_dir);

	purple_core_init("check");
}

/******************************************************************************
 * Check meat and potatoes
 *****************************************************************************/
Suite*
master_suite(void)
{
	Suite *s = suite_create("Master Suite");

	return s;
}

int main(void)
{
	int number_failed;
	SRunner *sr;

	/* Make g_return_... functions fatal, ALWAYS.
	 * As this is the test code, this is NOT controlled
	 * by PURPLE_FATAL_ASSERTS. */
	g_log_set_always_fatal(G_LOG_LEVEL_CRITICAL);

	sr = srunner_create (master_suite());

	srunner_add_suite(sr, cipher_suite());
	srunner_add_suite(sr, jabber_jutil_suite());
	srunner_add_suite(sr, util_suite());

	/* make this a libpurple "ui" */
	purple_check_init();

	srunner_run_all (sr, CK_NORMAL);
	number_failed = srunner_ntests_failed (sr);
	srunner_free (sr);

	purple_core_quit();

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
