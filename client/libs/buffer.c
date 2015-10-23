/* buffer.c
 * By Ron
 * Created August, 2008
 *
 * (See LICENSE.md)
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#include <winsock2.h> /* For htons/htonl. */
#else
#include <arpa/inet.h> /* For htons/htonl. */
#endif

#include "buffer.h"
#include "memory.h"
#include "types.h"

/* The initial max length of the string */
#define STARTING_LENGTH 64

static uint16_t host_to_network_16(uint16_t data)
{
  return htons(data);
}
static uint16_t host_to_host_16(uint16_t data)
{
  return data;
}
static uint16_t host_to_little_endian_16(uint16_t data)
{
  uint16_t network = host_to_network_16(data);

  return ((network >> 8) & 0x00FF) |
      ((network << 8) & 0xFF00);
}
static uint16_t host_to_big_endian_16(uint16_t data)
{
  return host_to_network_16(data);
}

static uint32_t host_to_network_32(uint32_t data)
{
  return htonl(data);
}
static uint32_t host_to_host_32(uint32_t data)
{
  return data;
}
static uint32_t host_to_little_endian_32(uint32_t data)
{
  uint32_t network = host_to_network_32(data);

  return ((network << 24)  & 0xFF000000) |
      ((network << 8)  & 0x00FF0000) |
      ((network >> 8)  & 0x0000FF00) |
      ((network >> 24) & 0x000000FF);
}
static uint32_t host_to_big_endian_32(uint32_t data)
{
  return host_to_network_32(data);
}


static uint16_t network_to_host_16(uint16_t data)
{
  return htons(data);
}
/*static uint16_t host_to_host_16(uint16_t data)
{
  return data;
} */
static uint16_t little_endian_to_host_16(uint16_t data)
{
  uint16_t network = network_to_host_16(data);

  return ((network >> 8) & 0x00FF) |
      ((network << 8) & 0xFF00);
}
static uint16_t big_endian_to_host_16(uint16_t data)
{
  return network_to_host_16(data);
}

static uint32_t network_to_host_32(uint32_t data)
{
  return htonl(data);
}
/*static uint32_t host_to_host_32(uint32_t data)
{
  return data;
} */
static uint32_t little_endian_to_host_32(uint32_t data)
{
  uint32_t network = network_to_host_32(data);

  return ((network << 24)  & 0xFF000000) |
      ((network << 8)  & 0x00FF0000) |
      ((network >> 8)  & 0x0000FF00) |
      ((network >> 24) & 0x000000FF);
}
static uint32_t big_endian_to_host_32(uint32_t data)
{
  return network_to_host_32(data);
}



/* Create a new packet buffer */
buffer_t *buffer_create(BYTE_ORDER_t byte_order)
{
  buffer_t *new_buffer = safe_malloc(sizeof(buffer_t));

  new_buffer->byte_order     = byte_order;
  new_buffer->valid          = TRUE;
  new_buffer->position       = 0;
  new_buffer->max_length     = STARTING_LENGTH;
  new_buffer->current_length = 0;
  new_buffer->data           = safe_malloc(STARTING_LENGTH * sizeof(char));

  return new_buffer;
}

/* Create a new packet buffer, with data.  The data shouldn't include the packet header,
 * it will be added.  The length is the length of the data, without the header. */
buffer_t *buffer_create_with_data(BYTE_ORDER_t byte_order, const void *data, const size_t length)
{
  buffer_t *new_buffer = buffer_create(byte_order);
  if(!new_buffer)
    DIE_MEM();

  buffer_add_bytes(new_buffer, data, length);

  return new_buffer;
}

/* Go to the start of the buffer. */
void buffer_reset(buffer_t *buffer)
{
  buffer->position = 0;
}

/* Destroy the buffer and free resources.  If this isn't used, memory will leak. */
void buffer_destroy(buffer_t *buffer)
{
  if(!buffer->valid)
    DIE("Program attempted to use deleted buffer.");
  buffer->valid = FALSE;

  memset(buffer->data, 0, buffer->max_length);
  safe_free(buffer->data);

  memset(buffer, 0, sizeof(buffer_t));
  safe_free(buffer);
}

