EXTRA_DIST = \
		COPYRIGHT \
		ChangeLog.API \
		ChangeLog.win32 \
		Doxyfile.in \
		HACKING \
		Makefile.mingw \
		PLUGIN_HOWTO \
		README.MTN \
		README.mingw \
		config.h.mingw \
		doxy2devhelp.xsl \
		gaim.pc.in \
		gaim-uninstalled.pc.in \
		intltool-extract.in \
		intltool-merge.in \
		intltool-update.in \
		pidgin.apspec.in \
		pidgin.spec.in \
		pidgin.desktop.in \
		po/Makefile.mingw \
		valgrind-suppressions

noinst_HEADERS = config.h

dist-hook: pidgin.spec
	cp pidgin.spec $(distdir)
	rm $(distdir)/config.h

distcheck-hook: libpurple/plugins/perl/common/Purple.pm pidgin/plugins/perl/common/Pidgin.pm
#	cp libpurple/plugins/perl/common/Gaim.pm $(distdir)/libpurple/plugins/perl/common

commit-check:
	(cd po ; intltool-update -m 2>&1 | grep -v '^mismatched quotes.*\.py$$' | sed "s|current directory|po directory|" | grep . ; if [ $$? = 0 ] ; then exit 1 ; else exit 0 ; fi)
	LC_ALL=C sort -c -t/ -u po/POTFILES.in
	LC_ALL=C sort -c -t/ -u po/POTFILES.skip
	iconv -f utf8 -t utf8 COPYRIGHT | cmp COPYRIGHT -

version-check: commit-check
# We don't want to release development versions.
	test x`echo $(PACKAGE_VERSION) | grep dev` = x

# Make sure there is a NEWS entry for this version
	head NEWS | grep "^$(PACKAGE_VERSION) (`date +%m/%d/%Y`):$$" > /dev/null

# Ensure NEWS has no spaces at the start of a line.
# Using spaces instead of tabs there is a common mistake.
	test x`grep "^ " NEWS` = x

# When doing a new minor (or major) release (X.Y.0), there must be a section in
# ChangeLog.API.
	echo $(PACKAGE_VERSION) | grep -v "^[0-9]\+\.[0-9]\+\.0$$" >/dev/null || head ChangeLog.API | grep "^version $(PACKAGE_VERSION) (`date +%m/%d/%Y`):$$" >/dev/null

# For all releases, check the ChangeLogs.
	head ChangeLog | grep "^version $(PACKAGE_VERSION) (`date +%m/%d/%Y`):$$" >/dev/null
	head ChangeLog.win32 | grep "^version $(PACKAGE_VERSION) (`date +%m/%d/%Y`):$$" >/dev/null
	head po/ChangeLog | grep "^version $(PACKAGE_VERSION)$$" >/dev/null

# Ensure we're working from a tag...
	test x`hg log -r "tag($(PACKAGE_VERSION))" --template "{node}"` = x`hg id -i --debug`
# ... and have no changes in the working copy. (this isn't really necessary with hg because hg id appends a "+")
	test "x`hg st -mard`" = x

release: version-check distcheck packages

packages:
	gpg -ab pidgin-$(PACKAGE_VERSION).tar.gz
	gpg -ab pidgin-$(PACKAGE_VERSION).tar.bz2
	gpg --verify pidgin-$(PACKAGE_VERSION).tar.gz.asc pidgin-$(PACKAGE_VERSION).tar.gz
	gpg --verify pidgin-$(PACKAGE_VERSION).tar.bz2.asc pidgin-$(PACKAGE_VERSION).tar.bz2

if ENABLE_GTK
appsdir = $(datadir)/applications
apps_in_files = pidgin.desktop.in
apps_DATA = $(apps_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@
GTK_DIR=pidgin
endif

if ENABLE_GNT
GNT_DIR=finch
endif

if INSTALL_I18N
PO_DIR=po
endif

SUBDIRS = libpurple doc $(GNT_DIR) $(GTK_DIR) m4macros $(PO_DIR) share/ca-certs share/sounds

docs: Doxyfile
if HAVE_DOXYGEN
	@echo "Running doxygen..."
	@doxygen
if HAVE_XSLTPROC
	@echo "Generating devhelp index..."
	@xsltproc $(top_srcdir)/doxy2devhelp.xsl doc/xml/index.xml > doc/html/pidgin.devhelp
	@echo "(Symlink doc/html to ~/.local/share/gtk-doc/html/pidgin to make devhelp see the documentation)"
else
	@echo "Not generating devhelp index: xsltproc was not found by configure"
endif
else
	@echo "doxygen was not found during configure.  Aborting."
	@echo;
endif

# perl's MakeMaker uninstall foo doesn't work well with DESTDIR set, which
# breaks "make distcheck" unless we ignore perl things

distuninstallcheck_listfiles = \
	find . -type f -print | grep -v perl | grep -v Purple.3pm

DISTCLEANFILES= pidgin.desktop libpurple/gconf/purple.schemas intltool-extract \
			intltool-merge intltool-update
