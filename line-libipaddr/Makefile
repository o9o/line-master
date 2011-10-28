# Makes libipaddr.so

INSTALL=/usr/bin/install -c
PREFIX=/usr

all: Makefile libipaddr.so

libipaddr.so:	libipaddr.o
	gcc -shared libipaddr.o -ldl -o libipaddr.so

libipaddr.o:	libipaddr.c
	gcc -g -c -Wall -Wextra -Wunused -Wshadow -Wpointer-arith -Wstrict-prototypes -Wmissing-prototypes libipaddr.c -fPIC -DPIC -o libipaddr.o

clean:
	rm -f $(TARGET) *.o *~ libipaddr.so

install: libipaddr.so
	@echo installing libipaddr with prefix $(PREFIX)
	$(INSTALL) libipaddr.so $(PREFIX)/lib
