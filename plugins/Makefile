CC = gcc
CFLAGS = -Wall
LDFLAGS = -ggdb

simple.so: simple.c
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -Wl,-soname,$@ -o $@ $<
