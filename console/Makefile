CC=gcc
CFLAGS=`pkg-config --cflags gaim gobject-2.0 gnt` -g -Wall
LDFLAGS=`pkg-config --libs gaim gobject-2.0 libxml-2.0 gnt` -pg

GG_SOURCES = \
	gntaccount.c \
	gntblist.c \
	gntconv.c \
	gntui.c

GG_HEADERS = \
	gntaccount.h \
	gntblist.h \
	gntconv.h \
	gntui.h

GG_OBJECTS = \
	gntaccount.o \
	gntblist.o \
	gntconv.o \
	gntui.o

all: gntgaim

gntgaim: gntgaim.o $(GG_OBJECTS)
	$(CC) -o gntgaim gntgaim.o $(GG_OBJECTS) $(LDFLAGS)
gntaccount.o: gntaccount.c $(GG_HEADERS)
gntblist.o: gntblist.c $(GG_HEADERS)
gntconv.o: gntconv.c $(GG_HEADERS)
gntgaim.o: gntgaim.c gntgaim.h $(GG_HEADERS)
gntui.o: gntui.c $(GG_HEADERS)

clean:
	rm -f *.o
	rm -f gntgaim