buffer_t *buffer_duplicate(buffer_t *base)
{
  /* Allocate memory. */
  buffer_t *new = safe_malloc(sizeof(buffer_t));

  /* Make an exact copy (won't copy pointers properly). */
  memcpy(new, base, sizeof(buffer_t));

  /* Create a new 'data' pointer. */
  new->data = safe_malloc(new->max_length);

  /* Copy the data into the new data pointer. */
  memcpy(new->data, base->data, new->max_length);

  return new;
}

size_t buffer_get_length(buffer_t *buffer)
{
  return buffer->current_length;
}

size_t buffer_get_current_offset(buffer_t *buffer)
{
  return buffer->position;
}

void buffer_set_current_offset(buffer_t *buffer, size_t position)
{
  buffer->position = position;
}

size_t buffer_get_remaining_bytes(buffer_t *buffer)
{
  return buffer_get_length(buffer) - buffer_get_current_offset(buffer);
}

void buffer_clear(buffer_t *buffer)
{
  memset(buffer->data, 0, buffer->current_length);
  buffer->position = 0;
  buffer->current_length = 0;
}

void buffer_read_align(buffer_t *buffer, size_t align)
{
  while(buffer_get_current_offset(buffer) % align)
    buffer_read_next_int8(buffer);
}

void buffer_write_align(buffer_t *buffer, size_t align)
{
  while(buffer_get_length(buffer) % align)
    buffer_add_int8(buffer, 0);
}

void buffer_consume(buffer_t *buffer, size_t count)
{
  buffer->position += count;
}

uint8_t *buffer_create_string(buffer_t *buffer, size_t *length)
{
  uint8_t *ret;

  if(!buffer->valid)
    DIE("Program attempted to use deleted buffer.");

  ret = safe_malloc(buffer_get_length(buffer));
  memcpy(ret, buffer->data, buffer_get_length(buffer));

  if(length)
    *length = buffer_get_length(buffer);

  return ret;
}

uint8_t *buffer_create_string_and_destroy(buffer_t *buffer, size_t *length)
{
  uint8_t *ret = buffer_create_string(buffer, length);

  buffer_destroy(buffer);

  return ret;
}

uint8_t *buffer_read_remaining_bytes(buffer_t *buffer, size_t *length, size_t max_bytes, NBBOOL consume)
{
  uint8_t *ret;

  if(!buffer->valid)
    DIE("Program attempted to use a deleted buffer.");

  if(buffer->current_length < buffer->position)
    DIE("Position is outside the buffer");

  /* The number of bytes remaining */
  *length = buffer->current_length - buffer->position;

  if(max_bytes != (size_t)-1 && *length > max_bytes)
    *length = max_bytes;

  /* Allocate room for that many bytes */
  if(*length+1 < *length)
    DIE("Overflow.");

  ret = safe_malloc(*length+1);

  /* Copy the data into the new buffer */
  if(consume)
    buffer_read_next_bytes(buffer, ret, *length);
  else
    buffer_peek_next_bytes(buffer, ret, *length);

  /* Return the new buffer */
  return ret;
}

/* Add data to the end of the buffer */
buffer_t *buffer_add_int8(buffer_t *buffer, const uint8_t data)
{
  if(!buffer->valid)
    DIE("Program attempted to use deleted buffer.");

  buffer_add_bytes(buffer, &data, 1);

  return buffer;
}

buffer_t *buffer_add_int16(buffer_t *buffer, const uint16_t data)
{
  uint16_t converted;

  if(!buffer->valid)
    DIE("Program attempted to use deleted buffer.");

  switch(buffer->byte_order)
  {
    case BO_NETWORK:       converted = host_to_network_16(data);       break;
    case BO_HOST:          converted = host_to_host_16(data);          break;
    case BO_LITTLE_ENDIAN: converted = host_to_little_endian_16(data); break;
    case BO_BIG_ENDIAN:    converted = host_to_big_endian_16(data);    break;
  }

  buffer_add_bytes(buffer, &converted, 2);

  return buffer;
}

