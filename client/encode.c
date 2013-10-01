/* types.c
 * By Ron Bowes
 * Created September 1, 2008
 *
 * (See LICENSE.txt)
 */

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "memory.h"
#include "types.h"

#include "encode.h"

char *encode(encoding_type_t type, uint8_t *value, size_t  length)
{
  if(type == HEX)
    return hex_encode(value, length);
  else if(type == BASE32)
    return base32_encode(value, length);
  else
    return NULL;
}

uint8_t *decode(encoding_type_t type, char *text,  size_t *length)
{
  if(type == HEX)
    return hex_decode(text, length);
  else if(type == BASE32)
    return base32_decode(text, length);
  else
    return NULL;
}
size_t get_decoded_size(encoding_type_t type, size_t encoded_bytes)
{
  if(type == HEX)
    return hex_get_decoded_size(encoded_bytes);
  else if(type == BASE32)
    return base32_get_decoded_size(encoded_bytes);
  else
    return -1;
}

size_t hex_get_decoded_size(size_t encoded_bytes)
{
  return encoded_bytes / 2;
}

static char *hex_chars = "0123456789abcdef";
char *hex_encode(uint8_t *value, size_t length)
{
  char *encoded;
  size_t i;

  encoded = safe_malloc((length * 2) + 1);

  for(i = 0; i < length; i++)
  {
    encoded[(i * 2) + 0] = hex_chars[(value[i] >> 4) & 0x0F];
    encoded[(i * 2) + 1] = hex_chars[(value[i] >> 0) & 0x0F];
  }
  encoded[length * 2] = '\0';

  return encoded;
}

uint8_t *hex_decode(char *text, size_t *length)
{
  size_t in_length = (*length == -1) ? strlen(text) : (*length);
  uint8_t *decoded = safe_malloc(in_length / 2);
  size_t i;

  *length = hex_get_decoded_size(in_length);

  for(i = 0; i < in_length; i += 2)
  {
    char c1, c2;

    if(text[i] >= '0' && text[i] <= '9')
      c1 = text[i] - '0';
    else if(text[i] >= 'a' && text[i] <= 'f')
      c1 = text[i] - 'a' + 10;
    else if(text[i] >= 'A' && text[i] <= 'F')
      c1 = text[i] - 'A' + 10;
    else
    {
      safe_free(decoded);
      return NULL;
    }

    if(text[i+1] >= '0' && text[i+1] <= '9')
      c2 = text[i+1] - '0';
    else if(text[i+1] >= 'a' && text[i+1] <= 'f')
      c2 = text[i+1] - 'a' + 10;
    else if(text[i+1] >= 'A' && text[i+1] <= 'F')
      c2 = text[i+1] - 'A' + 10;
    else
    {
      safe_free(decoded);
      return NULL;
    }


    decoded[i / 2] = (c1 << 4) | (c2 << 0);

  }

  return decoded;
}


#define c_to_b32(B) ((char)( \
     (B) ==  0  ? 'A': (B) ==  1  ? 'B' : (B) == 2  ? 'C' : (B) == 3   ? 'D' :  \
     (B) ==  4  ? 'E': (B) ==  5  ? 'F' : (B) == 6  ? 'G' : (B) == 7   ? 'H' :  \
     (B) ==  8  ? 'I': (B) ==  9  ? 'J' : (B) == 10 ? 'K' : (B) == 11  ? 'L' :  \
     (B) ==  12 ? 'M': (B) ==  13 ? 'N' : (B) == 14 ? 'O' : (B) == 15  ? 'P' :  \
     (B) ==  16 ? 'Q': (B) ==  17 ? 'R' : (B) == 18 ? 'S' : (B) == 19  ? 'T' :  \
     (B) ==  20 ? 'U': (B) ==  21 ? 'V' : (B) == 22 ? 'W' : (B) == 23  ? 'X' :  \
     (B) ==  24 ? 'Y': (B) ==  25 ? 'Z' : (B) == 26 ? '2' : (B) == 27  ? '3' :  \
     (B) ==  28 ? '4': (B) ==  29 ? '5' : (B) == 30 ? '6' : (B) == 31  ? '7' :  \
     -1))

