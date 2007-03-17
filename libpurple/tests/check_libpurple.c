#include <glib.h>
#include <stdlib.h>

#include "../core.h"
#include "../eventloop.h"

#include "tests.h"

/******************************************************************************
 * libpurple goodies
 *****************************************************************************/
static guint
gaim_check_input_add(gint fd, GaimInputCondition condition,
                     GaimInputFunction function, gpointer data)
{
	/* this is a no-op for now, feel free to implement it */
	return 0;
}

static GaimEventLoopUiOps eventloop_ui_ops = {
	g_timeout_add,
	(guint (*)(guint))g_source_remove,
	gaim_check_input_add,
	(guint (*)(guint))g_source_remove,
};

static void
gaim_check_init(void) {
	gchar *home_dir;

	gaim_eventloop_set_ui_ops(&eventloop_ui_ops);

	/* build our fake home directory */
	home_dir = g_build_path(BUILDDIR, "libpurple", "tests", "home", NULL);
	gaim_util_set_user_dir(home_dir);
	g_free(home_dir);

	gaim_core_init("check");
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
	SRunner *sr = srunner_create (master_suite());

	srunner_add_suite(sr, cipher_suite());
	srunner_add_suite(sr, jabber_jutil_suite());
	srunner_add_suite(sr, util_suite());

	/* make this a libpurple "ui" */
	gaim_check_init();

	srunner_run_all (sr, CK_NORMAL);
	number_failed = srunner_ntests_failed (sr);
	srunner_free (sr);

	gaim_core_quit();

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
