EXTRA_DIST = xmltok_impl.c xmltok_ns.c

pkgdir = $(libdir)/gaim

CFLAGS += -I\$(top_srcdir)/src $(st) $(DEBUG_CFLAGS)

if STATIC_JABBER

st = -DSTATIC
pkg_LTLIBRARIES =
noinst_LIBRARIES = libjabber.a

libjabber_a_SOURCES = \
			asciitab.h \
			expat.c \
			genhash.c \
			hashtable.c \
			hashtable.h \
			iasciitab.h \
			latin1tab.h \
			libxode.h \
			nametab.h \
			pool.c \
			sha.c \
			snprintf.c \
			socket.c \
			str.c \
			utf8tab.h \
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

else

st =
pkg_LTLIBRARIES = libjabber.la
noinst_LIBRARIES =

libjabber_la_SOURCES = \
			asciitab.h \
			expat.c \
			genhash.c \
			hashtable.c \
			hashtable.h \
			iasciitab.h \
			latin1tab.h \
			libxode.h \
			nametab.h \
			pool.c \
			sha.c \
			snprintf.c \
			socket.c \
			str.c \
			utf8tab.h \
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

endif
