#include <glib.h>

#include "../xmlnode.h"

/*
 * If we really wanted to test the billion laughs attack we would
 * need to have more than just 4 ha's.  But as long as this shorter
 * document fails to parse, the longer one should also fail to parse.
 */
static void
test_xmlnode_billion_laughs_attack(void) {
	const char *malicious_xml_doc = "<!DOCTYPE root [ <!ENTITY ha \"Ha !\"><!ENTITY ha2 \"&ha; &ha;\"><!ENTITY ha3 \"&ha2; &ha2;\"> ]><root>&ha3;</root>";

	/* Uncomment this line if you want to see the error message given by
	   the parser for the above XML document */
	/* purple_debug_set_enabled(TRUE); */

	g_assert_null(purple_xmlnode_from_str(malicious_xml_doc, -1));
}

#define check_doc_structure(x) { \
	PurpleXmlNode *ping, *child1, *child2; \
	g_assert_nonnull(x); \
	ping = purple_xmlnode_get_child(x, "ping"); \
	g_assert_nonnull(ping); \
	child1 = purple_xmlnode_get_child(ping, "child1"); \
	g_assert_nonnull(child1); \
	child2 = purple_xmlnode_get_child(child1, "child2"); \
	g_assert_nonnull(child2); \
	purple_xmlnode_new_child(child2, "a"); \
	\
	g_assert_cmpstr("jabber:client", ==, purple_xmlnode_get_namespace(x)); \
	/* NOTE: purple_xmlnode_get_namespace() returns the namespace of the element, not the
	 * current default namespace.  See http://www.w3.org/TR/xml-names/#defaulting and
	 * http://www.w3.org/TR/xml-names/#dt-defaultNS.
	 */ \
	g_assert_cmpstr("urn:xmpp:ping", ==, purple_xmlnode_get_namespace(ping)); \
	g_assert_cmpstr("jabber:client", ==, purple_xmlnode_get_namespace(child1)); \
	g_assert_cmpstr("urn:xmpp:ping", ==, purple_xmlnode_get_namespace(child2)); \
	/*
	 * This fails (well, actually crashes [the ns is NULL]) unless
	 * purple_xmlnode_new_child() actually sets the element namespace.
	g_assert_cmpstr("jabber:client", ==, purple_xmlnode_get_namespace(purple_xmlnode_get_child(child2, "a")));
	 */ \
	\
	g_assert_cmpstr("jabber:client", ==, purple_xmlnode_get_default_namespace(x)); \
	g_assert_cmpstr("jabber:client", ==, purple_xmlnode_get_default_namespace(ping)); \
	g_assert_cmpstr("jabber:client", ==, purple_xmlnode_get_default_namespace(child1)); \
	g_assert_cmpstr("jabber:client", ==, purple_xmlnode_get_default_namespace(child2)); \
}

static void
test_xmlnode_prefixes(void) {
	const char *xml_doc =
		"<iq type='get' xmlns='jabber:client' xmlns:ping='urn:xmpp:ping'>"
			"<ping:ping>"
				"<child1>"
					"<ping:child2></ping:child2>" /* xmlns='jabber:child' */
				"</child1>"
			"</ping:ping>"
		"</iq>";
	char *str;
	PurpleXmlNode *xml, *reparsed;

	xml = purple_xmlnode_from_str(xml_doc, -1);
	check_doc_structure(xml);

	/* Check that purple_xmlnode_from_str(purple_xmlnode_to_str(xml, NULL), -1) is idempotent. */
	str = purple_xmlnode_to_str(xml, NULL);
	g_assert_nonnull(str);
	reparsed = purple_xmlnode_from_str(str, -1);
	g_assert_nonnull(reparsed);
	check_doc_structure(reparsed);

	g_free(str);
	purple_xmlnode_free(xml);
	purple_xmlnode_free(reparsed);
}

static void
test_strip_prefixes(void) {
	const char *xml_doc = "<message xmlns='jabber:client' from='user@gmail.com/resource' to='another_user@darkrain42.org' type='chat' id='purple'>"
		"<cha:active xmlns:cha='http://jabber.org/protocol/chatstates'/>"
		"<body>xvlc xvlc</body>"
		"<im:html xmlns:im='http://jabber.org/protocol/xhtml-im'>"
			"<xht:body xmlns:xht='http://www.w3.org/1999/xhtml'>"
				"<xht:p>xvlc <xht:span style='font-weight: bold;'>xvlc</xht:span></xht:p>"
			"</xht:body>"
		"</im:html>"
	"</message>";
	const char *out = "<message xmlns='jabber:client' from='user@gmail.com/resource' to='another_user@darkrain42.org' type='chat' id='purple'>"
		"<active xmlns:cha='http://jabber.org/protocol/chatstates' xmlns='http://jabber.org/protocol/chatstates'/>"
		"<body>xvlc xvlc</body>"
		"<html xmlns:im='http://jabber.org/protocol/xhtml-im' xmlns='http://jabber.org/protocol/xhtml-im'>"
			"<body xmlns:xht='http://www.w3.org/1999/xhtml' xmlns='http://www.w3.org/1999/xhtml'>"
				"<p>xvlc <span style='font-weight: bold;'>xvlc</span></p>"
			"</body>"
		"</html>"
	"</message>";
	char *str;
	PurpleXmlNode *xml;

	xml = purple_xmlnode_from_str(xml_doc, -1);
	g_assert_nonnull(xml);

	purple_xmlnode_strip_prefixes(xml);
	str = purple_xmlnode_to_str(xml, NULL);
	g_assert_cmpstr(out, ==, str);
	g_free(str);

	purple_xmlnode_free(xml);
}

gint
main(gint argc, gchar **argv) {
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/xmlnode/billion_laughs_attack",
	                test_xmlnode_billion_laughs_attack);
	g_test_add_func("/xmlnode/prefixes",
	                test_xmlnode_prefixes);
	g_test_add_func("/xmlnode/strip_prefixes",
	                test_strip_prefixes);

	return g_test_run();
}
