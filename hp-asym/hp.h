#pragma once

#include <stdatomic.h>

#if HP_STATS
typedef struct hp_stats {
	unsigned long api_calls;
	unsigned long reclaimed;
	unsigned long retired;
	unsigned long guarded; // i.e. protected
	unsigned long cleared;
} HpStats;
#endif

typedef struct hp_retired {
	void* pointer;
	struct hp_retired* next;
} HpRetired;

typedef struct hp_record {
	_Atomic(int) active; // TID of owner thread
	_Atomic(struct hp_record*) next;
	_Atomic(void*)* pointers;
	int index;
	HpRetired* rlist;
	unsigned rcount;
#if HP_STATS
	HpStats stats;
#endif
} HpRecord;

#if HP_STATS
int hp_count_domains();
void hp_clear_stats();
HpStats* hp_gather_stats();
#endif

typedef struct hp_domain {
	_Atomic(HpRecord*) records;
	_Atomic(struct hp_domain*) next;
	unsigned h;
	unsigned k;
} HpDomain;

// domain related functions
// pointers[] -> array of hazard pointers to protect
// k -> length of pointers[] array
//      maximum number of pointers per thread
// that can be protected at once
HpDomain* hp_create(int k, int h);

// THREAD UNSAFE!
// should be called by the main thread
// after no other thread is left
void hp_destroy();

// get new hazard pointers
HpRecord* hp_alloc(HpDomain* d, int tid);
void hp_release(HpDomain* d, HpRecord* hp);

// manipulate hazard pointers
int hp_set(HpDomain* d, HpRecord* hp, void* p, unsigned i);
int hp_protect(HpDomain* d, HpRecord* hp, void* p);
void hp_clear(HpDomain* d, HpRecord* hp);

// reclaim memory
void hp_retire(HpDomain* d, int tid, HpRecord* hp, void* p);
