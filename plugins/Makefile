CC = gcc
CFLAGS = -Wall `gnome-config --cflags gtk` -I../src -g
LDFLAGS = -ggdb `gnome-config --libs gtk` -shared -lpthread

all: simple.so gaiminc.so autorecon.so spellchk.so chkmail.so filectl.so

.SUFFIXES: .c .so

.c.so:
	$(CC) $(CFLAGS) $(LDFLAGS) -Wl,-soname,$@ -o $@ $<