#define b32_to_c(B) ((char)( \
     (B) == 'A' ? 0  : (B) == 'B' ? 1  : (B) == 'C' ? 2   : (B) == 'D' ? 3 : \
     (B) == 'E' ? 4  : (B) == 'F' ? 5  : (B) == 'G' ? 6   : (B) == 'H' ? 7 : \
     (B) == 'I' ? 8  : (B) == 'J' ? 9  : (B) == 'K' ? 10  : (B) == 'L' ? 11: \
     (B) == 'M' ? 12 : (B) == 'N' ? 13 : (B) == 'O' ? 14  : (B) == 'P' ? 15: \
     (B) == 'Q' ? 16 : (B) == 'R' ? 17 : (B) == 'S' ? 18  : (B) == 'T' ? 19: \
     (B) == 'U' ? 20 : (B) == 'V' ? 21 : (B) == 'W' ? 22  : (B) == 'X' ? 23: \
     (B) == 'Y' ? 24 : (B) == 'Z' ? 25 : (B) == '2' ? 26  : (B) == '3' ? 27: \
     (B) == '4' ? 28 : (B) == '5' ? 29 : (B) == '6' ? 30  : (B) == '7' ? 31: \
     -1))

size_t base32_get_decoded_size(size_t encoded_bytes)
{
  size_t length;
  if(encoded_bytes % 8 == 0)
  {
    length = (encoded_bytes / 8) * 5;
  }
  else
  {
    size_t data_len = encoded_bytes % 8;
    length = ((encoded_bytes + 8) / 8) * 5;

    if(data_len <= 2)
      length -= 4;
    else if(data_len <= 4)
      length -= 3;
    else if(data_len <= 5)
      length -= 2;
    else if(data_len <= 7)
      length -= 1;
  }

  return length;
}

char *base32_encode(uint8_t *data, size_t length)
{
  char *encoded;
  size_t i;

  /* 5 bytes become 8 */
  size_t out_size = ((((length) + 4) / 5) * 8) + 1;

  encoded = safe_malloc(out_size);

  size_t index_out = 0;
  for(i = 0; i < length; i += 5)
  {
    char out0, out1, out2, out3, out4, out5, out6, out7;
    char in0,  in1,  in2,  in3,  in4;

    in0 = in1 = in2 = in3 = in4 = 0;
    out0 = out1 = out2 = out3 = out4 = out5 = out6 = out7 = 0;

    in0 = (i+0 < length ? data[i+0] : 0);
    in1 = (i+1 < length ? data[i+1] : 0);
    in2 = (i+2 < length ? data[i+2] : 0);
    in3 = (i+3 < length ? data[i+3] : 0);
    in4 = (i+4 < length ? data[i+4] : 0);

    out0 = ((in0 & 0xf8) >> 3);  /* 00000XXX XXXXXXXX XXXXXXXX XXXXXXXX XXXXXXXX */

    out1 = ((in0 & 0x07) << 2) | /* XXXXX111 11XXXXXX XXXXXXXX XXXXXXXX XXXXXXXX */
           ((in1 & 0xc0) >> 6);

    out2 = ((in1 & 0x3e) >> 1);  /* XXXXXXXX XX22222X XXXXXXXX XXXXXXXX XXXXXXXX */

    out3 = ((in1 & 0x01) << 4) | /* XXXXXXXX XXXXXXX3 3333XXXX XXXXXXXX XXXXXXXX */
           ((in2 & 0xF0) >> 4);

    out4 = ((in2 & 0x0F) << 1) | /* XXXXXXXX XXXXXXXX XXXX4444 4XXXXXXX XXXXXXXX */
           ((in3 & 0x80) >> 7);

    out5 = ((in3 & 0x7c) >> 2);  /* XXXXXXXX XXXXXXXX XXXXXXXX X55555XX XXXXXXXX */

    out6 = ((in3 & 0x03) << 3) | /* XXXXXXXX XXXXXXXX XXXXXXXX XXXXXX66 666XXXXX */
           ((in4 & 0xe0) >> 5);

    out7 = ((in4 & 0x1f));       /* XXXXXXXX XXXXXXXX XXXXXXXX XXXXXXXX XXX77777 */


    if(i + 0 < length)
    {
      encoded[index_out+0] = c_to_b32(out0);
      encoded[index_out+1] = c_to_b32(out1);
    }

    if(i + 1 < length)
    {
      encoded[index_out+2] = c_to_b32(out2);
      encoded[index_out+3] = c_to_b32(out3);
    }
    else
    {
      encoded[index_out+2] = '\0';
      encoded[index_out+3] = '\0';
    }

    if(i + 2 < length)
    {
      encoded[index_out+4] = c_to_b32(out4);
    }
    else
    {
      encoded[index_out+4] = '\0';
    }

    if(i + 3 < length)
    {
      encoded[index_out+5] = c_to_b32(out5);
      encoded[index_out+6] = c_to_b32(out6);
    }
    else
    {
      encoded[index_out+5] = '\0';
      encoded[index_out+6] = '\0';
    }

    if(i + 4 < length)
    {
      encoded[index_out+7] = c_to_b32(out7);
    }
    else
    {
      encoded[index_out+7] = '\0';
    }

    /* Set the next character to '\0' in case we're at the end. */
    encoded[index_out+8] = '\0';

    index_out += 8;
  }

  return encoded;
}

