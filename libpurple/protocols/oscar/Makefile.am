EXTRA_DIST = \
	COPYING \
	AUTHORS \
	Makefile.mingw

pkgdir = @PURPLE_PLUGINDIR@

OSCARSOURCES = \
	authorization.c     \
	aim.c               \
	aim.h               \
	bstream.c           \
	clientlogin.c       \
	encoding.c          \
	encoding.h          \
	family_admin.c      \
	family_alert.c      \
	family_auth.c       \
	family_bart.c       \
	family_bos.c        \
	family_buddy.c      \
	family_chat.c       \
	family_chatnav.c    \
	family_icq.c        \
	family_icbm.c       \
	family_locate.c     \
	family_oservice.c   \
	family_popup.c      \
	family_feedbag.c    \
	family_stats.c      \
	family_userlookup.c \
	flap_connection.c   \
	icq.c               \
	icq.h               \
	misc.c              \
	msgcookie.c         \
	odc.c               \
	oft.c               \
	oscar.c             \
	oscar.h             \
	oscarcommon.h       \
	oscar_data.c        \
	peer.c              \
	peer.h              \
	peer_proxy.c        \
	rxhandlers.c        \
	snac.c              \
	snactypes.h         \
	tlv.c               \
	userinfo.c          \
	util.c              \
	visibility.c	    \
	visibility.h

AM_CFLAGS = $(st)

liboscar_la_LDFLAGS = -module @PLUGIN_LDFLAGS@

if STATIC_OSCAR

st = -DPURPLE_STATIC_PRPL
noinst_LTLIBRARIES  = liboscar.la
liboscar_la_SOURCES = $(OSCARSOURCES)
liboscar_la_CFLAGS  = $(AM_CFLAGS)

else

st =
pkg_LTLIBRARIES     = liboscar.la
liboscar_la_SOURCES = $(OSCARSOURCES)
liboscar_la_LIBADD  = @PURPLE_LIBS@

endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/libpurple \
	-I$(top_builddir)/libpurple \
	$(GLIB_CFLAGS) \
	$(GPLUGIN_CFLAGS) \
	$(DEBUG_CFLAGS)
