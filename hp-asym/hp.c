#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <hp.h>

// asymmetric mem barrier
#include <linux/membarrier.h>
#include <sys/syscall.h>
#include <unistd.h>
#define barrier() __asm__ __volatile__("": : :"memory")

#define CACHE_SIZE 64

// loop through records and reclaim memory
static void scan(HpDomain* d, HpRecord* hp);
static void help_scan(HpDomain* d, int tid);
static HpRecord* acquire_rec(HpDomain* d, int tid);

// sorting
static int qs_partition(void** arr, int left, int right);
static void qs(void** arr, int left, int right);
static void quick_sort(void** arr, int len);
static int binary_search(void** arr, int len, void* target);

// global domain list
_Atomic(HpDomain*) domains;

// Domains separate lists of hazard pointers (HP) to prevent 
// long scans
HpDomain* hp_create(int k, int h) {
	HpDomain* new = (HpDomain*) malloc(sizeof(HpDomain));
	atomic_init(&(new->records), NULL);
	atomic_init(&(new->next), NULL);
	new->k = k; // HPs per record
	new->h = h; // total ammount of HPs

	// try to append new domain
	_Atomic(HpDomain*)* target = &domains;
start: ;
	HpDomain* observed = atomic_load(target);

	while(observed) {
		// advance domain list
		target = &(observed->next);
		observed = atomic_load(target);
	}
	// observed == NULL => tail found

	if(!atomic_compare_exchange_weak(target, &observed, new)) {
		// someone else added a new domain
		// advance list and retry
		target = &(observed->next);
		goto start;
	}

	return new;
}

// THREAD UNSAFE!
// must be run by main thread at the end of the program
// free all domains
void hp_destroy() {
#if HP_STATS
	hp_clear_stats();
#endif

	HpDomain* head = atomic_load(&domains);

	// clear shared pointer to the domain list
	atomic_store(&domains, NULL);

	// free each domain
	while(head) {
		HpDomain* del = head;
		head = atomic_load(&(head->next));

		// clear all HP protections
		HpRecord* rec = atomic_load(&(del->records));
		while(rec) {
			hp_clear(del, rec);
			rec = atomic_load(&(rec->next));
		}

		help_scan(del, 0);

		// free all records from domain
		rec = atomic_load(&(del->records));
		while(rec) {
			HpRecord* prev = rec;
			rec = atomic_load(&(rec->next));
			free(prev);
		}

		// free domain
		free(del);
	}
}

// get a new hazard pointer
HpRecord* hp_alloc(HpDomain* d, int tid) {
	// try to reuse an existing record
	HpRecord* new = acquire_rec(d, tid);
	if(new) {
#if HP_STATS
		new->stats.api_calls++;
#endif
		return new;
	}

	// prevent cache invalidation of neighbors
	size_t alloc_size = CACHE_SIZE * ((sizeof(HpRecord) / CACHE_SIZE) + 1);
	new = (HpRecord*) aligned_alloc(CACHE_SIZE, alloc_size);

	atomic_init(&(new->active), tid);
	atomic_init(&(new->next), NULL);
	new->rlist = NULL;
	new->rcount = 0;
	new->index = 0;
#if HP_STATS
	new->stats.api_calls++;
#endif

	// set hazard pointers array
	// k => maximum number of HPs that can be protected at once
	size_t array_size = sizeof(_Atomic(void*)) * d->k;
	alloc_size = CACHE_SIZE * ((array_size / CACHE_SIZE) + 1);
	new->pointers = (_Atomic(void*)*) aligned_alloc(CACHE_SIZE, alloc_size);
	for(unsigned i = 0; i < d->k; i++) {
		atomic_init(&(new->pointers[i]), NULL);
	}

	// try to append new record
	_Atomic(HpRecord*)* target = &(d->records);
start: ;
	HpRecord* observed = atomic_load(target);

	while(observed) {
		// advance list
		target = &(observed->next);
		observed = atomic_load(target);
	}
	// observed == NULL => tail found

	if(!atomic_compare_exchange_weak(target, &observed, new)) {
		// someone else added a new record
		// advance list and retry
		target = &(observed->next);
		goto start;
	}

	return new;
}

// Before exiting, a thread discards its hazard pointer (HP).
// Released HPs may be reused by other threads later on.
void hp_release(HpDomain* d, HpRecord* hp) {
#if HP_STATS
	hp->stats.api_calls++;
#endif
	hp_clear(d, hp);
	scan(d, hp);
	atomic_store(&(hp->active), -1);
}

