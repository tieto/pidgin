noinst_LIBRARIES = libfaim.a

EXTRA_DIST = faim/aim.h faim/aim_cbtypes.h faim/faimconfig.h md5.h README \
		README.gaim CHANGES CHANGES.gaim COPYING BUGS AUTHORS

libfaim_a_SOURCES = 	aim_ads.c		\
			aim_auth.c 		\
			aim_buddylist.c		\
			aim_chat.c		\
			aim_chatnav.c		\
			aim_conn.c		\
			aim_ft.c		\
			aim_im.c		\
			aim_info.c		\
			aim_login.c		\
			aim_logoff.c		\
			aim_meta.c		\
			aim_misc.c		\
			aim_msgcookie.c		\
			aim_rxhandlers.c	\
			aim_rxqueue.c		\
			aim_search.c		\
			aim_snac.c		\
			aim_tlv.c		\
			aim_txqueue.c		\
			aim_util.c		\
			md5.c

CFLAGS += -DAIM_BUILDDATE=\"`date +%Y%m%d`\" -DAIM_BUILDTIME=\"`date +%H%M%S`\"

