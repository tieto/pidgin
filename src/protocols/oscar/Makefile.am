EXTRA_DIST = \
		COPYING \
		AUTHORS \
		Makefile.mingw

pkgdir = $(libdir)/gaim

OSCARSOURCES = \
	admin.c        \
	adverts.c      \
	aim.h          \
	aim_cbtypes.h  \
	aim_internal.h \
	auth.c         \
	bart.c         \
	bos.c          \
	bstream.c      \
	buddylist.c    \
	chat.c         \
	chatnav.c      \
	conn.c         \
	email.c        \
	ft.c           \
	icq.c          \
	im.c           \
	invite.c       \
	locate.c       \
	md5.c          \
	md5.h		   \
	misc.c         \
	msgcookie.c    \
	odir.c         \
	popups.c       \
	rxhandlers.c   \
	rxqueue.c      \
	search.c       \
	service.c      \
	snac.c         \
	ssi.c          \
	stats.c        \
	tlv.c          \
	translate.c    \
	txqueue.c      \
	util.c         \
	oscar.c

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
	-I$(top_srcdir)/src \
	$(GLIB_CFLAGS) \
	$(DEBUG_CFLAGS)
