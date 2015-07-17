/* memory.c
 * By Ron
 * Created January, 2010
 *
 * (See LICENSE.md)
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

static void die(char *msg, char *file, int line)
{
  printf("\n\nUnrecoverable error at %s:%d: %s\n\n", file, line, msg);
  abort();
  exit(1);
}

static void die_mem(char *file, int line)
{
  die("Out of memory", file, line);
}

void add_entry(char *file, int line, void *memory, size_t size)
{
#ifdef TESTMEMORY
  entry_t *current = (entry_t*) malloc(sizeof(entry_t));
  if(!current)
    die_mem(file, line);

  /* Put the new entry at the front of the list. */
  current->next = first;
  first         = current;

  current->file   = file;
  current->line   = line;
  current->memory = memory;
  current->size   = size;
#endif
}

void update_entry(void *old_memory, void *new_memory, int new_size, char *file, int line)
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

  die("Tried to re-allocate memory that doesn't exist.", file, line);
#endif
}

void remove_entry(void *memory, char *file, int line)
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

  die("Tried to free memory that we didn't allocate (or that's already been freed)", file, line);
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
    die_mem(file, line);
  memset(ret, 0, size);

  add_entry(file, line, ret, size);
  return ret;
}

void *safe_realloc_internal(void *ptr, size_t size, char *file, int line)
{
  void *ret = realloc(ptr, size);
  if(!ret)
    die_mem(file, line);

  update_entry(ptr, ret, size, file, line);
  return ret;
}

char *safe_strdup_internal(const char *str, char *file, int line)
{
  char *ret;

  if(strlen(str) + 1 < strlen(str))
    die("Overflow.", file, line);

  ret = safe_malloc_internal(strlen(str) + 1, file, line);
  memcpy(ret, str, strlen(str) + 1);

  return ret;
}

void *safe_memcpy_internal(const void *data, size_t length, char *file, int line)
{
  uint8_t *ret;

  ret = safe_malloc_internal(length, file, line);
  memcpy(ret, data, length);

  return ret;
}

void safe_free_internal(void *ptr, char *file, int line)
{
  remove_entry(ptr, file, line);
  free(ptr);
}
