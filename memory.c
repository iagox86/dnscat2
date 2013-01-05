/* memory.c
 * By Ron
 * Created January, 2010
 *
 * (See LICENSE.txt)
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"

typedef struct entry
{
	char          *file;
	int            line;
	void          *memory;
	size_t         size;
	struct entry  *next;
} entry_t;

#ifdef TESTMEMORY
static entry_t *first           = NULL;
#endif

void add_entry(char *file, int line, void *memory, size_t size)
{
#ifdef TESTMEMORY
	entry_t *current = (entry_t*) malloc(sizeof(entry_t));
	if(!current)
		DIE_MEM();

	/* Put the new entry at the front of the list. */
	current->next = first;
	first         = current;

	current->file   = file;
	current->line   = line;
	current->memory = memory;
	current->size   = size;
#endif
}

void update_entry(void *old_memory, void *new_memory, int new_size)
{
#ifdef TESTMEMORY
	entry_t *current = first;

	while(current)
	{
		if(current->memory == old_memory)
		{
			current->memory = new_memory;
			current->size   = new_size;
			return;
		}
		current = current->next;
	}

	DIE("Tried to re-allocate memory that doesn't exist.");
#endif
}

void remove_entry(void *memory)
{
#ifdef TESTMEMORY
	entry_t *last    = NULL;
	entry_t *current = first;

	while(current)
	{
		if(current->memory == memory)
		{
			if(current == first)
			{
				/* Beginning of the list. */
				first = first->next;
				free(current);
			}
			else
			{
				/* Anywhere else in the list. */
				last->next = current->next;
				free(current);
			}

			return;
		}
		last = current;
		current = current->next;
	}

	DIE("Tried to free memory that we didn't own");
#endif
}

void print_memory()
{
#ifdef TESTMEMORY
	if(first == NULL)
	{
		fprintf(stderr, "No allocated memory. Congratulations!\n");
	}
	else
	{
		entry_t *current = first;

		fprintf(stderr, "Allocated memory:\n");
		while(current)
		{
			fprintf(stderr, "%p: 0x%08x bytes allocated at %s:%d\n", current->memory, (unsigned int)current->size, current->file, current->line);
			current = current->next;
		}
	}
#endif
}

void *safe_malloc_internal(size_t size, char *file, int line)
{
	void *ret = malloc(size);
	if(!ret)
		DIE_MEM();
	memset(ret, 0, size);

	add_entry(file, line, ret, size);
	return ret;
}

void *safe_realloc_internal(void *ptr, size_t size, char *file, int line)
{
	void *ret = realloc(ptr, size);
	if(!ret)
		DIE_MEM();

	update_entry(ptr, ret, size);
	return ret;
}

char *safe_strdup_internal(const char *str, char *file, int line)
{
	char *ret;

	if(strlen(str) + 1 < strlen(str))
		DIE("Overflow.");

	ret = safe_malloc_internal(strlen(str) + 1, file, line);
	memcpy(ret, str, strlen(str) + 1);

	return ret;
}

void safe_free(void *ptr)
{
	free(ptr);
	remove_entry(ptr);
}

char *unicode_alloc(const char *string)
{
	size_t i;
	char *unicode;
	size_t unicode_length = (strlen(string) + 1) * 2;

	if(unicode_length < strlen(string))
		DIE("Overflow.");

	unicode = safe_malloc(unicode_length);

	memset(unicode, 0, unicode_length);
	for(i = 0; i < strlen(string); i++)
	{
		unicode[(i * 2)] = string[i];
	}

	return unicode;
}

char *unicode_alloc_upper(const char *string)
{
	size_t i;
	char *unicode;
	size_t unicode_length = (strlen(string) + 1) * 2;

	if(unicode_length < strlen(string))
		DIE("Overflow.");

	unicode = safe_malloc(unicode_length);

	memset(unicode, 0, unicode_length);
	for(i = 0; i < strlen(string); i++)
	{
		/* Note: int typecase fixes compile warning on cygwin. */
		unicode[(i * 2)] = toupper((int)string[i]);
	}

	return unicode;
}

