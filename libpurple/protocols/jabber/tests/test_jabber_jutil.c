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
		"noone@example.com",
		"noone@example.com/Test12345",
		"noone@example.com/Test@12345",
		"noone@example.com/Te/st@12@//345",
		"わいど@conference.jabber.org",
		"まりるーむ@conference.jabber.org",
		"noone@example.com/まりるーむ",
		"noone@example/stuff.org",
		"noone@nödåtXäYZ.example",
		"noone@nödåtXäYZ.example/まりるーむ",
		"noone@わいど.org",
		"noone@まつ.おおかみ.net",
		"noone@310.0.42.230/s",
		"noone@[::1]", /* IPv6 */
		"noone@[3001:470:1f05:d58::2]",
		"noone@[3001:470:1f05:d58::2]/foo",
		"no=one@310.0.42.230",
		"no,one@310.0.42.230",
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
		"noone@@example.com/Test12345",
		"no@one@example.com/Test12345",
		"@example.com/Test@12345",
		"/Test@12345",
		"noone@",
		"noone/",
		"noone@gmail_stuff.org",
		"noone@gmail[stuff.org",
		"noone@gmail\\stuff.org",
		"noone@[::1]124",
		"noone@2[::1]124/as",
		"noone@まつ.おおかみ/\x01",
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
	assert_jid_parts("noone", "example.com", "NoOne@example.com");
	assert_jid_parts("noone", "example.com", "noone@ExaMPle.CoM");

	/* These case-mapping tests culled from examining RFC3454 B.2 */

	/* Cyrillic capital EF (U+0424) maps to lowercase EF (U+0444) */
	assert_jid_parts("ф", "example.com", "Ф@example.com");

#ifdef USE_IDN
	/*
	 * These character (U+A664 and U+A665) are not mapped to anything in
	 * RFC3454 B.2. This first test *fails* when not using IDN because glib's
	 * case-folding/utf8_strdown improperly (for XMPP) lowercases the character.
	 *
	 * This is known, but not (very?) likely to actually cause a problem, so
	 * this test is commented out when using glib's functions.
	 */
	assert_jid_parts("Ꙥ", "example.com", "Ꙥ@example.com");
	assert_jid_parts("ꙥ", "example.com", "ꙥ@example.com");
#endif

	/* U+04E9 to U+04E9 */
	assert_jid_parts("noone", "өexample.com", "noone@Өexample.com");
}

static const gchar *
partial_jabber_normalize(const gchar *str) {
	return jabber_normalize(NULL, str);
}

static void
test_jabber_util_jabber_normalize(void) {
	PurpleTestStringData data[] = {
		{
			"NoOnE@ExAMplE.com",
			"noone@example.com",
		}, {
			"NoOnE@ExampLE.cOM/",
			"noone@example.com",
		}, {
			"NoONe@exAMPle.CoM/resource",
			"noone@example.com",
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