// protect pointer with HP
int hp_set(HpDomain* d, HpRecord* hp, void* p, unsigned i) {
#if HP_STATS
	hp->stats.api_calls++;
#endif

	if(i >= d->k) {
		fprintf(stderr, "[WARNING] %d exceeds the maximum number of simultaneous protected hazard pointers (%d).\n", i, d->k);
		return 0;
	}

#if HP_STATS
	hp->stats.guarded++;
#endif
	atomic_store_explicit(&(hp->pointers[i]), p, memory_order_relaxed);
	barrier();
	return 1;
}

int hp_protect(HpDomain* d, HpRecord* hp, void* p) {
#if HP_STATS
	hp->stats.api_calls++;
	hp->stats.guarded++;
#endif

	// check if pointer is already being protected
	//for(unsigned i = 0; i < d->k; i++) {
	//	if(hp->pointers[i] == p) {
	//		// *p already protected
	//		return 1;
	//	}
	//}

	int i = hp->index;
	atomic_store_explicit(&(hp->pointers[i]), p, memory_order_relaxed);
	barrier();

	// round robin
	hp->index = (i+1) % d->k;

	return 1;
}

// remove protection to pointers
void hp_clear(HpDomain* d, HpRecord* hp) {
#if HP_STATS
	hp->stats.api_calls++;
	hp->stats.cleared++;
#endif

	int found = 0;
	for(unsigned i = 0; i < d->k; i++) {
		if(!atomic_load_explicit(
					&(hp->pointers[i]),
					memory_order_relaxed)) {
			// prevent seq consistent write
			// pointer is already null
			continue;
		}

		found = 1;
		hp->pointers[i] = NULL;
	}

	if(!found) {
		return;
	}

	// flush changes
	atomic_store_explicit(&(hp->pointers[0]), NULL, memory_order_relaxed);
	barrier();
}

// issue reclamation of memory pointed by arg <p>
void hp_retire(HpDomain* d, int tid, HpRecord* hp, void* p) {
#if HP_STATS
	hp->stats.api_calls++;
	hp->stats.retired++;
#endif

	//hp_clear(d, hp);

	HpRetired* retired = (HpRetired*) malloc(sizeof(HpRetired));
	retired->pointer = p;

	HpRetired* rlist = hp->rlist;
	retired->next = rlist;
	hp->rlist = retired;
	hp->rcount++;

	if(d->h > 0 && hp->rcount < d->h + d->h / 2) {
		// to prevent constant record scanning,
		// we set a threshold of retired nodes
		return;
	}

	scan(d, hp);
	help_scan(d, tid);
}

// reuse released HPs or return null
static HpRecord* acquire_rec(HpDomain* d, int tid) {
	_Atomic(HpRecord*)* target = &(d->records);
	HpRecord* observed = atomic_load(target);

	while(observed) {
		_Atomic(int)* atomic_tid = &(observed->active);
		int observed_tid = atomic_load(atomic_tid);

		if(observed_tid < 0 &&
				atomic_compare_exchange_weak(
					atomic_tid,
					&observed_tid,
					tid)) {
			// acquired record
			return observed;
		}

		// advance list
		target = &(observed->next);
		observed = atomic_load(target);
	}

	return NULL;
}

// reclaim unprotected nodes on queue
static void scan(HpDomain* d, HpRecord* hp) {

	// asymmetric memory barrier
	int err = syscall(__NR_membarrier, MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED, 0, 0);
	if(err) {
		perror("Asymmetric memory barrier syscall error. Could not register intent.\n");
		exit(1);
	}

	err = syscall(__NR_membarrier, MEMBARRIER_CMD_PRIVATE_EXPEDITED, 0, 0);
	if(err) {
		perror("Asymmetric memory barrier syscall error. Could not run barrier.\n");
		exit(1);
	}

	// local copy of all pointers currently protected by HPs
	void** plist = NULL;
	int pcount = 0;
	int pcapacity = 0;

	// go through all records and make a copy of all protected pointers
	_Atomic(HpRecord*)* target = &(d->records);
	HpRecord* observed = atomic_load(target);

	if(!observed) {
		return;
	}
	pcapacity = 10;
	plist = (void**) malloc(sizeof(void*) * pcapacity);

	while(observed) {
		if(atomic_load(&(observed->active)) < 0) {
			// skip inactive records
			observed = atomic_load(&(observed->next));
			continue;
		}

		for(unsigned i = 0; i < d->k; i++) {
			void* p = atomic_load_explicit(&(observed->pointers[i]), memory_order_relaxed);

			if(!p) {
				// skip null hazard pointers
				continue;
			}

			plist[pcount++] = p;

			if(pcount >= pcapacity) {
				pcapacity *= 2;
				plist = (void**) realloc(plist, sizeof(void*) * pcapacity);
			}
		}

		// advance record list
		observed = atomic_load(&(observed->next));
	}

	// reduce memory footprint
	if(plist && pcapacity > pcount + 10) {
		pcapacity = pcount;
		plist = (void**) realloc(plist, sizeof(void*) * pcapacity);
	}

	// will help speed up later checks
	quick_sort(plist, pcount);

	// check if any retired nodes can be reclaimed
	HpRetired* rlist = hp->rlist;
	HpRetired* prev = NULL;
	while(rlist) {
		HpRetired* next = rlist->next;

		if(binary_search(plist, pcount, rlist->pointer)) {
			// node currently protected
			prev = rlist;
			rlist = next;
			continue;
		}

		// we may now reclaim memory
		free(rlist->pointer);
		free(rlist);
#if HP_STATS
		//hp->stats.reclaimed++;
#endif

		if(!prev) {
			// remove head
			hp->rlist = next;
		} else {
			prev->next = next;
		}
		hp->rcount--;
		rlist = next;
	}

	free(plist);
}

