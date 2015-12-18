/* ll.c
 * By Ron
 * Created January, 2010
 *
 * (See LICENSE.md)
 */

#include <stdio.h>

#include "libs/memory.h"

#if 0
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define safe_malloc malloc
#define safe_free free
#define FALSE 0
#define TRUE 1
#endif

#include "ll.h"

ll_t *ll_create(cmpfunc_t *cmpfunc)
{
  ll_t *ll = (ll_t*) safe_malloc(sizeof(ll_t));
  ll->first = NULL;
  ll->cmpfunc = cmpfunc;

  return ll;
}

static ll_element_t *create_element(ll_index_t index, void *data)
{
  ll_element_t *element = (ll_element_t*) safe_malloc(sizeof(ll_element_t));
  element->index = index;
  element->data = data;

  return element;
}

static ll_element_t *destroy_element(ll_element_t *element)
{
  void *data = element->data;
  safe_free(element);
  return data;
}

void *ll_add(ll_t *ll, ll_index_t index, void *data)
{
  ll_element_t *element = create_element(index, data);
  void *old_data = ll_remove(ll, index);

  element->next = NULL;
  if(ll->first)
    element->next = (struct ll_element_t *)ll->first;
  ll->first     = element;

  return old_data;
}

static int compare(ll_t *ll, ll_index_t a, ll_index_t b)
{
  if(a.type != b.type)
    return FALSE;

  switch(a.type)
  {
    case LL_8:
      return a.value.u8 == b.value.u8;

    case LL_16:
      return a.value.u16 == b.value.u16;

    case LL_32:
      return a.value.u32 == b.value.u32;

    case LL_64:
      return a.value.u64 == b.value.u64;

    case LL_PTR:
      if(ll->cmpfunc)
        return ll->cmpfunc(a.value.ptr, b.value.ptr);
      else
        return a.value.ptr == a.value.ptr;
  }

  printf("We forgot to handle a linked-list type!\n");
  exit(1);
}

void *ll_remove(ll_t *ll, ll_index_t index)
{
  ll_element_t *prev = NULL;
  ll_element_t *cur = ll->first;

  while(cur)
  {
    if(compare(ll, cur->index, index))
    {
      if(prev)
        prev->next = cur->next;
      else
        ll->first = (ll_element_t *)cur->next;

      return destroy_element(cur);
    }
    prev = cur;
    cur = (ll_element_t*)prev->next;
  }

  return NULL;
}

void *ll_remove_first(ll_t *ll)
{
  ll_element_t *first = ll->first;

  if(first)
    ll->first = (ll_element_t *)first->next;

  return first ? first->data : NULL;
}

void *ll_find(ll_t *ll, ll_index_t index)
{
  ll_element_t *cur = ll->first;

  while(cur)
  {
    if(compare(ll, cur->index, index))
      return cur->data;
    cur = (ll_element_t *)cur->next;
  }

  return NULL;
}

void ll_destroy(ll_t *ll)
{
  ll_element_t *cur = ll->first;
  ll_element_t *next = NULL;

  while(cur)
  {
    next = (ll_element_t *)cur->next;
    destroy_element(cur);
    cur = next;
  }

  safe_free(ll);
}

ll_index_t ll_8(uint8_t value)
{
  ll_index_t index;
  index.type = LL_8;
  index.value.u8 = value;

  return index;
}

ll_index_t ll_16(uint16_t value)
{
  ll_index_t index;
  index.type = LL_16;
  index.value.u16 = value;

  return index;
}

ll_index_t ll_32(uint32_t value)
{
  ll_index_t index;
  index.type = LL_32;
  index.value.u32 = value;

  return index;
}

ll_index_t ll_64(uint64_t value)
{
  ll_index_t index;
  index.type = LL_64;
  index.value.u64 = value;

  return index;
}

ll_index_t ll_ptr(void *value)
{
  ll_index_t index;
  index.type = LL_PTR;
  index.value.ptr = value;

  return index;
}

#if 0
int my_strcmp(const void *a, const void *b)
{
  return strcmp((const char*)a, (const char*)b);
}

int main(int argc, const char *argv[])
{
  ll_t *ll = ll_create(NULL);

  printf("\n");
  printf("nil: %p\n", ll_find(ll, ll_16(0x123)));

  printf("\n");
  ll_add(ll, ll_16(0x12), (void*)0x32);
  printf("32:  %p\n", ll_find(ll, ll_16(0x12)));
  printf("nil: %p\n", ll_find(ll, ll_16(0x31)));
  printf("nil: %p\n", ll_find(ll, ll_32(0x12)));
  printf("nil: %p\n", ll_find(ll, ll_8(0x12)));

  printf("\n");
  ll_remove(ll, ll_16(0x123));
  printf("nil: %p\n", ll_find(ll, ll_16(0x123)));

  ll_add(ll, ll_8(1), "8");
  ll_add(ll, ll_16(1), "16");
  ll_add(ll, ll_32(1), "32");
  ll_add(ll, ll_64(1), "64");
  ll_add(ll, ll_ptr((void*)1), "ptr");

  printf("8:   %s\n", (char*)ll_find(ll, ll_8(1)));
  printf("16:  %s\n", (char*)ll_find(ll, ll_16(1)));
  printf("32:  %s\n", (char*)ll_find(ll, ll_32(1)));
  printf("64:  %s\n", (char*)ll_find(ll, ll_64(1)));
  printf("ptr: %s\n", (char*)ll_find(ll, ll_ptr((void*)1)));

  ll_remove(ll, ll_8(1));
  ll_remove(ll, ll_16(1));
  ll_remove(ll, ll_32(1));
  ll_remove(ll, ll_64(1));
  ll_remove(ll, ll_ptr((void*)1));

  printf("nil: %p\n", (char*)ll_find(ll, ll_8(1)));
  printf("nil: %p\n", (char*)ll_find(ll, ll_16(1)));
  printf("nil: %p\n", (char*)ll_find(ll, ll_32(1)));
  printf("nil: %p\n", (char*)ll_find(ll, ll_64(1)));
  printf("nil: %p\n", (char*)ll_find(ll, ll_ptr((void*)1)));

  ll_destroy(ll);

  return 0;
}
#endif
