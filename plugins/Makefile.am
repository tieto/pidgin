if GNOMEAPPLET
CFLAGS += $(GTK_CFLAGS) $(GNOME_INCLUDEDIR) -I../src -DUSE_APPLET
else
CFLAGS += $(GTK_CFLAGS) -I../src -I../libfaim/faim -I../libfaim
endif



LDFLAGS += -ggdb $(GTK_LIBS) -shared
SUFFIXES = .c .so
.c.so:
	$(CC) $(CFLAGS) -DVERSION=\"$(VERSION)\" -fPIC -DPIC -o $@ $< $(LDFLAGS) $(PLUGIN_LIBS)



if PLUGINS
plugin_DATA = gen_away.so autorecon.so iconaway.so notify.so spellchk.so
plugindir = $(libdir)/gaim

clean distclean clean-recursive distclean-recursive:
	$(RM) $(plugin_DATA)
endif



EXTRA_DIST = ChangeLog CRAZY HOWTO SIGNALS autorecon.c error.c filectl.c \
	gaiminc.c iconaway.c lagmeter.c notify.c simple.c spellchk.c \
	toc_commands.c
