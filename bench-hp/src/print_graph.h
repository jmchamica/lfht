#include <stdio.h>
#include <stdint.h>

#define IC(x) 0
#define BP prev

const char *color(int val){
	if(val)
		return "red";
	else
		return "green";
}

void print_node(struct lfht_node *node, struct lfht_node *parent, FILE *file);

void print_compression_node(struct lfht_node *lnode, FILE *file){
	const char* f = "FREEZE";
	const char* u = "UNFREEZE";
	fprintf(
			file,
			"\t%lu [label=\"%lu|%s\"]\n",
			(uintptr_t)lnode,
			(uintptr_t)lnode,
			lnode->type == FREEZE ? f : u);
	fprintf(file, "\t%lu:out -> %lu:in\n", (uintptr_t)lnode, (uintptr_t)valid_ptr(lnode->leaf.next));
}

void print_hnode(struct lfht_node *hnode, struct lfht_node *parent, FILE *file){
	fprintf(
			file,
			"\t%lu [label=\"<in>%lu|{%d|%d}|{<BP>*|{%d}}",
			(uintptr_t)hnode,
			(uintptr_t)hnode,
			hnode->hash.hash_pos/hnode->hash.size,
			hnode->hash.size,
			//hnode->hash.counter);
			0);
	for(size_t i = 0; i < (size_t)(1 << hnode->hash.size); i++) {
		_Atomic(struct lfht_node *) *b = &(hnode->hash.array[i]);
		struct lfht_node *curr = valid_ptr(atomic_load_explicit(b, memory_order_consume));

		if(hnode == curr) {
			continue;
		}

		fprintf(file, "|<%zu>%zu", i, i);
	}
	fprintf(file, "\"]\n");
	if(hnode->hash.BP)
		fprintf(file, "\t%lu:BP -> %lu:in\n", (uintptr_t)hnode, (uintptr_t)hnode->hash.BP);
	if(parent && hnode->hash.BP != parent) {
		print_hnode(hnode->hash.BP, parent, file);
	}

	struct lfht_node *comp = NULL;
	for(size_t i = 0; i < (size_t)(1 << hnode->hash.size); i++){
		_Atomic(struct lfht_node *) *b = &(hnode->hash.array[i]);
		struct lfht_node *curr = atomic_load_explicit(b, memory_order_consume);
		unsigned invalid = is_invalid(curr);
		curr = valid_ptr(curr);

		if(hnode == curr) {
			continue;
		}

		fprintf(
				file,
				"\t%lu:%zu -> %lu:in [color=%s]\n",
				(uintptr_t)hnode,
				i,
				(uintptr_t)valid_ptr(curr),
				color(invalid));

		if(comp && is_compression_node(curr)) {
			continue;
		}
		if(is_compression_node(curr) && !comp) {
			print_compression_node(curr, file);
			comp = curr;
			continue;
		}

		print_node(valid_ptr(curr), hnode, file);
	}
}

void print_lnode(struct lfht_node *lnode, struct lfht_node *parent, FILE *file){
	fprintf(
			file,
			"\t%lu [label=\"%lu|{<out>%d}|%lu\"]\n",
			(uintptr_t)lnode,
			(uintptr_t)lnode,
			is_invalid(lnode->leaf.next),
			lnode->leaf.hash);
	fprintf(file, "\t%lu:out -> %lu:in [color=%s]\n", (uintptr_t)lnode, (uintptr_t)valid_ptr(lnode->leaf.next), color(is_invalid(lnode->leaf.next)));
	if(valid_ptr(lnode->leaf.next)->type == LEAF)
		print_lnode(valid_ptr(lnode->leaf.next), parent, file);
	if(valid_ptr(lnode->leaf.next)->type == HASH && parent != valid_ptr(lnode->leaf.next))
		print_hnode(valid_ptr(lnode->leaf.next), parent, file);
}

void print_node(struct lfht_node *node, struct lfht_node *parent, FILE *file){
	if(node->type == HASH)
		print_hnode(node, parent, file);
	else if(node->type == FREEZE || node->type == UNFREEZE)
		print_compression_node(node, file);
	else if(node->type == LEAF)
		print_lnode(node, parent, file);
}

FILE *init_graph(){
	FILE *file = fopen("graph.dot", "w+");
	fprintf(file, "digraph G {\n\trankdir=LR;\n\tnode [shape=record]\n");
	return file;
}

void end_graph(FILE *file){
	fprintf(file, "}");
	fclose(file);
}

__attribute__ ((used)) void print_graph(struct lfht_node *root, const char* fileName){
	FILE *file;

	if(fileName) {
		file = fopen(fileName, "w+");
		fprintf(file, "digraph G {\n\trankdir=LR;\n\tnode [shape=record]\n");
	} else {
		file = init_graph();
	}
	print_hnode(root, NULL, file);
	end_graph(file);
}
