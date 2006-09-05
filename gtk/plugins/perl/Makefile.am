SUBDIRS = . common

common_sources = \
	common/Gtk.pm \
	common/Gtk.xs \
	common/GtkAccount.xs \
	common/GtkBlist.xs \
	common/GtkConn.xs \
	common/GtkConv.xs \
	common/GtkConvWin.xs \
	common/GtkDebug.xs \
	common/GtkDialogs.xs \
	common/GtkFt.xs \
	common/GtkIMHtml.xs \
	common/GtkIMHtmlToolbar.xs \
	common/GtkLog.xs \
	common/GtkMenuTray.xs \
	common/GtkPlugin.xs \
	common/GtkPluginPref.xs \
	common/GtkPounce.xs \
	common/GtkPrefs.xs \
	common/GtkPrivacy.xs \
	common/GtkRoomlist.xs \
	common/GtkSavedStatuses.xs \
	common/GtkSession.xs \
	common/GtkSound.xs \
	common/GtkStatusBox.xs \
	common/GtkThemes.xs \
	common/GtkUtils.xs \
	common/gtkmodule.h \
	common/Makefile.PL.in \
	common/typemap

EXTRA_DIST = \
	Makefile.mingw \
	common/Makefile.mingw \
	$(common_sources)

all-local: common/Makefile

common/Makefile: common/Makefile.PL
	@cd common && $(perlpath) Makefile.PL $(PERL_MM_PARAMS)

common/Makefile.PL: common/Makefile.PL.in $(top_builddir)/config.status
	cd $(top_builddir) && $(SHELL) ./config.status $(subdir)/$@ $(am_depfiles_maybe)

AM_CPPFLAGS = \
	-DVERSION=\"$(VERSION)\" \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgaim \
	-I$(top_srcdir)/gtk \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS) \
	$(PLUGIN_CFLAGS) \
	$(PERL_CFLAGS)