buffer_t *buffer_add_int32(buffer_t *buffer, const uint32_t data)
{
  uint32_t converted;

  if(!buffer->valid)
    DIE("Program attempted to use deleted buffer.");

  switch(buffer->byte_order)
  {
    case BO_NETWORK:       converted = host_to_network_32(data);       break;
    case BO_HOST:          converted = host_to_host_32(data);          break;
    case BO_LITTLE_ENDIAN: converted = host_to_little_endian_32(data); break;
    case BO_BIG_ENDIAN:    converted = host_to_big_endian_32(data);    break;
  }

  buffer_add_bytes(buffer, &converted, 4);

  return buffer;
}

buffer_t *buffer_add_ntstring(buffer_t *buffer, const char *data)
{
  if(!buffer->valid)
    DIE("Program attempted to use deleted buffer.");

  buffer_add_bytes(buffer, data, strlen(data) + 1);

  return buffer;
}

buffer_t *buffer_add_string(buffer_t *buffer, const char *data)
{
  if(!buffer->valid)
    DIE("Program attempted to use deleted buffer.");

  buffer_add_bytes(buffer, data, strlen(data));

  return buffer;
}

buffer_t *buffer_add_unicode(buffer_t *buffer, const char *data)
{
  size_t i;
  if(!buffer->valid)
    DIE("Program attempted to use deleted buffer.");

  for(i = 0; i < (strlen(data) + 1); i++)
    buffer_add_int16(buffer, data[i]);

  return buffer;
}

buffer_t *buffer_add_bytes(buffer_t *buffer, const void *data, const size_t length)
{
  if(!buffer->valid)
    DIE("Program attempted to use deleted buffer.");

  if(buffer->current_length + length < buffer->current_length)
    DIE("Overflow.");

  if(length >= 0x80000000)
    DIE("Too big!");

  /* Resize the buffer, if necessary. */
  if(buffer->current_length + length > buffer->max_length)
  {
    do
    {
      /* Check for overflow. */
      if(buffer->max_length << 1 < buffer->max_length)
        DIE("Overflow.");

      /* Double the length. */
      buffer->max_length = buffer->max_length << 1;
    }
    while(buffer->current_length + length > buffer->max_length);

    buffer->data = safe_realloc(buffer->data, buffer->max_length);
  }

  memcpy(buffer->data + buffer->current_length, data, length);

  buffer->current_length += length;

  return buffer;
}

buffer_t *buffer_add_buffer(buffer_t *buffer, const buffer_t *source)
{
  if(!buffer->valid)
    DIE("Program attempted to use deleted buffer.");
  if(!source->valid)
    DIE("Program attempted to use deleted buffer.");

  buffer_add_bytes(buffer, source->data, source->current_length);

  return buffer;
}

buffer_t *buffer_add_int8_at(buffer_t *buffer,      const uint8_t data, size_t offset)
{
  return buffer_add_bytes_at(buffer, &data, 1, offset);
}

buffer_t *buffer_add_int16_at(buffer_t *buffer,     const uint16_t data, size_t offset)
{
  uint16_t converted;

  if(!buffer->valid)
    DIE("Program attempted to use deleted buffer.");

  switch(buffer->byte_order)
  {
    case BO_NETWORK:       converted = host_to_network_16(data);       break;
    case BO_HOST:          converted = host_to_host_16(data);          break;
    case BO_LITTLE_ENDIAN: converted = host_to_little_endian_16(data); break;
    case BO_BIG_ENDIAN:    converted = host_to_big_endian_16(data);    break;
  }

  return buffer_add_bytes_at(buffer, &converted, 2, offset);
}

buffer_t *buffer_add_int32_at(buffer_t *buffer,     const uint32_t data, size_t offset)
{
  uint32_t converted;

  if(!buffer->valid)
    DIE("Program attempted to use deleted buffer.");

  switch(buffer->byte_order)
  {
    case BO_NETWORK:       converted = host_to_network_32(data);       break;
    case BO_HOST:          converted = host_to_host_32(data);          break;
    case BO_LITTLE_ENDIAN: converted = host_to_little_endian_32(data); break;
    case BO_BIG_ENDIAN:    converted = host_to_big_endian_32(data);    break;
  }

  return buffer_add_bytes_at(buffer, &converted, 4, offset);
}

buffer_t *buffer_add_ntstring_at(buffer_t *buffer,  const char *data, size_t offset)
{
  return buffer_add_bytes_at(buffer, data, strlen(data) + 1, offset);
}

