noinst_LIBRARIES = $(LIBFAIM_DO)

EXTRA_LIBRARIES = libfaim.a

EXTRA_DIST = aim.h aim_cbtypes.h faimconfig.h README README.gaim CHANGES \
		CHANGES.gaim COPYING BUGS

#libfaim_a_SOURCES = aim_chatnav.c aim_info.c aim_rxhandlers.c \
#                    aim_tlv.c aim_auth.c aim_conn.c aim_login.c \
#                    aim_rxqueue.c aim_txqueue.c aim_buddylist.c \
#                    aim_global.c aim_logoff.c aim_search.c aim_util.c \
#                    aim_chat.c aim_im.c aim_misc.c aim_snac.c
libfaim_a_SOURCES = aim_auth.c aim_buddylist.c aim_chat.c aim_chatnav.c \
		    aim_conn.c aim_im.c aim_info.c aim_login.c aim_logoff.c \
		    aim_meta.c aim_misc.c aim_rxhandlers.c aim_rxqueue.c \
		    aim_search.c aim_snac.c aim_tlv.c aim_txqueue.c aim_util.c

CFLAGS += $(GAIM_CFLAGS) -I../src -DAIM_BUILDDATE=`date +%Y%m%e` -DAIM_BUILDTIME=`date +%H%M%S`

