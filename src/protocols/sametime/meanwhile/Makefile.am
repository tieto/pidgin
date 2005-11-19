noinst_HEADERS = \
	mw_channel.h \
	mw_cipher.h \
	mw_common.h \
	mw_debug.h \
	mw_error.h \
	mw_message.h \
	mw_service.h \
	mw_session.h \
	mw_srvc_aware.h \
	mw_srvc_conf.h \
	mw_srvc_ft.h \
	mw_srvc_im.h \
	mw_srvc_place.h \
	mw_srvc_resolve.h \
	mw_srvc_store.h \
	mw_st_list.h \
	mw_util.h 


MEANWHILESOURCES = \
	channel.c \
	cipher.c \
	common.c \
	error.c \
	message.c \
	mw_debug.c \
	mw_util.c \
	service.c \
	session.c \
	srvc_aware.c \
	srvc_conf.c \
	srvc_ft.c \
	srvc_im.c \
	srvc_place.c \
	srvc_store.c \
	srvc_resolve.c \
	st_list.c

AM_CFLAGS = \
	$(GLIB_CFLAGS) $(GMP_CFLAGS) \
	$(DEBUG_CFLAGS) \
	-DG_LOG_DOMAIN=\"meanwhile\"
	-I$(top_srcdir)/src 

libmeanwhile_la_SOURCES = $(MEANWHILESOURCES)

libmeanwhile_la_LIBADD = $(GLIB_LIBS) $(GMP_LIBS)

st =
noinst_LTLIBRARIES = libmeanwhile.la

