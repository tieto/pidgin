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
	gtkicon-theme.c \
	gtkicon-theme-loader.c \
	gtkidle.c \
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
	gtksourceview-marshal.c \
	gtkstatus-icon-theme.c \
	gtkstatusbox.c \
	gtkthemes.c \
	gtkutils.c \
	gtkwebview.c \
	gtkwebviewtoolbar.c \
	gtkwhiteboard.c \
	gtkxfer.c \
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
	gtkicon-theme.h \
	gtkicon-theme-loader.h \
	gtkidle.h \
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

pidginincludedir=$(includedir)/pidgin
pidgininclude_HEADERS = \
	$(pidgin_headers)

pidgin_builtheaders = gtkdebug.html.h

BUILT_SOURCES = $(pidgin_builtheaders)

%.html.h: %.html
	$(AM_V_GEN)echo "static const char $*_html[] = {" > $@
	$(AM_V_at)$(sedpath) -e 's/^[ 	]\+//g' -e 's/[ 	]\+/ /g' $< | $(xxdpath) -i | sed -e 's/\(0x[0-9a-f][0-9a-f]\)$$/\1, 0x00/' >> $@
	$(AM_V_at)echo "};" >> $@

pidgin_DEPENDENCIES = @LIBOBJS@
pidgin_LDFLAGS = -export-dynamic
pidgin_LDADD = \
	@LIBOBJS@ \
	$(GLIB_LIBS) \
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
	$(top_builddir)/libpurple/libpurple.la

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
	$(GSTVIDEO_CFLAGS) \
	$(GSTINTERFACES_CFLAGS) \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS) \
	$(X11_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(GTKSPELL_CFLAGS) \
	$(LIBXML_CFLAGS) \
	$(WEBKIT_CFLAGS)
endif  # ENABLE_GTK

