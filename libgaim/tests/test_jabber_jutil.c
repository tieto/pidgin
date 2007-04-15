#include "tests.h"
#include "../protocols/jabber/jutil.h"

START_TEST(test_get_resource)
{
	assert_string_equal_free("baz", jabber_get_resource("foo@bar/baz"));
	assert_string_equal_free("baz", jabber_get_resource("bar/baz"));
	assert_string_equal_free("baz/bat", jabber_get_resource("foo@bar/baz/bat"));
	assert_string_equal_free("baz/bat", jabber_get_resource("bar/baz/bat"));
}
END_TEST

START_TEST(test_get_resource_no_resource)
{

	fail_unless(NULL == jabber_get_resource("foo@bar"));
	fail_unless(NULL == jabber_get_resource("bar"));
}
END_TEST

START_TEST(test_get_bare_jid)
{
	assert_string_equal_free("foo@bar", jabber_get_bare_jid("foo@bar"));
	assert_string_equal_free("foo@bar", jabber_get_bare_jid("foo@bar/baz"));
	assert_string_equal_free("bar", jabber_get_bare_jid("bar"));
	assert_string_equal_free("bar", jabber_get_bare_jid("bar/baz"));
}
END_TEST

START_TEST(test_nodeprep_validate)
{
	fail_unless(jabber_nodeprep_validate(NULL));
	fail_unless(jabber_nodeprep_validate("foo"));
	fail_unless(jabber_nodeprep_validate("%d"));
	fail_unless(jabber_nodeprep_validate("y\\z"));

	char *longnode = g_strnfill(1023, 'a');
	fail_unless(jabber_nodeprep_validate(longnode));
	g_free(longnode);
}
END_TEST

START_TEST(test_nodeprep_validate_illegal_chars)
{
	fail_if(jabber_nodeprep_validate("don't"));
	fail_if(jabber_nodeprep_validate("m@ke"));
	fail_if(jabber_nodeprep_validate("\"me\""));
	fail_if(jabber_nodeprep_validate("&ngry"));
	fail_if(jabber_nodeprep_validate("c:"));
	fail_if(jabber_nodeprep_validate("a/b"));
	fail_if(jabber_nodeprep_validate("4>2"));
	fail_if(jabber_nodeprep_validate("4<7"));
}
END_TEST

START_TEST(test_nodeprep_validate_too_long)
{
	char *longnode = g_strnfill(1024, 'a');
	fail_if(jabber_nodeprep_validate(longnode));
	g_free(longnode);
}
END_TEST

Suite *
jabber_jutil_suite(void)
{
	Suite *s = suite_create("Jabber Utility Functions");

	TCase *tc = tcase_create("Get Resource");
	tcase_add_test(tc, test_get_resource);
	tcase_add_test(tc, test_get_resource_no_resource);
	suite_add_tcase(s, tc);

	tc = tcase_create("Get Bare JID");
	tcase_add_test(tc, test_get_bare_jid);
	suite_add_tcase(s, tc);

	tc = tcase_create("Nodeprep validate");
	tcase_add_test(tc, test_nodeprep_validate);
	tcase_add_test(tc, test_nodeprep_validate_illegal_chars);
	tcase_add_test(tc, test_nodeprep_validate_too_long);
	suite_add_tcase(s, tc);

	return s;
}
