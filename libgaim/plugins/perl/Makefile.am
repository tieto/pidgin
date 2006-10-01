plugindir = $(libdir)/gaim
hackdir = $(plugindir)/private

perl_dirs = common

plugin_LTLIBRARIES = perl.la
hack_LTLIBRARIES = libgaimperl.la

perl_la_LDFLAGS = -module -avoid-version
perl_la_LIBADD = $(GLIB_LIBS) $(PERL_LIBS) libgaimperl.la
perl_la_SOURCES = \
	perl.c \
	perl-common.c \
	perl-common.h \
	perl-handlers.c \
	perl-handlers.h

perl_la_DEPENDENCIES = \
	.libs/libperl_orig.a \
	.libs/DynaLoader.a \
	libgaimperl.la

libgaimperl_la_LDFLAGS = -module -avoid-version
libgaimperl_la_LIBADD = $(GLIB_LIBS)
libgaimperl_la_SOURCES = libgaimperl.c

.libs/libperl_orig.a:
	@mkdir -p .libs
	@rm -f .libs/libperl_orig.a
	@if [ x$(LIBPERL_A) = x ]; then \
		touch .libs/libperl_orig.a; \
	else \
		$(LN_S) $(LIBPERL_A) .libs/libperl_orig.a; \
	fi

.libs/DynaLoader.a:
	@mkdir -p .libs
	@rm -f .libs/DynaLoader.a
	@if [ x$(DYNALOADER_A) = x ]; then \
		touch .libs/DynaLoader.a; \
	else \
		$(LN_S) $(DYNALOADER_A) .libs/DynaLoader.a; \
	fi


common_sources = \
        common/Account.xs \
        common/AccountOpts.xs \
        common/BuddyIcon.xs \
        common/BuddyList.xs \
        common/Cipher.xs \
        common/Cmds.xs \
        common/Connection.xs \
        common/Conversation.xs \
        common/Debug.xs \
        common/FT.xs \
        common/Gaim.pm \
        common/Gaim.xs \
        common/ImgStore.xs \
        common/Log.xs \
        common/Makefile.PL.in \
        common/Network.xs \
        common/Notify.xs \
        common/Plugin.xs \
        common/PluginPref.xs \
        common/Pounce.xs \
        common/Prefs.xs \
        common/Privacy.xs \
        common/Proxy.xs \
        common/Prpl.xs \
        common/Request.xs \
        common/Roomlist.xs \
        common/SSLConn.xs \
        common/SavedStatuses.xs \
        common/Server.xs \
        common/Signal.xs \
        common/Sound.xs \
        common/Status.xs \
        common/Stringref.xs \
        common/Util.xs \
        common/XMLNode.xs \
        common/fallback/const-c.inc \
        common/fallback/const-xs.inc \
        common/module.h \
        common/typemap


EXTRA_DIST = \
	Makefile.mingw \
	common/Makefile.mingw \
	$(common_sources) \
	libgaimperl.c

common/Makefile: common/Makefile.PL
	@if test "x${top_srcdir}" != "x${top_builddir}"; then \
		for f in ${common_sources}; do \
			${LN_S} -f ../${srcdir}/$$f $$f; \
		done; \
	fi
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
# ... which doesn't work with DESTDIR installs. FIXME?
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
	@rm -f common/const-c.inc common/const-xs.inc

	@if test "x${top_srcdir}" != "x${top_builddir}"; then \
		for f in ${common_sources}; do \
			${LN_S} -f ../${srcdir}/$$f $$f; \
		done; \
	fi


AM_CPPFLAGS = \
	-DVERSION=\"$(VERSION)\" \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgaim \
	$(DEBUG_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(PLUGIN_CFLAGS) \
	$(PERL_CFLAGS)
