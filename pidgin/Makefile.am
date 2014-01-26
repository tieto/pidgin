EXTRA_DIST = \
		getopt.c \
		getopt.h \
		getopt1.c \
		gtk3compat.h \
		gtkdebug.html \
		Makefile.mingw \
		pidgin-3.pc.in \
		pidgin-3-uninstalled.pc.in \
		win32/MinimizeToTray.h \
		win32/MinimizeToTray.c \
		win32/pidgin_dll_rc.rc.in \
		win32/pidgin_exe_rc.rc.in \
		win32/prepare-workspace.sh \
		win32/gtkdocklet-win32.c \
		win32/gtkwin32dep.c \
		win32/gtkwin32dep.h \
		win32/resource.h \
		win32/untar.c \
		win32/untar.h \
		win32/winpidgin.c \
		win32/wspell.c \
		win32/wspell.h \
		win32/nsis/generate_gtk_zip.sh \
		win32/nsis/pixmaps/pidgin-header.bmp \
		win32/nsis/pixmaps/pidgin-intro.bmp \
		win32/nsis/pixmaps/pidgin-install.ico \
		win32/nsis/available.lst \
		win32/nsis/pidgin-installer.nsi \
		win32/nsis/pidgin-plugin.nsh \
		win32/nsis/create_nsis_translations.pl \
		win32/nsis/nsis_translations.desktop.in

if ENABLE_GTK

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = pidgin-3.pc

SUBDIRS = pixmaps plugins themes

noinst_LTLIBRARIES = libpidgin.la
bin_PROGRAMS = pidgin

libpidgin_la_SOURCES = \
	pidginstock.c \
	gtkaccount.c \
	gtkblist.c \
	gtkblist-theme.c \
	gtkblist-theme-loader.c \
	gtkcellrendererexpander.c \
	gtkcertmgr.c \
	gtkconn.c \
	gtkconv.c \
	gtkconv-theme.c \
	gtkconv-theme-loader.c \
	gtkdebug.c \
	gtkdialogs.c \
	gtkdnd-hints.c \
	gtkdocklet.c \
	gtkeventloop.c \
	gtkicon-theme.c \
	gtkicon-theme-loader.c \
	gtkidle.c \
	gtkimhtml.c \
	gtkimhtmltoolbar.c \
	gtklog.c \
	gtkmedia.c \
	gtkmenutray.c \
	gtknotify.c \
	gtkplugin.c \
	gtkpluginpref.c \
	gtkpounce.c \
	gtkprefs.c \
	gtkprivacy.c \
	gtkrequest.c \
	gtkroomlist.c \
	gtksavedstatuses.c \
	gtkscrollbook.c \
	gtksession.c \
	gtksmiley.c \
	gtksound.c \
	gtksourceiter.c \
	gtksourceundomanager.c \
	gtksourceview-marshal.c \
	gtkstatus-icon-theme.c \
	gtkstatusbox.c \
	gtkthemes.c \
	gtkutils.c \
	gtkwebview.c \
	gtkwebviewtoolbar.c \
	gtkwhiteboard.c \
	gtkxfer.c \
	libpidgin.c \
	minidialog.c \
	pidgintooltip.c \
	smileyparser.c

libpidgin_la_headers = \
	gtkaccount.h \
	gtkblist.h \
	gtkblist-theme.h \
	gtkblist-theme-loader.h \
	gtkcellrendererexpander.h \
	gtkcertmgr.h \
	gtkconn.h \
	gtkconv.h \
	gtkconvwin.h \
	gtkconv-theme.h \
	gtkconv-theme-loader.h \
	gtkdebug.h \
	gtkdialogs.h \
	gtkdnd-hints.h \
	gtkdocklet.h \
	gtkeventloop.h \
	gtkicon-theme.h \
	gtkicon-theme-loader.h \
	gtkidle.h \
	gtkimhtml.h \
	gtkimhtmltoolbar.h \
	gtklog.h \
	gtkmedia.h \
	gtkmenutray.h \
	gtknickcolors.h \
	gtknotify.h \
	gtkplugin.h \
	gtkpluginpref.h \
	gtkprefs.h \
	gtkprivacy.h \
	gtkpounce.h \
	gtkrequest.h \
	gtkroomlist.h \
	gtksavedstatuses.h \
	gtkscrollbook.h \
	gtksession.h \
	gtksmiley.h \
	gtksound.h \
	gtksourceiter.h \
	gtksourceundomanager.h \
	gtksourceview-marshal.h \
	gtkstatus-icon-theme.h \
	gtkstatusbox.h \
	pidginstock.h \
	gtkthemes.h \
	gtkutils.h \
	gtkwebview.h \
	gtkwebviewtoolbar.h \
	gtkwhiteboard.h \
	gtkxfer.h \
	minidialog.h \
	pidgintooltip.h \
	pidgin.h \
	smileyparser.h

