SUBDIRS = docklet ticker

CFLAGS += -I\$(top_srcdir) -I\$(top_srcdir)/src -DVERSION=\"$(VERSION)\"
SUFFIXES = .c .so
.c.so:
	$(LIBTOOL) --mode=compile $(CC) $(CFLAGS) -c $< -o tmp$@.lo $(PLUGIN_CFLAGS)
	$(LIBTOOL) --mode=link    $(CC) $(CFLAGS) -o libtmp$@.la -rpath $(plugindir) tmp$@.lo $(LIBS) $(LDFLAGS) -module -avoid-version $(PLUGIN_LIBS)
	@rm -f tmp$@.lo tmp$@.o libtmp$@.la
	@cp .libs/libtmp$@.so* $@
	@rm -f .libs/libtmp$@.*

if PLUGINS

plugindir = $(libdir)/gaim

plugin_DATA = autorecon.so chatlist.so iconaway.so notify.so spellchk.so history.so timestamp.so idle.so

$(plugin_DATA): $(top_srcdir)/src/gaim.h

clean distclean:
	rm -rf $(plugin_DATA) .libs

else

plugin_DATA = 

endif


EXTRA_DIST = ChangeLog PERL-HOWTO HOWTO SIGNALS autorecon.c filectl.c iconaway.c \
		notify.c spellchk.c gaim.pl mailchk.c chatlist.c gtik.c error.c  \
		history.c timestamp.c fortuneprofile.pl idle.c
