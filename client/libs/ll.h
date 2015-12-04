/* ll.h
 * By Ron
 * Created January, 2010
 *
 * (See LICENSE.md)
 *
 * Implements a simple singly-linked list.
 */

#ifndef __LL_H__
#define __LL_H__

#include "libs/memory.h"

typedef struct
{
  void *index;
  void *data;
  struct ll_element_t *next;
} ll_element_t;

typedef struct
{
  ll_element_t *first;
} ll_t;

ll_t *ll_create();
void *ll_add(ll_t *ll, void *index, void *data);
void *ll_remove(ll_t *ll, void *index);
void *ll_find(ll_t *ll, void *index);
void ll_destroy(ll_t *ll);

#endif
