EXTRA_DIST =	AUTHORS \
		COPYING \
		ChangeLog \
		INSTALL \
		README \
		VERSION

pkgdir = $(libdir)/gaim

CFLAGS += -I\$(top_srcdir)/src $(st) $(DEBUG_CFLAGS)
libicq_la_LDFLAGS =	-module -avoid-version

if STATIC_ICQ

st = -DSTATIC
pkg_LTLIBRARIES =
noinst_LIBRARIES = libicq.a

libicq_a_SOURCES = 	chatsession.c \
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

else

st =
pkg_LTLIBRARIES = libicq.la
noinst_LIBRARIES =

libicq_la_SOURCES =	chatsession.c \
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

endif
