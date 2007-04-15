CC = gcc
CFLAGS = `pkg-config --cflags gtk+-2.0 pygtk-2.0` -I/usr/include/python2.4/ -I.. -g -O0
LDFLAGS = `pkg-config --libs gtk+-2.0 pygtk-2.0 gnt`
 
gnt.so: gnt.o gntmodule.o
	$(CC) $(LDFLAGS) -shared $^ -o $@

gnt.c: gnt.def gnt.override
	pygtk-codegen-2.0 --prefix gnt \
	--override gnt.override \
	gnt.def > $@

clean:
	@rm *.so *.o gnt.c
