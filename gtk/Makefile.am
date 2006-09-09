if ENABLE_GTK

EXTRA_DIST = \
		getopt.c \
		getopt.h \
		getopt1.c \
		Makefile.mingw \
		win32/IdleTracker/Makefile.mingw \
		win32/IdleTracker/idletrack.c \
		win32/IdleTracker/idletrack.h \
		win32/gaimrc.rc.in \
		win32/gtkgaimrc.rc.in \
		win32/gtkwin32dep.c \
		win32/gtkwin32dep.h \
		win32/resource.h \
		win32/untar.c \
		win32/untar.h \
		win32/win_gaim.c \
		win32/wspell.c \
		win32/wspell.h \
		win32/nsis/gaim-header.bmp \
		win32/nsis/gaim-intro.bmp \
		win32/nsis/gaim-plugin.nsh \
		win32/nsis/langmacros.nsh \
		win32/nsis/translations/albanian.nsh \
		win32/nsis/translations/bulgarian.nsh \
		win32/nsis/translations/catalan.nsh \
		win32/nsis/translations/czech.nsh \
		win32/nsis/translations/danish.nsh \
		win32/nsis/translations/dutch.nsh \
		win32/nsis/translations/english.nsh \
		win32/nsis/translations/finnish.nsh \
		win32/nsis/translations/french.nsh \
		win32/nsis/translations/german.nsh \
		win32/nsis/translations/hebrew.nsh \
		win32/nsis/translations/hungarian.nsh \
		win32/nsis/translations/italian.nsh \
		win32/nsis/translations/japanese.nsh \
		win32/nsis/translations/korean.nsh \
		win32/nsis/translations/kurdish.nsh \
		win32/nsis/translations/lithuanian.nsh \
		win32/nsis/translations/norwegian.nsh \
		win32/nsis/translations/polish.nsh \
		win32/nsis/translations/portuguese.nsh \
		win32/nsis/translations/portuguese-br.nsh \
		win32/nsis/translations/romanian.nsh \
		win32/nsis/translations/russian.nsh \
		win32/nsis/translations/serbian-latin.nsh \
		win32/nsis/translations/simp-chinese.nsh \
		win32/nsis/translations/slovak.nsh \
		win32/nsis/translations/slovenian.nsh \
		win32/nsis/translations/spanish.nsh \
		win32/nsis/translations/swedish.nsh \
		win32/nsis/translations/trad-chinese.nsh \
		win32/nsis/translations/vietnamese.nsh

SUBDIRS = pixmaps plugins sounds

bin_PROGRAMS = gaim

gaim_SOURCES = \
	gaimcombobox.c \
	gaimstock.c \
	gtkaccount.c \
	gtkblist.c \
	gtkcelllayout.c \
	gtkcellrendererprogress.c \
	gtkcellview.c \
	gtkcellviewmenuitem.c \
	gtkconn.c \
	gtkconv.c \
	gtkdebug.c \
	gtkdialogs.c \
	gtkdnd-hints.c \
	gtkeventloop.c \
	gtkexpander.c \
	gtkft.c \
	gtkidle.c \
	gtkimhtml.c \
	gtkimhtmltoolbar.c \
	gtklog.c \
	gtkmain.c \
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
	gtksession.c \
	gtksound.c \
	gtksourceiter.c \
	gtkstatusbox.c \
	gtkthemes.c \
	gtkutils.c \
	gtkwhiteboard.c

gaim_headers = \
	gtkaccount.h \
	gtkblist.h \
	gtkcelllayout.h \
	gtkcellrendererprogress.h \
	gtkcellview.h \
	gtkcellviewmenuitem.h \
	gtkcellview.h \
	gtkcellviewmenuitem.h \
	gaimcombobox.h \
	gtkconn.h \
	gtkconv.h \
	gtkconvwin.h \
	gtkdebug.h \
	gtkdialogs.h \
	gtkdnd-hints.h \
	gtkeventloop.h \
	gtkexpander.h \
	gtkft.h \
	gtkgaim.h \
	gtkidle.h \
	gtkimhtml.h \
	gtkimhtmltoolbar.h \
	gtklog.h \
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
	gtksession.h \
	gtksound.h \
	gtksourceiter.h \
	gtkstatusbox.h \
	gaimstock.h \
	gtkthemes.h \
	gtkutils.h \
	gtkwhiteboard.h

gaimincludedir=$(includedir)/gaim
gaiminclude_HEADERS = \
	$(gaim_headers)


gaim_DEPENDENCIES = @LIBOBJS@ $(STATIC_LINK_LIBS)
gaim_LDFLAGS = -export-dynamic
gaim_LDADD = \
	@LIBOBJS@ \
	$(DBUS_LIBS) \
	$(GSTREAMER_LIBS) \
	$(STATIC_LINK_LIBS) \
	$(XSS_LIBS) \
	$(SM_LIBS) \
	$(INTLLIBS) \
	$(GTKSPELL_LIBS) \
	$(STARTUP_NOTIFICATION_LIBS) \
	$(LIBXML_LIBS) \
	$(GTK_LIBS) \
	$(CAP_LIBS) \
	$(top_builddir)/libgaim/libgaim.la

AM_CPPFLAGS = \
	-DBR_PTHREADS=0 \
	-DDATADIR=\"$(datadir)\" \
	-DLIBDIR=\"$(libdir)/gaim/\" \
	-DLOCALEDIR=\"$(datadir)/locale\" \
	-DSYSCONFDIR=\"$(sysconfdir)\" \
	-I$(top_srcdir)/libgaim/ \
	$(GLIB_CFLAGS) \
	$(GSTREAMER_CFLAGS) \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(GTKSPELL_CFLAGS) \
	$(STARTUP_NOTIFICATION_CFLAGS) \
	$(LIBXML_CFLAGS)
endif  # ENABLE_GTK
