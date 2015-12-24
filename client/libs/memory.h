/* memory.h
 * By Ron
 * Created January, 2010
 *
 * (See LICENSE.md)
 *
 * Implements functions for managing memory. Optionally (based on defining
 * TEST_MEMORY) keeps track of all memory allocated and prints out a summary at
 * the end. Great for finding memory leaks.
 */

#ifndef __MEMORY_H__
#define __MEMORY_H__

#include <stdlib.h> /* For size_t */

#include "types.h"

/* Make calls to malloc/realloc that die cleanly if the calls fail. safe_malloc() initializes buffer to 0. */
#define safe_malloc(size) safe_malloc_internal(size, __FILE__, __LINE__)
void *safe_malloc_internal(size_t size, char *file, int line);

#define safe_realloc(ptr,size) safe_realloc_internal(ptr, size, __FILE__, __LINE__)
void *safe_realloc_internal(void *ptr, size_t size, char *file, int line);

#define safe_strdup(str) safe_strdup_internal(str, __FILE__, __LINE__)
char *safe_strdup_internal(const char *str, char *file, int line);

#define safe_memcpy(str,len) safe_memcpy_internal(str, len, __FILE__, __LINE__)
void *safe_memcpy_internal(const void *data, size_t length, char *file, int line);

/* Free memory and remove it from our list of allocated memory. */
#define safe_free(ptr) safe_free_internal(ptr, __FILE__, __LINE__)
void safe_free_internal(void *ptr, char *file, int line);

/* Create a UNICODE string based on an ASCII one. Be sure to free the memory! */
char *unicode_alloc(const char *string);
/* Same as unicode_alloc(), except convert the string to uppercase first. */
char *unicode_alloc_upper(const char *string);

/* Print the currently allocated memory. Useful for checking for memory leaks. */
void print_memory();

#endif