// loop through all inactive records and reclaim remaining nodes
static void help_scan(HpDomain* d, int tid) {
	_Atomic(HpRecord*)* target = &(d->records);
	HpRecord* observed = atomic_load(target);

	while(observed) {
		_Atomic(int)* atomic_tid = &(observed->active);
		int observed_tid = atomic_load(atomic_tid);

		if(observed_tid < 0 &&
				atomic_compare_exchange_weak(
					atomic_tid,
					&observed_tid,
					tid)) {
			// acquired record
			hp_release(d, observed);
		}

		// advance record list
		observed = atomic_load(&(observed->next));
	}
}

static int qs_partition(void** arr, int left, int right) {
	void* pivot = arr[right];

	int i = left - 1;
	for(int j = left; j < right; j++) {
		if(arr[j] > pivot) {
			continue;
		}

		i++;
		void* vi = arr[i];
		void* vj = arr[j];
		arr[i] = vj;
		arr[j] = vi;
	}

	void* vi = arr[i + 1];
	void* vj = arr[right];
	arr[i + 1] = vj;
	arr[right] = vi;

	return i + 1;
}

static void qs(void** arr, int left, int right) {
	if(left >= right) {
		return;
	}

	int pivot = qs_partition(arr, left, right);
	qs(arr, left, pivot - 1);
	qs(arr, pivot + 1, right);
}

static void quick_sort(void** arr, int len) {
	qs(arr, 0, len - 1);
}

static int binary_search(void** arr, int len, void* target) {
	int left = 0;
	int right = len - 1;

	if(len < 1) {
		return 0;
	}

	if(len <= 1) {
		return arr[0] == target;
	}

	while(right > left) {
		int m = (int)(left + (right - left) / 2);

		if(arr[m] > target) {
			right = m - 1;
			continue;
		}

		if(arr[m] < target) {
			left = m + 1;
			continue;
		}

		// found value
		return 1;
	}

	if(right == left && arr[right] == target) {
		return 1;
	}

	return 0;
}

#if HP_STATS
int hp_count_domains() {
	_Atomic(HpDomain*)* target = &domains;
	HpDomain* observed = atomic_load(target);

	int i = 0;
	while(observed) {
		i++;

		// advance domain list
		target = &(observed->next);
		observed = atomic_load(target);
	}

	return i;
}

void hp_clear_stats() {
	_Atomic(HpDomain*)* target = &domains;
	HpDomain* observed = atomic_load(target);

	int i = 0;
	while(observed) {
		i++;

		// clear all stats
		HpRecord* rec = atomic_load(&(observed->records));
		while(rec) {
			rec->stats.api_calls = 0;
			rec->stats.reclaimed = 0;
			rec->stats.retired = 0;
			rec->stats.guarded = 0;
			rec->stats.cleared = 0;
			rec = atomic_load(&(rec->next));
		}

		// advance domain list
		target = &(observed->next);
		observed = atomic_load(target);
	}
}

HpStats* hp_gather_stats() {
	_Atomic(HpDomain*)* target = &domains;
	HpDomain* observed = atomic_load(target);

	size_t alloc_size = CACHE_SIZE * ((sizeof(HpStats) / CACHE_SIZE) + 1);
	HpStats* stats = (HpStats*) aligned_alloc(CACHE_SIZE, alloc_size);
	stats->api_calls = 0;
	stats->reclaimed = 0;
	stats->retired = 0;
	stats->guarded = 0;
	stats->cleared = 0;

	int i = 0;
	while(observed) {
		i++;

		// clear all stats
		HpRecord* rec = atomic_load(&(observed->records));
		while(rec) {
			stats->api_calls += rec->stats.api_calls;
			stats->reclaimed += rec->stats.reclaimed;
			stats->retired += rec->stats.retired;
			stats->guarded += rec->stats.guarded;
			stats->cleared += rec->stats.cleared;

			rec = atomic_load(&(rec->next));
		}

		// advance domain list
		target = &(observed->next);
		observed = atomic_load(target);
	}

	return stats;
}
#endif

