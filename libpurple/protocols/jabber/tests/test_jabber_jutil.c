#include <glib.h>

#include "account.h"
#include "conversation.h"
#include "glibcompat.h"
#include "tests.h"
#include "xmlnode.h"
#include "protocols/jabber/jutil.h"

static void
test_jabber_util_get_resource_exists(void) {
	PurpleTestStringData data[] = {
		{ "foo@bar/baz", "baz" },
		{ "bar/baz", "baz" },
		{ "foo@bar/baz/bat", "baz/bat" },
		{ "bar/baz/bat", "baz/bat" },
		{ NULL, NULL },
	};

	purple_test_string_compare_free(jabber_get_resource, data);
}

static void
test_jabber_util_get_resource_none(void) {
	PurpleTestStringData data[] = {
		{ "foo@bar", NULL },
		{ "bar", NULL },
		{ NULL, NULL },
	};

	purple_test_string_compare_free(jabber_get_resource, data);
}

static void
test_jabber_util_get_bare_jid(void) {
	PurpleTestStringData data[] = {
		{ "foo@bar", "foo@bar" },
		{ "foo@bar/baz", "foo@bar" },
		{ "bar", "bar" },
		{ "bar/baz", "bar" },
		{ NULL, NULL },
	};

	purple_test_string_compare_free(jabber_get_bare_jid, data);
}

static void
test_jabber_util_nodeprep_validate(void) {
	const gchar *data[] = {
		"foo",
		"%d",
		"y\\z",
		"a=",
		"a,",
		NULL,
	};
	gchar *longnode;
	gint i;

	for(i = 0; data[i]; i++) {
		g_assert_true(jabber_nodeprep_validate(data[i]));
	}

	longnode = g_strnfill(1023, 'a');
	g_assert_true(jabber_nodeprep_validate(longnode));
	g_free(longnode);

	longnode = g_strnfill(1024, 'a');
	g_assert_false(jabber_nodeprep_validate(longnode));
	g_free(longnode);
}

static void
test_jabber_util_nodeprep_validate_illegal_chars(void) {
	const gchar *data[] = {
		"don't",
		"m@ke",
		"\"me\"",
		"&ngry",
		"c:",
		"a/b",
		"4>2",
		"4<7",
		NULL,
	};
	gint i;

	for(i = 0; data[i]; i++)
		g_assert_false(jabber_nodeprep_validate(data[i]));
}

static void
test_jabber_util_nodeprep_validate_too_long(void) {
	gchar *longnode = g_strnfill(1024, 'a');

	g_assert_false(jabber_nodeprep_validate(longnode));

	g_free(longnode);
}

static void
test_jabber_util_jabber_id_new_valid(void) {
	const gchar *jids[] = {
		"gmail.com",
		"gmail.com/Test",
		"gmail.com/Test@",
		"gmail.com/@",
		"gmail.com/Test@alkjaweflkj",
		"mark.doliner@gmail.com",
		"mark.doliner@gmail.com/Test12345",
		"mark.doliner@gmail.com/Test@12345",
		"mark.doliner@gmail.com/Te/st@12@//345",
		"わいど@conference.jabber.org",
		"まりるーむ@conference.jabber.org",
		"mark.doliner@gmail.com/まりるーむ",
		"mark.doliner@gmail/stuff.org",
		"stuart@nödåtXäYZ.se",
		"stuart@nödåtXäYZ.se/まりるーむ",
		"mark.doliner@わいど.org",
		"nick@まつ.おおかみ.net",
		"paul@10.0.42.230/s",
		"paul@[::1]", /* IPv6 */
		"paul@[2001:470:1f05:d58::2]",
		"paul@[2001:470:1f05:d58::2]/foo",
		"pa=ul@10.0.42.230",
		"pa,ul@10.0.42.230",
		NULL
	};
	gint i;

	for(i = 0; jids[i]; i++) {
		JabberID *jid = jabber_id_new(jids[i]);

		g_assert_nonnull(jid);

		jabber_id_free(jid);
	}
}