uint8_t *base32_decode(const char *text, size_t *length)
{
  size_t   in_length = (*length == -1) ? strlen(text) : (*length);
  uint8_t *decoded;
  size_t   i;

  *length = base32_get_decoded_size(in_length);

  decoded = safe_malloc(*length);

  size_t index_out = 0;
  for(i = 0; i < in_length; i += 8)
  {
    char in0,  in1,  in2,  in3,  in4 , in5,  in6,  in7;

    in0 = in1 = in2 = in3 = in4 = in5 = in6 = in7 = 0;

    in0 = b32_to_c(text[i + 0]);

    if(index_out + 0 < *length)
    {
      in1 = b32_to_c(text[i + 1]);
      in2 = b32_to_c(text[i + 2]);
      decoded[index_out+0] = ((in0 << 3) & 0xF8) | (in1 >> 2); /* 00000111 */

      if(index_out + 1 < *length)
      {
        in3 = b32_to_c(text[i + 3]);
        decoded[index_out+1] = ((in1 << 6) & 0xC0) | ((in2 << 1) & 0x3E) | (in3 >> 4); /* 11222223 */

        if(index_out + 2 < *length)
        {
          in4 = b32_to_c(text[i + 4]);
          decoded[index_out+2] = ((in3 << 4) & 0xF0) | (in4 >> 1); /* 33334444 */

          if(index_out + 3 < *length)
          {
            in5 = b32_to_c(text[i + 5]);
            in6 = b32_to_c(text[i + 6]);
            decoded[index_out+3] = ((in4 << 7) & 0x80) | ((in5 << 2) & 0x7c) | (in6 >> 3); /* 45555566 */

            if(index_out + 4 < *length)
            {
              in7 = b32_to_c(text[i + 7]);
              decoded[index_out+4] = ((in6 << 5) & 0xE0) | (in7 & 0x1F); /* 66677777 */
            }
          }
        }
      }
    }

    index_out += 5;
  }

  return decoded;
}

#if 0
#define TESTS 20000
int main(int argc, const char *argv[])
{
  uint8_t input[TESTS];
  char    *output;
  uint8_t *other_output;
  size_t   size_in;
  size_t   size_out;

  size_t i, j;
  size_t predicted_size;

  srand(time(0));

  for(i = 0; i < TESTS; i++)
  {
    printf("%zu\n", i);
    size_in = i;

    for(j = 0; j < size_in; j++)
      input[j] = rand();

    /* Try to send 'invalid' data into the decoder. */
    size_out = -1;
    other_output = base32_decode(input, &size_out);
    if(other_output)
      safe_free(other_output);


    input[size_in] = 0;
    output = base32_encode(input, size_in);
    size_out = -1;
    other_output = base32_decode(output, &size_out);

    predicted_size = base32_get_decoded_size(strlen(output));
    if(predicted_size != size_out)
      printf("Predicted size = %zu, actual size = %zu!\n", predicted_size, size_out);

    if(size_out != size_in)
    {
      printf("Size error! In = %zu, out = %zu\n", size_in, size_out);
      exit(1);
    }

    if(memcmp(input, other_output, size_in))
    {
      printf("Output doesn't match input [base32]!\n");
      exit(1);
    }

    safe_free(output);
    safe_free(other_output);
    /* Try to send 'invalid' data into the decoder. */
    size_out = -1;
    other_output = hex_decode(input, &size_out);
    if(other_output)
      safe_free(other_output);

    output = hex_encode(input, size_in);
    size_out = -1;
    other_output = hex_decode(output, &size_out);

    if(size_out != size_in)
    {
      printf("Size error! In = %zu, out = %zu\n", size_in, size_out);
      exit(1);
    }

    if(memcmp(input, other_output, size_in))
    {
      printf("Output doesn't match input [hex]!\n");
      exit(1);
    }

    safe_free(output);
    safe_free(other_output);
  }

  print_memory();

  return 0;
}
#endif