buffer_t *buffer_add_string_at(buffer_t *buffer,  const char *data, size_t offset)
{
  return buffer_add_bytes_at(buffer, data, strlen(data), offset);
}

buffer_t *buffer_add_unicode_at(buffer_t *buffer,   const char *data, size_t offset)
{
  size_t i;
  if(!buffer->valid)
    DIE("Program attempted to use deleted buffer.");

  for(i = 0; i < (strlen(data) + 1); i++)
    buffer_add_int16_at(buffer, data[i], offset + (i * 2));

  return buffer;
}

buffer_t *buffer_add_bytes_at(buffer_t *buffer,     const void *data, const size_t length, size_t offset)
{
  /* Ensure the buffer is valid. */
  if(!buffer->valid)
    DIE("Program attempted to use deleted buffer.");

  /* Funnily enough, this works the same for reading as for writing. */
  if(!buffer_can_read_bytes_at(buffer, offset, length))
    DIE("Program read off the end of the buffer.");

  memcpy(buffer->data + offset, data, length);

  return buffer;
}

buffer_t *buffer_add_buffer_at(buffer_t *buffer,    const buffer_t *source, size_t offset)
{
  if(!source->valid)
    DIE("Program attempted to use deleted buffer.");

  buffer_add_bytes_at(buffer, source->data, source->current_length, offset);

  return buffer;
}


uint8_t buffer_read_next_int8(buffer_t *buffer)
{
  uint8_t ret = buffer_read_int8_at(buffer, buffer->position);
  buffer->position += 1;
  return ret;
}
uint16_t buffer_read_next_int16(buffer_t *buffer)
{
  uint16_t ret = buffer_read_int16_at(buffer, buffer->position);
  buffer->position += 2;
  return ret;
}
uint32_t buffer_read_next_int32(buffer_t *buffer)
{
  uint32_t ret = buffer_read_int32_at(buffer, buffer->position);
  buffer->position += 4;
  return ret;
}
char *buffer_read_next_ntstring(buffer_t *buffer, char *data_ret, size_t max_length)
{
  buffer_read_ntstring_at(buffer, buffer->position, data_ret, max_length);
  buffer->position += strlen(data_ret) + 1;

  return data_ret;
}
char *buffer_read_next_unicode(buffer_t *buffer, char *data_ret, size_t max_length)
{
  buffer_read_unicode_at(buffer, buffer->position, data_ret, max_length);
  buffer->position += (strlen(data_ret) + 1) * 2;

  return data_ret;
}
char *buffer_read_next_unicode_data(buffer_t *buffer, char *data_ret, size_t length)
{
  buffer_read_unicode_data_at(buffer, buffer->position, data_ret, length);
  buffer->position += length * 2;

  return data_ret;
}
void *buffer_read_next_bytes(buffer_t *buffer, void *data, size_t length)
{
  buffer_read_bytes_at(buffer, buffer->position, data, length);
  buffer->position += length;

  return data;
}

char *buffer_alloc_next_ntstring(buffer_t *buffer)
{
  char *result = buffer_alloc_ntstring_at(buffer, buffer->position);
  buffer->position += strlen(result) + 1;
  return result;
}

uint8_t buffer_peek_next_int8(buffer_t *buffer)
{
  return buffer_read_int8_at(buffer, buffer->position);
}
uint16_t buffer_peek_next_int16(buffer_t *buffer)
{
  return buffer_read_int16_at(buffer, buffer->position);
}
uint32_t buffer_peek_next_int32(buffer_t *buffer)
{
  return buffer_read_int32_at(buffer, buffer->position);
}
char *buffer_peek_next_ntstring(buffer_t *buffer, char *data_ret, size_t max_length)
{
  return buffer_read_ntstring_at(buffer, buffer->position, data_ret, max_length);
}
char *buffer_peek_next_unicode(buffer_t *buffer, char *data_ret, size_t max_length)
{
  return buffer_read_unicode_at(buffer, buffer->position, data_ret, max_length);
}
void *buffer_peek_next_bytes(buffer_t *buffer, void *data, size_t length)
{
  return buffer_read_bytes_at(buffer, buffer->position, data, length);
}

uint8_t buffer_read_int8_at(buffer_t *buffer, size_t offset)
{
  uint8_t ret;

  if(!buffer->valid)
    DIE("Program attempted to use deleted buffer.");

  buffer_read_bytes_at(buffer, offset, &ret, 1);

  return ret;
}

