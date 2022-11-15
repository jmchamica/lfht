#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <assert.h>
#include <stdatomic.h>
#include <hp.c>

static void* pthread_test_par_protect(void* tid);
static void* pthread_test_par_domains(void* tid);
static void test_par_protect();
static void test_par_protect_roundrobin();
static void test_par_domains();
static void test_seq_protect_multiple_entries();
static void test_seq_protect_roundrobin_multiple_entries();
static void test_seq_retire_multiple_entries();
static void test_seq_protect();
static void test_seq_protect_roundrobin();
static void test_seq_retire();
static void test_seq_domains();
static void test_quick_sort();
static void test_binary_search();

static _Atomic(int) success;
static HpDomain* dom;
static const int size = 6;
static _Atomic(int*) shared[6] = {
	NULL, NULL, NULL, NULL, NULL, NULL
};

int main() {
	// single threaded
	test_seq_domains();
	test_seq_retire();
	test_seq_protect();
	test_seq_protect_roundrobin();

	// single threaded
	// multiple records for hazard pointer protection
	test_seq_retire_multiple_entries();
	test_seq_protect_multiple_entries();
	test_seq_protect_roundrobin_multiple_entries();

	// multi threaded
	test_par_protect();
	test_par_protect_roundrobin();
	test_par_domains();

	test_quick_sort();
	test_binary_search();

	printf("All tests passed successfully!\n");
	return 0;
}

static void* pthread_test_par_protect(void* tid) {
	int id = (uintptr_t)tid;
	HpRecord* hp = hp_alloc(dom, id);

	for(int i = 0; i < size; i++) {
		_Atomic(int*)* target = &shared[i];
		int* observed = atomic_load(target);

		if(!observed) {
			// value is NULL
			// it has been disconnected...
			continue;
		}

		// protect node before using
		hp_set(dom, hp, observed, 0);

		// try to disconnect node
		if(!atomic_compare_exchange_weak(
					target,
					&observed,
					NULL)) {
			// value changed in the meantime...
			continue;
		}

		hp_retire(dom, id, hp, observed);
		hp_clear(dom, hp);
	}

	hp_release(dom, hp);
	return NULL;
}

static void test_par_protect() {
	printf("Multi threaded - testing hazard pointer protection and reclamation.\n");

	int nproc = 4;
	pthread_t* threads = (pthread_t*)malloc(nproc * sizeof(pthread_t));

	atomic_init(&success, 1);

	// create domain
	dom = hp_create(1, 1);
	assert(dom);

	// set values to free
	for(int i = 0; i < size; i++) {
		atomic_init(&shared[i], (int*)malloc(1));
	}

	for(int i = 0; i < nproc; i++) {
		pthread_create(
				&threads[i],
				NULL,
				pthread_test_par_protect,
				(void*)(intptr_t)i);
	}

	for(int i = 0; i < nproc; i++) {
		pthread_join(threads[i], NULL);
	}

	HpStats* stats = hp_gather_stats();
	assert(stats->guarded >= size);
	assert(stats->retired == size);
	assert(stats->reclaimed == size);
	free(stats);

	// all values must be null
	for(int i = 0; i < size; i++) {
		assert(!shared[i]);
	}

	int result = atomic_load(&success);
	assert(result);

	hp_destroy();
}

static void* pthread_test_par_protect_roundrobin(void* tid) {
	int id = (uintptr_t)tid;
	HpRecord* hp = hp_alloc(dom, id);

	for(int i = 0; i < size; i++) {
		_Atomic(int*)* target = &shared[i];
		int* observed = atomic_load(target);

		if(!observed) {
			// value is NULL
			// it has been disconnected...
			continue;
		}

		// protect node before using
		hp_protect(dom, hp, observed);

		// try to disconnect node
		if(!atomic_compare_exchange_weak(
					target,
					&observed,
					NULL)) {
			// value changed in the meantime...
			continue;
		}

		hp_retire(dom, id, hp, observed);
	}

	hp_release(dom, hp);
	return NULL;
}

