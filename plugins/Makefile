CC = gcc
CFLAGS = -Wall `gnome-config --cflags gtk` -I../src
LDFLAGS = -ggdb `gnome-config --libs gtk` -shared

all: simple.so gaiminc.so

simple.so: simple.c
	$(CC) $(CFLAGS) $(LDFLAGS) -Wl,-soname,$@ -o $@ $<

gaiminc.so: gaiminc.c
	$(CC) $(CFLAGS) $(LDFLAGS) -Wl,-soname,$@ -o $@ $<
