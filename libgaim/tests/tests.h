#ifndef TESTS_H
#  define TESTS_H

#include <glib.h>
#include <check.h>

/* define the test suites here */
/* remember to add the suite to the runner in check_libgaim.c */
Suite * cipher_suite(void);
Suite * jabber_jutil_suite(void);
Suite * util_suite(void);

/* helper macros */
#define assert_string_equal(expected, actual) { \
	const gchar *a = actual; \
	fail_unless(strcmp(expected, a) == 0, "Expecting '%s' but got '%s'", expected, a); \
}

#define assert_string_equal_free(expected, actual) { \
	gchar *b = actual; \
	assert_string_equal(expected, b); \
	g_free(b); \
}


#endif /* ifndef TESTS_H */

