EXTRA_DIST = \
		COPYRIGHT \
		ChangeLog.API \
		ChangeLog.win32 \
		HACKING \
		Makefile.mingw \
		PLUGIN_HOWTO \
		README.hg \
		README.mingw \
		config.h.mingw \
		fix-casts.sh \
		intltool-extract.in \
		intltool-merge.in \
		intltool-update.in \
		package_revision.h \
		pidgin.apspec.in \
		pidgin.desktop.in \
		po/Makefile.mingw \
		valgrind-suppressions

noinst_HEADERS = config.h package_revision.h

dist-pre-hook: package_revision.h
if !ENABLE_GTK_DOC
	$(AM_V_at)echo "error: gtk-doc must be installed and enabled in order to make dist"
	$(AM_V_at)echo "re-run ./configure with --enable-gtk-doc switch and try again"
	$(AM_V_at)false
endif
	$(AM_V_at)if [ ! -f "$(top_builddir)libpurple/libpurple.la" ]; then \
		$(MAKE) -C libpurple libpurple.la; \
	fi
if ENABLE_GNT
	$(AM_V_at)if [ ! -f "$(top_builddir)finch/libgnt/libgnt.la" ]; then \
		$(MAKE) -C finch/libgnt libgnt.la; \
	fi
	$(AM_V_at)if [ ! -f "$(top_builddir)finch/libfinch.la" ]; then \
		$(MAKE) -C finch libfinch.la; \
	fi
endif
if ENABLE_GTK
	$(AM_V_at)if [ ! -f "$(top_builddir)pidgin/libpidgin.la" ]; then \
		$(MAKE) -C pidgin libpidgin.la; \
	fi
endif

dist: dist-pre-hook

dist-hook:
	rm $(distdir)/config.h

distcheck-hook: libpurple/plugins/perl/common/Purple.pm pidgin/plugins/perl/common/Pidgin.pm
#	cp libpurple/plugins/perl/common/Gaim.pm $(distdir)/libpurple/plugins/perl/common

commit-check:
	(cd po ; intltool-update -m 2>&1 | grep -v '^mismatched quotes.*\.py$$' | sed "s|current directory|po directory|" | grep . ; if [ $$? = 0 ] ; then exit 1 ; else exit 0 ; fi)

# Ensure these files are sorted and contain no duplicates:
	LC_ALL=C sort -c -t/ -u po/POTFILES.in
	LC_ALL=C sort -c -t/ -u po/POTFILES.skip

# Ensure COPYRIGHT is 100% UTF-8
	iconv -f utf8 -t utf8 COPYRIGHT | cmp COPYRIGHT -

version-check:
# Ensure our version string does not contain "dev"
	test x`echo $(PACKAGE_VERSION) | grep dev` = x

# Ensure ChangeLogs have the correct version
	head ChangeLog     | grep "^version $(PACKAGE_VERSION) (.*):$$" >/dev/null
	head ChangeLog.API | grep "^version $(PACKAGE_VERSION):$$" >/dev/null

# Ensure we're working from a tag...
	test x`hg log -r "tag($(PACKAGE_VERSION))" --template "{node}"` = x`hg id -i --debug`
# ... and have no changes in the working copy. (this isn't really necessary with hg because hg id appends a "+")
	test "x`hg st -mard`" = x

sign-packages: dist
	gpg -ab pidgin-$(PACKAGE_VERSION).tar.gz
	gpg -ab pidgin-$(PACKAGE_VERSION).tar.bz2
	gpg --verify pidgin-$(PACKAGE_VERSION).tar.gz.asc pidgin-$(PACKAGE_VERSION).tar.gz
	gpg --verify pidgin-$(PACKAGE_VERSION).tar.bz2.asc pidgin-$(PACKAGE_VERSION).tar.bz2

release: commit-check version-check distcheck sign-packages

if INSTALL_I18N
PO_DIR=po
DESKTOP_FILE=pidgin.desktop

if ENABLE_GTK
appsdir = $(datadir)/applications
apps_in_files = pidgin.desktop.in
apps_DATA = $(apps_in_files:.desktop.in=.desktop)

# silenced INTLTOOL_DESKTOP_RULE
%.desktop: %.desktop.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*.po)
	$(AM_V_GEN) LC_ALL=C $(INTLTOOL_MERGE) -d -u -c $(top_builddir)/po/.intltool-merge-cache $(top_srcdir)/po $< $@ > /dev/null

endif #ENABLE_GTK

endif #INSTALL_I18N

if ENABLE_GTK
GTK_DIR=pidgin
endif

if ENABLE_GNT
GNT_DIR=finch
endif

# We always try to rebuild all objects that depends on this dummy target.
.FORCE:

# if both attempts fail, then we need to remove the empty file that >
# creates, and also make sure that the shell command exits
# successfully; the rm -f ensures both
package_revision_raw.txt: .FORCE
	$(AM_V_GEN)REAL_BLDDIR=$$PWD/$(top_builddir); \
	(hg --cwd $(srcdir) id -i --debug) 2>/dev/null >$@.new \
	|| rm -f $@.new
	$(AM_V_at)if test -f $@.new; then \
		if ! diff $@ $@.new > /dev/null; then \
			mv $@.new $@; \
		else \
			rm $@.new; \
		fi \
	fi
package_revision.h: package_revision_raw.txt
	$(AM_V_GEN)if test -f $<; then \
	  echo "#define REVISION \"`cat $<`\"" > $@; \
	fi
	$(AM_V_at)if test ! -f $@ -a -f $(srcdir)/$@; then \
	  cp $(srcdir)/$@ $@; \
	fi
	$(AM_V_at)test -f $@ || echo "#define REVISION \"unknown\"" > $@

# This is a magic directive copy-and-pasted, then modified, from the
# automake 1.9 manual, section 13.4, "Checking the distribution".
# Normally, 'distcheck' does a clean build, and then afterwards runs
# 'distclean', and 'distclean' is supposed to remove everything that
# the build created.  However, we have some targets (package_revision.txt)
# that we distribute, but then always attempt to rebuild optimistically, and
# then if that fails fall back on the distributed versions.  This
# means that 'distclean' should _not_ remove those files, since they
# are distributed, yet building the package will generate those files,
# thus automake thinks that 'distclean' _should_ remove those files,
# and 'distcheck' gets cranky if we don't.  So basically what this
# line does is tell 'distcheck' to shut up and ignore those two files.
distcleancheck_listfiles = find . -type f -a ! -name package_revision.h

AM_DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc

SUBDIRS = . m4macros libpurple $(GNT_DIR) $(GTK_DIR) $(PO_DIR) share/ca-certs share/sounds doc

# perl's MakeMaker uninstall foo doesn't work well with DESTDIR set, which
# breaks "make distcheck" unless we ignore perl things
# TODO drop it when we drop perl wrapper

distuninstallcheck_listfiles = \
	find . -type f -print | grep -v perl | grep -v Purple.3pm | grep -v Pidgin.3pm

DISTCLEANFILES= $(DESKTOP_FILE) libpurple/gconf/purple.schemas intltool-extract \
			intltool-merge intltool-update package_revision_raw.txt
