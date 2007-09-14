EXTRA_DIST = gendef.sh

pg_LTLIBRARIES = gnt.la

pgdir = $(libdir)

sources = \
	gnt.def \
	gnt.override \
	gntbox.override \
	gntfilesel.override \
	gnttree.override \
	gntwidget.override

gnt_la_SOURCES = gnt.c common.c common.h gntmodule.c

gnt_la_LDFLAGS = -module -avoid-version \
	`pkg-config --libs pygobject-2.0`

gnt_la_LIBADD = \
	$(GLIB_LIBS) \
	../libgnt.la

AM_CPPFLAGS = \
	-I../ \
	$(GLIB_CFLAGS) \
	$(GNT_CFLAGS)  \
	-I/usr/include/python2.4 \
	`pkg-config --cflags pygobject-2.0`

CLEANFILES = gnt.def gnt.c gnt.defe

gnt.def: $(srcdir)/../*.h
	$(srcdir)/gendef.sh

gnt.c: $(sources)
	pygtk-codegen-2.0 --prefix gnt \
	--override gnt.override \
	gnt.def > $@

