#include <glib.h>

#include "util.h"
#include "protocols/jabber/auth_digest_md5.h"
#include "protocols/jabber/jutil.h"

static void
test_jabber_digest_md5_parsing(void) {
	GHashTable *table;
	const gchar *value = NULL;

	#define check_value(name, expected) G_STMT_START {\
		value = g_hash_table_lookup(table, (name)); \
		g_assert_nonnull(value); \
		g_assert_cmpstr((expected), ==, value); \
	} G_STMT_END

	table = jabber_auth_digest_md5_parse("r=\"realm\",token=   \"   asdf\"");
	check_value("r", "realm");
	check_value("token", "asdf");
	g_hash_table_destroy(table);

	table = jabber_auth_digest_md5_parse("r=\"a\", token=   \"   asdf\"");
	check_value("r", "a");
	check_value("token", "asdf");
	g_hash_table_destroy(table);

	table = jabber_auth_digest_md5_parse("r=\"\", token=   \"   asdf\"");
	check_value("r", "");
	check_value("token", "asdf");
	g_hash_table_destroy(table);

	table = jabber_auth_digest_md5_parse("realm=\"somerealm\",nonce=\"OA6MG9tEQGm2hh\",qop=\"auth\",charset=utf-8,algorithm=md5-sess");
	check_value("realm", "somerealm");
	check_value("nonce", "OA6MG9tEQGm2hh");
	check_value("qop", "auth");
	check_value("charset", "utf-8");
	check_value("algorithm", "md5-sess");
	g_hash_table_destroy(table);
}

gint
main(gint argc, gchar **argv) {
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/jabber/digest/md5/parsing",
	                test_jabber_digest_md5_parsing);

	return g_test_run();
}
