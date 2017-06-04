#include <glib.h>

#include "xmlnode.h"
#include "protocols/jabber/caps.h"

static void
test_jabber_caps_parse_invalid_nodes(void) {
	PurpleXmlNode *query;

	g_assert_null(jabber_caps_parse_client_info(NULL));

	/* Something other than a disco#info query */
	query = purple_xmlnode_new("foo");
	g_assert_null(jabber_caps_parse_client_info(query));
	purple_xmlnode_free(query);

	query = purple_xmlnode_new("query");
	g_assert_null(jabber_caps_parse_client_info(query));

	purple_xmlnode_set_namespace(query, "jabber:iq:last");
	g_assert_null(jabber_caps_parse_client_info(query));
	purple_xmlnode_free(query);
}

static void
_test_jabber_caps_match(GChecksumType hash_type, const gchar *in, const gchar *expected) {
	PurpleXmlNode *query = purple_xmlnode_from_str(in, -1);
	JabberCapsClientInfo *info = jabber_caps_parse_client_info(query);
	gchar *got = NULL;

	got = jabber_caps_calculate_hash(info, hash_type);

	g_assert_cmpstr(expected, ==, got);
	g_free(got);
}

static void
test_jabber_caps_calculate_from_xmlnode(void) {
	_test_jabber_caps_match(
		G_CHECKSUM_SHA1,
		"<query xmlns='http://jabber.org/protocol/disco#info' node='http://tkabber.jabber.ru/#GNjxthSckUNvAIoCCJFttjl6VL8='><identity category='client' type='pc' name='Tkabber'/><x xmlns='jabber:x:data' type='result'><field var='FORM_TYPE' type='hidden'><value>urn:xmpp:dataforms:softwareinfo</value></field><field var='software'><value>Tkabber</value></field><field var='software_version'><value> ( 8.5.5 )</value></field><field var='os'><value>ATmega640-16AU</value></field><field var='os_version'><value/></field></x><feature var='games:board'/><feature var='google:mail:notify'/><feature var='http://jabber.org/protocol/activity'/><feature var='http://jabber.org/protocol/bytestreams'/><feature var='http://jabber.org/protocol/chatstates'/><feature var='http://jabber.org/protocol/commands'/><feature var='http://jabber.org/protocol/commands'/><feature var='http://jabber.org/protocol/disco#info'/><feature var='http://jabber.org/protocol/disco#items'/><feature var='http://jabber.org/protocol/feature-neg'/><feature var='http://jabber.org/protocol/geoloc'/><feature var='http://jabber.org/protocol/ibb'/><feature var='http://jabber.org/protocol/iqibb'/><feature var='http://jabber.org/protocol/mood'/><feature var='http://jabber.org/protocol/muc'/><feature var='http://jabber.org/protocol/mute#ancestor'/><feature var='http://jabber.org/protocol/mute#editor'/><feature var='http://jabber.org/protocol/rosterx'/><feature var='http://jabber.org/protocol/si'/><feature var='http://jabber.org/protocol/si/profile/file-transfer'/><feature var='http://jabber.org/protocol/tune'/><feature var='jabber:iq:avatar'/><feature var='jabber:iq:browse'/><feature var='jabber:iq:dtcp'/><feature var='jabber:iq:filexfer'/><feature var='jabber:iq:ibb'/><feature var='jabber:iq:inband'/><feature var='jabber:iq:jidlink'/><feature var='jabber:iq:last'/><feature var='jabber:iq:oob'/><feature var='jabber:iq:privacy'/><feature var='jabber:iq:time'/><feature var='jabber:iq:version'/><feature var='jabber:x:data'/><feature var='jabber:x:event'/><feature var='jabber:x:oob'/><feature var='urn:xmpp:ping'/><feature var='urn:xmpp:receipts'/><feature var='urn:xmpp:time'/></query>",
		"GNjxthSckUNvAIoCCJFttjl6VL8="
	);
}

gint
main(gint argc, gchar **argv) {
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/jabber/caps/parse invalid nodes",
	                test_jabber_caps_parse_invalid_nodes);

	g_test_add_func("/jabber/caps/calulate from xmlnode",
	                test_jabber_caps_calculate_from_xmlnode);

	return g_test_run();
}
