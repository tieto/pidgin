EXTRA_DIST = aim.h aim_cbtypes.h aim_internal.h faimconfig.h md5.h README \
		CHANGES COPYING BUGS AUTHORS

pkgdir = $(libdir)/gaim

CFLAGS += -DAIM_BUILDDATE=\"`date +%Y%m%d`\" -DAIM_BUILDTIME=\"`date +%H%M%S`\" -I\$(top_srcdir)/src $(st) $(DEBUG_CFLAGS)
liboscar_la_LDFLAGS =	-module -avoid-version

if STATIC_OSCAR

st = -DSTATIC
pkg_LTLIBRARIES =
noinst_LIBRARIES = liboscar.a

liboscar_a_SOURCES = 	admin.c		\
			adverts.c	\
			auth.c 		\
			bos.c 		\
			bstream.c 	\
			buddylist.c	\
			chat.c		\
			chatnav.c	\
			conn.c		\
			email.c		\
			ft.c		\
			icq.c		\
			im.c		\
			info.c		\
			invite.c	\
			meta.c		\
			misc.c		\
			msgcookie.c	\
			newsearch.c	\
			popups.c	\
			rxhandlers.c	\
			rxqueue.c	\
			search.c	\
			service.c	\
			snac.c		\
			ssi.c		\
			stats.c		\
			tlv.c		\
			translate.c	\
			txqueue.c	\
			util.c		\
			oscar.c

liboscar_a_CFLAGS = $(AM_CFLAGS)

else

st =
pkg_LTLIBRARIES = liboscar.la
noinst_LIBRARIES =

liboscar_la_SOURCES = 	admin.c		\
			adverts.c	\
			auth.c 		\
			bos.c 		\
			bstream.c 	\
			buddylist.c	\
			chat.c		\
			chatnav.c	\
			conn.c		\
			email.c		\
			ft.c		\
			icq.c		\
			im.c		\
			info.c		\
			invite.c	\
			meta.c		\
			misc.c		\
			msgcookie.c	\
			newsearch.c	\
			popups.c	\
			rxhandlers.c	\
			rxqueue.c	\
			search.c	\
			service.c	\
			snac.c		\
			ssi.c		\
			stats.c		\
			tlv.c		\
			translate.c	\
			txqueue.c	\
			util.c		\
			oscar.c

endif
