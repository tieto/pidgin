LDFLAGS += $(LDADD) $(LIBS) -ggdb -shared
SUFFIXES = .c .so
.c.so:
	$(CC) $(CFLAGS) -I../src -DVERSION=\"$(VERSION)\" -fPIC -o $@ $< $(LDFLAGS) $(PLUGIN_LIBS)

if PLUGINS

SUBDIRS = yay

plugin_DATA = autorecon.so iconaway.so irc.so notify.so spellchk.so lagmeter.so
plugindir = $(libdir)/gaim

clean distclean:
	$(RM) $(plugin_DATA)
endif



EXTRA_DIST = ChangeLog PERL-HOWTO CRAZY HOWTO SIGNALS autorecon.c error.c filectl.c \
	gaiminc.c iconaway.c lagmeter.c notify.c simple.c spellchk.c \
	toc_commands.c events.c gaim.pl
