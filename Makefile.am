EXTRA_DIST = \
		COPYRIGHT \
		Doxyfile.in \
		gaim.pc.in \
		gaim.spec.in \
		gaim.apspec.in \
		gaim.desktop.in \
		gaim.service.in \
		gaim-installer.nsi \
		HACKING \
		intltool-extract.in \
		intltool-merge.in \
		intltool-update.in \
		PROGRAMMING_NOTES \
		ChangeLog.win32 \
		config.h.mingw \
		Makefile.mingw \
		README.mingw \
		VERSION.in \
		gtk/plugins/win32/transparency/Makefile.mingw \
		gtk/plugins/win32/transparency/win2ktrans.c \
		gtk/plugins/win32/winprefs/gtkappbar.c \
		gtk/plugins/win32/winprefs/gtkappbar.h \
		gtk/plugins/win32/winprefs/Makefile.mingw \
		gtk/plugins/win32/winprefs/winprefs.c \
		po/Makefile.mingw

gaimincludedir=$(includedir)/gaim
gaiminclude_HEADERS = config.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gaim.pc

if ENABLE_DBUS
dbus_servicedir=$(DBUS_SERVICES_DIR)
dbus_service_DATA=gaim.service
endif

dist-hook: gaim.spec
	cp gaim.spec $(distdir)

distcheck-hook: plugins/perl/common/Gaim.pm
#	cp plugins/perl/common/Gaim.pm $(distdir)/plugins/perl/common

appsdir = $(datadir)/applications
apps_in_files = gaim.desktop.in
apps_DATA = $(apps_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

if ENABLE_GTK
GTK_DIR=gtk
endif

if ENABLE_GNT
GNT_DIR=console
endif

SUBDIRS = libgaim doc $(GNT_DIR) $(GTK_DIR) m4macros po

docs: Doxyfile
if HAVE_DOXYGEN
	@echo "Running doxygen..."
	@doxygen
else
	@echo "doxygen was not found during configure.  Aborting."
	@echo;
endif

# perl's MakeMaker uninstall foo doesn't work well with DESTDIR set, which
# breaks "make distcheck" unless we ignore perl things

distuninstallcheck_listfiles = \
	find . -type f -print | grep -v perl | grep -v Gaim.3pm

