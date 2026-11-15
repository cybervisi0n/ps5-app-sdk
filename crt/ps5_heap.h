#ifndef PS5_HEAP_H
#define PS5_HEAP_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif


bool ps5_heap_initialize();

void ps5_heap_shutdown();
void * ps5_malloc(size_t size);
void ps5_free(void* ptr);
void * ps5_calloc(size_t count, size_t size);
void * ps5_realloc(void* ptr, size_t size);

#ifdef __cplusplus
}
#endif

#endif