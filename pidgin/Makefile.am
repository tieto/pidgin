EXTRA_DIST = \
		getopt.c \
		getopt.h \
		getopt1.c \
		Makefile.mingw \
		pidgin-3.pc.in \
		pidgin-3-uninstalled.pc.in \
		win32/MinimizeToTray.h \
		win32/MinimizeToTray.c \
		win32/pidgin_dll_rc.rc.in \
		win32/pidgin_exe_rc.rc.in \
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
		win32/nsis/rpm2zip.sh \
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

bin_PROGRAMS = pidgin

pidgin_SOURCES = \
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
	gtkft.c \
	gtkicon-theme.c \
	gtkicon-theme-loader.c \
	gtkidle.c \
	gtkimhtml.c \
	gtkimhtmltoolbar.c \
	gtklog.c \
	gtkmain.c \
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
	gtkwhiteboard.c \
	minidialog.c \
	pidgintooltip.c \
	smileyparser.c

pidgin_headers = \
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
	gtkft.h \
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
	gtkwhiteboard.h \
	minidialog.h \
	pidgintooltip.h \
	pidgin.h \
	smileyparser.h

pidginincludedir=$(includedir)/pidgin
pidgininclude_HEADERS = \
	$(pidgin_headers)


pidgin_DEPENDENCIES = @LIBOBJS@
pidgin_LDFLAGS = -export-dynamic
pidgin_LDADD = \
	@LIBOBJS@ \
	$(GLIB_LIBS) \
	$(GCR_LIBS) \
	$(DBUS_LIBS) \
	$(GSTREAMER_LIBS) \
	$(XSS_LIBS) \
	$(SM_LIBS) \
	$(INTLLIBS) \
	$(GTKSPELL_LIBS) \
	$(LIBXML_LIBS) \
	$(WEBKIT_LIBS) \
	$(GTK_LIBS) \
	$(top_builddir)/libpurple/libpurple.la

if USE_INTERNAL_LIBGADU
INTGG_CFLAGS = -DUSE_INTERNAL_LIBGADU
endif

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
	$(GCR_CFLAGS) \
	$(GSTREAMER_CFLAGS) \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(GTKSPELL_CFLAGS) \
	$(LIBXML_CFLAGS) \
	$(WEBKIT_CFLAGS) \
	$(INTGG_CFLAGS)
endif  # ENABLE_GTK

