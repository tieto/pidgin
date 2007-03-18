EXTRA_DIST = \
		COPYRIGHT \
		ChangeLog.API \
		ChangeLog.win32 \
		Doxyfile.in \
		HACKING \
		Makefile.mingw \
		PLUGIN_HOWTO \
		PROGRAMMING_NOTES \
		README.SVN \
		README.dbus \
		README.mingw \
		gaim.pc.in \
		gaim.spec.in \
		gaim.apspec.in \
		pidgin.desktop.in \
		gaim.service.in \
		intltool-extract.in \
		intltool-merge.in \
		intltool-update.in \
		config.h.mingw \
		po/Makefile.mingw

noinst_HEADERS = config.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gaim.pc

if ENABLE_DBUS
dbus_servicedir=$(DBUS_SERVICES_DIR)
dbus_service_DATA=gaim.service
endif

dist-hook: gaim.spec
	cp gaim.spec $(distdir)
	rm $(distdir)/config.h

distcheck-hook: libpurple/plugins/perl/common/Gaim.pm gtk/plugins/perl/common/GtkUI.pm
#	cp libpurple/plugins/perl/common/Gaim.pm $(distdir)/libpurple/plugins/perl/common

appsdir = $(datadir)/applications
apps_in_files = pidgin.desktop.in
apps_DATA = $(apps_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

if ENABLE_GTK
GTK_DIR=pidgin
endif

if ENABLE_GNT
GNT_DIR=finch
endif

SUBDIRS = libpurple doc $(GNT_DIR) $(GTK_DIR) m4macros po

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

DISTCLEANFILES= pidgin.desktop libpurple/gconf/gaim.schemas intltool-extract \
			intltool-merge intltool-update

ACLOCAL_AMFLAGS = -I m4
