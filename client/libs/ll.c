/* ll.c
 * By Ron
 * Created January, 2010
 *
 * (See LICENSE.md)
 */

/*#include "libs/memory.h"*/

#include "ll.h"

ll_t *ll_create()
{
  ll_t *ll = (ll_t*) safe_malloc(sizeof(ll_t));
  ll->first = NULL;

  return ll;
}

static ll_element_t *create_element(void *index, void *data)
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

void *ll_add(ll_t *ll, void *index, void *data)
{
  ll_element_t *element = create_element(index, data);
  void *old_data = ll_remove(ll, index);

  element->next = NULL;
  if(ll->first)
    element->next = (struct ll_element_t *)ll->first;
  ll->first     = element;

  return old_data;
}

void *ll_remove(ll_t *ll, void *index)
{
  ll_element_t *prev = NULL;
  ll_element_t *cur = ll->first;

  while(cur)
  {
    if(cur->index == index)
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

void *ll_find(ll_t *ll, void *index)
{
  ll_element_t *cur = ll->first;

  while(cur)
  {
    if(cur->index == index)
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

#if 0
#include <stdio.h>
int main(int argc, const char *argv[])
{
  ll_t *ll = ll_create();

  printf("\n");
  printf("nil: %p\n", ll_find(ll, (void*)0x123));

  printf("\n");
  ll_add(ll, (void*)0x123, (void*)0x321);
  printf("321: %p\n", ll_find(ll, (void*)0x123));
  printf("nil: %p\n", ll_find(ll, (void*)0x312));

  printf("\n");
  ll_remove(ll, (void*)0x123);
  printf("nil: %p\n", ll_find(ll, (void*)0x123));

  printf("\n");
  ll_add(ll, (void*)0x123, (void*)0x456);
  ll_add(ll, (void*)0x456, (void*)0x789);
  printf("456: %p\n", ll_find(ll, (void*)0x123));
  printf("789: %p\n", ll_find(ll, (void*)0x456));
  ll_remove(ll, (void*)0x123);
  ll_remove(ll, (void*)0x456);

  printf("\n");
  ll_add(ll, (void*)0x123, (void*)0x456);
  ll_add(ll, (void*)0x456, (void*)0x789);
  printf("456: %p\n", ll_find(ll, (void*)0x123));
  printf("789: %p\n", ll_find(ll, (void*)0x456));
  ll_remove(ll, (void*)0x456);
  ll_remove(ll, (void*)0x123);

  printf("\n");
  ll_add(ll, (void*)0x123, (void*)0x456);
  ll_add(ll, (void*)0x456, (void*)0x789);
  ll_add(ll, (void*)0x789, (void*)0xabc);
  printf("456: %p\n", ll_find(ll, (void*)0x123));
  printf("789: %p\n", ll_find(ll, (void*)0x456));
  printf("abc: %p\n", ll_remove(ll, (void*)0x789));
  printf("nil: %p\n", ll_remove(ll, (void*)0x789));
  ll_remove(ll, (void*)0x456);
  ll_remove(ll, (void*)0x123);
  ll_remove(ll, (void*)0x789);

  ll_destroy(ll);

  return 0;
}
#endif
