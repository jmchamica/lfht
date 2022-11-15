#include "common.c"
#include <unistd.h>

#define SEED_GROUP_COUNT 4
#define NUMA_CORES 16

//
// [  A  ] [  B  ] [  C  ] [  D  ]
//
// each range of keys has length = test_size
// each range of keys starts with its own seed
// A -> key space for insertion purposes
// B -> key space for searches not found
// C -> key space for removal
// D -> key space for searches found
//
struct drand48_data **nproc_seeds[SEED_GROUP_COUNT];
struct drand48_data **thread_seeds[SEED_GROUP_COUNT];

long nproc;
struct op_ratios operations;

void *pt_preremove(void *entry_point)
{
	int tid = ((intptr_t)entry_point);
	int nodes = test_size / nproc;

	if (tid == nproc - 1 && test_size % nproc != 0) {
		// add remaining nodes
		// if the test_size is not divisible by n_threads
		nodes += test_size % nproc;
	}

	struct drand48_data key_gen;

	// remove A and B groups
	for(int i = 0; i < 2; i++) {
		key_gen = *nproc_seeds[i][tid];

		for(int i = 0; i < nodes; i++) {
			size_t rng;
			lrand48_r(&key_gen, (long int *) &rng);
			size_t value = rng * GOLD_RATIO;

			lfht_remove(head, value, tid);
		}
	}

	return NULL;
}

void *pt_preinsert(void *entry_point)
{
	int tid = ((intptr_t)entry_point);
	int nodes = test_size / nproc;

	if (tid == nproc - 1 && test_size % nproc != 0) {
		// add remaining nodes
		// if the test_size is not divisible by n_threads
		nodes += test_size % nproc;
	}

	struct drand48_data key_gen;

	// add all groups
	for(int i = 0; i < SEED_GROUP_COUNT; i++) {

		key_gen = *nproc_seeds[i][tid];

		for(int i = 0; i < nodes; i++) {
			size_t rng;
			lrand48_r(&key_gen, (long int *) &rng);
			size_t value = rng * GOLD_RATIO;

			lfht_insert(head, value, (void*)value, tid);
		}
	}

	return NULL;
}

void *pt_seedrecord(void *entry_point)
{
	int gid = ((intptr_t)entry_point);
	struct drand48_data key_gen;

	srand48_r(gid, nproc_seeds[gid][0]);
	srand48_r(gid, thread_seeds[gid][0]);
	key_gen = *nproc_seeds[gid][0];

	int nproc_state_i = test_size / nproc;
	int thread_state_i = test_size / n_threads;

	// preinsert nodes
	for(int i = 0; i < test_size; i++) {
		// register intermediate state for all procs
		if (i > 0 && nproc_state_i > 0 && i % nproc_state_i == 0) {
			int tid = i / nproc_state_i;
			if (tid <= 0 || tid >= nproc) {
				continue;
			}
			*nproc_seeds[gid][tid] = key_gen;
		}

		// register intermediate state for bench threads
		if (i > 0 && thread_state_i > 0 && i % thread_state_i == 0) {
			int tid = i / thread_state_i;
			if (tid <= 0 || tid >= n_threads) {
				continue;
			}
			*thread_seeds[gid][tid] = key_gen;
		}

		size_t rng;
		lrand48_r(&key_gen, (long int *) &rng);
	}

	return NULL;
}

void *pt_bench(void *entry_point)
{
	int tid = ((intptr_t)entry_point);
	int nodes = test_size / n_threads;

	if (tid == nproc - 1 && test_size % nproc != 0) {
		// the last thread will treat the rest of the nodes
		// to prevent hash collisions
		nodes += test_size % nproc;
	}

	struct drand48_data seed_i = *thread_seeds[0][tid];
	struct drand48_data seed_m = *thread_seeds[1][tid];
	struct drand48_data seed_r = *thread_seeds[2][tid];
	struct drand48_data seed_s = *thread_seeds[3][tid];
	struct drand48_data op_select;
	size_t rng;
	size_t value;

	struct op_ratios ratios = operations;
	ratios.ci = 0;
	ratios.cr = 0;
	ratios.cs = 0;
	ratios.cm = 0;

	srand48_r(tid, &op_select);

	for(int i = 0; i < nodes; i++) {

		// for the throughput benchmark
		if(dtime > 0) {
			// thread 0 will check if time's up, and notify other threads
			// using the "terminate" global atomic variable
			if(tid == 0 && i % THROUGHPUT_TIME_PRECISION && duration() > dtime) {
				atomic_store_explicit(
						&terminate,
						1,
						memory_order_release);
				lfht_end_thread(head, tid);
				return NULL;
			}

			// other threads check termination status
			if(tid > 0 && atomic_load_explicit(
						&terminate,
						memory_order_relaxed) > 0) {
				lfht_end_thread(head, tid);
				return NULL;
			}
		}

		lrand48_r(&op_select, (long int *) &rng);

		if (ratios.cs < operations.rs * nodes && rng < ratios.ls) {
			if (ratios.cs >= operations.rs * nodes) {
				calibrate_ratios(&ratios, ratios.ri, ratios.rr, 0, ratios.rm);
			}
			ratios.cs++;

			lrand48_r(&seed_s, (long int *) &rng);
			value = rng * GOLD_RATIO;
			lfht_search(head, value, tid);
			continue;
		}

		if (ratios.cr < operations.rr * nodes && rng < ratios.lr) {
			if (ratios.cr >= operations.rr * nodes) {
				calibrate_ratios(&ratios, ratios.ri, 0, ratios.rs, ratios.rm);
			}
			ratios.cr++;

			lrand48_r(&seed_r, (long int *) &rng);
			value = rng * GOLD_RATIO;
			lfht_remove(head, value, tid);
			continue;
		}

		if (ratios.ci < operations.ri * nodes && rng < ratios.li) {
			if (ratios.ci >= operations.ri * nodes) {
				calibrate_ratios(&ratios, 0, ratios.rr, ratios.rs, ratios.rm);
			}
			ratios.ci++;

			lrand48_r(&seed_i, (long int *) &rng);
			value = rng * GOLD_RATIO;
			lfht_insert(head, value, (void*)value, tid);
			continue;
		}

		if (ratios.cm >= operations.rm * nodes) {
			i--;
			continue;
		}

		if (ratios.cm >= operations.rm * nodes) {
			calibrate_ratios(&ratios, ratios.ri, ratios.rr, ratios.rs, 0);
		}
		ratios.cm++;

		lrand48_r(&seed_m, (long int *) &rng);
		value = rng * GOLD_RATIO;
		lfht_search(head, value, tid);
	}

	lfht_end_thread(head, tid);
	return NULL;
}

