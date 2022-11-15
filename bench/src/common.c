#include <lfht.c>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>

#if LFHT_DEBUG
#include "print_graph.h"
#endif

#define GOLD_RATIO 11400714819323198485ULL
#define LRAND_MAX (1ULL<<31)
#define THROUGHPUT_TIME_PRECISION 100

// thread global variables
struct op_ratios {
	// from 0 to 2**48
	// they allow for use with the RNG as an
	// operation selector
	size_t ls;
	size_t lr;
	size_t li;
	
	// percentages from 0 to 1
	// their sum should give 1
	double rs;
	double rr;
	double ri;
	double rm;

	// operation counters
	int ci;
	int cr;
	int cs;
	int cm;
};

size_t limit_sf;
size_t limit_r;
size_t limit_i;
int test_size;
int n_threads;
int contention = 1;
pthread_t *threads;
struct drand48_data **seed;
struct timespec start_monoraw;
struct timespec end_monoraw;
struct timespec start_process;
struct timespec end_process;
double dtime = -1;
_Atomic(int) terminate = 0;

struct lfht_head *head;

// descriptive final state of the LFHT
const int uncompressed_hashes_flag = 1;
const int freeze_nodes_flag = 1<<2;
const int unfreeze_nodes_flag = 1<<3;
const int expanded_flag = 1<<4;
const int invalid_nodes_flag = 1<<5;
const int valid_nodes_flag = 1<<6;
const int cycles_flag = 1<<7;
const int unfinished_expansion = 1<<8;
const int null_node = 1<<9;
const int any_flags = -1;
const int all_flags = uncompressed_hashes_flag |
	freeze_nodes_flag |
	unfreeze_nodes_flag |
	expanded_flag |
	invalid_nodes_flag |
	valid_nodes_flag |
	cycles_flag |
	unfinished_expansion |
	null_node;

void calibrate_ratios(
		struct op_ratios* ratios,
		double ri,
		double rr,
		double rs,
		double rm) {
	ratios->ri = ri;
	ratios->rr = rr;
	ratios->rs = rs;
	ratios->rm = rm;

	double total = ratios->ri + ratios->rr + ratios->rs + ratios->rm;
	if (total != 1) {
		int zeros = 0;
		if (ri <= 0) zeros++;
		if (rr <= 0) zeros++;
		if (rs <= 0) zeros++;
		if (rm <= 0) zeros++;

		if (zeros >= 4) {
			printf("EVERY COUNTER AT ZERO\n");
			exit(1);
		}

		if (zeros <= 0) {
			printf("UNbalanced ratios\n");
			exit(1);
		}
		
		double div = (double)(1-(ri+rr+rs+rm)) / (double)(4 - zeros);
		ratios->ri = ri <= 0 ? 0 : ri + div;
		ratios->rr = rr <= 0 ? 0 : rr + div;
		ratios->rs = rs <= 0 ? 0 : rs + div;
		ratios->rm = rm <= 0 ? 0 : rm + div;
	}

	total = ratios->ri + ratios->rr + ratios->rs + ratios->rm;
	if (total != 1) {
		printf("UNbalanced ratios\n");
		exit(1);
	}

	ratios->ls = ratios->rs * LRAND_MAX;
	ratios->lr = ratios->ls + ratios->rr * LRAND_MAX;
	ratios->li = ratios->rm <= 0 ? LRAND_MAX : ratios->lr + ratios->ri * LRAND_MAX;
}

// auxiliary structure used for graph DFS
// avoiding recursion
struct stack {
	int head;
	int size;
	struct lfht_node **buffer;
};

struct stack* init_stack() {
	struct stack* res = malloc(sizeof(struct stack));
	res->head = -1;
	res->size = 100;
	res->buffer = malloc(res->size*sizeof(struct lfht_node*));
	return res;
}

void free_stack(struct stack* s) {
	free(s->buffer);
	free(s);
}

void push(struct stack* s, struct lfht_node *n) {
	if (s->head + 1 >= s->size) {
		s->size *= 2;
		s->buffer = realloc(s->buffer, s->size*sizeof(struct lfht_node*));
	}
	s->buffer[++s->head] = n;
}

struct lfht_node* pop(struct stack* s) {
	if (s->head < 0) {
		return NULL;
	}
	return s->buffer[s->head--];
}

