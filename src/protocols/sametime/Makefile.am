

EXTRA_DIST = # Makefile.mingw


pkgdir = $(libdir)/gaim


noinst_HEADERS = \
	meanwhile/mw_channel.h \
	meanwhile/mw_cipher.h \
	meanwhile/mw_common.h \
	meanwhile/mw_debug.h \
	meanwhile/mw_error.h \
	meanwhile/mw_message.h \
	meanwhile/mw_service.h \
	meanwhile/mw_session.h \
	meanwhile/mw_srvc_aware.h \
	meanwhile/mw_srvc_conf.h \
	meanwhile/mw_srvc_ft.h \
	meanwhile/mw_srvc_im.h \
	meanwhile/mw_srvc_resolve.h \
	meanwhile/mw_srvc_store.h \
	meanwhile/mw_st_list.h \
	meanwhile/mw_util.h \
	sametime.h

meanwhile_SOURCES = \
	meanwhile/channel.c \
	meanwhile/cipher.c \
	meanwhile/common.c \
	meanwhile/error.c \
	meanwhile/message.c \
	meanwhile/mw_debug.c \
	meanwhile/mw_util.c \
	meanwhile/service.c \
	meanwhile/session.c \
	meanwhile/srvc_aware.c \
	meanwhile/srvc_conf.c \
	meanwhile/srvc_ft.c \
	meanwhile/srvc_im.c \
	meanwhile/srvc_store.c \
	meanwhile/srvc_resolve.c \
	meanwhile/st_list.c

sametime_SOURCES = \
	$(meanwhile_SOURCES) \
	sametime.c


if STATIC_SAMETIME

st = -DGAIM_STATIC_PRPL
noinst_LIBRARIES   = libsametime.a
libsametime_a_SOURCES = $(sametime_SOURCES)
libsametime_a_CFLAGS  = $(AM_CFLAGS)

else

st =
pkg_LTLIBRARIES     = libsametime.la
libsametime_la_SOURCES = $(sametime_SOURCES)

endif


libsametime_la_LDFLAGS = -module -avoid-version -no-undefined
libsametime_la_LIBADD = $(GLIB_LIBS)


AM_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(DEBUG_CFLAGS) \
	-I$(top_srcdir)/src \
	-Imeanwhile


AM_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"meanwhile\" \
	$(st)

