EXTRA_DIST = \
		COPYRIGHT \
		Doxyfile.in \
		gaim.pc.in \
		gaim.spec.in \
		gaim.apspec.in \
		gaim.desktop \
		gaim-installer.nsi \
		HACKING \
		PROGRAMMING_NOTES \
		setup-gettext \
		ChangeLog.win32 \
		config.h.mingw \
		Makefile.mingw \
		README.mingw \
		VERSION \
		VERSION.in \
		plugins/win32/transparency/Makefile.mingw \
		plugins/win32/transparency/win2ktrans.c \
		plugins/win32/winprefs/gtkappbar.c \
		plugins/win32/winprefs/gtkappbar.h \
		plugins/win32/winprefs/Makefile.mingw \
		plugins/win32/winprefs/winprefs.c \
		po/Makefile.mingw

gaimincludedir=$(includedir)/gaim
gaiminclude_HEADERS = config.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gaim.pc

dist-hook: gaim.spec
	cp gaim.spec $(distdir)

distcheck-hook: plugins/perl/common/Gaim.pm
#	cp plugins/perl/common/Gaim.pm $(distdir)/plugins/perl/common

appsdir = $(datadir)/applications
apps_DATA = gaim.desktop

SUBDIRS = doc intl m4macros pixmaps plugins po sounds src

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

ACLOCAL_AMFLAGS = -I m4