void print_stats() {
	double time = end_process.tv_sec - start_process.tv_sec + ((end_process.tv_nsec - start_process.tv_nsec)/1000000000.0);
	fprintf(stderr, "Process Time (s): %lf\n", time);
	time = end_monoraw.tv_sec - start_monoraw.tv_sec + ((end_monoraw.tv_nsec - start_monoraw.tv_nsec)/1000000000.0);
	fprintf(stderr, "Real Time (s): %lf\n", time);
	fprintf(stderr, "Cores: %d\n", n_threads);
	fprintf(stderr, "Test size: %d\n", test_size);

#if LFHT_STATS
	// statistics
	int compression_counter = 0;
	int compression_rollback_counter = 0;
	int expansion_counter = 0;
	int unfreeze_counter = 0;
	int freeze_counter = 0;
	int operations = 0;
	int api_calls = 0;
	int inserts = 0;
	int removes = 0;
	int searches = 0;
	unsigned long max_retry_counter = 0;
	int max_depth = 0;
	unsigned long paths = 0;
	unsigned long lookups = 0;
	int hashes = 0;
	unsigned long memory = 0;

	for(int i=0; i < n_threads; i++){
		memory += head->stats[i]->memory_alloc - head->stats[i]->memory_free;
		compression_counter += head->stats[i]->compression_counter;
		compression_rollback_counter += head->stats[i]->compression_rollback_counter;
		expansion_counter += head->stats[i]->expansion_counter;
		unfreeze_counter += head->stats[i]->unfreeze_counter;
		freeze_counter += head->stats[i]->freeze_counter;
		max_retry_counter += head->stats[i]->max_retry_counter;
		operations += head->stats[i]->operations;
		api_calls += head->stats[i]->api_calls;
		inserts += head->stats[i]->inserts;
		removes += head->stats[i]->removes;
		searches += head->stats[i]->searches;
		if (head->stats[i]->max_depth > max_depth) {
			max_depth = head->stats[i]->max_depth;
		}
		paths += head->stats[i]->paths;
		lookups += head->stats[i]->lookups;

		if(n_threads > 1) {
			fprintf(stderr, "Thread %d Real Time (s): %lf\n", i, head->stats[i]->term.tv_sec - start_monoraw.tv_sec + (head->stats[i]->term.tv_nsec - start_monoraw.tv_nsec) / 1000000000.0);
		}
	}

	fprintf(stderr, "Operations: %d\n", operations);
	fprintf(stderr, "Calls: %d\n", api_calls);
	fprintf(stderr, "Retries: %ld\n", max_retry_counter - operations);
	fprintf(stderr, "Inserts: %f%%\n", ((double)inserts/(double)api_calls)*100.0f);
	fprintf(stderr, "Removes: %f%%\n", ((double)removes/(double)api_calls)*100.0f);
	fprintf(stderr, "Searches: %f%%\n", ((double)searches/(double)api_calls)*100.0f);
	fprintf(stderr, "Throughput (ops/s): %lf\n", ((double)api_calls/time));
	fprintf(stderr, "Unfrozen: %d\n", unfreeze_counter);
	fprintf(stderr, "Frozen: %d\n", freeze_counter);
	fprintf(stderr, "Compressed: %d\n", compression_counter);
	fprintf(stderr, "Compressions rolledback: %d\n", compression_rollback_counter);
	fprintf(stderr, "Expanded: %d\n", expansion_counter);

	double fail_rate = operations > 0 && max_retry_counter > 0
		? 1-((double)operations)/((double)max_retry_counter)
		: 0;
	fprintf(stderr, "Failure ratio: %lf\n", fail_rate);

	fprintf(stderr, "Max depth: %d\n", max_depth);
	fprintf(stderr, "Paths Traversed: %lu\n", paths);
	fprintf(stderr, "Hash node count: %d\n", hashes);

	fprintf(stderr, "Lookups: %ld\n", lookups);
	fprintf(stderr, "Average path length: %lf\n", lookups > 0 && paths > 0 ? (double)paths/(double)lookups : 0);
	fprintf(stderr, "Memory: %lu\n", memory);

	if(n_threads <= 1) {
		return;
	}

	double min = -1.0;
	double max = -1.0;
	for(int i=0; i < n_threads; i++){
		double ttime = head->stats[i]->term.tv_sec - start_monoraw.tv_sec + (head->stats[i]->term.tv_nsec - start_monoraw.tv_nsec) / 1000000000.0;
		if (min < 0) {
			min = ttime;
		}

		if (max < 0) {
			max = ttime;
		}

		if (min > ttime) {
			min = ttime;
		}

		if (max < ttime) {
			max = ttime;
		}
	}

	fprintf(stderr, "Thread Termination Delta Time (s): %lf\n", max - min);

#endif
}

