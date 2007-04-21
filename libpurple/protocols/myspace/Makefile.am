EXTRA_DIST = \
		PROTOCOL \
		Makefile.mingw

pkgdir = $(libdir)/gaim

MSIMSOURCES = myspace.c

AM_CFLAGS = $(st)

libmyspace_la_LDFLAGS = -module -avoid-version

if STATIC_MYSPACE

st = -DGAIM_STATIC_PRPL
noinst_LIBRARIES = libmyspace.a
libmyspace_a_SOURCES = $(MSIMSOURCES)
libmyspace_a_CFLAGS  = $(AM_CFLAGS)

else

st =
pkg_LTLIBRARIES   = libmyspace.la
libmyspace_la_SOURCES = $(MSIMSOURCES)

endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/libgaim \
	$(GLIB_CFLAGS) \
	$(DEBUG_CFLAGS)
