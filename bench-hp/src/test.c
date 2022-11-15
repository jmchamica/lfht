#include "common.c"

int main(int argc, char **argv)
{
	if(argc < 3) {
		printf("usage: %s <test number (1-23)> <cores>\n", argv[0]);
		return 1;
	}

	int select = atoi(argv[1]);
	n_threads = atoi(argv[2]);

	size_t max_chain_nodes = 4;
	size_t root_hash_size = 4;
	size_t hash_size = 4;
	size_t inserts = 34;
	size_t removes = 34;
	size_t searches = 34;
	size_t total = inserts + removes + searches;

	test_size = 5000000;
	limit_sf = LRAND_MAX * searches / total;
	limit_r = limit_sf + LRAND_MAX * removes / total;
	limit_i = limit_r + LRAND_MAX * inserts / total;

	threads = malloc(n_threads*sizeof(pthread_t));
	seed = malloc(n_threads*sizeof(struct drand48_data *));
	for(int i = 0; i < n_threads; i++) {
		// initialize seeds for each thread
		seed[i] = aligned_alloc(CACHE_SIZE, CACHE_SIZE);
	}

	switch(select) {
	case 1:
		printf("%d. Single threaded, load map and check all inserted nodes... ", select);
		n_threads = 1;

		head = init_lfht_explicit(
				n_threads,
				root_hash_size,
				hash_size,
				max_chain_nodes);

		srand48_r(0, seed[0]);

		clock_gettime(CLOCK_MONOTONIC_RAW, &start_monoraw);
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_process);

		// load map and check nodes
		for(int i = 0; i < test_size; i++){
			size_t rng;
			size_t value;
			lrand48_r(seed[0], (long int *) &rng);
			value = rng * GOLD_RATIO;

			lfht_insert(head, value, (void*)value, 0);
			if((size_t)lfht_search(head, value, 0) != value) {
				printf("Failed\nTried to insert a node <%016lX> but can't find it.\n", value);
				exit(1);
			}
		}

		clock_gettime(CLOCK_MONOTONIC_RAW, &end_monoraw);
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_process);

		if (test_size > 0 && is_map_empty(head)) {
			printf("Failed\nTried to add nodes to map but map is empty.\n");
			exit(1);
		}

		assert_map_state(head, all_flags & ~valid_nodes_flag & ~expanded_flag);
		break;
	
	case 2:
		printf("%d. Single threaded, remove all nodes and check number of hash levels... ", select);
		n_threads = 1;

		head = init_lfht_explicit(
				n_threads,
				root_hash_size,
				hash_size,
				max_chain_nodes);
		srand48_r(0, seed[0]);

		clock_gettime(CLOCK_MONOTONIC_RAW, &start_monoraw);
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_process);

		load_map(head, test_size, 0, seed[0]);

		// remove all and check if map is empty
		srand48_r(0, seed[0]);
		remove_all(head, seed[0], test_size, 0);

		clock_gettime(CLOCK_MONOTONIC_RAW, &end_monoraw);
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_process);

		assert_map_state(head, all_flags);
		break;
	
	case 3:
		printf("%d. Single threaded, load two maps with the same seed and check for equality... ", select);
		n_threads = 1;

		clock_gettime(CLOCK_MONOTONIC_RAW, &start_monoraw);
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_process);

		// init map 1
		head = init_lfht_explicit(
				n_threads,
				root_hash_size,
				hash_size,
				max_chain_nodes);
		srand48_r(0, seed[0]);
		load_map(head, test_size, 0, seed[0]);

		// init map 2
		struct lfht_head *aux = init_lfht_explicit(
				n_threads,
				root_hash_size,
				hash_size,
				max_chain_nodes);
		srand48_r(0, seed[0]);
		load_map(aux, test_size, 0, seed[0]);

		clock_gettime(CLOCK_MONOTONIC_RAW, &end_monoraw);
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_process);

		// compare maps
		if (!are_maps_equal(head, aux)) {
			printf("Failed\nMaps loaded with the same keys but are different.\n");
			exit(1);
		}
		break;
	
	case 4:
		printf("%d. Multi threaded, load map and check all inserted nodes... ", select);

		head = init_lfht_explicit(
				n_threads,
				root_hash_size,
				hash_size,
				max_chain_nodes);

		for(int i=0; i < n_threads; i++){
			lfht_init_thread(head, i);
		}

		clock_gettime(CLOCK_MONOTONIC_RAW, &start_monoraw);
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_process);
		for(int i=0; i < n_threads; i++){
			srand48_r(i, seed[i]);
			pthread_create(&threads[i], NULL, pt_load_map, (void*)(intptr_t)i);
		}
		for(int i=0; i < n_threads; i++){
			pthread_join(threads[i], NULL);
		}
		clock_gettime(CLOCK_MONOTONIC_RAW, &end_monoraw);
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_process);

		if (test_size > 0 && is_map_empty(head)) {
			printf("Failed\nTried to add nodes to map but map is empty.\n");
			exit(1);
		}

		for (int i=0; i < n_threads; i++) {
			srand48_r(i, seed[i]);
			if (are_all_keys_inserted(head, test_size/n_threads, seed[i])) {
				continue;
			}
			printf("Failed\nTried to insert a node but can't find it.\n");
			exit(1);
		}

		assert_map_state(head, all_flags & ~valid_nodes_flag & ~expanded_flag);
		break;
	
	case 5:
		printf("%d. Multi threaded, remove all nodes and check number of hash levels... ", select);

		head = init_lfht_explicit(
				n_threads,
				root_hash_size,
				hash_size,
				max_chain_nodes);

		for(int i=0; i < n_threads; i++){
			lfht_init_thread(head, i);
		}

		clock_gettime(CLOCK_MONOTONIC_RAW, &start_monoraw);
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_process);

		// load map
		for(int i=0; i < n_threads; i++){
			srand48_r(i, seed[i]);
			pthread_create(&threads[i], NULL, pt_load_map, (void*)(intptr_t)i);
		}
		for(int i=0; i < n_threads; i++){
			pthread_join(threads[i], NULL);
		}
		assert_map_state(head, all_flags & ~valid_nodes_flag & ~expanded_flag);

		// remove all and check if map is empty
		for(int i=0; i < n_threads; i++){
			srand48_r(i, seed[i]);
			pthread_create(&threads[i], NULL, pt_remove_all, (void*)(intptr_t)i);
		}
		for(int i=0; i < n_threads; i++){
			pthread_join(threads[i], NULL);
		}
		clock_gettime(CLOCK_MONOTONIC_RAW, &end_monoraw);
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_process);

		assert_map_state(head, all_flags);
		break;

	case 6:
		printf("%d. Multi threaded, load two maps with the same seed and check for equality... ", select);

		// init map 1
		head = init_lfht_explicit(
				n_threads,
				root_hash_size,
				hash_size,
				max_chain_nodes);

		for(int i=0; i < n_threads; i++){
			lfht_init_thread(head, i);
		}

		clock_gettime(CLOCK_MONOTONIC_RAW, &start_monoraw);
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_process);

		for(int i=0; i < n_threads; i++){
			srand48_r(i, seed[i]);
			pthread_create(&threads[i], NULL, pt_load_map, (void*)(intptr_t)i);
		}
		for(int i=0; i < n_threads; i++){
			pthread_join(threads[i], NULL);
		}

		// init map 2
		aux = head;
		head = init_lfht_explicit(
				n_threads,
				root_hash_size,
				hash_size,
				max_chain_nodes);
		for(int i=0; i < n_threads; i++){
			lfht_init_thread(head, i);
		}

		for(int i=0; i < n_threads; i++){
			srand48_r(i, seed[i]);
			pthread_create(&threads[i], NULL, pt_load_map, (void*)(intptr_t)i);
		}
		for(int i=0; i < n_threads; i++){
			pthread_join(threads[i], NULL);
		}
		clock_gettime(CLOCK_MONOTONIC_RAW, &end_monoraw);
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_process);

		// compare maps
		if (!are_maps_equal(head, aux)) {
			printf("Failed\nMaps loaded with the same keys but are different.\n");
			exit(1);
		}
		break;

	case 7:
		printf("%d. Single threaded, check duplicate keys... ", select);
		n_threads = 1;
		head = init_lfht_explicit(
				n_threads,
				root_hash_size,
				hash_size,
				max_chain_nodes);

		clock_gettime(CLOCK_MONOTONIC_RAW, &start_monoraw);
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_process);
		size_t key = 0;
		for (int i = 0; i < 100; i++) {
			lfht_insert(head, key, (void*)key, 0);
		}
		clock_gettime(CLOCK_MONOTONIC_RAW, &end_monoraw);
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_process);

		assert_map_state(head, all_flags & ~valid_nodes_flag);

		int size = map_size(head);
		if (size < 1) {
			printf("Failed\nCould not add a node.\n");
			exit(1);
		}
		if (size > 1) {
			printf("Failed\nAdding the same key pair resulted in duplicate nodes.\n");
			exit(1);
		}
		break;

	case 8:
		printf("%d. Single threaded, add contiguous keys and check number of levels... ", select);
		test_size = 50;
		n_threads = 1;
		root_hash_size = 2;
		hash_size = 2;
		srand48_r(0, seed[0]);

		size_t* keys = shuffle(0, test_size, test_size/contention, seed[0]);

		// init map 1
		head = init_lfht_explicit(
				n_threads,
				root_hash_size,
				hash_size,
				max_chain_nodes);

		clock_gettime(CLOCK_MONOTONIC_RAW, &start_monoraw);
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_process);
		for(int i = 0; i < test_size; i++){
			size_t v = (intptr_t)keys[i];
			lfht_insert(head, v, (void*)v, 0);
		}

		if (test_size > 0 && is_map_empty(head)) {
			printf("Failed\nTried to add nodes to map but map is empty.\n");
			exit(1);
		}

		// some chains should have expanded to newer levels
		if (!is_expanded(head)) {
			printf("Failed\nAdded %d nodes but no chain expanded.\n", test_size);
			exit(1);
		}

		for(int i = 0; i < test_size; i++){
			size_t v = (intptr_t)keys[i];
			lfht_remove(head, v, 0);
		}
		clock_gettime(CLOCK_MONOTONIC_RAW, &end_monoraw);
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_process);

		assert_map_state(head, all_flags);
		break;

	case 9:
		printf("%d. Multi threaded, high contention add/remove with contiguous keys... ", select);

		// init map 1
		test_size = 5000000;
		contention = 500000;
		head = init_lfht_explicit(
				n_threads,
				root_hash_size,
				hash_size,
				max_chain_nodes);

		for(int i=0; i < n_threads; i++){
			lfht_init_thread(head, i);
		}

		clock_gettime(CLOCK_MONOTONIC_RAW, &start_monoraw);
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_process);
		for(int i=0; i < n_threads; i++){
			srand48_r(i, seed[i]);
			pthread_create(&threads[i], NULL, pt_contiguous_add_remove, (void*)(intptr_t)i);
		}
		for(int i=0; i < n_threads; i++){
			pthread_join(threads[i], NULL);
		}
		clock_gettime(CLOCK_MONOTONIC_RAW, &end_monoraw);
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_process);

		assert_map_state(head, all_flags);
		break;

	case 10:
		printf("%d. Multi threaded, lower contention add/remove with contiguous keys... ", select);

		// init map 1
		hash_size = 2;
		test_size = 5000000;
		head = init_lfht_explicit(
				n_threads,
				root_hash_size,
				hash_size,
				max_chain_nodes);

		for(int i=0; i < n_threads; i++){
			lfht_init_thread(head, i);
		}

		clock_gettime(CLOCK_MONOTONIC_RAW, &start_monoraw);
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_process);
		for(int i=0; i < n_threads; i++){
			srand48_r(i, seed[i]);
			pthread_create(&threads[i], NULL, pt_contiguous_add_remove, (void*)(intptr_t)i);
		}
		for(int i=0; i < n_threads; i++){
			pthread_join(threads[i], NULL);
		}
		clock_gettime(CLOCK_MONOTONIC_RAW, &end_monoraw);
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_process);

		assert_map_state(head, all_flags);
		break;

	case 11:
		printf("%d. Multi threaded, add/remove/lookup with random keys... ", select);

		// init map 1
		head = init_lfht_explicit(
				n_threads,
				root_hash_size,
				hash_size,
				max_chain_nodes);

		for(int i=0; i < n_threads; i++){
			lfht_init_thread(head, i);
		}

		clock_gettime(CLOCK_MONOTONIC_RAW, &start_monoraw);
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_process);
		for(int i=0; i < n_threads; i++){
			srand48_r(i, seed[i]);
			pthread_create(&threads[i], NULL, pt_random, (void*)(intptr_t)i);
		}
		for(int i=0; i < n_threads; i++){
			pthread_join(threads[i], NULL);
		}
		clock_gettime(CLOCK_MONOTONIC_RAW, &end_monoraw);
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_process);

		assert_map_state(head, all_flags & ~valid_nodes_flag & ~expanded_flag);
		break;

	case 12:
		printf("%d. Single threaded, remove ... ", select);

		n_threads = 1;
		root_hash_size = 4;
		hash_size = 4;
		srand48_r(0, seed[0]);

		// init map 1
		head = init_lfht_explicit(
				n_threads,
				root_hash_size,
				hash_size,
				max_chain_nodes);

		for(int i=0; i < n_threads; i++){
			lfht_init_thread(head, i);
		}

		clock_gettime(CLOCK_MONOTONIC_RAW, &start_monoraw);
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_process);
		size_t total_size = 100000;
		test_size = total_size;

		for(size_t i = 0; i < (size_t)test_size; i++){
			lfht_insert(head, i, (void*)i, 0);
		}

		for(; test_size > 0; test_size -= 100) {

			for(size_t i = 0; i < (size_t)total_size; i++){
				void* res = lfht_search(head, i, 0);

				if(i <= (size_t)test_size && res && i != (size_t)res) {
					printf("NOT FOUND!\n");
					exit(1);
				}

				if (i > (size_t)test_size && res) {
					printf("NOT REMOVED %zu/%zu!\n", (size_t)res, (size_t)test_size);
					exit(1);
				}
			}

			for(size_t i = test_size-100; i < (size_t)test_size; i++){
				lfht_remove(head, i, 0);
			}

		}
		clock_gettime(CLOCK_MONOTONIC_RAW, &end_monoraw);
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_process);

		assert_map_state(head, all_flags);
		break;

	default:
		fprintf(stderr, "No such test %d\n", select);
		return 1;
	}

	printf("OK\n");

	print_stats();

	free_lfht(head);
	return 0;
}