double duration() {
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC_RAW, &now);
	return now.tv_sec - start_monoraw.tv_sec + ((now.tv_nsec - start_monoraw.tv_nsec)/1000000000.0);
}

// e.g. 00011 -> 11000 = 3 shifts needed to slide value to its left most bound
int shift_count_to_slide_bit_string_left(size_t maximum_value) {
	if(maximum_value <= 0) {
		return 0;
	}

	size_t i = maximum_value;
	int j = 1;
	while(1) {
		i = i << 1;

		if(i&((size_t)1<<63) || i <= 0) {
			// if "i" is shifted all the way to the left
			// or if it is 0
			break;
		}
		j++;
	}

	// the amount of shifts needed to slide bit string left
	return j;
}

size_t* shuffle(size_t from, size_t n, size_t maximum_value, struct drand48_data *seed)
{
	if (n < 1) {
		return NULL;
	}

	int shift = shift_count_to_slide_bit_string_left(maximum_value);

	size_t i;
	size_t* a = (size_t*) malloc(n*sizeof(size_t));
	for (i = 0; i < n; i++) {
		a[i] = (from + i)<<shift;
	}

	for (i = 0; i < n - 1; i++) {
		size_t rng;
		lrand48_r(seed, (long int *) &rng);

		size_t j = i + rng * GOLD_RATIO / (LRAND_MAX / (n - i) + 1);
		j = j % n;
		size_t t = a[j];
		a[j] = a[i];
		a[i] = t;
	}

	return a;
}

int are_hashes_equal(struct lfht_node *h1, struct lfht_node *h2)
{
	if (h1->hash.size != h2->hash.size) {
		return 0;
	}

	for(int i = 0; i < 1<<h1->hash.size; i++) {
		if (h1->hash.hash_pos != h2->hash.hash_pos) {
			fprintf(stderr, "Hashes are on different levels H1:%d H2:%d\n", h1->hash.hash_pos, h2->hash.hash_pos);
			return 0;
		}

		struct lfht_node* b1 = valid_ptr(h1->hash.array[i]);
		struct lfht_node* b2 = valid_ptr(h2->hash.array[i]);
		if (b1->type != b2->type) {
			fprintf(stderr, "Buckets differ in type B1:%d B2:%d\n", b1->type, b2->type);
			return 0;
		}

		struct lfht_node* nxt1 = b1;
		struct lfht_node* nxt2 = b2;

		while(nxt1->type != HASH) {
			if (nxt1->type != nxt2->type) {
				fprintf(stderr, "Nodes differ in type N1:%d N2:%d\n", nxt1->type, nxt2->type);
				return 0;
			}

			if (nxt1->type == LEAF) {
				// nodes might not be sorted in sister chains
				// traverse chain of map2 looking for node os map1
				struct lfht_node* chain2 = b2;
				int found = 0;
				while (chain2->type != HASH) {
					if (chain2->type == LEAF && chain2->leaf.hash == nxt1->leaf.hash) {
						found = 1;
						break;
					}
					chain2 = valid_ptr(chain2->leaf.next);
				}
				if (!found) {
					fprintf(stderr, "Node <%016lX> of map 1 not found in sister chain of map 2 (Level %d, Bucket %d)).\n", nxt1->leaf.hash, h1->hash.hash_pos/h1->hash.size, i);
					return 0;
				}
			}

			nxt1 = valid_ptr(nxt1->leaf.next);
			nxt2 = valid_ptr(nxt2->leaf.next);
		}

		if ((nxt1 == h1 && nxt2 != h2) || (nxt1 != h1 && nxt2 == h2)) {
			fprintf(stderr, "Tail nodes point to different hash levels.\n");
			return 0;
		}

		if (nxt1 != h1 && !are_hashes_equal(nxt1, nxt2)) {
			return 0;
		}
	}
	return 1;
}