static void test_par_protect_roundrobin() {
	printf("Multi threaded - testing hazard pointer protection (round robin) and reclamation.\n");

	int nproc = 4;
	pthread_t* threads = (pthread_t*)malloc(nproc * sizeof(pthread_t));

	atomic_init(&success, 1);

	// create domain
	dom = hp_create(1, 1);
	assert(dom);

	// set values to free
	for(int i = 0; i < size; i++) {
		atomic_init(&shared[i], (int*)malloc(1));
	}

	for(int i = 0; i < nproc; i++) {
		pthread_create(
				&threads[i],
				NULL,
				pthread_test_par_protect_roundrobin,
				(void*)(intptr_t)i);
	}

	for(int i = 0; i < nproc; i++) {
		pthread_join(threads[i], NULL);
	}

	HpStats* stats = hp_gather_stats();
	assert(stats->guarded >= size);
	assert(stats->retired == size);
	assert(stats->reclaimed == size);
	free(stats);

	// all values must be null
	for(int i = 0; i < size; i++) {
		assert(!shared[i]);
	}

	int result = atomic_load(&success);
	assert(result);

	hp_destroy();
}


static void* pthread_test_par_domains(void* tid) {
	int id = (uintptr_t)tid;
	int k = 1;
	int t = 4;

	hp_create(k, t);
	return NULL;
}

static void test_par_domains() {
	printf("Multi threaded - testing domain management.\n");
	int nproc = 4;
	pthread_t* threads = (pthread_t*)malloc(nproc * sizeof(pthread_t));

	atomic_init(&success, 1);

	for(int i = 0; i < nproc; i++) {
		pthread_create(
				&threads[i],
				NULL,
				pthread_test_par_domains,
				(void*)(intptr_t)i);
	}

	for(int i = 0; i < nproc; i++) {
		pthread_join(threads[i], NULL);
	}

	assert(hp_count_domains() == 4);
	hp_destroy();
	assert(hp_count_domains() == 0);

	int result = atomic_load(&success);
	assert(result);
}

static void test_seq_protect_multiple_entries() {
	printf("Single threaded - testing protection with multiple records.\n");
	int k = 4;
	int h = 0;
	int tid1 = 0;
	int tid2 = 1;
	HpDomain* d1 = hp_create(k, h);

	int* v1 = (int*)malloc(1);
	int* v2 = (int*)malloc(1);
	int* v3 = (int*)malloc(1);
	int* v4 = (int*)malloc(1);
	HpRecord* t1 = hp_alloc(d1, tid1);
	HpRecord* t2 = hp_alloc(d1, tid2);

	assert(hp_set(d1, t2, v2, 0));
	assert(hp_set(d1, t2, v4, 1));

	hp_retire(d1, tid1, t1, v1);
	hp_retire(d1, tid1, t1, v2);
	hp_retire(d1, tid1, t1, v3);
	hp_retire(d1, tid1, t1, v4);

	// nodes v2 and v4 are protected
	assert(t1->stats.retired == 4);
	assert(t1->stats.reclaimed == 2);

	hp_release(d1, t2);
	assert(t1->stats.reclaimed == 2);

	hp_release(d1, t1);
	assert(t1->stats.reclaimed == 4);

	hp_destroy();

	// this time, try to loop
	k = 3;
	h = 0;
	tid1 = 0;
	tid2 = 1;
	d1 = hp_create(k, h);

	v1 = (int*)malloc(1);
	v2 = (int*)malloc(1);
	v3 = (int*)malloc(1);
	v4 = (int*)malloc(1);
	t1 = hp_alloc(d1, tid1);
	t2 = hp_alloc(d1, tid2);

	assert(hp_protect(d1, t2, v1));
	assert(hp_protect(d1, t2, v2));
	assert(hp_protect(d1, t2, v3));
	assert(hp_protect(d1, t2, v4));

	hp_retire(d1, tid1, t1, v1);
	hp_retire(d1, tid1, t1, v2);
	hp_retire(d1, tid1, t1, v3);
	hp_retire(d1, tid1, t1, v4);

	// nodes v2 and v4 are protected
	assert(t1->stats.retired == 4);
	assert(t1->stats.reclaimed == 1);

	hp_release(d1, t2);
	assert(t1->stats.reclaimed == 1);

	hp_release(d1, t1);
	assert(t1->stats.reclaimed == 4);

	hp_destroy();
}

