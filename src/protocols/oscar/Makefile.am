EXTRA_DIST = aim.h aim_cbtypes.h aim_internal.h faimconfig.h README \
		CHANGES COPYING BUGS AUTHORS

pkgdir = $(libdir)/gaim

CFLAGS += -DAIM_BUILDDATE=\"`date +%Y%m%d`\" -DAIM_BUILDTIME=\"`date +%H%M%S`\" -I\$(top_srcdir)/src $(st) $(DEBUG_CFLAGS)
liboscar_la_LDFLAGS =	-avoid-version

if STATIC_OSCAR

st = -DSTATIC
pkg_LTLIBRARIES =
noinst_LIBRARIES = liboscar.a

liboscar_a_SOURCES = 	admin.c		\
			adverts.c	\
			auth.c 		\
			bos.c 		\
			buddylist.c	\
			chat.c		\
			chatnav.c	\
			conn.c		\
			ft.c		\
			im.c		\
			info.c		\
			login.c		\
			meta.c		\
			misc.c		\
			msgcookie.c	\
			rxhandlers.c	\
			rxqueue.c	\
			search.c	\
			snac.c		\
			stats.c		\
			tlv.c		\
			txqueue.c	\
			util.c		\
			oscar.c


else

st =
pkg_LTLIBRARIES = liboscar.la
noinst_LIBRARIES =

liboscar_la_SOURCES = 	admin.c		\
			adverts.c	\
			auth.c 		\
			bos.c 		\
			buddylist.c	\
			chat.c		\
			chatnav.c	\
			conn.c		\
			ft.c		\
			im.c		\
			info.c		\
			login.c		\
			meta.c		\
			misc.c		\
			msgcookie.c	\
			rxhandlers.c	\
			rxqueue.c	\
			search.c	\
			snac.c		\
			stats.c		\
			tlv.c		\
			txqueue.c	\
			util.c		\
			oscar.c

endif
