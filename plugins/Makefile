CC = gcc
CFLAGS = -Wall `gnome-config --cflags gtk` -I../src
LDFLAGS = -ggdb `gnome-config --libs gtk` -shared

all: simple.so gaiminc.so file_control.so

.SUFFIXES: .c .so

.c.so:
	$(CC) $(CFLAGS) $(LDFLAGS) -Wl,-soname,$@ -o $@ $<
