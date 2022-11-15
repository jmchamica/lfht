CFLAGS=-std=gnu11 -Wall -Wextra -I. -fPIC
AR=ar
OPT=-O3 -DHP_STATS=1
LFLAGS=-shared
DEBUG=-g -ggdb -Og -DHP_DEBUG=1 -DHP_STATS=1

default: libhp.a
debug: libhp_debug.a libhp_debug.so
all: libhp.a libhp.so test

libhp.so: hp.o
	$(CC) hp.o $(CFLAGS) $(OPT) $(LFLAGS) -o libhp.so

libhp.a: hp.o
	$(AR) rc libhp.a hp.o

hp.o: hp.c
	$(CC) -c hp.c $(CFLAGS) $(OPT) $(LFLAGS)

libhp_debug.so: hp_debug.o
	$(CC) hp_debug.o $(CFLAGS) $(DEBUG) $(LFLAGS) -o libhp_debug.so

libhp_debug.a: hp_debug.o
	$(AR) rc libhp_debug.a hp_debug.o

hp_debug.o: hp.c
	$(CC) -c hp.c $(CFLAGS) $(DEBUG) $(LFLAGS) -o hp_debug.o

test: test.c libhp_debug.a hp.h
	$(CC) test.c libhp_debug.a -lpthread $(CFLAGS) $(DEBUG) -o test

clean:
	rm -f *.o *.a *.so test
