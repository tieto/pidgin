EXTRA_DIST = \
	Makefile.mingw \
	win32/posix.uname.c \
	win32/utsname.h

pkgdir = $(libdir)/purple-$(PURPLE_MAJOR_VERSION)

JABBERSOURCES = auth.c \
			  auth.h \
			  buddy.c \
			  buddy.h \
			  chat.c \
			  chat.h \
			  data.c \
			  data.h \
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
			  ping.c \
			  ping.h \
			  presence.c \
			  presence.h \
			  roster.c \
			  roster.h \
			  si.c \
			  si.h \
			  xdata.c \
			  xdata.h \
			  caps.c \
			  caps.h \
			  adhoccommands.c \
			  adhoccommands.h \
			  pep.c \
			  pep.h \
			  usermood.c \
			  usermood.h \
			  usernick.c \
			  usernick.h \
			  usertune.c \
			  usertune.h

AM_CFLAGS = $(st)

libxmpp_la_LDFLAGS = -module -avoid-version

if STATIC_JABBER

st = -DPURPLE_STATIC_PRPL
noinst_LTLIBRARIES   = libjabber.la
libjabber_la_SOURCES = $(JABBERSOURCES) libxmpp.c
libjabber_la_CFLAGS  = $(AM_CFLAGS)

else

st =
pkg_LTLIBRARIES      = libjabber.la libxmpp.la
libjabber_la_SOURCES = $(JABBERSOURCES)
libjabber_la_LIBADD  = $(GLIB_LIBS) $(SASL_LIBS) $(LIBXML_LIBS)

libxmpp_la_SOURCES = libxmpp.c
libxmpp_la_LIBADD = libjabber.la

endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/libpurple \
	-I$(top_builddir)/libpurple \
	$(DEBUG_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(LIBXML_CFLAGS)
