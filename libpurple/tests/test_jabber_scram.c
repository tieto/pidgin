#include <string.h>

#include "tests.h"
#include "../util.h"
#include "../protocols/jabber/auth_scram.h"

#define assert_pbkdf2_equal(password, salt, count, expected) { \
	GString *p = g_string_new(password); \
	GString *s = g_string_new(salt); \
	GString *result = jabber_auth_scram_hi("sha1", p, s, count); \
	fail_if(result == NULL, "Hi() returned NULL"); \
	fail_if(result->len != 20, "Hi() returned with unexpected length %u", result->len); \
	fail_if(0 != memcmp(result->str, expected, 20), "Hi() returned invalid result"); \
	g_string_free(result, TRUE); \
	g_string_free(s, TRUE); \
	g_string_free(p, TRUE); \
}

START_TEST(test_pbkdf2)
{
	assert_pbkdf2_equal("password", "salt", 1, "\x0c\x60\xc8\x0f\x96\x1f\x0e\x71\xf3\xa9\xb5\x24\xaf\x60\x12\x06\x2f\xe0\x37\xa6");
	
	assert_pbkdf2_equal("password", "salt", 2, "\xea\x6c\x01\x4d\xc7\x2d\x6f\x8c\xcd\x1e\xd9\x2a\xce\x1d\x41\xf0\xd8\xde\x89\x57");

	assert_pbkdf2_equal("password", "salt", 4096, "\x4b\x00\x79\x01\xb7\x65\x48\x9a\xbe\xad\x49\xd9\x26\xf7\x21\xd0\x65\xa4\x29\xc1");

#if 0
	/* This causes libcheck to time out :-D */
	assert_pbkdf2_equal("password", "salt", 16777216, "\xee\xfe\x3d\x61\xcd\x4d\xa4\xe4\xe9\x94\x5b\x3d\x6b\xa2\x15\x8c\x26\x34\xe9\x84");
#endif
}
END_TEST

Suite *
jabber_scram_suite(void)
{
	Suite *s = suite_create("Jabber SASL SCRAM functions");

	TCase *tc = tcase_create("PBKDF2 Functionality");
	tcase_add_test(tc, test_pbkdf2);
	suite_add_tcase(s, tc);

	return s;
}
