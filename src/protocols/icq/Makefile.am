EXTRA_DIST = \
	AUTHORS \
	COPYING \
	ChangeLog \
	INSTALL \
	README \
	VERSION

pkgdir = $(libdir)/gaim

ICQSOURCES = \
	chatsession.c \
	chatsession.h \
	contacts.c \
	contacts.h \
	cyrillic.c \
	eventhandle.c \
	eventhandle.h \
	filesession.c \
	filesession.h \
	icq.h \
	icqbyteorder.c \
	icqbyteorder.h \
	icqevent.c \
	icqevent.h \
	icqlib.c \
	icqlib.h \
	icqpacket.c \
	icqpacket.h \
	icqtypes.h \
	list.c \
	list.h \
	proxy.c \
	queue.c \
	queue.h \
	socketmanager.c \
	socketmanager.h \
	stdpackets.c \
	stdpackets.h \
	tcp.c \
	tcp.h \
	tcpchathandle.c \
	tcpfilehandle.c \
	tcphandle.c \
	tcplink.c \
	tcplink.h \
	timeout.c \
	timeout.h \
	udp.c \
	udp.h \
	udphandle.c \
	util.c \
	util.h \
	gaim_icq.c

AM_CFLAGS = $(st)

libicq_la_LDFLAGS =	-module -avoid-version

st =
pkg_LTLIBRARIES = libicq.la

libicq_la_SOURCES =	$(ICQSOURCES)

AM_CPPFLAGS = \
	-I$(top_srcdir)/src \
	$(DEBUG_CFLAGS)
