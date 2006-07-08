CC=gcc
CFLAGS=`pkg-config --cflags gobject-2.0` -g -I../
LDFLAGS=`pkg-config --libs gobject-2.0` -pg -lgnt -L../

EXAMPLES=combo focus tv multiwin

all:
	make examples

clean:
	rm -f $(EXAMPLES)

examples: $(EXAMPLES)
