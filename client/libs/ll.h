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

typedef int(cmpfunc_t)(const void *, const void *);

typedef enum
{
  LL_8,
  LL_16,
  LL_32,
  LL_64,
  LL_PTR,
} ll_index_type_t;

typedef struct
{
  ll_index_type_t type;
  union
  {
    uint16_t  u8;
    uint16_t  u16;
    uint32_t  u32;
    uint64_t  u64;
    void     *ptr;
  } value;
} ll_index_t;

typedef struct
{
  ll_index_t           index;
  void                *data;
  struct ll_element_t *next;
} ll_element_t;

typedef struct
{
  ll_element_t *first;
  cmpfunc_t    *cmpfunc;
} ll_t;

ll_t *ll_create(cmpfunc_t *cmpfunc);
void *ll_add(ll_t *ll,    ll_index_t index, void *data);
void *ll_remove(ll_t *ll, ll_index_t index);
void *ll_remove_first(ll_t *ll);
void *ll_find(ll_t *ll,   ll_index_t index);
void ll_destroy(ll_t *ll);

ll_index_t ll_8(uint8_t   value);
ll_index_t ll_16(uint16_t value);
ll_index_t ll_32(uint32_t value);
ll_index_t ll_64(uint64_t value);
ll_index_t ll_ptr(void   *value);

#endif