uint16_t buffer_read_int16_at(buffer_t *buffer, size_t offset)
{
  uint16_t ret;

  if(!buffer->valid)
    DIE("Program attempted to use deleted buffer.");

  buffer_read_bytes_at(buffer, offset, &ret, 2);

  switch(buffer->byte_order)
  {
    case BO_NETWORK:       ret = network_to_host_16(ret);       break;
    case BO_HOST:          ret = host_to_host_16(ret);          break;
    case BO_LITTLE_ENDIAN: ret = little_endian_to_host_16(ret); break;
    case BO_BIG_ENDIAN:    ret = big_endian_to_host_16(ret);    break;
  }

  return ret;
}

uint32_t buffer_read_int32_at(buffer_t *buffer, size_t offset)
{
  uint32_t ret;

  if(!buffer->valid)
    DIE("Program attempted to use deleted buffer.");

  buffer_read_bytes_at(buffer, offset, &ret, 4);

  switch(buffer->byte_order)
  {
    case BO_NETWORK:       ret = network_to_host_32(ret);       break;
    case BO_HOST:          ret = host_to_host_32(ret);          break;
    case BO_LITTLE_ENDIAN: ret = little_endian_to_host_32(ret); break;
    case BO_BIG_ENDIAN:    ret = big_endian_to_host_32(ret);    break;
  }

  return ret;
}

char *buffer_read_ntstring_at(buffer_t *buffer, size_t offset, char *data_ret, size_t max_length)
{
  size_t i = 0;
  uint8_t ch;

  if(!buffer->valid)
    DIE("Program attempted to use deleted buffer.");
  if(!buffer_can_read_ntstring_at(buffer, offset, max_length))
    DIE("Program read off the end of the buffer.");

  do
  {
    ch = buffer->data[offset + i];
    data_ret[i] = ch;
    i++;
  }
  while(ch && i < max_length);

  data_ret[i - 1] = 0;

  return data_ret;
}

char *buffer_read_unicode_at(buffer_t *buffer, size_t offset, char *data_ret, size_t max_length)
{
  size_t i = 0;
  uint8_t  ch;

  if(!buffer->valid)
    DIE("Program attempted to use deleted buffer.");
  if(!buffer_can_read_ntstring_at(buffer, offset, max_length))
    DIE("Program read off the end of the buffer.");

  do
  {
    ch = (uint8_t)buffer_read_int16_at(buffer, offset + (i * 2));
    data_ret[i] = ch;
    i++;
  }
  while(ch && i < max_length);

  data_ret[i - 1] = 0;

  return data_ret;
}

char *buffer_read_unicode_data_at(buffer_t *buffer, size_t offset, char *data_ret, size_t length)
{
  size_t i = 0;

  if(!buffer->valid)
    DIE("Program attempted to use deleted buffer.");
  if(length * 2 < length)
    DIE("Overflow.");
  if(!buffer_can_read_bytes_at(buffer, offset, length * 2))
    DIE("Program read off the end of the buffer.");

  for(i = 0; i < length; i++)
  {
    data_ret[i] = (char)buffer_read_int16_at(buffer, offset + (i * 2));
  }

  return data_ret;
}

void *buffer_read_bytes_at(buffer_t *buffer, size_t offset, void *data, size_t length)
{
  if(!buffer->valid)
    DIE("Program attempted to use deleted buffer.");
  if(!buffer_can_read_bytes_at(buffer, offset, length))
    DIE("Program read off the end of the buffer.");

  memcpy(data, buffer->data + offset, length);

  return data;
}

char *buffer_alloc_ntstring_at(buffer_t *buffer, size_t offset)
{
  size_t length = strlen((char*)buffer->data + offset) + 1;
  char *data_ret = safe_malloc(length);

  /* Catch overflows. */
  if(length == 0)
    DIE("Overflow?");

  return buffer_read_ntstring_at(buffer, offset, data_ret, length);
}

/* These NBBOOL functions check if there's enough bytes left in the buffer to remove
 * specified data.  These should always be used on the server side to verify valid
 * packets */
