LDFLAGS += $(LDADD) $(LIBS) -ggdb -shared
CFLAGS += -DHAVE_CONFIG_H
SUFFIXES = .c .so
.c.so:
	$(CC) $(CFLAGS) -I../src -DVERSION=\"$(VERSION)\" -o $@ $< $(LDFLAGS) $(PLUGIN_LIBS)

if PLUGINS
plugin_DATA = autorecon.so iconaway.so notify.so spellchk.so lagmeter.so
plugindir = $(libdir)/gaim

clean distclean:
	$(RM) $(plugin_DATA)
endif



EXTRA_DIST = ChangeLog PERL-HOWTO CRAZY HOWTO SIGNALS autorecon.c error.c filectl.c \
	gaiminc.c iconaway.c lagmeter.c notify.c simple.c spellchk.c \
	toc_commands.c events.c gaim.pl