int main(int argc, char **argv)
{
	if(argc < 8) {
		printf("usage: %s <nodes> <threads> <chain length> <hash size> <inserts> <removes> <searches found> <searches not found> [<time>]\n", argv[0]);
		return 1;
	}

	test_size = atoi(argv[1]);
	n_threads = atoi(argv[2]);
	size_t max_chain_nodes = atoi(argv[3]);
	size_t root_hash_size = atoi(argv[4]);
	size_t hash_size = root_hash_size;

	// settings the ranges for op selection
	// e.g. 50% inserts, 25% removes, 25% searches, 0% missmatches
	calibrate_ratios(
			&operations,
			atof(argv[5]),
			atof(argv[6]),
			atof(argv[7]),
			atof(argv[8]));

	if (argc > 9) {
		dtime = atof(argv[9]);
	}

	nproc = sysconf(_SC_NPROCESSORS_ONLN);
	// prevent scheduling of threads of different NUMA nodes when
	// using less n_threads than all cores of one node
	if (nproc > NUMA_CORES && n_threads <= NUMA_CORES) {
		nproc = NUMA_CORES;
	}

	if (n_threads > nproc) {
		fprintf(stderr, "Number of threads (%d) is higher than cores (%ld)\n", n_threads, nproc);
		return 1;
	}

	threads = malloc(nproc*sizeof(pthread_t));
	head = init_lfht_explicit(
			nproc,
			root_hash_size,
			hash_size,
			max_chain_nodes);

	for(int i = 0; i < nproc; i++) {
		lfht_init_thread(head);
	}

	if (test_size < nproc) {
		nproc = test_size;
	}

	for(int i = 0; i < SEED_GROUP_COUNT; i++) {
		// allocate a seed state recorder for each thread
		// operating in one of the 4 groups
		nproc_seeds[i] = malloc(nproc*sizeof(struct drand48_data *));
		for(int j = 0; j < nproc; j++) {
			nproc_seeds[i][j] = aligned_alloc(CACHE_SIZE, CACHE_SIZE);
		}

		thread_seeds[i] = malloc(n_threads*sizeof(struct drand48_data *));
		for(int j = 0; j < n_threads; j++) {
			thread_seeds[i][j] = aligned_alloc(CACHE_SIZE, CACHE_SIZE);
		}
	}

	// seed recorder
	printf("[Stage 1] Seed recorder...\n");
	for(int i = 0; i < SEED_GROUP_COUNT; i++) {
		pthread_create(&threads[i], NULL, pt_seedrecord, (void*)(intptr_t)i);
	}
	for(int i = 0; i < SEED_GROUP_COUNT; i++) {
		pthread_join(threads[i], NULL);
	}

	// insertion of groups
	printf("[Stage 2] Pre inserting...\n");
	for(int i = 0; i < nproc; i++) {
		pthread_create(&threads[i], NULL, pt_preinsert, (void*)(intptr_t)i);
	}
	for(int i = 0; i < nproc; i++) {
		pthread_join(threads[i], NULL);
	}

	// removal of "insertion" and "searches not found" groups
	printf("[Stage 3] Pre removing...\n");
	for(int i = 0; i < nproc; i++) {
		pthread_create(&threads[i], NULL, pt_preremove, (void*)(intptr_t)i);
	}
	for(int i = 0; i < nproc; i++) {
		pthread_join(threads[i], NULL);
	}

#if LFHT_STATS
	for(int i = 0; i < nproc; i++) {
		lfht_reset_stats(head, i);
	}
#endif

	printf("[Stage 4] Start test..\n");
	clock_gettime(CLOCK_MONOTONIC_RAW, &start_monoraw);
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_process);
	for(int i = 0; i < n_threads; i++) {
		pthread_create(&threads[i], NULL, pt_bench, (void*)(intptr_t)i);
	}
	for(int i = 0; i < n_threads; i++) {
		pthread_join(threads[i], NULL);
	}
	clock_gettime(CLOCK_MONOTONIC_RAW, &end_monoraw);
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_process);

	print_stats();
	//free_lfht(head);

	return 0;
}
