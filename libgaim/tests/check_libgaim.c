#include <glib.h>
#include <check.h>
#include <stdlib.h>
#include "../util.h"

START_TEST(test_util_base16_encode)
{
	gchar *out = gaim_base16_encode("hello, world!", 14);
	fail_unless(strcmp("68656c6c6f2c20776f726c642100", out) == 0, NULL);
	g_free(out);
}
END_TEST

START_TEST(test_util_base16_decode)
{
	gsize sz = 0;
	guchar *out = gaim_base16_decode("21646c726f77202c6f6c6c656800", &sz);
	fail_unless(sz == 14, NULL);
	fail_unless(strcmp("!dlrow ,olleh", out) == 0, NULL);
	g_free(out);
}
END_TEST

START_TEST(test_util_base64_encode)
{
	gchar *out = gaim_base64_encode("forty-two", 10);
	fail_unless(strcmp("Zm9ydHktdHdvAA==",out) == 0, NULL);
	g_free(out);
}
END_TEST

START_TEST(test_util_base64_decode)
{
	gsize sz;
	guchar *out = gaim_base64_decode("b3d0LXl0cm9mAA==", &sz);
	fail_unless(sz == 10, NULL);
	fail_unless(strcmp("owt-ytrof", out) == 0, NULL);
	g_free(out);
}
END_TEST

Suite *
util_suite(void)
{
	Suite *s = suite_create("Utility Functions");

	TCase *tc = tcase_create("Base16");
	tcase_add_test(tc, test_util_base16_encode);
	tcase_add_test(tc, test_util_base16_decode);
	suite_add_tcase(s, tc);

	tc = tcase_create("Base64");
	tcase_add_test(tc, test_util_base64_encode);
	tcase_add_test(tc, test_util_base64_decode);
	suite_add_tcase(s, tc);

	return s;
}

int main(void)
{
	int number_failed;
	Suite *s = util_suite ();
	SRunner *sr = srunner_create (s);
	srunner_run_all (sr, CK_NORMAL);
	number_failed = srunner_ntests_failed (sr);
	srunner_free (sr);
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
