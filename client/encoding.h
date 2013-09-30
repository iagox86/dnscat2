#ifndef __ENCODING_H__
#define __ENCODING_H__

typedef enum
{
  HEX_ENCODE,
  BASE32_ENCODE
} encode_types_t;

char    *encode(encode_types_t type, uint8_t *value, size_t  length);
uint8_t *decode(encode_types_t type, char    *text,  size_t *length);

char    *encode_hex(uint8_t *value, size_t length);
uint8_t *decode_hex(char *text,     size_t *length);
char    *encode_base32(uint8_t *data,    size_t length);
uint8_t *decode_base32(const char *text, size_t *length);

#endif
