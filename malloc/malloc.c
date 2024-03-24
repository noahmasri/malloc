#define _DEFAULT_SOURCE

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include "malloc.h"
#include "printfmt.h"
#include <stddef.h>

#define MIN_SIZE 128
#define SMALL 16384
#define MEDIUM 1048576
#define LARGE 33554432


#define ALIGN4(s) (((((s) -1) >> 2) << 2) + 4)
#define REGION2PTR(r) ((r) + 1)
#define PTR2REGION(ptr) ((struct region *) (ptr) -1)

enum blocksize {
	SMALL_SIZED,
	MEDIUM_SIZED,
	LARGE_SIZED,
};

struct region {
	int blockid;
	bool free;
	enum blocksize blocksize;
	size_t size;  // in bytes
	struct region *next;
	struct region *prev;
};

struct region *small_regions_list = NULL;
size_t available_at_smallregs = 0;

struct region *medium_regions_list = NULL;
size_t available_at_mediumregs = 0;

struct region *large_regions_list = NULL;
size_t available_at_largeregs = 0;

int amount_of_blocks = 0;
int amount_of_mallocs = 0;
int amount_of_frees = 0;
int requested_memory = 0;

static struct region *
find_first_fit(struct region *curr, size_t size)
{
	if (!curr)
		return NULL;

	if (curr->free && size < curr->size)
		return curr;

	return find_first_fit(curr->next, size);
}

static struct region *
find_best_region(struct region *curr, size_t size, struct region *best_fit, int block_id)
{
	if (!curr || curr->blockid != block_id)
		return best_fit;

	if (curr->free) {
		if (curr->size == size)
			return curr;
		if (curr->size > size && curr->size < best_fit->size)
			best_fit = curr;
	}

	return find_best_region(curr->next, size, best_fit, block_id);
}

static struct region *
find_best_fit(struct region *curr, size_t size)
{
	struct region *first_fit = find_first_fit(curr, size);
	if (!first_fit)
		return NULL;

	return find_best_region(curr, size, first_fit, first_fit->blockid);
}

bool
region_needs_large(size_t size)
{
	return size > (MEDIUM - sizeof(struct region)) &&
	       size <= (LARGE - sizeof(struct region));
}

bool
region_needs_medium(size_t size)
{
	return size > (SMALL - sizeof(struct region)) &&
	       size <= (MEDIUM - sizeof(struct region));
}

bool
region_needs_small(size_t size)
{
	return size <= (SMALL - sizeof(struct region));
}

struct region *
fit(size_t size, struct region *(*find_region)(struct region *, size_t))
{
	struct region *found = NULL;
	if (region_needs_large(size)) {
		if (size <= available_at_largeregs)
			found = find_region(large_regions_list, size);
	} else if (region_needs_medium(size)) {
		if (size <= available_at_mediumregs)
			found = find_region(medium_regions_list, size);
		if (!found && size <= available_at_largeregs)
			found = find_region(large_regions_list, size);
	} else {
		if (size <= available_at_smallregs)
			found = find_region(small_regions_list, size);
		if (!found && size <= available_at_mediumregs)
			found = find_region(medium_regions_list, size);
		if (!found && size <= available_at_largeregs)
			found = find_region(large_regions_list, size);
	}
	return found;
}

// finds the next free region
// that holds the requested size
//
static struct region *
find_free_region(size_t size)
{
	struct region *found = NULL;
#ifdef FIRST_FIT
	found = fit(size, find_first_fit);
#endif
#ifdef BEST_FIT
	found = fit(size, find_best_fit);
#endif
	return found;
}

enum blocksize
find_needed_blocksize(size_t size)
{
	if (region_needs_large(size)) {
		return LARGE_SIZED;
	} else if (region_needs_medium(size)) {
		return MEDIUM_SIZED;
	} else {
		return SMALL_SIZED;
	}
}

size_t
size_of_blocksize(enum blocksize blocksize)
{
	switch (blocksize) {
	case LARGE_SIZED: {
		return LARGE;
	}
	case MEDIUM_SIZED: {
		return MEDIUM;
	}
	case SMALL_SIZED: {
		return SMALL;
	}
	}
	return 0;
}

void
update_available_space(struct region *freed, int sign)
{
	if (!freed)
		return;
	switch (freed->blocksize) {
	case SMALL_SIZED: {
		available_at_smallregs =
		        available_at_smallregs + freed->size * sign;
	}
	case MEDIUM_SIZED: {
		available_at_mediumregs =
		        available_at_mediumregs + freed->size * sign;
	}
	case LARGE_SIZED: {
		available_at_largeregs =
		        available_at_largeregs + freed->size * sign;
	}
	}
}

struct region *
update_list(struct region *new_region, size_t required_mem)
{
	switch (new_region->blocksize) {
	case LARGE_SIZED: {
		new_region->next = large_regions_list;
		if (large_regions_list)
			large_regions_list->prev = new_region;
		available_at_largeregs += required_mem;
		large_regions_list = new_region;
	}
	case MEDIUM_SIZED: {
		new_region->next = medium_regions_list;
		if (medium_regions_list)
			medium_regions_list->prev = new_region;
		available_at_mediumregs += required_mem;
		medium_regions_list = new_region;
	}
	case SMALL_SIZED: {
		new_region->next = small_regions_list;
		if (small_regions_list)
			small_regions_list->prev = new_region;
		available_at_smallregs += required_mem;
		small_regions_list = new_region;
	}
	}

	return new_region;
}

