EXTRA_DIST = \
		Makefile.mingw

pkgdir = $(libdir)/gaim

MSNSOURCES = \
	cmdproc.c \
	cmdproc.h \
	command.c \
	command.h \
	error.c \
	error.h \
	group.c \
	group.h \
	history.c \
	history.h \
	httpmethod.c \
	httpmethod.h \
	msg.c \
	msg.h \
	msn.c \
	msn.h \
	msnobject.c \
	msnobject.h \
	msnslp.c \
	msnslp.h \
	nexus.c \
	nexus.h \
	notification.c \
	notification.h \
	page.c \
	page.h \
	servconn.c \
	servconn.h \
	session.c \
	session.h \
	state.c \
	state.h \
	switchboard.c \
	switchboard.h \
	table.c \
	table.h \
	transaction.c \
	transaction.h \
	user.c \
	user.h \
	utils.c \
	utils.h

AM_CFLAGS = $(st)

libmsn_la_LDFLAGS = -module -avoid-version

if STATIC_MSN

st = -DGAIM_STATIC_PRPL
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
	$(GLIB_CFLAGS) \
	$(DEBUG_CFLAGS)
