#
# global.mak
#
# This file should be included by all Makefile.mingw files for project
# wide definitions (after correctly defining PIDGIN_TREE_TOP).
#

#include optional $(PIDGIN_TREE_TOP)/local.mak to allow overriding of any definitions
-include $(PIDGIN_TREE_TOP)/local.mak

# Locations of our various dependencies
WIN32_DEV_TOP ?= $(PIDGIN_TREE_TOP)/../win32-dev
ASPELL_TOP ?= $(WIN32_DEV_TOP)/aspell-dev-0-50-3-3
GTKSPELL_TOP ?= $(WIN32_DEV_TOP)/gtkspell-2.0.6
GTK_TOP ?= $(WIN32_DEV_TOP)/gtk_2_0
GTK_BIN ?= $(GTK_TOP)/bin
HOWL_TOP ?= $(WIN32_DEV_TOP)/howl-1.0.0
LIBXML2_TOP ?= $(WIN32_DEV_TOP)/libxml2
MEANWHILE_TOP ?= $(WIN32_DEV_TOP)/meanwhile-1.0.2
NSPR_TOP ?= $(WIN32_DEV_TOP)/nspr-4.6.4
NSS_TOP ?= $(WIN32_DEV_TOP)/nss-3.11.4
PERL_LIB_TOP ?= $(WIN32_DEV_TOP)/perl58
SILC_TOOLKIT ?= $(WIN32_DEV_TOP)/silc-toolkit-1.0.2
TCL_LIB_TOP ?= $(WIN32_DEV_TOP)/tcl-8.4.5

# Where we installing this stuff to?
PIDGIN_INSTALL_DIR := $(PIDGIN_TREE_TOP)/win32-install-dir
PURPLE_INSTALL_DIR := $(PIDGIN_TREE_TOP)/win32-install-dir
PIDGIN_INSTALL_PERLMOD_DIR := $(PIDGIN_INSTALL_DIR)/perlmod
PIDGIN_INSTALL_PLUGINS_DIR := $(PIDGIN_INSTALL_DIR)/plugins
PURPLE_INSTALL_PERLMOD_DIR := $(PURPLE_INSTALL_DIR)/perlmod
PURPLE_INSTALL_PLUGINS_DIR := $(PURPLE_INSTALL_DIR)/plugins
PURPLE_INSTALL_PO_DIR := $(PURPLE_INSTALL_DIR)/locale

# Important (enough) locations in our source code
PURPLE_TOP := $(PIDGIN_TREE_TOP)/libpurple
PURPLE_PLUGINS_TOP := $(PURPLE_TOP)/plugins
PURPLE_PERL_TOP := $(PURPLE_PLUGINS_TOP)/perl
PIDGIN_TOP := $(PIDGIN_TREE_TOP)/pidgin
PIDGIN_IDLETRACK_TOP := $(PIDGIN_TOP)/win32/IdleTracker
PIDGIN_PIXMAPS_TOP := $(PIDGIN_TOP)/pixmaps
PIDGIN_PLUGINS_TOP := $(PIDGIN_TOP)/plugins
PIDGIN_SOUNDS_TOP := $(PIDGIN_TOP)/sounds
PURPLE_PO_TOP := $(PIDGIN_TREE_TOP)/po
PURPLE_PROTOS_TOP := $(PURPLE_TOP)/protocols

# Locations of important (in-tree) build targets
PIDGIN_CONFIG_H := $(PIDGIN_TREE_TOP)/config.h
PURPLE_CONFIG_H := $(PIDGIN_TREE_TOP)/config.h
PIDGIN_IDLETRACK_DLL := $(PIDGIN_IDLETRACK_TOP)/idletrack.dll
PURPLE_DLL := $(PURPLE_TOP)/libpurple.dll
PURPLE_PERL_DLL := $(PURPLE_PERL_TOP)/perl.dll
PIDGIN_DLL := $(PIDGIN_TOP)/pidgin.dll
PIDGIN_EXE := $(PIDGIN_TOP)/pidgin.exe
PIDGIN_PORTABLE_EXE := $(PIDGIN_TOP)/pidgin-portable.exe

GCCWARNINGS := -Waggregate-return -Wcast-align -Wdeclaration-after-statement -Werror-implicit-function-declaration -Wextra -Wno-sign-compare -Wno-unused-parameter -Winit-self -Wmissing-declarations -Wmissing-prototypes -Wnested-externs -Wpointer-arith -Wundef

# parse the version number from the configure.ac file if it is newer
#AC_INIT([pidgin], [2.0.0dev], [devel@pidgin.im])
PIDGIN_VERSION := $(shell \
  if [ ! $(PIDGIN_TREE_TOP)/VERSION -nt $(PIDGIN_TREE_TOP)/configure.ac ]; then \
    awk 'BEGIN {FS="\\] *, *\\["} /^AC_INIT\(.+\)/ {printf("%s",$$2); exit}' \
      $(PIDGIN_TREE_TOP)/configure.ac > $(PIDGIN_TREE_TOP)/VERSION; \
  fi; \
  cat $(PIDGIN_TREE_TOP)/VERSION \
)
PURPLE_VERSION := $(PIDGIN_VERSION)

DEFINES += 	-DVERSION=\"$(PIDGIN_VERSION)\" \
		-DHAVE_CONFIG_H

# Use -g flag when building debug version of Pidgin (including plugins).
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
ifeq "$(origin CC)" "default"
  CC := gcc.exe
endif
GMSGFMT ?= $(GTK_BIN)/msgfmt
MAKENSIS ?= makensis.exe
PERL ?= /cygdrive/c/perl/bin/perl
WINDRES ?= windres
STRIP ?= strip

PIDGIN_COMMON_RULES := $(PURPLE_TOP)/win32/rules.mak
PIDGIN_COMMON_TARGETS := $(PURPLE_TOP)/win32/targets.mak
MINGW_MAKEFILE := Makefile.mingw
