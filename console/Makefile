VERSION=gntgaim-0.0.0dev
CC=gcc
CFLAGS=`pkg-config --cflags gaim gobject-2.0 gnt` -g -Wall -DVERSION=\"$(VERSION)\" -DSTANDALONE
LDFLAGS=`pkg-config --libs gaim gobject-2.0 libxml-2.0 gnt` -pg -lgaim

GG_SOURCES = \
	gntaccount.c \
	gntblist.c \
	gntconn.c \
	gntconv.c \
	gntdebug.c \
	gntnotify.c \
	gntplugin.c \
	gntprefs.c \
	gntrequest.c \
	gntui.c

GG_HEADERS = \
	gntaccount.h \
	gntblist.h \
	gntconn.h \
	gntconv.h \
	gntdebug.h \
	gntnotify.h \
	gntprefs.h \
	gntplugin.h \
	gntrequest.h \
	gntui.h

GG_OBJECTS = \
	gntaccount.o \
	gntblist.o \
	gntconn.o \
	gntconv.o \
	gntdebug.o \
	gntnotify.o \
	gntplugin.o \
	gntprefs.o \
	gntrequest.o \
	gntui.o

all: gntgaim

gntgaim: gntgaim.o $(GG_OBJECTS)
	$(CC) -o gntgaim gntgaim.o $(GG_OBJECTS) $(LDFLAGS)
gntaccount.o: gntaccount.c $(GG_HEADERS)
gntblist.o: gntblist.c $(GG_HEADERS)
gntconv.o: gntconv.c $(GG_HEADERS)
gntgaim.o: gntgaim.c gntgaim.h $(GG_HEADERS)
gntnotify.o: gntnotify.c $(GG_HEADERS)
gntui.o: gntui.c $(GG_HEADERS)

clean:
	rm -f *.o
	rm -f gntgaim

