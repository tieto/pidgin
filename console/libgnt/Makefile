CFLAGS=`pkg-config --cflags gobject-2.0` -g
LDFLAGS=`pkg-config --libs gobject-2.0` -lncursesw

HEADERS = \
	gntwidget.h \
	gntbox.h \
	gntbutton.h \
	gntcolors.h \
	gntentry.h \
	gntlabel.h \
	gnttree.h \
	gntutils.h \
	gnt.h

SOURCES = \
	gntwidget.c \
	gntbox.c \
	gntbutton.c \
	gntcolors.c \
	gntentry.c \
	gntlabel.c \
	gnttree.c \
	gntutils.c \
	gntmain.c

OBJECTS = \
	gntwidget.o \
	gntbox.o \
	gntbutton.o \
	gntcolors.o \
	gntentry.o \
	gntlabel.o \
	gnttree.o \
	gntutils.o \
	gntmain.o

all: libgnt

test: $(OBJECTS)
key: $(OBJECTS)

gntwidget.o: gntwidget.c $(HEADERS)
gntbox.o: gntbox.c $(HEADERS)
gntbutton.o: gntbutton.c $(HEADERS)
gntcolors.o: gntcolors.c $(HEADERS)
gntentry.o: gntentry.c $(HEADERS)
gntlabel.o: gntlabel.c $(HEADERS)
gnttree.o: gnttree.c $(HEADERS)
gntutils.o: gntutils.c $(HEADERS)
gntmain.o: gntmain.c $(HEADERS)

libgnt: $(OBJECTS)
	$(CC) --shared -o libgnt.so $(OBJECTS)

clean:
	rm *.o
	rm libgnt.so

