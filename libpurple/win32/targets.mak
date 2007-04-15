#
# targets.mak
#
# This file should be included at the end of all Makefile.mingw
# files for better handling of cross directory dependencies
#

$(PIDGIN_CONFIG_H): $(PIDGIN_TREE_TOP)/config.h.mingw
	cp $(PIDGIN_TREE_TOP)/config.h.mingw $(PIDGIN_CONFIG_H)

$(PURPLE_DLL) $(PURPLE_DLL).a:
	$(MAKE) -C $(PURPLE_TOP) -f $(MINGW_MAKEFILE) libpurple.dll

$(PURPLE_PERL_DLL) $(PURPLE_PERL_DLL).a:
	$(MAKE) -C $(PURPLE_PERL_TOP) -f $(MINGW_MAKEFILE) perl.dll

$(PIDGIN_DLL) $(PIDGIN_DLL).a:
	$(MAKE) -C $(PIDGIN_TOP) -f $(MINGW_MAKEFILE) pidgin.dll

$(PIDGIN_IDLETRACK_DLL) $(PIDGIN_IDLETRACK_DLL).a:
	$(MAKE) -C $(PIDGIN_IDLETRACK_TOP) -f $(MINGW_MAKEFILE) idletrack.dll

$(PIDGIN_EXE):
	$(MAKE) -C $(PIDGIN_TOP) -f $(MINGW_MAKEFILE) pidgin.exe

$(PIDGIN_PORTABLE_EXE):
	$(MAKE) -C $(PIDGIN_TOP) -f $(MINGW_MAKEFILE) pidgin-portable.exe

# Installation Directories
$(PIDGIN_INSTALL_DIR):
	mkdir -p $(PIDGIN_INSTALL_DIR)

$(PIDGIN_INSTALL_PERLMOD_DIR):
	mkdir -p $(PURPLE_INSTALL_PERLMOD_DIR)

$(PIDGIN_INSTALL_PLUGINS_DIR):
	mkdir -p $(PIDGIN_INSTALL_PLUGINS_DIR)

$(PURPLE_INSTALL_PO_DIR):
	mkdir -p $(PURPLE_INSTALL_PO_DIR)