int are_maps_equal(struct lfht_head *h1, struct lfht_head *h2)
{
	struct lfht_node* r1 = h1->entry_hash;
	struct lfht_node* r2 = h2->entry_hash;
	return are_hashes_equal(r1, r2);
}

int hash_size(struct lfht_node *hnode)
{
	int res = 0;
	struct stack* dfs = init_stack();
	push(dfs, hnode);

start: ;
	struct lfht_node *root = pop(dfs);
	if (!root) {
		free_stack(dfs);
		return res;
	}

	for(int i = 0; i < 1<<root->hash.size; i++) {
		struct lfht_node* nxt = root->hash.array[i];

		if (nxt == root) {
			continue;
		}

		while (nxt->type != HASH) {
			if (nxt->type == LEAF && !is_invalid(nxt->leaf.next)) {
				res++;
			}
			nxt = valid_ptr(nxt->leaf.next);
		}

		if (nxt == root) {
			continue;
		}

		push(dfs, nxt);
	}

	goto start;
}

int map_size(struct lfht_head *head)
{
	struct lfht_node* root = head->entry_hash;
	return hash_size(root);
}

int examine_hash_state(struct lfht_node *hnode, int flags)
{
	int res = 0;
	struct stack* dfs = init_stack();
	push(dfs, hnode);

start: ;
	struct lfht_node *root = pop(dfs);
	if (!root || (res & flags) == flags) {
		free_stack(dfs);
		return res;
	}

	if (root != hnode) {
		res |= expanded_flag;
	}

	for(int i = 0; i < 1<<root->hash.size; i++) {
		struct lfht_node* nxt = valid_ptr(root->hash.array[i]);

		if (!nxt) {
			res |= null_node;
			continue;
		}

		if (nxt == root) {
			continue;
		}

		while (nxt->type != HASH) {
			if (nxt->type == FREEZE) {
				res |= freeze_nodes_flag;
			} else if (nxt->type == UNFREEZE) {
				res |= unfreeze_nodes_flag;
			} else if (is_invalid(nxt->leaf.next)) {
				res |= invalid_nodes_flag;
			} else {
				res |= valid_nodes_flag;
			}

			nxt = valid_ptr(nxt->leaf.next);
			if (!nxt) {
				break;
			}
		}

		if (!nxt) {
			res |= null_node;
			continue;
		}

		if (nxt == root) {
			continue;
		}

		if (nxt->hash.hash_pos <= root->hash.hash_pos) {
			res |= cycles_flag;
			continue;
		}

		push(dfs, nxt);
	}

	goto start;
}

int is_map_empty(struct lfht_head *head)
{
	struct lfht_node* root = head->entry_hash;
	return examine_hash_state(root, all_flags) == 0;
}

void assert_map_state(struct lfht_head *head, int flags) {
	int state = examine_hash_state(head->entry_hash, flags);

	int fail = 0;
	if (flags & expanded_flag && state & expanded_flag) {
		if (!fail) {
			fail = 1;
			printf("Failed\n");
		}
		printf("Expected no expansions but map is expanded.\n");
	}

	if (flags & uncompressed_hashes_flag && state & uncompressed_hashes_flag) {
		if (!fail) {
			fail = 1;
			printf("Failed\n");
		}
		printf("Found uncompressed empty hashes.\n");
	}

	if (flags & freeze_nodes_flag && state & freeze_nodes_flag) {
		if (!fail) {
			fail = 1;
			printf("Failed\n");
		}
		printf("Found freeze nodes left on map.\n");
	}

	if (flags & unfreeze_nodes_flag && state & unfreeze_nodes_flag) {
		if (!fail) {
			fail = 1;
			printf("Failed\n");
		}
		printf("Found unfreeze nodes left on map.\n");
	}

	if (flags & invalid_nodes_flag && state & invalid_nodes_flag) {
		if (!fail) {
			fail = 1;
			printf("Failed\n");
		}
		printf("Found invalid nodes left on map.\n");
	}

	if (flags & valid_nodes_flag && state & valid_nodes_flag) {
		if (!fail) {
			fail = 1;
			printf("Failed\n");
		}
		printf("Found nodes left on map.\n");
	}

	if (flags & cycles_flag && state & cycles_flag) {
		if (!fail) {
			fail = 1;
			printf("Failed\n");
		}
		printf("Found cycles in map.\n");
	}

	if (flags & unfinished_expansion && state & unfinished_expansion) {
		if (!fail) {
			fail = 1;
			printf("Failed\n");
		}
		printf("Found unfinished expansion of a hash.\n");
	}

	if (flags & null_node && state & null_node) {
		if (!fail) {
			fail = 1;
			printf("Failed\n");
		}
		printf("Found a null node.\n");
	}

	if (fail) {
		exit(1);
	}
}

