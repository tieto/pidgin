if HAVE_CHECK
TESTS=check_libgaim

check_PROGRAMS=check_libgaim

check_libgaim_SOURCES=\
        check_libgaim.c \
	    tests.h \
		test_cipher.c \
		test_jabber_jutil.c \
		test_util.c \
		$(top_builddir)/libpurple/util.h

check_libgaim_CFLAGS=\
        @CHECK_CFLAGS@ \
		$(GLIB_CFLAGS) \
		-I.. \
		-DBUILDDIR=\"$(top_builddir)\"

check_libgaim_LDADD=\
        @CHECK_LIBS@ \
		$(GLIB_LIBS) \
		$(top_builddir)/libpurple/protocols/jabber/libjabber.la \
		$(top_builddir)/libpurple/libgaim.la

endif
