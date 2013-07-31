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

static char *hex_chars = "01234567890abcdef";
static char *encode_hex(uint8_t *value, size_t length)
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

uint8_t *decode_hex(char *text, size_t *length)
{
  uint8_t *decoded = safe_malloc(strlen(text) / 2);
  size_t i;

  *length = strlen(text) / 2;

  for(i = 0; i < strlen(text); i += 2)
  {
    char c1, c2;

    if(text[i] >= '0' && text[i] <= '9')
      c1 = text[i] - '0';
    else if(text[i] >= 'a' && text[i] <= 'f')
      c1 = text[i] - 'a';
    else if(text[i] >= 'A' && text[i] <= 'F')
      c1 = text[i] - 'A';
    else
      return NULL;

    if(text[i+1] >= '0' && text[i+1] <= '9')
      c2 = text[i+1] - '0';
    else if(text[i+1] >= 'a' && text[i+1] <= 'f')
      c2 = text[i+1] - 'a';
    else if(text[i+1] >= 'A' && text[i+1] <= 'F')
      c2 = text[i+1] - 'A';
    else
      return NULL;


    decoded[i / 2] = (c1 << 4) | (c2 << 0);

  }

  return decoded;
}

static char base32_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567=";
static char find_char(char c)
{
  size_t i;

  if(c == '=')
    return 0;

  for(i = 0; i < sizeof(base32_chars); i++)
    if(base32_chars[i] == c)
      return i;

  printf("Couldn't find char: %c\n", c);
  return -1;
}

static char *encode_base32(uint8_t *data, size_t length)
{
  char *encoded;
  size_t i;

  /* 5 bytes become 8 */
  size_t out_size = (((length + 5 - 1) / 5) * 8) + 1;

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
      encoded[index_out+0] = base32_chars[out0];
      encoded[index_out+1] = base32_chars[out1];
    }

    if(i + 1 < length)
    {
      encoded[index_out+2] = base32_chars[out2];
      encoded[index_out+3] = base32_chars[out3];
    }
    else
    {
      encoded[index_out+2] = '=';
      encoded[index_out+3] = '=';
    }

    if(i + 2 < length)
    {
      encoded[index_out+4] = base32_chars[out4];
    }
    else
    {
      encoded[index_out+4] = '=';
    }

    if(i + 3 < length)
    {
      encoded[index_out+5] = base32_chars[out5];
      encoded[index_out+6] = base32_chars[out6];
    }
    else
    {
      encoded[index_out+5] = '=';
      encoded[index_out+6] = '=';
    }

    if(i + 4 < length)
    {
      encoded[index_out+7] = base32_chars[out7];
    }
    else
    {
      encoded[index_out+7] = '=';
    }

    /* Set the next character to '\0' in case we're at the end. */
    encoded[index_out+8] = '\0';

    index_out += 8;
  }

  return encoded;
}

uint8_t *decode_base32(const char *text, size_t *length)
{
  uint8_t *decoded;
  size_t i;

  /* 5 bytes become 8 */
  char *end = strchr(text, '=');
  if(!end)
  {
    *length = (strlen(text) / 8) * 5;
  }
  else
  {
    size_t data_len = (end - text) % 8;
    *length = (strlen(text) / 8) * 5;

    if(data_len <= 2)
      *length -= 4;
    else if(data_len <= 4)
      *length -= 3;
    else if(data_len <= 5)
      *length -= 2;
    else if(data_len <= 7)
      *length -= 1;
  }

  decoded = safe_malloc(*length);

  size_t index_out = 0;
  for(i = 0; i < strlen(text); i += 8)
  {
    char in0,  in1,  in2,  in3,  in4 , in5,  in6,  in7;
    char out0, out1, out2, out3, out4;

    in0 = in1 = in2 = in3 = in4 = in5 = in6 = in7 = 0;
    out0 = out1 = out2 = out3 = out4 = 0;

    decoded[index_out+0] = '\0';
    decoded[index_out+1] = '\0';
    decoded[index_out+2] = '\0';
    decoded[index_out+3] = '\0';
    decoded[index_out+4] = '\0';

    in0 = find_char(text[i + 0]);

    if(index_out + 0 < *length)
    {
      in1 = find_char(text[i + 1]);
      in2 = find_char(text[i + 2]);
      decoded[index_out+0] = ((in0 << 3) & 0xF8) | (in1 >> 2); /* 00000111 */

      if(index_out + 1 < *length)
      {
        in3 = find_char(text[i + 3]);
        decoded[index_out+1] = ((in1 << 6) & 0xC0) | ((in2 << 1) & 0x3E) | (in3 >> 4); /* 11222223 */

        if(index_out + 2 < *length)
        {
          in4 = find_char(text[i + 4]);
          decoded[index_out+2] = ((in3 << 4) & 0xF0) | (in4 >> 1); /* 33334444 */

          if(index_out + 3 < *length)
          {
            in5 = find_char(text[i + 5]);
            in6 = find_char(text[i + 6]);
            decoded[index_out+3] = ((in4 << 7) & 0x80) | ((in5 << 2) & 0x7c) | (in6 >> 3); /* 45555566 */

            if(index_out + 4 < *length)
            {
              in7 = find_char(text[i + 7]);
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

#define TESTS 1000000
#define MAX_LENGTH 26
int main(int argc, const char *argv[])
{
  uint8_t input[1024];
  char    *output;
  uint8_t *other_output;
  size_t   size_in;
  size_t   size_out;

  size_t i, j;

  for(i = 0; i < TESTS; i++)
  {
    size_in = rand() % MAX_LENGTH;
    for(j = 0; j < size_in; j++)
      input[j] = rand() % 0xFF;

    output = encode_base32(input, size_in);
    other_output = decode_base32(output, &size_out);

    /*
    for(j = 0; j < size_in; j++)
      printf("%02x", input[j]);
    printf("\n");
    for(j = 0; j < size_in; j++)
      printf("%02x", other_output[j]);
    printf("\n"); */

    if(size_out != size_in)
    {
      printf("Size error! In = %d, out = %d\n", size_in, size_out);
      exit(1);
    }

    if(memcmp(input, other_output, size_in))
    {
      printf("Output doesn't match input!\n");
      exit(1);
    }
  }

  return 0;
}