static struct region *
grow_heap(size_t size)
{
	enum blocksize blocksize = find_needed_blocksize(size);
	size_t mem_a_pedir = size_of_blocksize(blocksize);
	void *aux = mmap(NULL,
	                 mem_a_pedir,
	                 PROT_READ | PROT_WRITE,
	                 MAP_ANONYMOUS | MAP_PRIVATE,
	                 -1,
	                 0);

	if (aux == (void *) -1) {
		perror("mmap failed");
		fprintf(stderr, "Error description: %s\n", strerror(errno));
		return NULL;
	}
	struct region *new_region = aux;
	amount_of_blocks++;

	new_region->blockid = amount_of_blocks;
	new_region->blocksize = blocksize;
	new_region->prev = NULL;
	new_region->size = mem_a_pedir - sizeof(struct region);
	new_region->free = true;
	new_region = update_list(new_region, mem_a_pedir);

	update_available_space(new_region, 1);
	return new_region;
}

bool
is_size_valid(size_t size)
{
	return size < LARGE - sizeof(struct region);
}

// este chequeo es para evitar que la region restante sea mas chica
// que el min size

bool
is_separable(struct region *region, size_t size)
{
	return region->size >= size + 2 * sizeof(struct region) + MIN_SIZE;
}

/// Public API of malloc library ///

struct region *
region_creation(void *reg, size_t size)
{
	struct region *next_region = reg + sizeof(struct region) +
	                             size;  // si no casteamos a void no funciona
	struct region *region = (struct region *) reg;
	next_region->prev = region;
	next_region->next = region->next;
	next_region->size = region->size - size - sizeof(struct region);
	next_region->free = true;
	next_region->blockid = region->blockid;
	next_region->blocksize = region->blocksize;
	return next_region;
}

struct region *
split_free_region(struct region *region, size_t size)
{
	if (!region)
		return NULL;

	if (!is_separable(region, size)) {
		// le damos espacio de mas al usuario antes que dejar un espacio inutilizable
		region->free = false;
		return region;
	}
	struct region *next_region = region_creation(region, size);
	region->size = size;
	region->next = next_region;
	region->free = false;

	return region;
}

void *
my_malloc(size_t size)
{
	if (size == 0)
		return NULL;
	struct region *next;
	size_t requested_size = size;
	if (size < MIN_SIZE)
		size = MIN_SIZE;

	// aligns to multiple of 4 bytes
	size = ALIGN4(size);


	if (!is_size_valid(size)) {
		return NULL;
	}

	// updates statistics
	amount_of_mallocs++;
	requested_memory += requested_size;

	next = find_free_region(size);

	if (!next)
		next = grow_heap(size);

	next = split_free_region(next, size);
	update_available_space(next, -1);
	return REGION2PTR(next);
}

bool
is_coleasceable(struct region *left, struct region *right)
{
	return left->blockid == right->blockid && left->free && right->free;
}

struct region *
coleasce_regions(struct region *left, struct region *right)
{
	if (!left || !right || !is_coleasceable(left, right))
		return NULL;


	left->next = right->next;
	left->size += (right->size + sizeof(struct region));

	return left;
}

bool
is_unmapable(struct region *region)
{
	return (region->size == size_of_blocksize(region->blocksize) -
	                                sizeof(struct region) &&
	        region->free);
}

void
my_free(void *ptr)
{
	// updates statistics
	amount_of_frees++;

	struct region *curr = PTR2REGION(ptr);
	if (!curr)
		return;

	curr->free = true;
	update_available_space(curr, 1);

	struct region *right_col = coleasce_regions(curr, curr->next);
	struct region *left_col = coleasce_regions(curr->prev, curr);

	if (left_col) {
		curr = left_col;
	}

	if (is_unmapable(curr)) {
		if (curr->next)
			curr->next->prev = curr->prev;

		if (curr->prev)
			curr->prev->next = curr->next;

		update_available_space(curr, -1);
		munmap(curr, (curr->size + sizeof(struct region)));
	}
}

void *
my_calloc(size_t nmemb, size_t size)
{
	void *new = my_malloc(nmemb * size);
	if (!new)
		return NULL;

	memset(new, 0, nmemb * size);
	return new;
}

// se fija si la suma de las regiones curr y next son suficientes para
// satisfacer el pedido de memoria.
bool
is_new_size_enough(struct region *curr, struct region *next, size_t mem_req)
{
	return next->size + sizeof(struct region) + curr->size >= mem_req;
}

void *
my_realloc(void *ptr, size_t size)
{
	if (!ptr)
		return my_malloc(size);

	struct region *curr = PTR2REGION(ptr);
	if (size == 0) {
		my_free(ptr);
		return NULL;
	}

	if (!curr)
		return my_malloc(size);
	if (!is_size_valid(size)) {
		return NULL;
	}

	update_available_space(curr, 1);  // hacemos como que se libero
	requested_memory -= curr->size;
	requested_memory += size;

	if (size < curr->size) {
		curr = split_free_region(curr, size);
		update_available_space(curr, -1);
		return REGION2PTR(curr);
	}

	// hay q encapsular esta cond en una funcion
	if (size > curr->size && curr->next->free &&
	    is_new_size_enough(curr, curr->next, size)) {
		curr->free = true;  // para poder hacer coleasce
		curr = coleasce_regions(curr, curr->next);
		if (curr) {
			curr = split_free_region(curr, size);
			update_available_space(
			        curr, -1);  // quita de disponible el nuevo tamano
			return REGION2PTR(curr);
		}
	}

	void *next = my_malloc(size);
	if (!next)
		return NULL;

	memcpy(next, ptr, curr->size);
	my_free(ptr);
	return REGION2PTR(next);
}

void
get_stats(struct malloc_stats *stats)
{
	stats->mallocs = amount_of_mallocs;
	stats->frees = amount_of_frees;
	stats->requested_memory = requested_memory;
}
