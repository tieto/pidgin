EXTRA_DIST = PROTOCOL md5.h

pkgdir = $(libdir)/gaim

MSNSOURCES = \
	buddyicon.c \
	buddyicon.h \
	dispatch.c \
	dispatch.h \
	error.c \
	error.h \
	md5.h \
	msg.c \
	msg.h \
	msn.c \
	msn.h \
	notification.c \
	notification.h \
	servconn.c \
	servconn.h \
	session.c \
	session.h \
	state.c \
	state.h \
	switchboard.c \
	switchboard.h \
	user.c \
	user.h \
	utils.c \
	utils.h

AM_CFLAGS = $(st)

libmsn_la_LDFLAGS = -module -avoid-version

if STATIC_MSN

st = -DSTATIC
noinst_LIBRARIES = libmsn.a
libmsn_a_SOURCES = $(MSNSOURCES)
libmsn_a_CFLAGS  = $(AM_CFLAGS)

else

st =
pkg_LTLIBRARIES   = libmsn.la
libmsn_la_SOURCES = $(MSNSOURCES)

endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/src \
	$(DEBUG_CFLAGS)