static void test_seq_protect_roundrobin_multiple_entries() {
	printf("Single threaded - testing protection with multiple records (round robin).\n");
	int k = 4;
	int h = 0;
	int tid1 = 0;
	int tid2 = 1;
	HpDomain* d1 = hp_create(k, h);

	int* v1 = (int*)malloc(1);
	int* v2 = (int*)malloc(1);
	int* v3 = (int*)malloc(1);
	int* v4 = (int*)malloc(1);
	HpRecord* t1 = hp_alloc(d1, tid1);
	HpRecord* t2 = hp_alloc(d1, tid2);

	assert(hp_protect(d1, t2, v2));
	assert(hp_protect(d1, t2, v4));

	hp_retire(d1, tid1, t1, v1);
	hp_retire(d1, tid1, t1, v2);
	hp_retire(d1, tid1, t1, v3);
	hp_retire(d1, tid1, t1, v4);

	// nodes v2 and v4 are protected
	assert(t1->stats.retired == 4);
	assert(t1->stats.reclaimed == 2);

	hp_release(d1, t2);
	assert(t1->stats.reclaimed == 2);

	hp_release(d1, t1);
	assert(t1->stats.reclaimed == 4);

	hp_destroy();
}


static void test_seq_retire_multiple_entries() {
	printf("Single threaded - testing retirement with multiple records.\n");
}

static void test_seq_protect() {
	printf("Single threaded - testing protection with one record.\n");

	int k = 4;
	int h = 2;
	int tid = 0;
	HpDomain* d1 = hp_create(k, h);

	int* v1 = (int*)malloc(1);
	int* v2 = (int*)malloc(1);
	int* v3 = (int*)malloc(1);
	int* v4 = (int*)malloc(1);
	HpRecord* t1 = hp_alloc(d1, tid);

	assert(t1->stats.guarded == 0);
	assert(hp_set(d1, t1, v1, 0));
	assert(t1->stats.guarded == 1);

	// should fail
	// we're trying to add a HP at index 4 which exceeds
	// the agreed "k = 4" pointers per thread
	assert(!hp_set(d1, t1, v2, 4));
	assert(t1->stats.guarded == 1);

	// protect rest of pointers
	assert(hp_set(d1, t1, v2, 1));
	assert(t1->stats.guarded == 2);
	assert(hp_set(d1, t1, v3, 2));
	assert(t1->stats.guarded == 3);
	assert(hp_set(d1, t1, v4, 3));
	assert(t1->stats.guarded == 4);

	assert(t1->stats.retired == 0);
	hp_retire(d1, tid, t1, v1);
	assert(t1->stats.retired == 1);
	assert(t1->stats.reclaimed == 0);

	hp_retire(d1, tid, t1, v2);
	assert(t1->stats.retired == 2);
	assert(t1->stats.reclaimed == 0);

	hp_clear(d1, t1);
	hp_retire(d1, tid, t1, v3);
	assert(t1->stats.retired == 3);
	// reclaimed because hp_retire clears all pointers
	assert(t1->stats.reclaimed == 3);

	hp_retire(d1, tid, t1, v4);
	assert(t1->stats.retired == 4);
	// reclaim frequency is set to >2 nodes
	assert(t1->stats.reclaimed == 3);

	hp_release(d1, t1);
	assert(t1->stats.retired == 4);
	assert(t1->stats.reclaimed == 4);

	hp_destroy();
}

static void test_seq_protect_roundrobin() {
	printf("Single threaded - testing protection (round robin) with one record.\n");

	int k = 2;
	int h = 0;
	int tid = 0;
	HpDomain* d1 = hp_create(k, h);

	int* v1 = (int*)malloc(1);
	int* v2 = (int*)malloc(1);
	int* v3 = (int*)malloc(1);
	int* v4 = (int*)malloc(1);
	HpRecord* t1 = hp_alloc(d1, tid);

	assert(t1->stats.guarded == 0);
	assert(hp_protect(d1, t1, v1));
	assert(t1->stats.guarded == 1);

	// protect rest of pointers
	assert(hp_protect(d1, t1, v2));
	assert(t1->stats.guarded == 2);
	assert(hp_protect(d1, t1, v3));
	assert(t1->stats.guarded == 3);
	assert(hp_protect(d1, t1, v4));
	assert(t1->stats.guarded == 4);

	// at this point, both v1 and v2 are not protected

	assert(t1->stats.retired == 0);
	hp_retire(d1, tid, t1, v1);
	assert(t1->stats.retired == 1);
	assert(t1->stats.reclaimed == 1);

	hp_retire(d1, tid, t1, v2);
	assert(t1->stats.retired == 2);
	assert(t1->stats.reclaimed == 2);

	hp_retire(d1, tid, t1, v3);
	assert(t1->stats.retired == 3);
	assert(t1->stats.reclaimed == 2);

	hp_clear(d1, t1);

	hp_retire(d1, tid, t1, v4);
	assert(t1->stats.retired == 4);
	assert(t1->stats.reclaimed == 4);

	hp_release(d1, t1);
	assert(t1->stats.retired == 4);
	assert(t1->stats.reclaimed == 4);

	hp_destroy();
}

