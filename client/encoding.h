#ifndef __ENCODING_H__
#define __ENCODING_H__

char *encode_hex(uint8_t *value, size_t length);
uint8_t *decode_hex(char *text, size_t *length);
static char *encode_base32(uint8_t *data, size_t length);
uint8_t *decode_base32(const char *text, size_t *length);
