CFLAGS=`pkg-config --cflags gaim gobject-2.0` -g -I./libgnt/
LDFLAGS=`pkg-config --libs gaim gobject-2.0 libxml-2.0` -lncursesw -L./libgnt/ -lgnt -pg

GG_SOURCES = \
	gntblist.c \
	gntui.c

GG_HEADERS = \
	gntblist.h \
	gntui.h

GG_OBJECTS = \
	gntblist.o \
	gntui.o

all:
	cd libgnt && make
	make gntgaim

gntgaim: gntgaim.o $(GG_OBJECTS)
	$(CC) -o gntgaim gntgaim.o $(GG_OBJECTS) $(LDFLAGS)
gntblist.o: gntblist.c $(GG_HEADERS)
gntgaim.o: gntgaim.c gntgaim.h $(GG_HEADERS)
gntui.o: gntui.c $(GG_HEADERS)

clean:
	cd libgnt && make clean
	rm *.o
	rm gntgaim

