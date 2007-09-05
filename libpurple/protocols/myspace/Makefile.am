EXTRA_DIST = Makefile.mingw

pkgdir = $(libdir)/purple-$(PURPLE_MAJOR_VERSION)

SOURCES = myspace.c \
	  myspace.h \
	  persist.h \
	  message.c \
	  message.h \
	  zap.c \
	  session.c \
	  session.h \
	  markup.c \
	  markup.h \
	  user.c \
	  user.h

AM_CFLAGS = $(st)

libmyspace_la_LDFLAGS = -module -avoid-version

if STATIC_MYSPACE

st = -DPURPLE_STATIC_PRPL
noinst_LIBRARIES     = libmyspace.a
libmyspace_a_SOURCES = $(SOURCES)
libmyspace_a_CFLAGS  = $(AM_CFLAGS)

else

st =
pkg_LTLIBRARIES       = libmyspace.la
libmyspace_la_SOURCES = $(SOURCES)
libmyspace_la_LIBADD  = $(GLIB_LIBS)

endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/libpurple \
	$(GLIB_CFLAGS) \
	$(FARSIGHT_CFLAGS) \
	$(DEBUG_CFLAGS)
