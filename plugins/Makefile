CC = gcc
CFLAGS = -Wall
LDFLAGS = -ggdb

all: simple.so gaiminc.so

simple.so: simple.c
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -Wl,-soname,$@ -o $@ $<

gaiminc.so: gaiminc.c
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -Wl,-soname,$@ -o $@ $< -I../src `gnome-config --cflags --libs gtk gnome gnomeui`
