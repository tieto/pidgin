#
# targets.mak
#
# This file should be included at the end of all Makefile.mingw
# files for better handling of cross directory dependencies
#

$(GAIM_CONFIG_H): $(GAIM_TOP)/config.h.mingw
	cp $(GAIM_TOP)/config.h.mingw $(GAIM_CONFIG_H)

$(GAIM_LIBGAIM_DLL) $(GAIM_LIBGAIM_DLL).a:
	$(MAKE) -C $(GAIM_LIB_TOP) -f $(GAIM_WIN32_MAKEFILE) libgaim.dll

$(GAIM_LIBGAIM_PERL_DLL) $(GAIM_LIBGAIM_PERL_DLL).a:
	$(MAKE) -C $(GAIM_LIB_PERL_TOP) -f $(GAIM_WIN32_MAKEFILE) perl.dll

$(GAIM_GTKGAIM_DLL) $(GAIM_GTKGAIM_DLL).a:
	$(MAKE) -C $(GAIM_GTK_TOP) -f $(GAIM_WIN32_MAKEFILE) gtkgaim.dll

$(GAIM_IDLETRACK_DLL) $(GAIM_IDLETRACK_DLL).a:
	$(MAKE) -C $(GAIM_GTK_IDLETRACK_TOP) -f $(GAIM_WIN32_MAKEFILE) idletrack.dll

$(GAIM_EXE):
	$(MAKE) -C $(GAIM_GTK_TOP) -f $(GAIM_WIN32_MAKEFILE) gaim.exe

$(GAIM_PORTABLE_EXE):
	$(MAKE) -C $(GAIM_GTK_TOP) -f $(GAIM_WIN32_MAKEFILE) gaim-portable.exe

# Installation Directories
$(GAIM_INSTALL_DIR):
	mkdir -p $(GAIM_INSTALL_DIR)

$(GAIM_INSTALL_PERLMOD_DIR):
	mkdir -p $(GAIM_INSTALL_PERLMOD_DIR)

$(GAIM_INSTALL_PLUGINS_DIR):
	mkdir -p $(GAIM_INSTALL_PLUGINS_DIR)

$(GAIM_INSTALL_PIXMAPS_DIR):
	mkdir -p $(GAIM_INSTALL_PIXMAPS_DIR)

$(GAIM_INSTALL_PO_DIR):
	mkdir -p $(GAIM_INSTALL_PO_DIR)

$(GAIM_INSTALL_SOUNDS_DIR):
	mkdir -p $(GAIM_INSTALL_SOUNDS_DIR)
