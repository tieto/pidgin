SUFFIXES = .c .so
CFLAGS += $(GTK_CFLAGS) -I../src $(GAIM_CFLAGS)
LDFLAGS += -ggdb $(GTK_LIBS) -shared
.c.so:
	$(CC) $(CFLAGS) -Wl,-soname,$@ -o $@ $< $(LDFLAGS)
plugin_DATA = autorecon.so iconaway.so spellchk.so
plugindir = $(libdir)/gaim
EXTRA_DIST = CRAZY HOWTO SIGNALS chkmail.c filectl.c gaiminc.c simple.c toc_commands.c
