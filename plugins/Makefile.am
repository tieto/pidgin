SUFFIXES = .c .so
if GNOMEAPPLET
CFLAGS += $(GTK_CFLAGS) -I../src -DUSE_APPLET
else
CFLAGS += $(GTK_CFLAGS) -I../src
endif
LDFLAGS += -ggdb $(GTK_LIBS) -shared
.c.so:
	$(CC) $(CFLAGS) -Wl,-soname,$@ -o $@ $< $(LDFLAGS)
plugin_DATA = autorecon.so iconaway.so spellchk.so
plugindir = $(libdir)/gaim
EXTRA_DIST = CRAZY HOWTO SIGNALS autorecon.c chkmail.c filectl.c gaiminc.c \
	iconaway.c simple.c spellchk.c
