SUFFIXES = .c .so
if GNOMEAPPLET
CFLAGS += $(GTK_CFLAGS) $(GNOME_INCLUDEDIR) -I../src -DUSE_APPLET
else
CFLAGS += $(GTK_CFLAGS) -I../src
endif
LDFLAGS += -ggdb $(GTK_LIBS) -shared
.c.so:
	$(CC) $(CFLAGS) -fPIC -DPIC -o $@ $< $(LDFLAGS)
plugin_DATA = autorecon.so iconaway.so spellchk.so
plugindir = $(libdir)/gaim
EXTRA_DIST = CRAZY HOWTO SIGNALS autorecon.c chkmail.c filectl.c gaiminc.c \
	iconaway.c lagmeter.c notify.c simple.c spellchk.c toc_commands.c