NBBOOL buffer_can_read_int8(buffer_t *buffer)
{
  return buffer_can_read_bytes(buffer, 1);
}
NBBOOL buffer_can_read_int16(buffer_t *buffer)
{
  return buffer_can_read_bytes(buffer, 2);
}
NBBOOL buffer_can_read_int32(buffer_t *buffer)
{
  return buffer_can_read_bytes(buffer, 4);
}
NBBOOL buffer_can_read_ntstring(buffer_t *buffer)
{
  size_t i;
  if(!buffer->valid)
    DIE("Program attempted to use deleted buffer.");

  for(i = buffer->position; i < buffer->current_length; i++)
    if(buffer->data[i] == '\0')
      return TRUE;

  return FALSE;
}
/* It's important for the logic in this function to closely match the logic in read_unicode_at(), which
 * is why I convert it to a uint8_t. */
NBBOOL buffer_can_read_unicode(buffer_t *buffer)
{
  size_t i;
  if(!buffer->valid)
    DIE("Program attempted to use deleted buffer.");

  for(i = buffer->position; i < buffer->current_length; i++)
    if(((uint8_t)buffer_read_int16_at(buffer, i)) == 0)
      return TRUE;

  return FALSE;
}
NBBOOL buffer_can_read_bytes(buffer_t *buffer, size_t length)
{
  if(!buffer->valid)
    DIE("Program attempted to use deleted buffer.");
  if(buffer->position + length < buffer->position)
    DIE("Overflow.");

  return buffer_can_read_bytes_at(buffer, buffer->position, length);
}

NBBOOL buffer_can_read_int8_at(buffer_t *buffer, size_t offset)
{
  return buffer_can_read_bytes_at(buffer, offset, 1);
}

NBBOOL buffer_can_read_int16_at(buffer_t *buffer, size_t offset)
{
  return buffer_can_read_bytes_at(buffer, offset, 2);
}

NBBOOL buffer_can_read_int32_at(buffer_t *buffer, size_t offset)
{
  return buffer_can_read_bytes_at(buffer, offset, 4);
}

NBBOOL buffer_can_read_ntstring_at(buffer_t *buffer, size_t offset, size_t max_length)
{
  size_t i;
  NBBOOL good = TRUE;
  NBBOOL done = FALSE;

  for(i = 0; i < max_length - 1 && !done && good; i++)
  {
    if(buffer->data[offset + i] == '\0')
      done = TRUE;

    if(offset + i + 1 > buffer->current_length)
      good = FALSE;
  }

  return good;
}

NBBOOL buffer_can_read_unicode_at(buffer_t *buffer, size_t offset, size_t max_length)
{
  size_t i;
  NBBOOL good = TRUE;
  NBBOOL done = FALSE;

  for(i = 0; i < max_length - 1 && !done && good; i++)
  {
    if(!buffer_can_read_int16_at(buffer, offset + (i * 2)))
      good = FALSE;
    else if(((uint8_t)buffer_read_int16_at(buffer, offset + (i * 2))) == 0)
      done = TRUE;
  }

  return good;
}

NBBOOL buffer_can_read_bytes_at(buffer_t *buffer, size_t offset, size_t length)
{
  if(!buffer->valid)
    DIE("Program attempted to use deleted buffer.");
  if(offset + length < offset)
    DIE("Overflow");

/*  printf("Offset = %d\n", offset);
  printf("Length = %d\n", length);
  printf("Current length = %d\n", buffer->current_length);  */

  /* A special case - we can always read zero-length strings {*/
  if(length == 0)
    return TRUE;

  return (offset + length - 1) < buffer->current_length;
}


static char get_character_from_byte(uint8_t byte)
{
  if(byte < 0x20 || byte > 0x7F)
    return '.';
  return byte;
}