static void
test_jabber_util_jabber_id_new_invalid(void) {
	const gchar *jids[] = {
		"@gmail.com",
		"@@gmail.com",
		"mark.doliner@@gmail.com/Test12345",
		"mark@doliner@gmail.com/Test12345",
		"@gmail.com/Test@12345",
		"/Test@12345",
		"mark.doliner@",
		"mark.doliner/",
		"mark.doliner@gmail_stuff.org",
		"mark.doliner@gmail[stuff.org",
		"mark.doliner@gmail\\stuff.org",
		"paul@[::1]124",
		"paul@2[::1]124/as",
		"paul@まつ.おおかみ/\x01",
		/*
		 * RFC 3454 Section 6 reads, in part,
		 * "If a string contains any RandALCat character, the
		 *  string MUST NOT contain any LCat character."
		 * The character is U+066D (ARABIC FIVE POINTED STAR).
		 */
		"foo@example.com/٭simplexe٭",
		NULL,
	};
	gint i;

	for(i = 0; jids[i]; i++)
		g_assert_null(jabber_id_new(jids[i]));
}

#define assert_jid_parts(expect_node, expect_domain, str) G_STMT_START { \
	JabberID *jid = jabber_id_new(str); \
	g_assert_nonnull(jid); \
	g_assert_nonnull(jid->node); \
	g_assert_nonnull(jid->domain); \
	g_assert_null(jid->resource); \
	g_assert_cmpstr(expect_node, ==, jid->node); \
	g_assert_cmpstr(expect_domain, ==, jid->domain); \
	jabber_id_free(jid); \
} G_STMT_END


static void
test_jabber_util_jid_parts(void) {
	/* Ensure that jabber_id_new is properly lowercasing node and domains */
	assert_jid_parts("paul", "darkrain42.org", "PaUL@darkrain42.org");
	assert_jid_parts("paul", "darkrain42.org", "paul@DaRkRaIn42.org");

	/* These case-mapping tests culled from examining RFC3454 B.2 */

	/* Cyrillic capital EF (U+0424) maps to lowercase EF (U+0444) */
	assert_jid_parts("ф", "darkrain42.org", "Ф@darkrain42.org");

#ifdef USE_IDN
	/*
	 * These character (U+A664 and U+A665) are not mapped to anything in
	 * RFC3454 B.2. This first test *fails* when not using IDN because glib's
	 * case-folding/utf8_strdown improperly (for XMPP) lowercases the character.
	 *
	 * This is known, but not (very?) likely to actually cause a problem, so
	 * this test is commented out when using glib's functions.
	 */
	assert_jid_parts("Ꙥ", "darkrain42.org", "Ꙥ@darkrain42.org");
	assert_jid_parts("ꙥ", "darkrain42.org", "ꙥ@darkrain42.org");
#endif

	/* U+04E9 to U+04E9 */
	assert_jid_parts("paul", "өarkrain42.org", "paul@Өarkrain42.org");
}

static const gchar *
partial_jabber_normalize(const gchar *str) {
	return jabber_normalize(NULL, str);
}

static void
test_jabber_util_jabber_normalize(void) {
	PurpleTestStringData data[] = {
		{
			"PaUL@DaRkRain42.org",
			"paul@darkrain42.org",
		}, {
			"PaUL@DaRkRain42.org/",
			"paul@darkrain42.org",
		}, {
			"PaUL@DaRkRain42.org/resource",
			"paul@darkrain42.org",
		}, {
			NULL,
			NULL,
		}
	};

	purple_test_string_compare(partial_jabber_normalize, data);
}

gint
main(gint argc, gchar **argv) {
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/jabber/util/get_resource/exists",
	                test_jabber_util_get_resource_exists);
	g_test_add_func("/jabber/util/get_resource/none",
	                test_jabber_util_get_resource_none);

	g_test_add_func("/jabber/util/get_bare_jid",
	                test_jabber_util_get_bare_jid);

	g_test_add_func("/jabber/util/nodeprep/validate/valid",
	                test_jabber_util_nodeprep_validate);
	g_test_add_func("/jabber/util/nodeprep/validate/illegal_chars",
	                test_jabber_util_nodeprep_validate_illegal_chars);
	g_test_add_func("/jabber/util/nodeprep/validate/too_long",
	                test_jabber_util_nodeprep_validate_too_long);

	g_test_add_func("/jabber/util/id_new/valid",
	                test_jabber_util_jabber_id_new_valid);
	g_test_add_func("/jabber/util/id_new/invalid",
	                test_jabber_util_jabber_id_new_invalid);
	g_test_add_func("/jabber/util/id_new/jid_parts",
	                test_jabber_util_jid_parts);

	g_test_add_func("/jabber/util/normalize",
	                test_jabber_util_jabber_normalize);

	return g_test_run();
}
