LDFLAGS += $(LDADD) $(LIBS) -ggdb -shared
SUFFIXES = .c .so
.c.so:
	$(CC) $(CFLAGS) -I\$(top_srcdir)/src -I\$(top_srcdir) -DVERSION=\"$(VERSION)\" -fPIC -o $@ $< $(LDFLAGS) $(PLUGIN_LIBS)

if PLUGINS

SUBDIRS = yay icq msn jabber

prpls = irc.so napster.so
plugin_DATA = $(prpls) autorecon.so iconaway.so notify.so spellchk.so lagmeter.so
plugindir = $(libdir)/gaim

$(plugin_DATA): $(top_srcdir)/src/gaim.h
$(prpls): $(top_srcdir)/src/prpl.h

clean distclean:
	$(RM) $(plugin_DATA)
endif



EXTRA_DIST = ChangeLog PERL-HOWTO CRAZY HOWTO SIGNALS autorecon.c error.c filectl.c \
	gaiminc.c iconaway.c lagmeter.c notify.c simple.c spellchk.c \
	toc_commands.c events.c gaim.pl irc.c
