pkgdir = $(libdir)/gaim

TREPIASOURCES = \
	md5.h \
	profile.c \
	profile.h \
	trepia.c

AM_CFLAGS = $(st)

libtrepia_la_LDFLAGS = -module -avoid-version

if STATIC_TREPIA

st = -DSTATIC
noinst_LIBRARIES = libtrepia.a
libtrepia_a_SOURCES = $(TREPIASOURCES)
libtrepia_a_CFLAGS  = $(AM_CFLAGS)

else

st =
pkg_LTLIBRARIES   = libtrepia.la
libtrepia_la_SOURCES = $(TREPIASOURCES)

endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/src \
	$(DEBUG_CFLAGS)
