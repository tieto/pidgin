EXTRA_DIST = \
		COPYRIGHT \
		Doxyfile.in \
		gaim.spec.in \
		gaim.desktop \
		gaim2blt.pl \
		gaim-installer.nsi \
		HACKING \
		PROGRAMMING_NOTES \
		licq2gaim.pl \
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

dist-hook: gaim.spec
	cp gaim.spec $(distdir)

distcheck-hook: plugins/perl/common/Gaim.pm
#	cp plugins/perl/common/Gaim.pm $(distdir)/plugins/perl/common

appsdir = $(datadir)/applications
apps_DATA = gaim.desktop

SUBDIRS = doc intl pixmaps plugins po sounds src

docs: Doxyfile
	@doxygen
