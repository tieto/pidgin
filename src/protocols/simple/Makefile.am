pkgdir = $(libdir)/gaim

SIMPLESOURCES = \
	simple.c \
	simple.h \
	sipmsg.c \
	sipmsg.h \
	digcalc.c \
	digcalc.h \
	srvresolve.h \
	srvresolve.c

AM_CFLAGS = $(st) -lresolv

libsimple_la_LDFLAGS = -module -avoid-version $(GLIB_LIBS)

if STATIC_MSN

st = -DGAIM_STATIC_PRPL
noinst_LIBRARIES = libsimple.a
libsimple_a_SOURCES = $(SIMPLESOURCES)
libsimple_a_CFLAGS  = $(AM_CFLAGS)

else

st =
pkg_LTLIBRARIES   = libsimple.la
libsimple_la_SOURCES = $(SIMPLESOURCES)

endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/src \
	$(GLIB_CFLAGS) \
	$(DEBUG_CFLAGS)
