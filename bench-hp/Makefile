LIB_PATH?=../lfht
HP_PATH?=../hp
SRC_PATH?=./src

CFLAGS=-std=gnu11 -Wall -Wextra -I$(LIB_PATH) -I$(HP_PATH)
OPT=-O3 -g -ggdb -DLFHT_STATS=1 -DHP_STATS=1
LFLAGS=-lpthread
DEBUG=-g -ggdb -O3 -DLFHT_DEBUG=1 -DLFHT_STATS=1 -DHP_STATS=1 -DHP_DEBUG=1

all: test bench dbench
default: test

$(HP_PATH)/libhp.a: $(HP_PATH)/hp.c $(HP_PATH)/hp.h
	cd $(HP_PATH) && make libhp.a

$(HP_PATH)/libhp_debug.a: $(HP_PATH)/hp.c $(HP_PATH)/hp.h
	cd $(HP_PATH) && make libhp_debug.a

$(LIB_PATH)/liblfht.a: $(LIB_PATH)/lfht.c $(LIB_PATH)/lfht.h
	cd $(LIB_PATH) && make liblfht.a

$(LIB_PATH)/liblfht_debug.a: $(LIB_PATH)/lfht.c $(LIB_PATH)/lfht.h
	cd $(LIB_PATH) && make liblfht_debug.a

test: $(SRC_PATH)/test.c $(SRC_PATH)/common.c $(HP_PATH)/libhp_debug.a $(LIB_PATH)/liblfht_debug.a $(LIB_PATH)/lfht.h
	$(CC) $(SRC_PATH)/test.c $(HP_PATH)/libhp_debug.a $(LIB_PATH)/liblfht_debug.a $(CFLAGS) $(DEBUG) $(LFLAGS) -o test

bench: $(SRC_PATH)/bench.c $(SRC_PATH)/common.c $(HP_PATH)/libhp.a $(LIB_PATH)/liblfht.a $(LIB_PATH)/lfht.h
	$(CC) $(SRC_PATH)/bench.c $(HP_PATH)/libhp.a $(LIB_PATH)/liblfht.a $(CFLAGS) $(OPT) $(LFLAGS) -o bench

dbench: $(SRC_PATH)/bench.c $(SRC_PATH)/common.c $(HP_PATH)/libhp_debug.a $(LIB_PATH)/liblfht_debug.a $(LIB_PATH)/lfht.h
	$(CC) $(SRC_PATH)/bench.c $(HP_PATH)/libhp_debug.a $(LIB_PATH)/liblfht_debug.a $(CFLAGS) $(DEBUG) $(LFLAGS) -o dbench

clean:
	rm -f *.o test bench dbench *.pdf *.dot cachegrind*
	cd $(LIB_PATH) && make clean