pidgin_SOURCES = \
	pidgin.c

libpidginincludedir=$(includedir)/pidgin
libpidgininclude_HEADERS = \
	$(libpidgin_la_headers)

libpidgin_la_builtheaders = gtkdebug.html.h

BUILT_SOURCES = $(libpidgin_la_builtheaders)

%.html.h: %.html
	$(AM_V_GEN)echo "static const char $*_html[] = {" > $@
	$(AM_V_at)$(sedpath) -e 's/^[ 	]\+//g' -e 's/[ 	]\+/ /g' $< | $(xxdpath) -i | sed -e 's/\(0x[0-9a-f][0-9a-f]\)$$/\1, 0x00/' >> $@
	$(AM_V_at)echo "};" >> $@

libpidgin_la_DEPENDENCIES = @LIBOBJS@
libpidgin_la_LDFLAGS = -export-dynamic
libpidgin_la_LIBADD = \
	@LIBOBJS@ \
	$(GLIB_LIBS) \
	$(GPLUGIN_LIBS) \
	$(GCR_LIBS) \
	$(DBUS_LIBS) \
	$(GSTREAMER_LIBS) \
	$(GSTVIDEO_LIBS) \
	$(GSTINTERFACES_LIBS) \
	$(XSS_LIBS) \
	$(SM_LIBS) \
	$(INTLLIBS) \
	$(GTKSPELL_LIBS) \
	$(LIBXML_LIBS) \
	$(WEBKIT_LIBS) \
	$(GTK_LIBS) \
	$(X11_LIBS) \
	$(INTROSPECTION_LIBS) \
	$(top_builddir)/libpurple/libpurple.la

pidgin_DEPENDENCIES = $(builddir)/libpidgin.la
pidgin_LDFLAGS = -export-dynamic
pidgin_LDADD = $(builddir)/libpidgin.la $(libpidgin_la_LIBADD)

AM_CPPFLAGS = \
	-DDATADIR=\"$(datadir)\" \
	-DLIBDIR=\"$(libdir)/pidgin/\" \
	-DLOCALEDIR=\"$(datadir)/locale\" \
	-DSYSCONFDIR=\"$(sysconfdir)\" \
	-I$(top_builddir)/libpurple \
	-I$(top_srcdir)/libpurple/ \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	$(GLIB_CFLAGS) \
	$(GPLUGIN_CFLAGS) \
	$(GCR_CFLAGS) \
	$(GSTREAMER_CFLAGS) \
	$(GSTVIDEO_CFLAGS) \
	$(GSTINTERFACES_CFLAGS) \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS) \
	$(X11_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(GTKSPELL_CFLAGS) \
	$(LIBXML_CFLAGS) \
	$(WEBKIT_CFLAGS) \
	$(INTROSPECTION_CFLAGS)

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(builddir) --add-include-path=$(top_builddir)/libpurple
INTROSPECTION_COMPILER_ARGS = --includedir=$(top_builddir)/libpurple

if HAVE_INTROSPECTION
introspection_sources = $(libpidgininclude_HEADERS)

Pidgin-$(PURPLE_MAJOR_VERSION).$(PURPLE_MINOR_VERSION).gir: $(builddir)/libpidgin.la
Pidgin_3_0_gir_INCLUDES = GObject-2.0 Gtk-$(GTK_VERSION).0 Purple-$(PURPLE_MAJOR_VERSION).$(PURPLE_MINOR_VERSION)
Pidgin_3_0_gir_CFLAGS = \
	$(INCLUDES) \
	-DDATADIR=\"$(datadir)\" \
	-DLIBDIR=\"$(libdir)/pidgin/\" \
	-DLOCALEDIR=\"$(datadir)/locale\" \
	-DSYSCONFDIR=\"$(sysconfdir)\" \
	-I$(top_builddir)/libpurple \
	-I$(top_srcdir)/libpurple/ \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	$(GLIB_CFLAGS) \
	$(GPLUGIN_CFLAGS) \
	$(GCR_CFLAGS) \
	$(GSTREAMER_CFLAGS) \
	$(GSTVIDEO_CFLAGS) \
	$(GSTINTERFACES_CFLAGS) \
	$(GTK_CFLAGS) \
	$(X11_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(GTKSPELL_CFLAGS) \
	$(LIBXML_CFLAGS) \
	$(WEBKIT_CFLAGS) \
	$(INTROSPECTION_CFLAGS)

Pidgin_3_0_gir_LIBS = $(builddir)/libpidgin.la
Pidgin_3_0_gir_FILES = $(introspection_sources)
INTROSPECTION_GIRS += Pidgin-$(PURPLE_MAJOR_VERSION).$(PURPLE_MINOR_VERSION).gir

girdir = $(INTROSPECTION_GIRDIR)
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(INTROSPECTION_TYPELIBDIR)
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES = $(gir_DATA) $(typelib_DATA)
endif

endif  # ENABLE_GTK
