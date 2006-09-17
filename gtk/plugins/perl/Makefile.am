perl_dirs = common

common_sources = \
	common/GtkAccount.xs \
	common/GtkBlist.xs \
	common/GtkConn.xs \
	common/GtkConv.xs \
	common/GtkConvWin.xs \
	common/GtkDebug.xs \
	common/GtkDialogs.xs \
	common/GtkFt.xs \
	common/GtkIMHtml.xs \
	common/GtkIMHtmlToolbar.xs \
	common/GtkLog.xs \
	common/GtkMenuTray.xs \
	common/GtkPlugin.xs \
	common/GtkPluginPref.xs \
	common/GtkPounce.xs \
	common/GtkPrefs.xs \
	common/GtkPrivacy.xs \
	common/GtkRoomlist.xs \
	common/GtkSavedStatuses.xs \
	common/GtkSession.xs \
	common/GtkSound.xs \
	common/GtkStatusBox.xs \
	common/GtkThemes.xs \
	common/GtkUI.pm \
	common/GtkUI.xs \
	common/GtkUtils.xs \
	common/Makefile.PL.in \
	common/gtkmodule.h \
	common/typemap

EXTRA_DIST = \
	Makefile.mingw \
	common/Makefile.mingw \
	$(common_sources)

common/Makefile: common/Makefile.PL
	@cd common && $(perlpath) Makefile.PL $(PERL_MM_PARAMS)

common/Makefile.PL: common/Makefile.PL.in $(top_builddir)/config.status
	cd $(top_builddir) && $(SHELL) ./config.status $(subdir)/$@ $(am__depfiles_maybe)

all-local: common/Makefile
	@for dir in $(perl_dirs); do \
		cd $$dir && \
		if [ ! -f Makefile ]; then \
			$(perlpath) Makefile.PL $(PERL_MM_PARAMS); \
		fi && \
		($(MAKE) CC="$(CC)" CCFLAGS="$(PERL_CFLAGS) $(CFLAGS)" $(PERL_EXTRA_OPTS) || \
		$(MAKE) CC="$(CC)" CCFLAGS="$(PERL_CFLAGS) $(CFLAGS)" $(PERL_EXTRA_OPTS)) && \
		cd ..; \
	done

install-exec-local:
	@for dir in $(perl_dirs); do \
		cd $$dir; \
		$(MAKE) install; \
		cd ..; \
	done

# Evil Hack (TM)
# ... which doesn't work with DESTDIR installs.  FIXME?
uninstall-local:
	@for dir in $(perl_dirs); do \
		cd $$dir && \
		`$(MAKE) uninstall | grep unlink | sed -e 's#/usr#${prefix}#' -e 's#unlink#rm -f#'` && \
		cd ..; \
	done

clean-generic:
	@for dir in $(perl_dirs); do \
		cd $$dir; \
		$(MAKE) clean; \
		cd ..; \
	done
	rm -f *.so

distclean-generic:
	@for dir in $(perl_dirs); do \
		cd $$dir; \
		$(MAKE) realclean; \
		rm -f Makefile.PL; \
		rm -f Makefile.old; \
		rm -f Makefile; \
		cd ..; \
	done

	@rm -f Makefile

AM_CPPFLAGS = \
	-DVERSION=\"$(VERSION)\" \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgaim \
	-I$(top_srcdir)/gtk \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS) \
	$(PLUGIN_CFLAGS) \
	$(PERL_CFLAGS)
