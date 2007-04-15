if HAVE_CHECK
TESTS=check_libpurple

check_PROGRAMS=check_libpurple

check_libpurple_SOURCES=\
        check_libpurple.c \
	    tests.h \
		test_cipher.c \
		test_jabber_jutil.c \
		test_util.c \
		$(top_builddir)/libpurple/util.h

check_libpurple_CFLAGS=\
        @CHECK_CFLAGS@ \
		$(GLIB_CFLAGS) \
		$(DEBUG_CFLAGS) \
		-I.. \
		-DBUILDDIR=\"$(top_builddir)\"

check_libpurple_LDADD=\
        @CHECK_LIBS@ \
		$(GLIB_LIBS) \
		$(top_builddir)/libpurple/protocols/jabber/libjabber.la \
		$(top_builddir)/libpurple/libpurple.la

endif
