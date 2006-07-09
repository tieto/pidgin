EXTRA_DIST = \
		Makefile.mingw

pkgdir = $(libdir)/gaim

MSNSOURCES = \
	cmdproc.c \
	cmdproc.h \
	command.c \
	command.h \
	contact.c\
	contact.h\
	dialog.c \
	dialog.h \
	directconn.c \
	directconn.h \
	error.c \
	error.h \
	group.c \
	group.h \
	history.c \
	history.h \
	httpconn.c \
	httpconn.h \
	msg.c \
	msg.h \
	msn.c \
	msn.h \
	nexus.c \
	nexus.h \
	notification.c \
	notification.h \
	object.c \
	object.h \
	page.c \
	page.h \
	servconn.c \
	servconn.h \
	session.c \
	session.h \
	slp.c \
	slp.h \
	slpcall.c \
	slpcall.h \
	slplink.c \
	slplink.h \
	slpmsg.c \
	slpmsg.h \
	slpsession.c \
	slpsession.h \
	soap.c\
	soap.h\
	state.c \
	state.h \
	switchboard.c \
	switchboard.h \
	sync.c \
	sync.h \
	table.c \
	table.h \
	transaction.c \
	transaction.h \
	user.c \
	user.h \
	userlist.c \
	userlist.h \
	msn-utils.c \
	msn-utils.h

AM_CFLAGS = $(st)

libmsn_la_LDFLAGS = -module -avoid-version $(GLIB_LIBS)

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
