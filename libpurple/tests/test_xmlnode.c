#include <string.h>

#include "tests.h"
#include "../xmlnode.h"

/*
 * If we really wanted to test the billion laughs attack we would
 * need to have more than just 4 ha's.  But as long as this shorter
 * document fails to parse, the longer one should also fail to parse.
 */
START_TEST(test_xmlnode_billion_laughs_attack)
{
	const char *malicious_xml_doc = "<!DOCTYPE root [ <!ENTITY ha \"Ha !\"><!ENTITY ha2 \"&ha; &ha;\"><!ENTITY ha3 \"&ha2; &ha2;\"> ]><root>&ha3;</root>";

	/* Uncomment this line if you want to see the error message given by
	   the parser for the above XML document */
	/* purple_debug_set_enabled(TRUE); */

	fail_if(xmlnode_from_str(malicious_xml_doc, -1),
			"xmlnode_from_str() returned an XML tree, but we didn't want it to");
}
END_TEST

#define check_doc_structure(x) { \
	xmlnode *ping, *child1, *child2; \
	fail_if(x == NULL, "Failed to parse document"); \
	ping = xmlnode_get_child(x, "ping"); \
	fail_if(ping == NULL, "Failed to find 'ping' child"); \
	child1 = xmlnode_get_child(ping, "child1"); \
	fail_if(child1 == NULL, "Failed to find 'child1'"); \
	child2 = xmlnode_get_child(child1, "child2"); \
	fail_if(child2 == NULL, "Failed to find 'child2'"); \
	xmlnode_new_child(child2, "a"); \
	\
	assert_string_equal("jabber:client", xmlnode_get_namespace(x)); \
	/* NOTE: xmlnode_get_namespace() returns the namespace of the element, not the
	 * current default namespace.  See http://www.w3.org/TR/xml-names/#defaulting and
	 * http://www.w3.org/TR/xml-names/#dt-defaultNS.
	 */ \
	assert_string_equal("urn:xmpp:ping", xmlnode_get_namespace(ping)); \
	assert_string_equal("jabber:client", xmlnode_get_namespace(child1)); \
	assert_string_equal("urn:xmpp:ping", xmlnode_get_namespace(child2)); \
	/*
	 * This fails (well, actually crashes [the ns is NULL]) unless
	 * xmlnode_new_child() actually sets the element namespace.
	assert_string_equal("jabber:client", xmlnode_get_namespace(xmlnode_get_child(child2, "a")));
	 */ \
	\
	assert_string_equal("jabber:client", xmlnode_get_default_namespace(x)); \
	assert_string_equal("jabber:client", xmlnode_get_default_namespace(ping)); \
	assert_string_equal("jabber:client", xmlnode_get_default_namespace(child1)); \
	assert_string_equal("jabber:client", xmlnode_get_default_namespace(child2)); \
}

START_TEST(test_xmlnode_prefixes)
{
	const char *xml_doc =
		"<iq type='get' xmlns='jabber:client' xmlns:ping='urn:xmpp:ping'>"
			"<ping:ping>"
				"<child1>"
					"<ping:child2></ping:child2>" /* xmlns='jabber:child' */
				"</child1>"
			"</ping:ping>"
		"</iq>";
	char *str;
	xmlnode *xml, *reparsed;

	xml = xmlnode_from_str(xml_doc, -1);
	check_doc_structure(xml);

	/* Check that xmlnode_from_str(xmlnode_to_str(xml, NULL), -1) is idempotent. */
	str = xmlnode_to_str(xml, NULL);
	fail_if(str == NULL, "Failed to serialize XMLnode");
	reparsed = xmlnode_from_str(str, -1);
	fail_if(reparsed == NULL, "Failed to reparse xml document");
	check_doc_structure(reparsed);

	g_free(str);
	xmlnode_free(xml);
	xmlnode_free(reparsed);
}
END_TEST

Suite *
xmlnode_suite(void)
{
	Suite *s = suite_create("Utility Functions");

	TCase *tc = tcase_create("xmlnode");
	tcase_add_test(tc, test_xmlnode_billion_laughs_attack);
	tcase_add_test(tc, test_xmlnode_prefixes);

	suite_add_tcase(s, tc);

	return s;
}
