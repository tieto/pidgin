CC=gcc
CFLAGS=`pkg-config --cflags gobject-2.0 gmodule-2.0` -g -I../ -DSTANDALONE
LDFLAGS=`pkg-config --libs gobject-2.0 gmodule-2.0 gnt` -pg

EXAMPLES=combo focus tv multiwin keys menu parse

all:
	make examples

clean:
	rm -f $(EXAMPLES) *.so wm

WM: wm
	for i in $(EXAMPLES); do gcc -shared $(CFLAGS) -USTANDALONE $(LDFLAGS) $${i}.c -o $${i}.so ; done

examples: $(EXAMPLES)
