plugindir = $(libdir)/gaim

perl_dirs = common

plugin_LTLIBRARIES = perl.la

perl_la_LDFLAGS = -module -avoid-version $(PERL_LIBS)
perl_la_LIBADD = $(PERL_LIBS)
perl_la_SOURCES = \
	perl.c \
	perl-common.c \
	perl-common.h \
	perl-handlers.c \
	perl-handlers.h

perl_la_DEPENDENCIES = \
	.libs/libperl_orig.a \
	.libs/DynaLoader.a

.libs/libperl_orig.a:
	@if [ ! -d .libs ]; then mkdir .libs; fi
	@rm -f .libs/libperl_orig.a
	@if [ x$(LIBPERL_A) = x ]; then \
		touch .libs/libperl_orig.a; \
	else \
		$(LN_S) $(LIBPERL_A) .libs/libperl_orig.a; \
	fi

.libs/DynaLoader.a:
	@if [ ! -d .libs ]; then mkdir .libs; fi
	@rm -f .libs/DynaLoader.a
	@if [ x$(DYNALOADER_H) = x ]; then \
		touch .libs/DynaLoader.a; \
	else \
		$(LN_S) $(DYNALOADER_A) .libs/DynaLoader.a; \
	fi

common_sources = \
	common/Accounts.xs \
	common/Gaim.xs \
	common/Gaim.pm \
	common/Makefile.PL.in \
	common/typemap \
	common/module.h

EXTRA_DIST = \
	Makefile.mingw \
	$(common_sources)

all-local:
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

clean-generic:
	@for dir in $(perl_dirs); do \
		cd $$dir; \
		$(MAKE) clean; \
		cd ..; \
	done

distclean-generic:
	@for dir in $(perl_dirs); do \
		cd $$dir; \
		$(MAKE) realclean; \
		rm -f Makefile.PL; \
		cd ..; \
	done

AM_CPPFLAGS = \
	-DVERSION=\"$(VERSION)\" \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS) \
	$(PLUGIN_CFLAGS) \
	$(PERL_CFLAGS)
