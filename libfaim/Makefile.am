noinst_LIBRARIES = libfaim.a

EXTRA_DIST = aim.h aim_cbtypes.h aim_internal.h faimconfig.h md5.h README \
		CHANGES COPYING BUGS AUTHORS

libfaim_a_SOURCES = 	admin.c		\
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
			md5.c		\
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
			util.c

CFLAGS += -DAIM_BUILDDATE=\"`date +%Y%m%d`\" -DAIM_BUILDTIME=\"`date +%H%M%S`\"

