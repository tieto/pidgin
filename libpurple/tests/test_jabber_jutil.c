#include <string.h>

#include "tests.h"
#include "../account.h"
#include "../conversation.h"
#include "../xmlnode.h"
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
	char *longnode;

	fail_unless(jabber_nodeprep_validate(NULL));
	fail_unless(jabber_nodeprep_validate("foo"));
	fail_unless(jabber_nodeprep_validate("%d"));
	fail_unless(jabber_nodeprep_validate("y\\z"));

	longnode = g_strnfill(1023, 'a');
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

#define assert_valid_jid(str) { \
	JabberID *jid = jabber_id_new(str); \
	fail_if(jid == NULL, "JID '%s' is valid but jabber_id_new() rejected it", str); \
	jabber_id_free(jid); \
}

#define assert_invalid_jid(str) { \
	JabberID *jid = jabber_id_new(str); \
	fail_if(jid != NULL, "JID '%s' is invalid but jabber_id_new() allowed it", str); \
	jabber_id_free(jid); \
}

START_TEST(test_jabber_id_new)
{
	assert_valid_jid("gmail.com");
	assert_valid_jid("gmail.com/Test");
	assert_valid_jid("gmail.com/Test@");
	assert_valid_jid("gmail.com/@");
	assert_valid_jid("gmail.com/Test@alkjaweflkj");
	assert_valid_jid("mark.doliner@gmail.com");
	assert_valid_jid("mark.doliner@gmail.com/Test12345");
	assert_valid_jid("mark.doliner@gmail.com/Test@12345");
	assert_valid_jid("mark.doliner@gmail.com/Te/st@12@//345");
	assert_valid_jid("わいど@conference.jabber.org");
	assert_valid_jid("まりるーむ@conference.jabber.org");
	assert_valid_jid("mark.doliner@gmail.com/まりるーむ");
	assert_valid_jid("mark.doliner@gmail/stuff.org");

	assert_invalid_jid("@gmail.com");
	assert_invalid_jid("@@gmail.com");
	assert_invalid_jid("mark.doliner@@gmail.com/Test12345");
	assert_invalid_jid("mark@doliner@gmail.com/Test12345");
	assert_invalid_jid("@gmail.com/Test@12345");
	assert_invalid_jid("/Test@12345");
	assert_invalid_jid("mark.doliner@");
	assert_invalid_jid("mark.doliner/");
	assert_invalid_jid("mark.doliner@gmail_stuff.org");
	assert_invalid_jid("mark.doliner@gmail[stuff.org");
	assert_invalid_jid("mark.doliner@gmail\\stuff.org");
	assert_invalid_jid("mark.doliner@わいど.org");
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

	tc = tcase_create("JID validate");
	tcase_add_test(tc, test_nodeprep_validate);
	tcase_add_test(tc, test_nodeprep_validate_illegal_chars);
	tcase_add_test(tc, test_nodeprep_validate_too_long);
	tcase_add_test(tc, test_jabber_id_new);
	suite_add_tcase(s, tc);

	return s;
}