/* Print out the buffer in a nice format */
void buffer_print(buffer_t *buffer)
{
  size_t length = buffer->current_length;
  size_t i, j;
  if(!buffer->valid)
    DIE("Program attempted to use deleted buffer.");
  printf("Position = %zd\n", buffer->position);

  printf("Buffer contents:");
  for(i = 0; i < length; i++)
  {
    if(!(i % 16))
    {
      if(i > 0)
      {
        printf("   ");
        for(j = 16; j > 0; j--)
        {
          printf("%c", get_character_from_byte(buffer->data[i - j]));
        }
      }
      if(i == buffer->position)
        printf("\n%04zX:<", i);
      else
        printf("\n%04zX: ", i);
    }

    if(i == buffer->position)
      printf("%02X>", buffer->data[i]);
    else if(i == buffer->position - 1)
      printf("%02X<", buffer->data[i]);
    else
      printf("%02X ", buffer->data[i]);
  }

  for(i = length % 16; i < 17; i++)
    printf("   ");
  for(i = length - (length % 16); i < length; i++)
    printf("%c", get_character_from_byte(buffer->data[i]));

  printf("\nLength: 0x%zX (%zd)\n", length, length);
}

/* Returns a pointer to the actual buffer */
uint8_t *buffer_get(buffer_t *buffer, size_t *length)
{
  if(!buffer->valid)
    DIE("Program attempted to use deleted buffer.");
  *length = buffer->current_length;
  return buffer->data;
}

