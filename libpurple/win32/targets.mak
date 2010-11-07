#
# targets.mak
#
# This file should be included at the end of all Makefile.mingw
# files for better handling of cross directory dependencies
#

$(PIDGIN_CONFIG_H): $(PIDGIN_CONFIG_H).mingw $(PIDGIN_TREE_TOP)/configure.ac
	sed -e 's/@VERSION@/$(PIDGIN_VERSION)/; s/@DISPLAY_VERSION@/$(DISPLAY_VERSION)/' $@.mingw > $@

$(PURPLE_PURPLE_H): $(PURPLE_PURPLE_H).in
	sed -e 's/@PLUGINS_DEFINE@/#define PURPLE_PLUGINS 1/' $@.in > $@

$(PURPLE_VERSION_H): $(PURPLE_VERSION_H).in $(PIDGIN_TREE_TOP)/configure.ac
	awk 'BEGIN {FS="[\\(\\)\\[\\]]"} \
	  /^m4_define..purple_major_version/ {system("sed -e s/@PURPLE_MAJOR_VERSION@/"$$5"/ $@.in > $@");} \
	  /^m4_define..purple_minor_version/ {system("sed -e s/@PURPLE_MINOR_VERSION@/"$$5"/ $@ > $@.tmp & mv $@.tmp $@");} \
	  /^m4_define..purple_micro_version/ {system("sed -e s/@PURPLE_MICRO_VERSION@/"$$5"/ $@ > $@.tmp & mv $@.tmp $@"); exit}' $(PIDGIN_TREE_TOP)/configure.ac

$(PURPLE_DLL) $(PURPLE_DLL).a: $(PURPLE_VERSION_H)
	$(MAKE) -C $(PURPLE_TOP) -f $(MINGW_MAKEFILE) libpurple.dll

$(PURPLE_PERL_DLL) $(PURPLE_PERL_DLL).a:
	$(MAKE) -C $(PURPLE_PERL_TOP) -f $(MINGW_MAKEFILE) perl.dll

$(PIDGIN_DLL) $(PIDGIN_DLL).a:
	$(MAKE) -C $(PIDGIN_TOP) -f $(MINGW_MAKEFILE) pidgin.dll

$(PIDGIN_IDLETRACK_DLL) $(PIDGIN_IDLETRACK_DLL).a:
	$(MAKE) -C $(PIDGIN_IDLETRACK_TOP) -f $(MINGW_MAKEFILE) idletrack.dll

$(PIDGIN_EXE):
	$(MAKE) -C $(PIDGIN_TOP) -f $(MINGW_MAKEFILE) pidgin.exe

# Installation Directories
$(PIDGIN_INSTALL_DIR):
	mkdir -p $(PIDGIN_INSTALL_DIR)

$(PIDGIN_INSTALL_PERLMOD_DIR):
	mkdir -p $(PURPLE_INSTALL_PERLMOD_DIR)

$(PIDGIN_INSTALL_PLUGINS_DIR):
	mkdir -p $(PIDGIN_INSTALL_PLUGINS_DIR)

$(PURPLE_INSTALL_PO_DIR):
	mkdir -p $(PURPLE_INSTALL_PO_DIR)

