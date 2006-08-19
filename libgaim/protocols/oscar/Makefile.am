EXTRA_DIST = \
		COPYING \
		AUTHORS \
		Makefile.mingw

pkgdir = $(libdir)/gaim

OSCARSOURCES = \
	bstream.c           \
	family_admin.c      \
	family_advert.c     \
	family_alert.c      \
	family_auth.c       \
	family_bart.c       \
	family_bos.c        \
	family_buddy.c      \
	family_chat.c       \
	family_chatnav.c    \
	family_icq.c        \
	family_icbm.c       \
	family_invite.c     \
	family_locate.c     \
	family_odir.c       \
	family_oservice.c   \
	family_popup.c      \
	family_feedbag.c    \
	family_stats.c      \
	family_translate.c  \
	family_userlookup.c \
	flap_connection.c   \
	misc.c         \
	msgcookie.c         \
	odc.c               \
	oft.c               \
	oscar.c             \
	oscar.h             \
	oscar_data.c        \
	peer.c              \
	peer.h              \
	peer_proxy.c        \
	rxhandlers.c        \
	snac.c              \
	snactypes.h         \
	tlv.c               \
	util.c

AM_CFLAGS = $(st)

liboscar_la_LDFLAGS = -module -avoid-version $(GLIB_LIBS)

if STATIC_OSCAR

st = -DGAIM_STATIC_PRPL
noinst_LIBRARIES   = liboscar.a
liboscar_a_SOURCES = $(OSCARSOURCES)
liboscar_a_CFLAGS  = $(AM_CFLAGS)

else

st =
pkg_LTLIBRARIES     = liboscar.la
liboscar_la_SOURCES = $(OSCARSOURCES)

endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/libgaim \
	$(GLIB_CFLAGS) \
	$(DEBUG_CFLAGS)
