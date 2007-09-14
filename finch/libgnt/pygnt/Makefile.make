CC = gcc
CFLAGS = `pkg-config --cflags gtk+-2.0 pygtk-2.0` -I/usr/include/python2.4/ -I.. -g -O0
LDFLAGS = `pkg-config --libs gtk+-2.0 pygtk-2.0 gnt`
 
gnt.so: gnt.o gntmodule.o common.o
	$(CC) $(LDFLAGS) -shared $^ -o $@

gnt.c: gnt.def *.override common.c common.h
	pygtk-codegen-2.0 --prefix gnt \
	--override gnt.override \
	gnt.def > $@

#python codegen/codegen.py --prefix gnt \

clean:
	@rm *.so *.o gnt.c