#if 0
int main(int argc, char *argv[])
{
  size_t i;
  buffer_t *buffer;

  char     unicode[8];
  char     data[17];
  uint8_t  data8;
  uint16_t data16;
  uint32_t data32;

  printf("\n\n-----------------------\n");
  printf("Host ordering:\n");
  printf("-----------------------\n\n");
  buffer = buffer_create(BO_HOST);

  buffer_add_unicode(buffer, "unicode");
  buffer_add_ntstring(buffer, "ntString #1");
  buffer_add_ntstring(buffer, "ntString #2");
  buffer_add_int8(buffer, 0x01);
  buffer_add_int16(buffer, 0x0203);
  buffer_add_int32(buffer, 0x04050607);
  buffer_add_int32(buffer, 0x08090a0b);
  buffer_add_int32(buffer, 0x0c0d0e0f);
  buffer_print(buffer);

  buffer_read_next_unicode(buffer, unicode, 8);
  printf("Unicode: %s\n", unicode);
  for(i = 0; i < 2; i++)
  {
    buffer_read_next_ntstring(buffer, data, 16);
    printf("Read %d: %s\n", i, data);
  }
  buffer_print(buffer);

  data8  = buffer_read_next_int8(buffer);
  buffer_print(buffer);
  data16 = buffer_read_next_int16(buffer);
  buffer_print(buffer);
  data32 = buffer_read_next_int32(buffer);
  buffer_print(buffer);
  printf("%08x\n", data8);
  printf("%08x\n", data16);
  printf("%08x\n", data32);
  data32 = buffer_read_next_int32(buffer);
  printf("%08x\n", data32);
  data32 = buffer_read_next_int32(buffer);
  printf("%08x\n", data32);
  buffer_destroy(buffer);



  printf("\n\n-----------------------\n");
  printf("Network ordering:\n");
  printf("-----------------------\n\n");
  buffer = buffer_create(BO_NETWORK);

  buffer_add_unicode(buffer, "unicode");
  buffer_add_ntstring(buffer, "ntString #1");
  buffer_add_ntstring(buffer, "ntString #2");
  buffer_add_int8(buffer, 0x01);
  buffer_add_int16(buffer, 0x0203);
  buffer_add_int32(buffer, 0x04050607);
  buffer_add_int32(buffer, 0x08090a0b);
  buffer_add_int32(buffer, 0x0c0d0e0f);
  buffer_print(buffer);

  buffer_read_next_unicode(buffer, unicode, 8);
  printf("Unicode: %s\n", unicode);
  for(i = 0; i < 2; i++)
  {
    buffer_read_next_ntstring(buffer, data, 16);
    printf("Read %d: %s\n", i, data);
  }
  buffer_print(buffer);

  data8  = buffer_read_next_int8(buffer);
  data16 = buffer_read_next_int16(buffer);
  data32 = buffer_read_next_int32(buffer);
  printf("%08x\n", data8);
  printf("%08x\n", data16);
  printf("%08x\n", data32);
  data32 = buffer_read_next_int32(buffer);
  printf("%08x\n", data32);
  data32 = buffer_read_next_int32(buffer);
  printf("%08x\n", data32);
  buffer_destroy(buffer);





  printf("\n\n-----------------------\n");
  printf("Little endian ordering:\n");
  printf("-----------------------\n\n");
  buffer = buffer_create(BO_LITTLE_ENDIAN);

  buffer_add_unicode(buffer, "unicode");
  buffer_add_ntstring(buffer, "ntString #1");
  buffer_add_ntstring(buffer, "ntString #2");
  buffer_add_int8(buffer, 0x01);
  buffer_add_int16(buffer, 0x0203);
  buffer_add_int32(buffer, 0x04050607);
  buffer_add_int32(buffer, 0x08090a0b);
  buffer_add_int32(buffer, 0x0c0d0e0f);
  buffer_print(buffer);

  buffer_read_next_unicode(buffer, unicode, 8);
  printf("Unicode: %s\n", unicode);
  for(i = 0; i < 2; i++)
  {
    buffer_read_next_ntstring(buffer, data, 16);
    printf("Read %d: %s\n", i, data);
  }
  buffer_print(buffer);

  data8  = buffer_read_next_int8(buffer);
  data16 = buffer_read_next_int16(buffer);
  data32 = buffer_read_next_int32(buffer);
  printf("%08x\n", data8);
  printf("%08x\n", data16);
  printf("%08x\n", data32);
  data32 = buffer_read_next_int32(buffer);
  printf("%08x\n", data32);
  data32 = buffer_read_next_int32(buffer);
  printf("%08x\n", data32);
  buffer_destroy(buffer);



  printf("\n\n-----------------------\n");
  printf("Big endian ordering:\n");
  printf("-----------------------\n\n");
  buffer = buffer_create(BO_BIG_ENDIAN);

  buffer_add_unicode(buffer, "unicode");
  buffer_add_ntstring(buffer, "ntString #1");
  buffer_add_ntstring(buffer, "ntString #2");
  buffer_add_int8(buffer, 0x01);
  buffer_add_int16(buffer, 0x0203);
  buffer_add_int32(buffer, 0x04050607);
  buffer_add_int32(buffer, 0x08090a0b);
  buffer_add_int32(buffer, 0x0c0d0e0f);
  buffer_print(buffer);

  buffer_read_next_unicode(buffer, unicode, 8);
  printf("Unicode: %s\n", unicode);
  for(i = 0; i < 2; i++)
  {
    buffer_read_next_ntstring(buffer, data, 16);
    printf("Read %d: %s\n", i, data);
  }
  buffer_print(buffer);

  data8  = buffer_read_next_int8(buffer);
  data16 = buffer_read_next_int16(buffer);
  data32 = buffer_read_next_int32(buffer);
  printf("%08x\n", data8);
  printf("%08x\n", data16);
  printf("%08x\n", data32);
  data32 = buffer_read_next_int32(buffer);
  printf("%08x\n", data32);
  data32 = buffer_read_next_int32(buffer);
  printf("%08x\n", data32);
  buffer_destroy(buffer);



  buffer = buffer_create(BO_NETWORK);
  buffer_add_int32(buffer, 0x01020304);
  buffer_print(buffer);
  printf("Test: %08x\n", buffer_read_next_int32(buffer));
  printf("Test: %08x\n", buffer_read_int32_at(buffer, 0));
  buffer_destroy(buffer);
  printf("\n");

  buffer = buffer_create(BO_HOST);
  buffer_add_int32(buffer, 0x01020304);
  buffer_print(buffer);
  printf("Test: %08x\n", buffer_read_next_int32(buffer));
  printf("Test: %08x\n", buffer_read_int32_at(buffer, 0));
  buffer_destroy(buffer);
  printf("\n");

  buffer = buffer_create(BO_LITTLE_ENDIAN);
  buffer_add_int32(buffer, 0x01020304);
  buffer_print(buffer);
  printf("Test: %08x\n", buffer_read_next_int32(buffer));
  printf("Test: %08x\n", buffer_read_int32_at(buffer, 0));
  buffer_destroy(buffer);
  printf("\n");

  buffer = buffer_create(BO_BIG_ENDIAN);
  buffer_add_int32(buffer, 0x01020304);
  buffer_print(buffer);
  printf("Test: %08x\n", buffer_read_next_int32(buffer));
  printf("Test: %08x\n", buffer_read_int32_at(buffer, 0));
  buffer_destroy(buffer);



  return 0;
}
#endif

