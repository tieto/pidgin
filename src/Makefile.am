SUBDIRS = protocols

if GNOMEAPPLET
bin_PROGRAMS = gaim_applet
gaim_applet_SOURCES =	about.c \
			aim.c \
			applet.c \
			away.c \
			browser.c \
			buddy.c \
			buddy_chat.c \
			conversation.c \
			dialogs.c \
			gaimrc.c \
			gtkimhtml.c \
			gtkspell.c \
			gtkticker.c \
			html.c \
			idle.c \
			multi.c \
			perl.c \
			plugins.c \
			prefs.c \
			proxy.c \
			prpl.c \
			server.c \
			sound.c \
			ticker.c \
			util.c
gaim_applet_DEPENDENCIES = @LIBOBJS@
gaim_applet_LDFLAGS = -export-dynamic
gaim_applet_LDADD = @LDADD@ @LIBOBJS@
else
bin_PROGRAMS = gaim
gaim_SOURCES =	about.c \
		aim.c \
		applet.c \
		away.c \
		browser.c \
		buddy.c \
		buddy_chat.c \
		conversation.c \
		dialogs.c \
		gaimrc.c \
		gtkimhtml.c \
		gtkspell.c \
		gtkticker.c \
		html.c \
		idle.c \
		multi.c \
		perl.c \
		plugins.c \
		prefs.c \
		proxy.c \
		prpl.c \
		server.c \
		sound.c \
		ticker.c \
		util.c
gaim_DEPENDENCIES = @LIBOBJS@
gaim_LDFLAGS = -export-dynamic
gaim_LDADD = @LDADD@ @LIBOBJS@
endif

# Static protocols
LIBS += $(STATIC_LINK_LIBS)

# Perl stuff. plugins shouldn't need this either (right?)
CFLAGS += $(PERL_CFLAGS)
LIBS += $(PERL_LIBS)

# X screensaver extension
LIBS += $(XSS_LIBS)

# gettext stuff
LIBS += $(INTLLIBS)

# any other stuff
CFLAGS += -DLOCALEDIR=\"$(datadir)/locale\" -DLIBDIR=\"$(libdir)/gaim/\" $(DEBUG_CFLAGS)
LIBS += $(DEBUG_LIBS)

EXTRA_DIST =	applet.h \
		convo.h \
		gaim.h \
		getopt.c \
		getopt.h \
		getopt1.c \
		gtkimhtml.h \
		gtkspell.h \
		gtkticker.h \
		multi.h \
		prpl.h \
		proxy.h