// check if hash has any bucket which points to a
// deeper level hash
int is_expanded(struct lfht_head *head)
{
	struct lfht_node* root = head->entry_hash;
	for(int i = 0; i < 1<<root->hash.size; i++) {
		struct lfht_node* nxt = root->hash.array[i];
		if (nxt != root && nxt->type == HASH) {
			return 1;
		}
	}
	return 0;
}

void remove_all(struct lfht_head *head, struct drand48_data *seed, int count, int tid)
{
	for(int i = 0; i < count; i++){
		size_t rng;
		size_t value;
		lrand48_r(seed, (long int *) &rng);
		value = rng * GOLD_RATIO;

		lfht_remove(head, value, tid);
	}
}

int are_all_keys_inserted(struct lfht_head *head, int nodes, void *seed)
{
	for(int i = 0; i < nodes; i++){
		size_t rng;
		size_t value;
		lrand48_r(seed, (long int *) &rng);
		value = rng * GOLD_RATIO;

		if((size_t)lfht_search(head, value, 0) != value) {
			fprintf(stderr, "<%016lX> key not found in map.\n", value);
			return 0;
		}
	}
	return 1;
}

void load_map(struct lfht_head *head, int nodes, int tid, void *seed)
{
	for(int i = 0; i < nodes; i++){
		size_t rng;
		size_t value;
		lrand48_r(seed, (long int *) &rng);
		value = rng * GOLD_RATIO;

		lfht_insert(head, value, (void*)value, tid);
	}
}

void load_all_map(struct lfht_head *head, struct drand48_data **seed)
{
	int nodes = test_size/n_threads;
	for (int i=0; i < n_threads; i++) {
		srand48_r(i, seed[i]);
		load_map(head, nodes, i, seed[i]);
		srand48_r(i, seed[i]);
	}
}

void *pt_contiguous_remove_all(void *entry_point)
{
	int tid = ((intptr_t)entry_point);
	struct drand48_data *s = seed[tid];
	size_t* keys = shuffle(0, test_size, test_size/contention, s);

	for(int i = 0; i < test_size; i++){
		size_t v = (intptr_t)keys[i];
		lfht_remove(head, v, tid);
	}

	free(keys);
	lfht_end_thread(head, tid);
	return NULL;
}

void *pt_contiguous_add_remove_timed(void *entry_point)
{
	int tid = ((intptr_t)entry_point);
	struct drand48_data *s = seed[tid];
	size_t* keys = shuffle(0, test_size, test_size/contention, s);

	for(;;) {
		for(int i = 0; i < test_size; i++){
			if(tid == 0 && dtime > 0 && i % THROUGHPUT_TIME_PRECISION && duration() > dtime) {
				free(keys);
				atomic_store_explicit(
						&terminate,
						1,
						memory_order_release);
				lfht_end_thread(head, tid);
				pthread_exit(NULL);
			}

			if(tid > 0) {
				if (atomic_load_explicit(
					&terminate,
					memory_order_relaxed) > 0) {
					lfht_end_thread(head, tid);
					pthread_exit(NULL);
				}
			}

			size_t rng;
			lrand48_r(s, (long int *) &rng);

			size_t v = (intptr_t)keys[i];
			if(rng % 2) {
				lfht_insert(head, v, (void*)v, tid);
				continue;
			}
			lfht_remove(head, v, tid);
		}
	}
	return NULL;
}

