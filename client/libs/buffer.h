/* buffer.h
 * By Ron
 * Created August, 2008
 *
 * (See LICENSE.md)
 *
 * This is a generic class for buffering (marshalling) data that in many ways
 * dates back to my days as a Battle.net developers. It gives programmers an
 * easy way to prepare data to be sent on the network, as well as a simplified
 * way to build strings in a language where string building isn't always
 * straight forward. In many ways, it's like the pack()/unpack() functions from
 * Perl.
 *
 * I've strived to keep this implementation the same on every platform. You'll
 * notice that there's no Windows-specific code here, and also that all
 * calculations will work on both a little- or big-endian system. The only time
 * you won't get the same results is when you create a buffer with the type
 * BO_HOST, which always uses the host's byte ordering. I don't recommend that.
 */

#ifndef __BUFFER_H__
#define __BUFFER_H__

#include <stdlib.h> /* For "size_t". */

#include "types.h"

typedef enum
{
  BO_HOST,          /* Use the byte order of the host (bad idea -- changes on systems. */
  BO_NETWORK,       /* Use network byte order which, as it turns out, is big endian. */
  BO_LITTLE_ENDIAN, /* Use big endian byte ordering (0x12345678 => 78 56 34 12). */
  BO_BIG_ENDIAN,    /* Use big endian byte ordering (0x12345678 => 12 34 56 78). */
} BYTE_ORDER_t;

/* This struct shouldn't be accessed directly */
typedef struct
{
  /* Byte order to use */
  BYTE_ORDER_t byte_order;

  /* The current position in the string, used when reading it. */
  size_t position;

  /* The maximum length of the buffer that "buffer" is pointing to.  When
   * space in this runs out, it's expanded  */
  size_t max_length;

  /* The current length of the buffer. */
  size_t current_length;

  /* The current buffer.  Will always point to a string of length max_length */
  uint8_t *data;

  /* Set to FALSE when the packet is destroyed, to make sure I don't accidentally
   * re-use it (again) */
  NBBOOL valid;

} buffer_t;

/* Create a new packet buffer */
buffer_t *buffer_create(BYTE_ORDER_t byte_order);

/* Create a new packet buffer, with data. */
buffer_t *buffer_create_with_data(BYTE_ORDER_t byte_order, const void *data, const size_t length);

/* Go to the start of the buffer. */
void buffer_reset(buffer_t *buffer);

/* Destroy the buffer and free resources.  If this isn't used, memory will leak. */
void buffer_destroy(buffer_t *buffer);

/* Makes a copy of the buffer. */
buffer_t *buffer_duplicate(buffer_t *base);

/* Get the length of the buffer. */
size_t buffer_get_length(buffer_t *buffer);

/* Get the current location in the buffer. */
size_t buffer_get_current_offset(buffer_t *buffer);
/* Set the current location in the buffer. */
void buffer_set_current_offset(buffer_t *buffer, size_t position);

/* Get the number of bytes between the current offset and the end of the buffer. */
size_t buffer_get_remaining_bytes(buffer_t *buffer);

/* Clear out the buffer. Memory is kept, but the contents are blanked out and the pointer is returned
 * to the beginning. */
void buffer_clear(buffer_t *buffer);

/* Align the buffer to even multiples. */
void buffer_read_align(buffer_t *buffer, size_t align);
void buffer_write_align(buffer_t *buffer, size_t align);

/* Consume (discard) bytes. */
void buffer_consume(buffer_t *buffer, size_t count);

/* Return the contents of the buffer in a newly allocated string. Fill in the length, if a pointer
 * is given. Note that this allocates memory that has to be freed! */
uint8_t *buffer_create_string(buffer_t *buffer, size_t *length);
/* Does the same thing as above, but also frees up the buffer (good for a function return). */
uint8_t *buffer_create_string_and_destroy(buffer_t *buffer, size_t *length);

/* Returns the contents of the buffer - starting at the current position - in a newly allocated
 * string. Returns the length in the length pointer. If max_bytes is -1, all bytes are returned. */
uint8_t *buffer_read_remaining_bytes(buffer_t *buffer, size_t *length, size_t max_bytes, NBBOOL consume);

/* Add data to the end of the buffer */
buffer_t *buffer_add_int8(buffer_t *buffer,      const uint8_t data);
buffer_t *buffer_add_int16(buffer_t *buffer,     const uint16_t data);
buffer_t *buffer_add_int32(buffer_t *buffer,     const uint32_t data);
buffer_t *buffer_add_ntstring(buffer_t *buffer,  const char *data);
buffer_t *buffer_add_string(buffer_t *buffer,    const char *data);
/* Note: UNICODE support is a hack -- it adds every second character as a NULL, but is otherwise ASCII. */
buffer_t *buffer_add_unicode(buffer_t *buffer,   const char *data);
buffer_t *buffer_add_bytes(buffer_t *buffer,     const void *data, const size_t length);
buffer_t *buffer_add_buffer(buffer_t *buffer,    const buffer_t *source);

