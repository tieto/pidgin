CFLAGS += -I\$(top_srcdir) -I\$(top_srcdir)/src -DVERSION=\"$(VERSION)\"
plugindir = $(libdir)/gaim

if PLUGINS

tickersources = ticker.c gtkticker.so

all:
	$(LIBTOOL) --mode=compile $(CC) $(CFLAGS) -c gtkticker.c -o tmpgtkticker.lo $(PLUGIN_CFLAGS)
	$(LIBTOOL) --mode=compile $(CC) $(CFLAGS) -c ticker.c -o tmpticker.lo $(PLUGIN_CFLAGS)	
	$(LIBTOOL) --mode=link    $(CC) $(CFLAGS) -o libtmpticker.la -rpath $(plugindir) tmpgtkticker.lo tmpticker.lo $(LIBS) $(LDFLAGS) -module -avoid-version $(PLUGIN_LIBS)
	@rm -f tmpticker.lo tmpgtkticker.lo tmpticker.o tmpgtkticker.o libtmpgtkticker.la libticker.la
	@cp .libs/libtmpticker.so* ticker.so
	@rm -f .libs/libtmpticker.*

clean distclean:
	rm -rf $(plugin_DATA) .libs

install:
	cp ticker.so $(plugindir)


endif

EXTRA_DIST = ticker.c gtkticker.c gtkticker.h