void *pt_contiguous_add_remove(void *entry_point)
{
	int tid = ((intptr_t)entry_point);
	struct drand48_data *s = seed[tid];
	size_t* keys = shuffle(0, test_size, test_size/contention, s);

	for(int i = 0; i < test_size; i++){
		size_t v = (intptr_t)keys[i];
		lfht_insert(head, v, (void*)v, tid);
	}

	for(int j = 0; j < 5; j++) {
		for(int i = 0; i < test_size; i++){
			size_t rng;
			lrand48_r(s, (long int *) &rng);

			size_t v = (intptr_t)keys[i];
			if(rng % 2) {
				lfht_insert(head, v, (void*)v, tid);
				continue;
			}
			lfht_remove(head, v, tid);
		}
	}

	for(int i = 0; i < test_size; i++){
		size_t v = (intptr_t)keys[i];
		lfht_remove(head, v, tid);
	}

	free(keys);
	lfht_end_thread(head, tid);
	return NULL;
}

void *pt_random_load(void *entry_point)
{
	int tid = ((intptr_t)entry_point);
	struct drand48_data *s = seed[tid];
	int nodes = test_size/n_threads;
	for(int i = 0; i < nodes; i++){
		size_t rng;
		size_t value;
		lrand48_r(s, (long int *) &rng);
		value = rng * GOLD_RATIO;

		if(rng < limit_r) {
			lfht_insert(head, value, (void*)value, tid);
		}
	}

	lfht_end_thread(head, tid);
	return NULL;
}

void *pt_random_timed(void *entry_point)
{
	int tid = ((intptr_t)entry_point);
	struct drand48_data *s = seed[tid];

	unsigned int i = 0;
	for (;;){
		i++;
		size_t rng;
		size_t value;
		lrand48_r(s, (long int *) &rng);
		value = rng * GOLD_RATIO;

		if(tid == 0 && dtime > 0 && i % THROUGHPUT_TIME_PRECISION && duration() > dtime) {
			atomic_store_explicit(
					&terminate,
					1,
					memory_order_release);
			lfht_end_thread(head, tid);
			pthread_exit(NULL);
		}

		if(tid > 0) {
			if (atomic_load_explicit(
				&terminate,
				memory_order_relaxed) > 0) {
				lfht_end_thread(head, tid);
				pthread_exit(NULL);
			}
		}

		if ((rng & 3) <= 0){
			lfht_remove(head, value, tid);
			continue;
		}

		if ((rng & 3) <= 1){
			lfht_insert(head, value, (void*)value, tid);
			continue;
		}

		lfht_search(head, value, tid);
	}
	return NULL;
}

void *pt_random(void *entry_point)
{
	int tid = ((intptr_t)entry_point);
	struct drand48_data *s = seed[tid];
	int nodes = test_size/n_threads;

	//for(int i = 0; i < nodes; i++){
	//	size_t rng;
	//	size_t value;
	//	lrand48_r(s, (long int *) &rng);
	//	value = rng * GOLD_RATIO;

	//	if(rng < limit_r) {
	//		lfht_insert(head, value, (void*)value, tid);
	//	}
	//}

	for (int i = 0; i < nodes; i++){
		size_t rng;
		size_t value;
		lrand48_r(s, (long int *) &rng);
		value = rng * GOLD_RATIO;

		if (rng < limit_sf){
			lfht_search(head, value, tid);
			continue;
		}

		if (rng < limit_r){
			lfht_remove(head, value, tid);
			continue;
		}

		if (rng < limit_i){
			lfht_insert(head, value, (void*)value, tid);
			continue;
		}

		lfht_search(head, value, tid);
	}

	lfht_end_thread(head, tid);
	return NULL;
}

void *pt_sparse_all(void *entry_point) {
	int tid = ((intptr_t)entry_point);
	struct drand48_data *s = seed[tid];
	int nodes = test_size/n_threads;
	size_t* keys = shuffle(nodes*tid, nodes, 0, s);

	if(dtime > 0) {
		for(;;){
			for(int i = 0; i < nodes; i++){
				if(dtime > 0 && duration() > dtime) {
					pthread_exit(NULL);
					return NULL;
				}

				size_t rng;
				lrand48_r(s, (long int *) &rng);
				if (rng < limit_sf){
					lfht_search(head, keys[i], tid);
					continue;
				}

				if (rng < limit_r){
					lfht_remove(head, keys[i], tid);
					continue;
				}

				if (rng < limit_i){
					lfht_insert(head, keys[i], (void*)keys[i], tid);
					continue;
				}

				lfht_search(head, keys[i], tid);
			}
		}
		return NULL;
	}

	for(int i = 0; i < nodes; i++){
		size_t rng;
		lrand48_r(s, (long int *) &rng);
		if (rng < limit_sf){
			lfht_search(head, keys[i], tid);
			continue;
		}

		if (rng < limit_r){
			lfht_remove(head, keys[i], tid);
			continue;
		}

		if (rng < limit_i){
			lfht_insert(head, keys[i], (void*)keys[i], tid);
			continue;
		}

		lfht_search(head, keys[i], tid);
	}

	lfht_end_thread(head, tid);
	return NULL;
}

