#
# global.mak
#
# This file should be included by all Makefile.mingw files for project
# wide definitions (after correctly defining GAIM_TOP).
#

# Locations of our various dependencies
WIN32_DEV_TOP := $(GAIM_TOP)/../win32-dev
ASPELL_TOP := $(WIN32_DEV_TOP)/aspell-dev-0-50-3-3
GTKSPELL_TOP := $(WIN32_DEV_TOP)/gtkspell-2.0.6
GTK_TOP := $(WIN32_DEV_TOP)/gtk_2_0
GTK_BIN := $(GTK_TOP)/bin
HOWL_TOP := $(WIN32_DEV_TOP)/howl-1.0.0
LIBXML2_TOP := $(WIN32_DEV_TOP)/libxml2
MEANWHILE_TOP := $(WIN32_DEV_TOP)/meanwhile-1.0.2
NSPR_TOP := $(WIN32_DEV_TOP)/nspr-4.4.1
NSS_TOP := $(WIN32_DEV_TOP)/nss-3.9
PERL_LIB_TOP := $(WIN32_DEV_TOP)/perl58
SILC_TOOLKIT := $(WIN32_DEV_TOP)/silc-toolkit-1.0.2
TCL_LIB_TOP := $(WIN32_DEV_TOP)/tcl-8.4.5

# Where we installing this stuff to?
GAIM_INSTALL_DIR := $(GAIM_TOP)/win32-install-dir
GAIM_INSTALL_PERLMOD_DIR := $(GAIM_INSTALL_DIR)/perlmod
GAIM_INSTALL_PIXMAPS_DIR := $(GAIM_INSTALL_DIR)/pixmaps
GAIM_INSTALL_PLUGINS_DIR := $(GAIM_INSTALL_DIR)/plugins
GAIM_INSTALL_PO_DIR := $(GAIM_INSTALL_DIR)/locale
GAIM_INSTALL_SOUNDS_DIR := $(GAIM_INSTALL_DIR)/sounds

# Important (enough) locations in our source code
GAIM_LIB_TOP := $(GAIM_TOP)/libgaim
GAIM_LIB_PLUGINS_TOP := $(GAIM_LIB_TOP)/plugins
GAIM_LIB_PERL_TOP := $(GAIM_LIB_PLUGINS_TOP)/perl
GAIM_GTK_TOP := $(GAIM_TOP)/gtk
GAIM_GTK_IDLETRACK_TOP := $(GAIM_GTK_TOP)/win32/IdleTracker
GAIM_GTK_PIXMAPS_TOP := $(GAIM_GTK_TOP)/pixmaps
GAIM_GTK_PLUGINS_TOP := $(GAIM_GTK_TOP)/plugins
GAIM_GTK_SOUNDS_TOP := $(GAIM_GTK_TOP)/sounds
GAIM_PO_TOP := $(GAIM_TOP)/po
GAIM_PROTOS_TOP := $(GAIM_LIB_TOP)/protocols

# Locations of important (in-tree) build targets
GAIM_CONFIG_H := $(GAIM_TOP)/config.h
GAIM_IDLETRACK_DLL := $(GAIM_GTK_IDLETRACK_TOP)/idletrack.dll
GAIM_LIBGAIM_DLL := $(GAIM_LIB_TOP)/libgaim.dll
GAIM_LIBGAIM_PERL_DLL := $(GAIM_LIB_PERL_TOP)/perl.dll
GAIM_GTKGAIM_DLL := $(GAIM_GTK_TOP)/gtkgaim.dll
GAIM_EXE := $(GAIM_GTK_TOP)/gaim.exe
GAIM_PORTABLE_EXE := $(GAIM_GTK_TOP)/gaim-portable.exe

GCCWARNINGS := -Waggregate-return -Wcast-align -Wdeclaration-after-statement -Werror-implicit-function-declaration -Wextra -Wno-sign-compare -Wno-unused-parameter -Winit-self -Wmissing-declarations -Wmissing-prototypes -Wnested-externs -Wpointer-arith -Wundef

# parse the version number from the configure.ac file if it is newer
#AC_INIT([gaim], [2.0.0dev], [gaim-devel@lists.sourceforge.net])
GAIM_VERSION := $(shell \
  if [ ! $(GAIM_TOP)/VERSION -nt $(GAIM_TOP)/configure.ac ]; then \
    awk 'BEGIN {FS="\\] *, *\\["} /^AC_INIT\(.+\)/ {printf("%s",$$2); exit}' \
      $(GAIM_TOP)/configure.ac > $(GAIM_TOP)/VERSION; \
  fi; \
  cat $(GAIM_TOP)/VERSION \
)

DEFINES += 	-DVERSION=\"$(GAIM_VERSION)\" \
		-DHAVE_CONFIG_H

# Use -g flag when building debug version of Gaim (including plugins).
# Use -fnative-struct instead of -mms-bitfields when using mingw 1.1
# (gcc 2.95)
CFLAGS += -O2 -Wall $(GCCWARNINGS) -pipe -mno-cygwin -mms-bitfields -g

# If not specified, dlls are built with the default base address of 0x10000000.
# When loaded into a process address space a dll will be rebased if its base
# address colides with the base address of an existing dll.  To avoid rebasing 
# we do the following.  Rebasing can slow down the load time of dlls and it
# also renders debug info useless.
DLL_LD_FLAGS += -Wl,--enable-auto-image-base

# Build programs
CC := gcc.exe
GMSGFMT := $(GTK_BIN)/msgfmt
MAKENSIS := makensis.exe
PERL := /cygdrive/c/perl/bin/perl
WINDRES := windres

GAIM_COMMON_RULES := $(GAIM_LIB_TOP)/win32/rules.mak
GAIM_COMMON_TARGETS := $(GAIM_LIB_TOP)/win32/targets.mak
GAIM_WIN32_MAKEFILE := Makefile.mingw