/* Add data to the middle of a buffer. These functions won't write past the end of the buffer, so it's
 * up to the programmer to be careful. */
buffer_t *buffer_add_int8_at(buffer_t *buffer,      const uint8_t data, size_t offset);
buffer_t *buffer_add_int16_at(buffer_t *buffer,     const uint16_t data, size_t offset);
buffer_t *buffer_add_int32_at(buffer_t *buffer,     const uint32_t data, size_t offset);
buffer_t *buffer_add_ntstring_at(buffer_t *buffer,  const char *data, size_t offset);
buffer_t *buffer_add_string_at(buffer_t *buffer,    const char *data, size_t offset);
buffer_t *buffer_add_unicode_at(buffer_t *buffer,   const char *data, size_t offset);
buffer_t *buffer_add_bytes_at(buffer_t *buffer,     const void *data, const size_t length, size_t offset);
buffer_t *buffer_add_buffer_at(buffer_t *buffer,    const buffer_t *source, size_t offset);

/* Read the next data from the buffer.  The first read will be at the beginning.
 * An assertion will fail and the program will end if read off
 * the end of the buffer; it's probably a good idea to verify that enough data can be removed
 * before actually attempting to remove it; otherwise, a DoS condition can occur */
uint8_t   buffer_read_next_int8(buffer_t *buffer);
uint16_t  buffer_read_next_int16(buffer_t *buffer);
uint32_t  buffer_read_next_int32(buffer_t *buffer);
char     *buffer_read_next_ntstring(buffer_t *buffer, char *data_ret, size_t max_length);
char     *buffer_read_next_unicode(buffer_t *buffer, char *data_ret, size_t max_length);
char     *buffer_read_next_unicode_data(buffer_t *buffer, char *data_ret, size_t length);
void     *buffer_read_next_bytes(buffer_t *buffer, void *data, size_t length);

/* Allocate memory to hold the result. */
char     *buffer_alloc_next_ntstring(buffer_t *buffer);

/* Read the next data, without incrementing the current pointer. */
uint8_t   buffer_peek_next_int8(buffer_t *buffer);
uint16_t  buffer_peek_next_int16(buffer_t *buffer);
uint32_t  buffer_peek_next_int32(buffer_t *buffer);
char     *buffer_peek_next_ntstring(buffer_t *buffer, char *data_ret, size_t max_length);
char     *buffer_peek_next_unicode(buffer_t *buffer, char *data_ret, size_t max_length);
void     *buffer_peek_next_bytes(buffer_t *buffer, void *data, size_t length);

/* Read data at the specified location in the buffer (counting the first byte as 0). */
uint8_t   buffer_read_int8_at(buffer_t *buffer, size_t offset);
uint16_t  buffer_read_int16_at(buffer_t *buffer, size_t offset);
uint32_t  buffer_read_int32_at(buffer_t *buffer, size_t offset);
char     *buffer_read_ntstring_at(buffer_t *buffer, size_t offset, char *data_ret, size_t max_length);
char     *buffer_read_unicode_at(buffer_t *buffer, size_t offset, char *data_ret, size_t max_length);
char     *buffer_read_unicode_data_at(buffer_t *buffer, size_t offset, char *data_ret, size_t length);
void     *buffer_read_bytes_at(buffer_t *buffer, size_t offset, void *data, size_t length);

/* Allocate memory to hold the result. */
char *buffer_alloc_ntstring_at(buffer_t *buffer, size_t offset);

/* These NBBOOL functions check if there are enough bytes left in the buffer to remove
 * specified data.  These should always be used on the server side to verify valid
 * packets for a critical service. */
NBBOOL buffer_can_read_int8(buffer_t *buffer);
NBBOOL buffer_can_read_int16(buffer_t *buffer);
NBBOOL buffer_can_read_int32(buffer_t *buffer);
NBBOOL buffer_can_read_ntstring(buffer_t *buffer);
NBBOOL buffer_can_read_unicode(buffer_t *buffer);
NBBOOL buffer_can_read_bytes(buffer_t *buffer, size_t length);

/* These functions check if there are enough bytes in the buffer at the specified location. */
NBBOOL buffer_can_read_int8_at(buffer_t *buffer, size_t offset);
NBBOOL buffer_can_read_int16_at(buffer_t *buffer, size_t offset);
NBBOOL buffer_can_read_int32_at(buffer_t *buffer, size_t offset);
NBBOOL buffer_can_read_ntstring_at(buffer_t *buffer, size_t offset, size_t max_length);
NBBOOL buffer_can_read_unicode_at(buffer_t *buffer, size_t offset, size_t max_length);
NBBOOL buffer_can_read_bytes_at(buffer_t *buffer, size_t offset, size_t length);

/* Print out the buffer in a nice format -- useful for debugging. */
void buffer_print(buffer_t *buffer);

/* Returns a pointer to the actual buffer (I don't recommend using this). */
uint8_t *buffer_get(buffer_t *buffer, size_t *length);

#endif
