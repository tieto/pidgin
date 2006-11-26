#include "tests.h"
#include <stdlib.h>

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

	srunner_add_suite(sr, util_suite());

	srunner_run_all (sr, CK_NORMAL);
	number_failed = srunner_ntests_failed (sr);
	srunner_free (sr);
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