static void test_seq_retire() {
	printf("Single threaded - testing retirement with one record.\n");
	int k = 2;
	int h = 2;
	int tid = 0;
	HpDomain* d1 = hp_create(k, h);

	int* v1 = (int*)malloc(1);
	int* v2 = (int*)malloc(1);
	int* v3 = (int*)malloc(1);
	int* v4 = (int*)malloc(1);
	HpRecord* t1 = hp_alloc(d1, tid);

	assert(t1->stats.guarded == 0);
	assert(t1->stats.retired == 0);
	hp_retire(d1, tid, t1, v1);
	assert(t1->stats.retired == 1);
	assert(t1->stats.reclaimed == 0);

	hp_retire(d1, tid, t1, v2);
	assert(t1->stats.retired == 2);
	assert(t1->stats.reclaimed == 0);

	hp_retire(d1, tid, t1, v3);
	assert(t1->stats.retired == 3);
	assert(t1->stats.reclaimed == 3);

	hp_retire(d1, tid, t1, v4);
	assert(t1->stats.retired == 4);
	assert(t1->stats.reclaimed == 3);

	hp_release(d1, t1);
	assert(t1->stats.retired == 4);
	assert(t1->stats.reclaimed == 4);

	hp_destroy();
}

static void test_seq_domains() {
	printf("Single threaded - testing domain management.\n");
	int k = 1;
	int t = 1;

	HpDomain* d1 = hp_create(k, t);
	HpDomain* d2 = hp_create(k, t);
	HpDomain* d3 = hp_create(k, t);

	assert(d1 != d2);
	assert(d1 != d3);
	assert(d2 != d3);
	assert(hp_count_domains() == 3);

	hp_destroy();
	assert(hp_count_domains() == 0);
}

static void test_quick_sort() {
	printf("Quick sort test.\n");

	// value generator
	struct drand48_data seed;
	long int rng;

	int pcount = 10000;
	void** plist = (void**) malloc(sizeof(void*) * pcount);

	srand48_r(0, &seed);
	for(int i = 0; i < pcount; i++) {
		lrand48_r(&seed, &rng);
		uintptr_t val = (uintptr_t) rng;
		plist[i] = (void*) val;
	}

	quick_sort(plist, pcount);

	for(int i = 0; i < pcount; i++) {
		for(int j = 0; j < pcount; j++) {
			if(i < j) {
				assert(plist[i] <= plist[j]);
			} else if(i > j) {
				assert(plist[i] >= plist[j]);
			} else {
				assert(plist[i] == plist[j]);
			}
		}
	}
}

static void test_binary_search() {
	printf("Binary search test.\n");

	// value generator
	struct drand48_data seed;
	long int rng;

	int pcount = 10000;
	void** plist = (void**) malloc(sizeof(void*) * pcount);

	srand48_r(0, &seed);
	for(int i = 0; i < pcount; i++) {
		lrand48_r(&seed, &rng);
		uintptr_t val = (uintptr_t) rng;
		plist[i] = (void*) val;
	}

	quick_sort(plist, pcount);

	srand48_r(0, &seed);
	for(int i = 0; i < pcount; i++) {
		lrand48_r(&seed, &rng);
		uintptr_t val = (uintptr_t) rng;
		assert(binary_search(plist, pcount, (void*)val));
	}

	for(int i = 0; i < 100; i++) {
		lrand48_r(&seed, &rng);
		uintptr_t val = (uintptr_t) rng;

		if(!binary_search(plist, pcount, (void*)val)) {
			return;
		}
	}

	printf("Non inserted value matched by binary search.\n");
	exit(1);
}

