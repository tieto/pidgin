EXTRA_DIST = xmltok_impl.c xmltok_ns.c

pkgdir = $(libdir)/gaim

JABBERSOURCES = \
			asciitab.h \
			expat.c \
			genhash.c \
			hashtable.c \
			hashtable.h \
			karma.c \
			iasciitab.h \
			latin1tab.h \
			lib.h \
			libxode.h \
			nametab.h \
			pool.c \
			sha.c \
			snprintf.c \
			socket.c \
			str.c \
			utf8tab.h \
			xhash.c \
			xmldef.h \
			xmlnode.c \
			xmlparse.c \
			xmlparse.h \
			xmlrole.c \
			xmlrole.h \
			xmltok.c \
			xmltok.h \
			xmltok_impl.h \
			xstream.c \
			\
			jabber.h \
			jconn.c \
			jid.c \
			jpacket.c \
			jutil.c \
			log.c \
			log.h \
			pproxy.c \
			rate.c \
			\
			jabber.c

AM_CFLAGS = $(st)

libjabber_la_LDFLAGS = -module -avoid-version

if STATIC_JABBER

st = -DSTATIC
noinst_LIBRARIES = libjabber.a

libjabber_a_SOURCES = $(JABBERSOURCES)
libjabber_a_CFLAGS = $(AM_CFLAGS)

else

st =
pkg_LTLIBRARIES = libjabber.la

libjabber_la_SOURCES = $(JABBERSOURCES)

endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/src \
	$(DEBUG_CFLAGS)
