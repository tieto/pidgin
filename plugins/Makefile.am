CFLAGS += -I\$(top_srcdir) -I\$(top_srcdir)/src -DVERSION=\"$(VERSION)\"
SUFFIXES = .c .so
.c.so:
	$(LIBTOOL) --mode=compile $(CC) $(CFLAGS) -c $< -o tmp$@.lo $(PLUGIN_CFLAGS)
	$(LIBTOOL) --mode=link    $(CC) $(CFLAGS) -o libtmp$@.la -rpath $(plugindir) tmp$@.lo $(LIBS) $(LDFLAGS) $(PLUGIN_LIBS)
	rm -f tmp$@.lo tmp$@.o libtmp$@.la
	if test -f .libs/libtmp$@.so ; then \
		cp .libs/libtmp$@.so $@ ; \
	elif test -f .libs/libtmp$@.so.0.0 ; then \
		cp .libs/libtmp$@.so.0.0 $@ ; \
	fi
	rm -f .libs/libtmp$@.*

if PLUGINS

plugindir = $(libdir)/gaim

plugin_DATA = autorecon.so chatlist.so iconaway.so lagmeter.so notify.so spellchk.so

$(plugin_DATA): $(top_srcdir)/src/gaim.h

clean distclean:
	$(RM) $(plugin_DATA)

else

plugin_DATA = 

endif


EXTRA_DIST = ChangeLog PERL-HOWTO HOWTO SIGNALS autorecon.c filectl.c iconaway.c \
		lagmeter.c notify.c spellchk.c gaim.pl mailchk.c chatlist.c
