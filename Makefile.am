EXTRA_DIST = \
		Doxyfile.in \
		gaim.spec.in \
		gaim.desktop \
		gaim2blt.pl \
		HACKING \
		PROGRAMMING_NOTES \
		licq2gaim.pl \
		setup-gettext \
		ChangeLog.win32 \
		config.h.mingw \
		Makefile.mingw \
		README.mingw \
		VERSION \
		plugins/Makefile.mingw \
		plugins/win32/transparency/Makefile.mingw \
		plugins/win32/transparency/win2ktrans.c \
		plugins/win32/winprefs/Makefile.mingw \
		plugins/win32/winprefs/winprefs.c \
		po/Makefile.mingw

dist-hook: gaim.spec
	cp gaim.spec $(distdir)

appsdir = $(datadir)/applications
apps_DATA = gaim.desktop

SUBDIRS = sounds plugins pixmaps doc po intl src

docs: Doxyfile.in
	@doxygen
