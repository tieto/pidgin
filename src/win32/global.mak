#
# global.mak
#
# This file should be included by all Makefile.mingw files for project
# wide definitions.
#

CC = gcc.exe

# Don't use -g flag when building Plugin DLLs
CFLAGS += -O2 -Wall -mno-cygwin -fnative-struct

VERSION := $(shell cat $(GAIM_TOP)/VERSION)

DEFINES += 	-DAIM_BUILDDATE=\"`date +%Y%m%d`\" \
		-DAIM_BUILDTIME=\"`date +%H%M%S`\" \
		-DVERSION=\"$(VERSION)\" \
		-DHAVE_CONFIG_H \
		-DGTK_ENABLE_BROKEN

