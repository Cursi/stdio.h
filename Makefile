CC = gcc
CFLAGS = -Wall -fPIC -g
LIBFLAGS = -shared

.PHONY: build clean

build: libso_stdio.so

libso_stdio.so: so-stdio.c
	$(CC) $(CFLAGS) -c so-stdio.c
	$(CC) $(LIBFLAGS) so-stdio.o -o libso_stdio.so

clean: so-stdio.o libso_stdio.so
	rm so-stdio.o libso_stdio.so