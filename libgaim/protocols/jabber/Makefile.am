EXTRA_DIST = \
		Makefile.mingw \
		win32/posix.uname.c \
		win32/utsname.h

pkgdir = $(libdir)/gaim

JABBERSOURCES = auth.c \
			  auth.h \
			  buddy.c \
			  buddy.h \
			  chat.c \
			  chat.h \
			  disco.c \
			  disco.h \
			  google.c \
			  google.h \
			  iq.c \
			  iq.h \
			  jabber.c \
			  jabber.h \
			  jutil.c \
			  jutil.h \
			  message.c \
			  message.h \
			  oob.c \
			  oob.h \
			  parser.c \
			  parser.h \
			  presence.c \
			  presence.h \
			  roster.c \
			  roster.h \
			  si.c \
			  si.h \
			  xdata.c \
			  xdata.h

AM_CFLAGS = $(st)

libjabber_la_LDFLAGS = -module -avoid-version

if STATIC_JABBER

st = -DGAIM_STATIC_PRPL
noinst_LIBRARIES = libjabber.a
pkg_LTLIBRARIES =

libjabber_a_SOURCES = $(JABBERSOURCES)
libjabber_a_CFLAGS  = $(AM_CFLAGS)

else

st =
pkg_LTLIBRARIES = libjabber.la
noinst_LIBRARIES =

libjabber_la_SOURCES = $(JABBERSOURCES)
libjabber_la_LIBADD = $(GLIB_LIBS) $(SASL_LIBS) $(LIBXML_LIBS)

endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/libgaim \
	$(DEBUG_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(LIBXML_CFLAGS)
