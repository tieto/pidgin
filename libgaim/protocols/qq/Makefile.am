EXTRA_DIST = \
		Makefile.mingw

pkgdir = $(libdir)/gaim

QQSOURCES = \
	utils.c \
	utils.h \
	packet_parse.c \
	packet_parse.h \
	buddy_info.c \
	buddy_info.h \
	buddy_list.c \
	buddy_list.h \
	buddy_opt.c \
	buddy_opt.h \
	buddy_status.c \
	buddy_status.h \
	qq.c \
	char_conv.c \
	char_conv.h \
	crypt.c \
	crypt.h \
	group.c \
	group_conv.c \
	group_conv.h \
	group_find.c \
	group_find.h \
	group_free.c \
	group_free.h \
	group.h \
	group_hash.c \
	group_hash.h \
	group_im.c \
	group_im.h \
	group_info.c \
	group_info.h \
	group_join.c \
	group_join.h \
	group_misc.c \
	group_misc.h \
	group_network.c \
	group_network.h \
	group_opt.c \
	group_opt.h \
	group_search.c \
	group_search.h \
	qq.h \
	header_info.c \
	header_info.h \
	im.c \
	im.h \
	keep_alive.c \
	keep_alive.h \
	login_logout.c \
	login_logout.h \
	qq_proxy.c \
	qq_proxy.h \
	recv_core.c \
	recv_core.h \
	send_core.c \
	send_core.h \
	sendqueue.c \
	sendqueue.h \
	sys_msg.c \
	sys_msg.h \
	udp_proxy_s5.c \
	udp_proxy_s5.h \
	send_file.c \
	send_file.h \
	file_trans.c \
	file_trans.h

AM_CFLAGS = $(st)

libqq_la_LDFLAGS = -module -avoid-version

if STATIC_QQ

st = -DGAIM_STATIC_PRPL
noinst_LIBRARIES = libqq.a
libqq_a_SOURCES  = $(QQSOURCES)
libqq_a_CFLAGS   = $(AM_CFLAGS)

else

st =
pkg_LTLIBRARIES  = libqq.la
libqq_la_SOURCES = $(QQSOURCES)

endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/libgaim \
        -DVERSION=\"$(VERSION)\" \
        $(DEBUG_CFLAGS) \
        $(GLIB_CFLAGS) \
	$(GAIM_CFLAGS)
