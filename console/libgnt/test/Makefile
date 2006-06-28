CFLAGS=`pkg-config --cflags gobject-2.0` -g -I../
LDFLAGS=`pkg-config --libs gobject-2.0` -lncursesw -pg -lgnt -L../

EXAMPLES=focus tv multiwin

all:
	cd .. && make
	make examples

clean:
	rm -f $(EXAMPLES)

examples: $(EXAMPLES)
