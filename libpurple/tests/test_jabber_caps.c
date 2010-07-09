#include <string.h>

#include "tests.h"
#include "../xmlnode.h"
#include "../protocols/jabber/caps.h"

START_TEST(test_parse_invalid)
{
	xmlnode *query;

	fail_unless(NULL == jabber_caps_parse_client_info(NULL));

	/* Something other than a disco#info query */
	query = xmlnode_new("foo");
	fail_unless(NULL == jabber_caps_parse_client_info(query));
	xmlnode_free(query);

	query = xmlnode_new("query");
	fail_unless(NULL == jabber_caps_parse_client_info(query));
	xmlnode_set_namespace(query, "jabber:iq:last");
	fail_unless(NULL == jabber_caps_parse_client_info(query));
	xmlnode_free(query);
}
END_TEST

Suite *
jabber_caps_suite(void)
{
	Suite *s = suite_create("Jabber Caps Functions");

	TCase *tc = tcase_create("Parsing invalid ndoes");
	tcase_add_test(tc, test_parse_invalid);
	suite_add_tcase(s, tc);

	return s;
}
