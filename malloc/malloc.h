#ifndef _MALLOC_H_
#define _MALLOC_H_
#include <stddef.h>

struct malloc_stats {
	int mallocs;
	int frees;
	int requested_memory;
};

void *my_malloc(size_t size);

void my_free(void *ptr);

void *my_calloc(size_t nmemb, size_t size);

void *my_realloc(void *ptr, size_t size);

void get_stats(struct malloc_stats *stats);

#endif  // _MALLOC_H_