void *pt_sparse_insert_all(void *entry_point) {
	int tid = ((intptr_t)entry_point);
	struct drand48_data *s = seed[tid];
	int nodes = test_size/n_threads;
	size_t* keys = shuffle(nodes*tid, nodes, 0, s);

	if(dtime > 0) {
		for(;;){
			for(int i = 0; i < nodes; i++){
				if(dtime > 0 && duration() > dtime) {
					pthread_exit(NULL);
					return NULL;
				}
				lfht_insert(head, keys[i], (void*)keys[i], tid);
			}
		}
		return NULL;
	}

	for(int i = 0; i < nodes; i++){
		lfht_insert(head, keys[i], (void*)keys[i], tid);
	}

	lfht_end_thread(head, tid);
	return NULL;
}

void *pt_search_all(void *entry_point) {
	int tid = ((intptr_t)entry_point);
	struct drand48_data *s = seed[tid];
	int nodes = test_size/n_threads;

	if(dtime > 0) {
		for(;;){
			for(int i = 0; i < nodes; i++){
				size_t rng;
				size_t value;
				lrand48_r(s, (long int *) &rng);
				value = rng * GOLD_RATIO;
				if(dtime > 0 && duration() > dtime) {
					pthread_exit(NULL);
					return NULL;
				}
				lfht_search(head, value, tid);
			}
		}
		return NULL;
	}

	for(int i = 0; i < nodes; i++){
		size_t rng;
		size_t value;
		lrand48_r(s, (long int *) &rng);
		value = rng * GOLD_RATIO;

		lfht_search(head, value, tid);
	}

	lfht_end_thread(head, tid);
	return NULL;
}

void *pt_remove_all(void *entry_point) {
	int tid = ((intptr_t)entry_point);
	struct drand48_data *s = seed[tid];

	int nodes = test_size/n_threads;
	for(int i = 0; i < nodes; i++){
		if(dtime > 0 && duration() > dtime) {
			pthread_exit(NULL);
			return NULL;
		}

		size_t rng;
		size_t value;
		lrand48_r(s, (long int *) &rng);
		value = rng * GOLD_RATIO;

		lfht_remove(head, value, tid);
	}

	lfht_end_thread(head, tid);
	return NULL;
}

void *pt_insert_all(void *entry_point) {
	int tid = ((intptr_t)entry_point);
	struct drand48_data *s = seed[tid];

	if(dtime > 0) {
		for(;;){
			if(dtime > 0 && duration() > dtime) {
				pthread_exit(NULL);
				return NULL;
			}

			size_t rng;
			size_t value;
			lrand48_r(s, (long int *) &rng);
			value = rng * GOLD_RATIO;

			lfht_insert(head, value, (void*)value, tid);
		}
		return NULL;
	}

	int nodes = test_size/n_threads;
	for(int i = 0; i < nodes; i++){
		size_t rng;
		size_t value;
		lrand48_r(s, (long int *) &rng);
		value = rng * GOLD_RATIO;

		lfht_insert(head, value, (void*)value, tid);
	}

	lfht_end_thread(head, tid);
	return NULL;
}

void *pt_load_map(void *entry_point)
{
	int tid = ((intptr_t)entry_point);
	struct drand48_data *s = seed[tid];
	int nodes = test_size/n_threads;
	load_map(head, nodes, tid, s);
	return NULL;
}

void *pt_load_seq(void *entry_point)
{
	int tid = ((intptr_t)entry_point);
	size_t nodes = (size_t)(test_size / n_threads);

	for(size_t i = nodes*tid; i < nodes*tid + nodes; i++) {
		lfht_insert(head, i, (void*)i, tid);
	}

	return NULL;
}

#if LFHT_DEBUG
void dump_graph_handler() {
	if(!head) {
		return;
	}

	printf("Dumped...\n");
	print_graph(head->entry_hash, NULL);
}
#endif
