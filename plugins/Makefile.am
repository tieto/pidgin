SUFFIXES = .c .so
CFLAGS += $(GTK_CFLAGS) -I../src
LDFLAGS += -ggdb $(GTK_LIBS) -shared
.c.so:
	$(CC) $(CFLAGS) -Wl,-soname,$@ -o $@ $< $(LDFLAGS)
plugin_DATA = autorecon.so iconaway.so spellchk.so
plugindir = $(libdir)/gaim
