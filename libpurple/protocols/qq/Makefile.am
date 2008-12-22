EXTRA_DIST = \
	Makefile.mingw

pkgdir = $(libdir)/purple-$(PURPLE_MAJOR_VERSION)

QQSOURCES = \
	buddy_info.c \
	buddy_info.h \
	buddy_list.c \
	buddy_list.h \
	buddy_opt.c \
	buddy_opt.h \
	char_conv.c \
	char_conv.h \
	qq_crypt.c \
	qq_crypt.h \
	file_trans.c \
	file_trans.h \
	group.c \
	group.h \
	group_internal.c \
	group_internal.h \
	group_im.c \
	group_im.h \
	group_info.c \
	group_info.h \
	group_join.c \
	group_join.h \
	group_opt.c \
	group_opt.h \
	qq_define.c \
	qq_define.h \
	im.c \
	im.h \
	qq_process.c \
	qq_process.h \
	qq_base.c \
	qq_base.h \
	packet_parse.c \
	packet_parse.h \
	qq.c \
	qq.h \
	qq_network.c \
	qq_network.h \
	send_file.c \
	send_file.h \
	qq_trans.c \
	qq_trans.h \
	utils.c \
	utils.h

AM_CFLAGS = $(st)

libqq_la_LDFLAGS = -module -avoid-version

if STATIC_QQ

st = -DPURPLE_STATIC_PRPL
noinst_LTLIBRARIES = libqq.la
libqq_la_SOURCES   = $(QQSOURCES)
libqq_la_CFLAGS    = $(AM_CFLAGS)

else

st =
pkg_LTLIBRARIES  = libqq.la
libqq_la_SOURCES = $(QQSOURCES)
libqq_la_LIBADD  = $(GLIB_LIBS)

endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/libpurple \
	-I$(top_builddir)/libpurple \
	-DQQ_BUDDY_ICON_DIR=\"$(datadir)/pixmaps/purple/buddy_icons/qq\" \
	$(DEBUG_CFLAGS) \
	$(GLIB_CFLAGS)
