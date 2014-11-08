EXTRA_DIST = \
		getopt.c \
		getopt.h \
		getopt1.c \
		gtk3compat.h \
		gtkdebug.html \
		Makefile.mingw \
		data/pidgin.desktop.in \
		data/pidgin-3.pc.in \
		data/pidgin-3-uninstalled.pc.in \
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

SUBDIRS = . pixmaps plugins themes

# XXX: should this be lib_, or noinst_?
lib_LTLIBRARIES = libpidgin.la
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
	gtksmiley-manager.c \
	gtksmiley-theme.c \
	gtksound.c \
	gtkstatus-icon-theme.c \
	gtkstatusbox.c \
	gtkutils.c \
	gtkwebview.c \
	gtkwebviewtoolbar.c \
	gtkwhiteboard.c \
	gtkxfer.c \
	libpidgin.c \
	minidialog.c \
	pidgintooltip.c

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
	gtksmiley-manager.h \
	gtksmiley-theme.h \
	gtksound.h \
	gtkstatus-icon-theme.h \
	gtkstatusbox.h \
	pidginstock.h \
	gtkutils.h \
	gtkwebview.h \
	gtkwebviewtoolbar.h \
	gtkwhiteboard.h \
	gtkxfer.h \
	minidialog.h \
	pidgintooltip.h \
	pidgin.h

pidgin_SOURCES = \
	pidgin.c

noinst_HEADERS= gtkinternal.h

if IS_WIN32
libpidgin_la_SOURCES += \
	win32/gtkwin32dep.c \
	win32/untar.c

# Files that looks like obsolete (were used in Pidgin2):
#	win32/gtkdocklet-win32.c
#	win32/MinimizeToTray.c
#	win32/MinimizeToTray.h

pidgin_SOURCES += \
	win32/winpidgin.c \
	win32/pidgin_exe_rc.rc

libpidgin_la_headers += \
	win32/gtkwin32dep.h \
	win32/resource.h \
	win32/untar.h

.rc.o: %.rc
	$(AM_V_GEN)$(WINDRES) -I $(srcdir)/win32 -I $(srcdir) -i $< -o $@

LIBPIDGIN_WIN32RES = win32/pidgin_dll_rc.o
LIBPIDGIN_WIN32RES_LDFLAGS = -Wl,$(LIBPIDGIN_WIN32RES)
endif

libpidginincludedir=$(includedir)/pidgin
libpidgininclude_HEADERS = \
	$(libpidgin_la_headers)

libpidgin_la_builtheaders = gtkdebug.html.h

BUILT_SOURCES = $(libpidgin_la_builtheaders)

CLEANFILES = gtkdebug.html.h

%.html.h: %.html
	$(AM_V_GEN)echo "static const char $*_html[] = {" > $@
	$(AM_V_at)$(sedpath) -e 's/^[ 	]\+//g' -e 's/[ 	]\+/ /g' $< | $(xxdpath) -i | sed -e 's/\(0x[0-9a-f][0-9a-f]\)$$/\1, 0x00/' >> $@
	$(AM_V_at)echo "};" >> $@

gtkdebug.c: gtkdebug.html.h

libpidgin_la_DEPENDENCIES = @LIBOBJS@ $(LIBPIDGIN_WIN32RES)
libpidgin_la_LDFLAGS = -export-dynamic -no-undefined \
	-version-info $(PURPLE_LT_VERSION_INFO) $(LIBPIDGIN_WIN32RES_LDFLAGS)
libpidgin_la_LIBADD = \
	@LIBOBJS@ \
	$(ENCHANT_LIBS) \
	$(GLIB_LIBS) \
	$(GCR_LIBS) \
	$(DBUS_LIBS) \
	$(GSTREAMER_LIBS) \
	$(GSTVIDEO_LIBS) \
	$(GSTINTERFACES_LIBS) \
	$(XSS_LIBS) \
	$(SM_LIBS) \
	$(INTLLIBS) \
	$(LIBXML_LIBS) \
	$(WEBKIT_LIBS) \
	$(GTK_LIBS) \
	$(X11_LIBS) \
	$(top_builddir)/libpurple/libpurple.la

pidgin_DEPENDENCIES = $(builddir)/libpidgin.la
pidgin_LDFLAGS = -export-dynamic
pidgin_LDADD = $(builddir)/libpidgin.la $(libpidgin_la_LIBADD)

if IS_WIN32
libpidgin_la_LIBADD += -lwinmm
pidgin_LDFLAGS += -mwindows
endif

AM_CPPFLAGS = \
	-I$(top_builddir)/libpurple \
	-I$(top_srcdir)/libpurple/ \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	$(ENCHANT_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GCR_CFLAGS) \
	$(GSTREAMER_CFLAGS) \
	$(GSTVIDEO_CFLAGS) \
	$(GSTINTERFACES_CFLAGS) \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS) \
	$(X11_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(LIBXML_CFLAGS) \
	$(WEBKIT_CFLAGS)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = data/pidgin-3.pc

if INSTALL_I18N
DESKTOP_FILE=data/pidgin.desktop

appsdir = $(datadir)/applications
apps_in_files = data/pidgin.desktop.in
apps_DATA = $(apps_in_files:.desktop.in=.desktop)

# silenced INTLTOOL_DESKTOP_RULE
%.desktop: %.desktop.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*.po)
	$(AM_V_GEN) LC_ALL=C $(INTLTOOL_MERGE) -d -u -c $(top_builddir)/po/.intltool-merge-cache $(top_srcdir)/po $< $@ > /dev/null

endif # INSTALL_I18N

DISTCLEANFILES = $(DESKTOP_FILE)

endif  # ENABLE_GTK

