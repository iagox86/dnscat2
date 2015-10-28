/* dns.c
 * By Ron Bowes
 * Created December, 2009
 *
 * (See LICENSE.md)
 */

#include <assert.h>
#include <ctype.h> /* For isspace(). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#include <winsock2.h>
#include <windns.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif

#include "buffer.h"
#include "memory.h"

#include "dns.h"

static void buffer_add_dns_name(buffer_t *buffer, char *name)
{
  char *domain_base = safe_strdup(name);
  char *domain_start;
  char *domain_end;

  domain_start = domain_base;
  while((domain_end = strchr(domain_start, '.')))
  {
    /* Set the end of the string to null. */
    *domain_end = '\0';
    /* Add it to the buffer. */
    buffer_add_int8(buffer, strlen(domain_start));
    buffer_add_string(buffer, domain_start);

    /* Move the 'start' string to the next character. */
    domain_start = domain_end + 1;
  }
  /* Add the last part to the buffer, if we have one (if not, just add the null byte). */
  if(strlen(domain_start))
  {
    buffer_add_int8(buffer, strlen(domain_start));
    buffer_add_string(buffer, domain_start);
  }

  /* Add the final null byte. */
  buffer_add_int8(buffer, 0x00);

  /* Can't forget to free our pointer. */
  safe_free(domain_base);
}

static char *buffer_read_dns_name_at(buffer_t *buffer, uint32_t offset, uint32_t *real_length)
{
  uint8_t  piece_length;
  uint32_t pos = 0;
  buffer_t *ret = buffer_create(BO_NETWORK);

  /* Read the first character -- it's the size of the initial string. */
  piece_length = buffer_read_int8_at(buffer, offset + pos);
  pos++;

  while(piece_length)
  {
    if(piece_length & 0x80)
    {
      if(piece_length == 0xc0)
      {
        uint8_t relative_pos = buffer_read_int8_at(buffer, offset + pos);
        char *new_data;
        pos++;

        new_data = buffer_read_dns_name_at(buffer, relative_pos, NULL);
        buffer_add_string(ret, new_data);
        safe_free(new_data);

        /* Setting piece_length to 0 makes the loop end. */
        piece_length = 0;
      }
      else
      {
        fprintf(stderr, "DNS server returned an unknown character in the string: 0x%02x\n", piece_length);
        DIE("Couldn't process string");
      }
    }
    else
    {
      uint8_t *piece = (uint8_t*) safe_malloc(piece_length);
      buffer_read_bytes_at(buffer, offset + pos, piece, piece_length);
      buffer_add_bytes(ret, piece, piece_length);
      safe_free(piece);

      pos = pos + piece_length;

      /* Read the next length. */
      piece_length = buffer_read_int8_at(buffer, offset + pos);

      /* If the next piece exists, add a period. */
      if(piece_length)
        buffer_add_int8(ret, '.');

      /* Increment the position. */
      pos++;
    }
  }

  /* Add a final null terminator to the string. */
  buffer_add_int8(ret, '\0');

  if(real_length)
    *real_length = pos;

  return (char*)buffer_create_string_and_destroy(ret, NULL);
}

static char *buffer_read_next_dns_name(buffer_t *buffer)
{
  char *result;
  uint32_t actual_length;

  result = buffer_read_dns_name_at(buffer, buffer_get_current_offset(buffer), &actual_length);
  buffer_set_current_offset(buffer, buffer_get_current_offset(buffer) + actual_length);

  return result;
}

static char *buffer_read_ipv4_address_at(buffer_t *buffer, uint32_t offset, char result[16])
{
#ifdef WIN32
  printf("NOT IMPLEMENTED!\n");
  exit(1);
#else
  uint8_t addr[4];

  buffer_read_bytes_at(buffer, offset, addr, 4);
  inet_ntop(AF_INET, addr, result, 16);
#endif

  return result;
}

static char *buffer_read_next_ipv4_address(buffer_t *buffer, char result[16])
{
  buffer_read_ipv4_address_at(buffer, buffer_get_current_offset(buffer), result);
  buffer_set_current_offset(buffer, buffer_get_current_offset(buffer) + 4);

  return result;
}

static buffer_t *buffer_add_ipv4_address(buffer_t *buffer, char *address)
{
#ifdef WIN32
  SOCKADDR_IN  MyAddrValue;
  int MyAddrSize = sizeof(struct sockaddr_storage);
  MyAddrValue.sin_family = AF_INET;
  WSAStringToAddressA(address, AF_INET, NULL, (LPSOCKADDR)&MyAddrValue, &MyAddrSize);

  buffer_add_int8(buffer, MyAddrValue.sin_addr.S_un.S_un_b.s_b1);
  buffer_add_int8(buffer, MyAddrValue.sin_addr.S_un.S_un_b.s_b2);
  buffer_add_int8(buffer, MyAddrValue.sin_addr.S_un.S_un_b.s_b3);
  buffer_add_int8(buffer, MyAddrValue.sin_addr.S_un.S_un_b.s_b4);
#else
  uint8_t out[4];
  inet_pton(AF_INET, address, out);
  buffer_add_bytes(buffer, out, 4);
#endif

  return buffer;
}

static char *buffer_read_ipv6_address_at(buffer_t *buffer, uint32_t offset, char result[40])
{
#ifdef WIN32
  printf("NOT IMPLEMENTED!\n");
  exit(1);
#else
  uint8_t addr[16];

  buffer_read_bytes_at(buffer, offset, addr, 16);
  inet_ntop(AF_INET6, addr, result, 40);
#endif

  return result;
}

static char *buffer_read_next_ipv6_address(buffer_t *buffer, char result[40])
{
  buffer_read_ipv6_address_at(buffer, buffer_get_current_offset(buffer), result);
  buffer_set_current_offset(buffer, buffer_get_current_offset(buffer) + 16);

  return result;
}

buffer_t *buffer_add_ipv6_address(buffer_t *buffer, char *address)
{
#ifdef WIN32
/*  Only works on modern versions of Windows -- not good enough.

  SOCKADDR_IN6  MyAddrValue;
  int MyAddrSize = sizeof(struct sockaddr_storage);
  MyAddrValue.sin6_family = AF_INET6;

  printf("String: %s\n", address);
  if(WSAStringToAddressA(address, AF_INET6, NULL, (LPSOCKADDR)&MyAddrValue, &MyAddrSize) != 0)
  {
    printf("WSAStringToAddress() failed with error code %ld\n", getlasterror());
  }

  buffer_add_bytes(buffer, MyAddrValue.sin6_addr.u.Byte, 16); */
  fprintf(stderr, "Sorry, IPv6 (AAAA) addresses aren't supported on Windows.\n");
  buffer_add_bytes(buffer, "AAAAAAAAAAAAAAAA", 16);
#else
  uint8_t out[16];

  inet_pton(AF_INET6, address, out);
  buffer_add_bytes(buffer, out, 16);
#endif

  return buffer;
}

static dns_t *dns_create_internal()
{
  dns_t *dns = (dns_t*) safe_malloc(sizeof(dns_t));
  memset(dns, 0, sizeof(dns_t));

  return dns;
}

dns_t *dns_create(dns_opcode_t opcode, dns_flag_t flags, dns_rcode_t rcode)
{
  dns_t *dns = dns_create_internal();

  dns->trn_id = rand() & 0xFFFF;
  dns->opcode = opcode;
  dns->flags  = flags;
  dns->rcode  = rcode;

  return dns;
}

dns_t *dns_create_from_packet(uint8_t *packet, size_t length)
{
  uint16_t i;
  buffer_t *buffer = buffer_create_with_data(BO_NETWORK, packet, length);
  dns_t *dns = dns_create_internal();
  uint16_t flags;

  dns->trn_id           = buffer_read_next_int16(buffer);
  flags                 = buffer_read_next_int16(buffer);
  dns->question_count   = buffer_read_next_int16(buffer);
  dns->answer_count     = buffer_read_next_int16(buffer);
  dns->authority_count  = buffer_read_next_int16(buffer);
  dns->additional_count = buffer_read_next_int16(buffer);

  /* Parse the 'flags' field:
   * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
   * |                               1  1  1  1  1  1|
   * | 0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5|
   * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
   * |QR|   Opcode  |AA|TC|RD|RA|   Z    |   RCODE   |
   * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+ */
  dns->opcode = flags & 0x7800;
  dns->flags  = flags & 0x8780;
  dns->rcode  = flags & 0x000F;

  if(dns->question_count)
  {
    dns->questions = (question_t*) safe_malloc(dns->question_count * sizeof(question_t));
    for(i = 0; i < dns->question_count; i++)
    {
      dns->questions[i].name = buffer_read_next_dns_name(buffer);
      dns->questions[i].type  = buffer_read_next_int16(buffer);
      dns->questions[i].class = buffer_read_next_int16(buffer);
    }
  }

  if(dns->answer_count)
  {
    dns->answers = (answer_t*) safe_malloc(dns->answer_count * sizeof(answer_t));
    for(i = 0; i < dns->answer_count; i++)
    {
      dns->answers[i].question = buffer_read_next_dns_name(buffer); /* The question. */
      dns->answers[i].type     = buffer_read_next_int16(buffer); /* Type. */
      dns->answers[i].class    = buffer_read_next_int16(buffer); /* Class. */
      dns->answers[i].ttl      = buffer_read_next_int32(buffer); /* Time to live. */
      dns->answers[i].answer   = (answer_types_t *) safe_malloc(sizeof(answer_types_t));

      if(dns->answers[i].type == _DNS_TYPE_A) /* 0x0001 */
      {
        buffer_read_next_int16(buffer); /* String size (don't care) */

        dns->answers[i].answer->A.address = safe_malloc(16);
        buffer_peek_next_bytes(buffer, dns->answers[i].answer->A.bytes, 4);
        buffer_read_next_ipv4_address(buffer, dns->answers[i].answer->A.address);
      }
      else if(dns->answers[i].type == _DNS_TYPE_NS) /* 0x0002 */
      {
        buffer_read_next_int16(buffer); /* String size. */
        dns->answers[i].answer->NS.name = buffer_read_next_dns_name(buffer); /* The answer. */
      }
      else if(dns->answers[i].type == _DNS_TYPE_CNAME) /* 0x0005 */
      {
        buffer_read_next_int16(buffer); /* String size (don't care). */
        dns->answers[i].answer->CNAME.name = buffer_read_next_dns_name(buffer); /* The answer. */
      }
      else if(dns->answers[i].type == _DNS_TYPE_MX) /* 0x000F */
      {
        buffer_read_next_int16(buffer); /* String size (don't care). */
        dns->answers[i].answer->MX.preference = buffer_read_next_int16(buffer); /* Preference. */
        dns->answers[i].answer->MX.name       = buffer_read_next_dns_name(buffer); /* The answer. */
      }
      else if(dns->answers[i].type == _DNS_TYPE_TEXT) /* 0x0010 */
      {
        buffer_read_next_int16(buffer); /* String size (don't care). */
        dns->answers[i].answer->TEXT.length = buffer_read_next_int8(buffer); /* The actual length. */
        dns->answers[i].answer->TEXT.text = safe_malloc(dns->answers[i].answer->TEXT.length); /* Allocate room for the answer. */
        buffer_read_next_bytes(buffer, dns->answers[i].answer->TEXT.text, dns->answers[i].answer->TEXT.length); /* Read the answer. */
      }
#ifndef WIN32
      else if(dns->answers[i].type == _DNS_TYPE_AAAA) /* 0x001C */
      {
        buffer_read_next_int16(buffer); /* String size (don't care). */

        dns->answers[i].answer->AAAA.address = safe_malloc(40);
        buffer_peek_next_bytes(buffer, dns->answers[i].answer->AAAA.bytes, 16);
        buffer_read_next_ipv6_address(buffer, dns->answers[i].answer->AAAA.address);
      }
#endif
      else if(dns->answers[i].type == _DNS_TYPE_NB) /* 0x0020 */
      {
        buffer_read_next_int16(buffer); /* String size (don't care). */

        dns->answers[i].answer->NB.flags   = buffer_read_next_int16(buffer);
        dns->answers[i].answer->NB.address = safe_malloc(16);
        buffer_read_next_ipv4_address(buffer, dns->answers[i].answer->NB.address);
      }
      else if(dns->answers[i].type == _DNS_TYPE_NBSTAT) /* 0x0021 */
      {
        uint8_t j;

        uint16_t size = buffer_read_next_int16(buffer); /* String size (don't care). */
        dns->answers[i].answer->NBSTAT.name_count = buffer_read_next_int8(buffer);
        dns->answers[i].answer->NBSTAT.names      = (NBSTAT_name_t*) safe_malloc(sizeof(NBSTAT_name_t) * dns->answers[i].answer->NBSTAT.name_count);

        /* Read the list of names. */
        for(j = 0; j < dns->answers[i].answer->NBSTAT.name_count; j++)
        {
          char  tmp[16];
          char *end;

          /* Read the full name. */
          buffer_read_next_bytes(buffer, tmp, 16);

          /* The type is the last byte -- read it then terminate the string properly. */
          dns->answers[i].answer->NBSTAT.names[j].name_type = tmp[15];
          tmp[15] = 0;

          /* Find the end and, if it was found, terminate it. */
          end = strchr(tmp, ' ');
          if(end)
            *end = 0;

          /* Save this name. */
          dns->answers[i].answer->NBSTAT.names[j].name = safe_strdup(tmp);

          /* Finally, read the flags. */
          dns->answers[i].answer->NBSTAT.names[j].name_flags = buffer_read_next_int16(buffer);
        }

        /* Read the rest of the data -- for a bit of safety so we don't read too far, do some math to figure out exactly what's left. */
        buffer_read_next_bytes(buffer, dns->answers[i].answer->NBSTAT.stats, MIN(64, size - 1 - (dns->answers[i].answer->NBSTAT.name_count * 16)));
      }
      else
      {
        uint16_t size;

        fprintf(stderr, "WARNING: Don't know how to parse an answer of type 0x%04x (discarding)\n", dns->answers[i].type);
        size = buffer_read_next_int16(buffer);
        buffer_consume(buffer, size);
      }
    }
  }

  /* TODO */
  if(dns->authority_count)
  {
    dns->authorities = (authority_t*) safe_malloc(dns->question_count * sizeof(question_t));
  }

  if(dns->additional_count)
  {
    dns->additionals = (additional_t*) safe_malloc(dns->additional_count * sizeof(additional_t));
    for(i = 0; i < dns->additional_count; i++)
    {
      dns->additionals[i].question   = buffer_read_next_dns_name(buffer); /* The question. */
      dns->additionals[i].type       = buffer_read_next_int16(buffer); /* Type. */
      dns->additionals[i].class      = buffer_read_next_int16(buffer); /* Class. */
      dns->additionals[i].ttl        = buffer_read_next_int32(buffer); /* Time to live. */
      dns->additionals[i].additional = (additional_types_t *) safe_malloc(sizeof(additional_types_t));

      if(dns->additionals[i].type == _DNS_TYPE_A) /* 0x0001 */
      {
        buffer_read_next_int16(buffer); /* String size (don't care) */

        dns->additionals[i].additional->A.address = safe_malloc(16);
        buffer_read_next_ipv4_address(buffer, dns->additionals[i].additional->A.address);
      }
      else if(dns->additionals[i].type == _DNS_TYPE_NS) /* 0x0002 */
      {
        buffer_read_next_int16(buffer); /* String size. */
        dns->additionals[i].additional->NS.name = buffer_read_next_dns_name(buffer); /* The additional. */
      }
      else if(dns->additionals[i].type == _DNS_TYPE_CNAME) /* 0x0005 */
      {
        buffer_read_next_int16(buffer); /* String size (don't care). */
        dns->additionals[i].additional->CNAME.name = buffer_read_next_dns_name(buffer); /* The additional. */
      }
      else if(dns->additionals[i].type == _DNS_TYPE_MX) /* 0x000F */
      {
        buffer_read_next_int16(buffer); /* String size (don't care). */
        dns->additionals[i].additional->MX.preference = buffer_read_next_int16(buffer); /* Preference. */
        dns->additionals[i].additional->MX.name       = buffer_read_next_dns_name(buffer); /* The additional. */
      }
      else if(dns->additionals[i].type == _DNS_TYPE_TEXT) /* 0x0010 */
      {
        buffer_read_next_int16(buffer); /* String size (don't care). */
        dns->additionals[i].additional->TEXT.length = buffer_read_next_int8(buffer); /* The actual length. */
        dns->additionals[i].additional->TEXT.text = safe_malloc(dns->additionals[i].additional->TEXT.length); /* Allocate room for the additional. */
        buffer_read_next_bytes(buffer, dns->additionals[i].additional->TEXT.text, dns->additionals[i].additional->TEXT.length); /* Read the additional. */
      }
#ifndef WIN32
      else if(dns->additionals[i].type == _DNS_TYPE_AAAA) /* 0x001C */
      {
        buffer_read_next_int16(buffer); /* String size (don't care). */

        dns->additionals[i].additional->AAAA.address = safe_malloc(40);
        buffer_read_next_ipv6_address(buffer, dns->additionals[i].additional->AAAA.address);
      }
#endif
      else if(dns->additionals[i].type == _DNS_TYPE_NB) /* 0x0020 */
      {
        buffer_read_next_int16(buffer); /* String size (don't care). */

        dns->additionals[i].additional->NB.flags   = buffer_read_next_int16(buffer);
        dns->additionals[i].additional->NB.address = safe_malloc(16);
        buffer_read_next_ipv4_address(buffer, dns->additionals[i].additional->NB.address);
      }
      else if(dns->additionals[i].type == _DNS_TYPE_NBSTAT) /* 0x0021 */
      {
        uint8_t j;

        uint16_t size = buffer_read_next_int16(buffer); /* String size (don't care). */
        dns->additionals[i].additional->NBSTAT.name_count = buffer_read_next_int8(buffer);
        dns->additionals[i].additional->NBSTAT.names      = (NBSTAT_name_t*) safe_malloc(sizeof(NBSTAT_name_t) * dns->additionals[i].additional->NBSTAT.name_count);

        /* Read the list of names. */
        for(j = 0; j < dns->additionals[i].additional->NBSTAT.name_count; j++)
        {
          char  tmp[16];
          char *end;

          /* Read the full name. */
          buffer_read_next_bytes(buffer, tmp, 16);

          /* The type is the last byte -- read it then terminate the string properly. */
          dns->additionals[i].additional->NBSTAT.names[j].name_type = tmp[15];
          tmp[15] = 0;

          /* Find the end and, if it was found, terminate it. */
          end = strchr(tmp, ' ');
          if(end)
            *end = 0;

          /* Save this name. */
          dns->additionals[i].additional->NBSTAT.names[j].name = safe_strdup(tmp);

          /* Finally, read the flags. */
          dns->additionals[i].additional->NBSTAT.names[j].name_flags = buffer_read_next_int16(buffer);
        }

        /* Read the rest of the data -- for a bit of safety so we don't read too far, do some math to figure out exactly what's left. */
        buffer_read_next_bytes(buffer, dns->additionals[i].additional->NBSTAT.stats, MIN(64, size - 1 - (dns->additionals[i].additional->NBSTAT.name_count * 16)));
      }
      else
      {
        uint16_t size;

/*        fprintf(stderr, "WARNING: Don't know how to parse an additional of type 0x%04x (discarding)\n", dns->additionals[i].type);*/
        size = buffer_read_next_int16(buffer);
        buffer_consume(buffer, size);
      }
    }
  }

  buffer_destroy(buffer);

  return dns;
}

void dns_destroy(dns_t *dns)
{
  uint32_t i;

  if(dns->questions)
  {
    /* Free the names. */
    for(i = 0; i < dns->question_count; i++)
      safe_free(dns->questions[i].name);

    /* Free the question. */
    safe_free(dns->questions);
  }

  if(dns->answers)
  {
    /* Free the names. */
    for(i = 0; i < dns->answer_count; i++)
    {
      safe_free(dns->answers[i].question);

      if(dns->answers[i].type == _DNS_TYPE_A)
      {
        safe_free(dns->answers[i].answer->A.address);
      }
      else if(dns->answers[i].type == _DNS_TYPE_NS)
      {
        safe_free(dns->answers[i].answer->NS.name);
      }
      else if(dns->answers[i].type == _DNS_TYPE_CNAME)
      {
        safe_free(dns->answers[i].answer->CNAME.name);
      }
      else if(dns->answers[i].type == _DNS_TYPE_MX)
      {
        safe_free(dns->answers[i].answer->MX.name);
      }
      else if(dns->answers[i].type == _DNS_TYPE_TEXT)
      {
        safe_free(dns->answers[i].answer->TEXT.text);
      }
#ifndef WIN32
      else if(dns->answers[i].type == _DNS_TYPE_AAAA)
      {
        safe_free(dns->answers[i].answer->AAAA.address);
      }
#endif
      else if(dns->answers[i].type == _DNS_TYPE_NB)
      {
        safe_free(dns->answers[i].answer->NB.address);
      }
      else if(dns->answers[i].type == _DNS_TYPE_NBSTAT)
      {
        uint8_t j;
        for(j = 0; j < dns->answers[i].answer->NBSTAT.name_count; j++)
          safe_free(dns->answers[i].answer->NBSTAT.names[j].name);
        safe_free(dns->answers[i].answer->NBSTAT.names);
      }
      safe_free(dns->answers[i].answer);
    }
    safe_free(dns->answers);
  }

  if(dns->authorities)
  {
    safe_free(dns->authorities);
  }

  if(dns->additionals)
  {
    /* Free the names. */
    for(i = 0; i < dns->additional_count; i++)
    {
      safe_free(dns->additionals[i].question);

      if(dns->additionals[i].type == _DNS_TYPE_A)
      {
        safe_free(dns->additionals[i].additional->A.address);
      }
      else if(dns->additionals[i].type == _DNS_TYPE_NS)
      {
        safe_free(dns->additionals[i].additional->NS.name);
      }
      else if(dns->additionals[i].type == _DNS_TYPE_CNAME)
      {
        safe_free(dns->additionals[i].additional->CNAME.name);
      }
      else if(dns->additionals[i].type == _DNS_TYPE_MX)
      {
        safe_free(dns->additionals[i].additional->MX.name);
      }
      else if(dns->additionals[i].type == _DNS_TYPE_TEXT)
      {
        safe_free(dns->additionals[i].additional->TEXT.text);
      }
#ifndef WIN32
      else if(dns->additionals[i].type == _DNS_TYPE_AAAA)
      {
        safe_free(dns->additionals[i].additional->AAAA.address);
      }
#endif
      else if(dns->additionals[i].type == _DNS_TYPE_NB)
      {
        safe_free(dns->additionals[i].additional->NB.address);
      }
      else if(dns->additionals[i].type == _DNS_TYPE_NBSTAT)
      {
        uint8_t j;
        for(j = 0; j < dns->additionals[i].additional->NBSTAT.name_count; j++)
          safe_free(dns->additionals[i].additional->NBSTAT.names[j].name);
        safe_free(dns->additionals[i].additional->NBSTAT.names);
      }
      safe_free(dns->additionals[i].additional);
    }
    safe_free(dns->additionals);
  }

  safe_free(dns);
}

void dns_set_trn_id(dns_t *dns, uint16_t trn_id)
{
  dns->trn_id = trn_id;
}

uint16_t dns_get_trn_id(dns_t *dns)
{
  return dns->trn_id;
}

void dns_set_flags(dns_t *dns, uint16_t flags)
{
  dns->flags = flags;
}

uint16_t dns_get_flags(dns_t *dns)
{
  return dns->flags;
}

void dns_add_question(dns_t *dns, char *name, dns_type_t type, dns_class_t class)
{
  /* Increment the question count. */
  dns->question_count = dns->question_count + 1;

  /* Create or embiggen the questions array (this isn't efficient, but it typically
   * isn't called much. */
  if(dns->questions)
    dns->questions = (question_t*) safe_realloc(dns->questions, sizeof(question_t) * dns->question_count);
  else
    dns->questions = (question_t*) safe_malloc(sizeof(question_t));

  /* Set up the last element. */
  (dns->questions[dns->question_count - 1]).name  = safe_strdup(name);
  (dns->questions[dns->question_count - 1]).type  = type;
  (dns->questions[dns->question_count - 1]).class = class;
}

void dns_add_netbios_question(dns_t *dns, char *name, uint8_t name_type, char *scope, dns_type_t type, dns_class_t class)
{
  /* Create a buffer where we're going to build our complete name. */
  buffer_t *buffer = buffer_create(BO_NETWORK);
  char     *encoded;
  char      padding  = !strcmp(name, "*") ? '\0' : ' ';
  size_t    i;

  /* Encode the name. */
  for(i = 0; i < strlen(name); i++)
  {
    buffer_add_int8(buffer, ((name[i] >> 4) & 0x0F) + 'A');
    buffer_add_int8(buffer, ((name[i] >> 0) & 0x0F) + 'A');
  }

  /* Padding the name. */
  for(; i < 15; i++)
  {
    buffer_add_int8(buffer, ((padding >> 4) & 0x0F) + 'A');
    buffer_add_int8(buffer, ((padding >> 0) & 0x0F) + 'A');
  }

  /* Type. */
  buffer_add_int8(buffer, ((name_type >> 4) & 0x0F) + 'A');
  buffer_add_int8(buffer, ((name_type >> 0) & 0x0F) + 'A');

  /* Scope. */
  buffer_add_int8(buffer, '.');
  if(scope)
    buffer_add_string(buffer, scope);

  /* Null terminator. */
  buffer_add_int8(buffer, 0);

  /* Add the question as usual. */
  encoded = (char*)buffer_create_string_and_destroy(buffer, NULL);
  dns_add_question(dns, encoded, type, class);
  safe_free(encoded);
}

static void dns_add_answer(dns_t *dns, char *question, dns_type_t type, dns_class_t class, uint32_t ttl, answer_types_t *answer)
{
  /* Increment the answer count. */
  dns->answer_count = dns->answer_count + 1;

  /* Create or embiggen the answers array (this isn't efficient, but it typically
   * isn't called much. */
  if(dns->answers)
    dns->answers = (answer_t*) safe_realloc(dns->answers, sizeof(answer_t) * dns->answer_count);
  else
    dns->answers = (answer_t*) safe_malloc(sizeof(answer_t));

  /* Set up the last element. */
  (dns->answers[dns->answer_count - 1]).question  = safe_strdup(question);
  (dns->answers[dns->answer_count - 1]).type      = type;
  (dns->answers[dns->answer_count - 1]).class     = class;
  (dns->answers[dns->answer_count - 1]).ttl       = ttl;
  (dns->answers[dns->answer_count - 1]).answer    = answer;

}

void dns_add_answer_A(dns_t *dns, char *question, dns_class_t class, uint32_t ttl, char *address)
{
  answer_types_t *answer = safe_malloc(sizeof(answer_types_t));
  answer->A.address      = safe_strdup(address);
  dns_add_answer(dns, question, _DNS_TYPE_A, class, ttl, answer);
}

void dns_add_answer_NS(dns_t *dns,    char *question, dns_class_t class, uint32_t ttl, char *name)
{
  answer_types_t *answer = safe_malloc(sizeof(answer_types_t));
  answer->NS.name        = safe_strdup(name);
  dns_add_answer(dns, question, _DNS_TYPE_NS, class, ttl, answer);
}

void dns_add_answer_CNAME(dns_t *dns, char *question, dns_class_t class, uint32_t ttl, char *name)
{
  answer_types_t *answer = safe_malloc(sizeof(answer_types_t));
  answer->CNAME.name     = safe_strdup(name);
  dns_add_answer(dns, question, _DNS_TYPE_CNAME, class, ttl, answer);
}

void dns_add_answer_MX(dns_t *dns,    char *question, dns_class_t class, uint32_t ttl, uint16_t preference, char *name)
{
  answer_types_t *answer = safe_malloc(sizeof(answer_types_t));
  answer->MX.preference  = preference;
  answer->MX.name        = safe_strdup(name);
  dns_add_answer(dns, question, _DNS_TYPE_MX, class, ttl, answer);
}

void dns_add_answer_TEXT(dns_t *dns,  char *question, dns_class_t class, uint32_t ttl, uint8_t *text, uint8_t length)
{
  answer_types_t *answer = safe_malloc(sizeof(answer_types_t));
  uint8_t *text_copy     = safe_malloc(length);
  memcpy(text_copy, text, length);
  answer->TEXT.text      = text_copy;
  answer->TEXT.length    = length;
  dns_add_answer(dns, question, _DNS_TYPE_TEXT, class, ttl, answer);
}

#ifndef WIN32
void dns_add_answer_AAAA(dns_t *dns,  char *question, dns_class_t class, uint32_t ttl, char *address)
{
  answer_types_t *answer = safe_malloc(sizeof(answer_types_t));
  answer->AAAA.address   = safe_strdup(address);
  dns_add_answer(dns, question, _DNS_TYPE_AAAA, class, ttl, answer);
}
#endif

void dns_add_answer_NB(dns_t *dns,  char *question, uint8_t question_type, char *scope, dns_class_t class, uint32_t ttl, uint16_t flags, char *address)
{
  /* Create a buffer where we're going to build our complete question. */
  buffer_t       *buffer = buffer_create(BO_NETWORK);
  char           *encoded;
  char            padding  = !strcmp(question, "*") ? '\0' : ' ';
  size_t          i;
  answer_types_t *answer;

  /* Encode the question. */
  for(i = 0; i < strlen(question); i++)
  {
    buffer_add_int8(buffer, ((question[i] >> 4) & 0x0F) + 'A');
    buffer_add_int8(buffer, ((question[i] >> 0) & 0x0F) + 'A');
  }

  /* Padding the question. */
  for(; i < 15; i++)
  {
    buffer_add_int8(buffer, ((padding >> 4) & 0x0F) + 'A');
    buffer_add_int8(buffer, ((padding >> 0) & 0x0F) + 'A');
  }

  /* Type. */
  buffer_add_int8(buffer, ((question_type >> 4) & 0x0F) + 'A');
  buffer_add_int8(buffer, ((question_type >> 0) & 0x0F) + 'A');

  /* Scope. */
  buffer_add_int8(buffer, '.');
  if(scope)
    buffer_add_string(buffer, scope);

  /* Null terminator. */
  buffer_add_int8(buffer, 0);

  /* Add the question as usual. */
  encoded = (char*)buffer_create_string_and_destroy(buffer, NULL);

  answer = safe_malloc(sizeof(answer_types_t));
  answer->NB.flags     = flags;
  answer->NB.address   = safe_strdup(address);
  dns_add_answer(dns, encoded, _DNS_TYPE_NB, class, ttl, answer);

  safe_free(encoded);
}

/* This is pretty much identical to dns_add_answer. */
static void dns_add_additional(dns_t *dns, char *question, dns_type_t type, dns_class_t class, uint32_t ttl, additional_types_t *additional)
{
  /* Increment the additional count. */
  dns->additional_count = dns->additional_count + 1;

  /* Create or embiggen the additionals array (this isn't efficient, but it typically
   * isn't called much. */
  if(dns->additionals)
    dns->additionals = (additional_t*) safe_realloc(dns->additionals, sizeof(additional_t) * dns->additional_count);
  else
    dns->additionals = (additional_t*) safe_malloc(sizeof(additional_t));

  /* Set up the last element. */
  (dns->additionals[dns->additional_count - 1]).question   = safe_strdup(question);
  (dns->additionals[dns->additional_count - 1]).type       = type;
  (dns->additionals[dns->additional_count - 1]).class      = class;
  (dns->additionals[dns->additional_count - 1]).ttl        = ttl;
  (dns->additionals[dns->additional_count - 1]).additional = additional;
}

void dns_add_additional_A(dns_t *dns, char *question, dns_class_t class, uint32_t ttl, char *address)
{
  additional_types_t *additional = safe_malloc(sizeof(additional_types_t));
  additional->A.address      = safe_strdup(address);
  dns_add_additional(dns, question, _DNS_TYPE_A, class, ttl, additional);
}

void dns_add_additional_NS(dns_t *dns,    char *question, dns_class_t class, uint32_t ttl, char *name)
{
  additional_types_t *additional = safe_malloc(sizeof(additional_types_t));
  additional->NS.name        = safe_strdup(name);
  dns_add_additional(dns, question, _DNS_TYPE_NS, class, ttl, additional);
}

void dns_add_additional_CNAME(dns_t *dns, char *question, dns_class_t class, uint32_t ttl, char *name)
{
  additional_types_t *additional = safe_malloc(sizeof(additional_types_t));
  additional->CNAME.name     = safe_strdup(name);
  dns_add_additional(dns, question, _DNS_TYPE_CNAME, class, ttl, additional);
}

void dns_add_additional_MX(dns_t *dns,    char *question, dns_class_t class, uint32_t ttl, uint16_t preference, char *name)
{
  additional_types_t *additional = safe_malloc(sizeof(additional_types_t));
  additional->MX.preference  = preference;
  additional->MX.name        = safe_strdup(name);
  dns_add_additional(dns, question, _DNS_TYPE_MX, class, ttl, additional);
}

void dns_add_additional_TEXT(dns_t *dns,  char *question, dns_class_t class, uint32_t ttl, uint8_t *text, uint8_t length)
{
  additional_types_t *additional = safe_malloc(sizeof(additional_types_t));
  uint8_t *text_copy     = safe_malloc(length);
  memcpy(text_copy, text, length);
  additional->TEXT.text      = text_copy;
  additional->TEXT.length    = length;
  dns_add_additional(dns, question, _DNS_TYPE_TEXT, class, ttl, additional);
}

#ifndef WIN32
void dns_add_additional_AAAA(dns_t *dns,  char *question, dns_class_t class, uint32_t ttl, char *address)
{
  additional_types_t *additional = safe_malloc(sizeof(additional_types_t));
  additional->AAAA.address   = safe_strdup(address);
  dns_add_additional(dns, question, _DNS_TYPE_AAAA, class, ttl, additional);
}
#endif

void dns_add_additional_NB(dns_t *dns,  char *question, uint8_t question_type, char *scope, dns_class_t class, uint32_t ttl, uint16_t flags, char *address)
{
  /* Create a buffer where we're going to build our complete question. */
  buffer_t       *buffer = buffer_create(BO_NETWORK);
  char           *encoded;
  char            padding  = !strcmp(question, "*") ? '\0' : ' ';
  size_t          i;
  additional_types_t *additional;

  /* Encode the question. */
  for(i = 0; i < strlen(question); i++)
  {
    buffer_add_int8(buffer, ((question[i] >> 4) & 0x0F) + 'A');
    buffer_add_int8(buffer, ((question[i] >> 0) & 0x0F) + 'A');
  }

  /* Padding the question. */
  for(; i < 15; i++)
  {
    buffer_add_int8(buffer, ((padding >> 4) & 0x0F) + 'A');
    buffer_add_int8(buffer, ((padding >> 0) & 0x0F) + 'A');
  }

  /* Type. */
  buffer_add_int8(buffer, ((question_type >> 4) & 0x0F) + 'A');
  buffer_add_int8(buffer, ((question_type >> 0) & 0x0F) + 'A');

  /* Scope. */
  buffer_add_int8(buffer, '.');
  if(scope)
    buffer_add_string(buffer, scope);

  /* Null terminator. */
  buffer_add_int8(buffer, 0);

  /* Add the question as usual. */
  encoded = (char*)buffer_create_string_and_destroy(buffer, NULL);

  additional = safe_malloc(sizeof(additional_types_t));
  additional->NB.flags     = flags;
  additional->NB.address   = safe_strdup(address);
  dns_add_additional(dns, encoded, _DNS_TYPE_NB, class, ttl, additional);

  safe_free(encoded);
}

uint8_t *dns_to_packet(dns_t *dns, size_t *length)
{
  uint16_t i;
  uint16_t flags;

  /* Create the buffer. */
  buffer_t *buffer = buffer_create(BO_NETWORK);

  /* Validate and format the flags:
   * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
   * |                               1  1  1  1  1  1|
   * | 0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5|
   * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
   * |QR|   Opcode  |AA|TC|RD|RA|   Z    |   RCODE   |
   * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
   */
  assert((dns->opcode & 0x7800) == dns->opcode);
  assert((dns->rcode  & 0x000F) == dns->rcode);
  assert((dns->flags  & 0x8780) == dns->flags);

  flags = dns->opcode | dns->flags | dns->rcode;

  /* Marshall the base stuff. */
  buffer_add_int16(buffer, dns->trn_id);
  buffer_add_int16(buffer, flags);
  buffer_add_int16(buffer, dns->question_count);
  buffer_add_int16(buffer, dns->answer_count);
  buffer_add_int16(buffer, dns->authority_count);
  buffer_add_int16(buffer, dns->additional_count);

  /* Marshall the other fields. */
  for(i = 0; i < dns->question_count; i++)
  {
    buffer_add_dns_name(buffer, (char*)dns->questions[i].name);
    buffer_add_int16(buffer, dns->questions[i].type);
    buffer_add_int16(buffer, dns->questions[i].class);
  }

  for(i = 0; i < dns->answer_count; i++)
  {
    buffer_add_dns_name(buffer, (char*)dns->answers[i].question); /* Pointer to the name. */
/*    buffer_add_int16(buffer, 0xc00c);*/
    buffer_add_int16(buffer, dns->answers[i].type); /* Type. */
    buffer_add_int16(buffer, dns->answers[i].class); /* Class. */
    buffer_add_int32(buffer, dns->answers[i].ttl); /* Time to live. */

    if(dns->answers[i].type == _DNS_TYPE_A)
    {
      buffer_add_int16(buffer, 4); /* Length. */
      buffer_add_ipv4_address(buffer, dns->answers[i].answer->A.address);
    }
    else if(dns->answers[i].type == _DNS_TYPE_NS)
    {
      buffer_add_int16(buffer, strlen(dns->answers[i].answer->NS.name) + 2);
      buffer_add_dns_name(buffer, dns->answers[i].answer->NS.name);
    }
    else if(dns->answers[i].type == _DNS_TYPE_CNAME)
    {
      buffer_add_int16(buffer, strlen(dns->answers[i].answer->CNAME.name) + 2);
      buffer_add_dns_name(buffer, dns->answers[i].answer->CNAME.name);
    }
    else if(dns->answers[i].type == _DNS_TYPE_MX)
    {
      buffer_add_int16(buffer, strlen(dns->answers[i].answer->MX.name) + 2 + 2);
      buffer_add_int16(buffer, dns->answers[i].answer->MX.preference);
      buffer_add_dns_name(buffer, dns->answers[i].answer->MX.name);
    }
    else if(dns->answers[i].type == _DNS_TYPE_TEXT)
    {
      buffer_add_int16(buffer, dns->answers[i].answer->TEXT.length + 1);
      buffer_add_int8(buffer, dns->answers[i].answer->TEXT.length);
      buffer_add_bytes(buffer, dns->answers[i].answer->TEXT.text, dns->answers[i].answer->TEXT.length);
    }
#ifndef WIN32
    else if(dns->answers[i].type == _DNS_TYPE_AAAA)
    {
      buffer_add_int16(buffer, 16);
      buffer_add_ipv6_address(buffer, dns->answers[i].answer->AAAA.address);
    }
#endif
    else if(dns->answers[i].type == _DNS_TYPE_NB)
    {
      buffer_add_int16(buffer, 6);
      buffer_add_int16(buffer, dns->answers[i].answer->NB.flags);
      buffer_add_ipv4_address(buffer, dns->answers[i].answer->NB.address);
    }
    else
    {
      fprintf(stderr, "WARNING: Don't know how to build answer type 0x%02x; skipping!\n", dns->answers[i].type);
    }
  }
  for(i = 0; i < dns->authority_count; i++)
  {
    /* TODO */
  }


  for(i = 0; i < dns->additional_count; i++)
  {
    buffer_add_dns_name(buffer, (char*)dns->additionals[i].question); /* Pointer to the name. */
/*    buffer_add_int16(buffer, 0xc00c);*/
    buffer_add_int16(buffer, dns->additionals[i].type); /* Type. */
    buffer_add_int16(buffer, dns->additionals[i].class); /* Class. */
    buffer_add_int32(buffer, dns->additionals[i].ttl); /* Time to live. */

    if(dns->additionals[i].type == _DNS_TYPE_A)
    {
      buffer_add_int16(buffer, 4); /* Length. */
      buffer_add_ipv4_address(buffer, dns->additionals[i].additional->A.address);
    }
    else if(dns->additionals[i].type == _DNS_TYPE_NS)
    {
      buffer_add_int16(buffer, strlen(dns->additionals[i].additional->NS.name) + 2);
      buffer_add_dns_name(buffer, dns->additionals[i].additional->NS.name);
    }
    else if(dns->additionals[i].type == _DNS_TYPE_CNAME)
    {
      buffer_add_int16(buffer, strlen(dns->additionals[i].additional->CNAME.name) + 2);
      buffer_add_dns_name(buffer, dns->additionals[i].additional->CNAME.name);
    }
    else if(dns->additionals[i].type == _DNS_TYPE_MX)
    {
      buffer_add_int16(buffer, strlen(dns->additionals[i].additional->MX.name) + 2 + 2);
      buffer_add_int16(buffer, dns->additionals[i].additional->MX.preference);
      buffer_add_dns_name(buffer, dns->additionals[i].additional->MX.name);
    }
    else if(dns->additionals[i].type == _DNS_TYPE_TEXT)
    {
      buffer_add_int16(buffer, dns->additionals[i].additional->TEXT.length + 1);
      buffer_add_int8(buffer, dns->additionals[i].additional->TEXT.length);
      buffer_add_bytes(buffer, dns->additionals[i].additional->TEXT.text, dns->additionals[i].additional->TEXT.length);
    }
#ifndef WIN32
    else if(dns->additionals[i].type == _DNS_TYPE_AAAA)
    {
      buffer_add_int16(buffer, 16);
      buffer_add_ipv6_address(buffer, dns->additionals[i].additional->AAAA.address);
    }
#endif
    else if(dns->additionals[i].type == _DNS_TYPE_NB)
    {
      buffer_add_int16(buffer, 6);
      buffer_add_int16(buffer, dns->additionals[i].additional->NB.flags);
      buffer_add_ipv4_address(buffer, dns->additionals[i].additional->NB.address);
    }
    else
    {
      fprintf(stderr, "WARNING: Don't know how to build additional type 0x%02x; skipping!\n", dns->additionals[i].type);
    }
  }

  return buffer_create_string_and_destroy(buffer, length);
}

void dns_print(dns_t *dns)
{
  uint16_t i;
  fprintf(stderr, "trn_id: 0x%04x\n", dns->trn_id);
  fprintf(stderr, "flags:  0x%04x\n", dns->flags);
  fprintf(stderr, "rcode:  0x%04x\n", dns->rcode);
  for(i = 0; i < dns->question_count; i++)
    fprintf(stderr, "question: %s 0x%04x 0x%04x\n", dns->questions[i].name, dns->questions[i].type, dns->questions[i].class);

  for(i = 0; i < dns->answer_count; i++)
  {
    if(dns->answers[i].type == _DNS_TYPE_A)
      fprintf(stderr, "answer: %s => %s A      0x%04x %08x\n", dns->answers[i].question, dns->answers[i].answer->A.address, dns->answers[i].class, dns->answers[i].ttl);
    else if(dns->answers[i].type == _DNS_TYPE_NS)
      fprintf(stderr, "answer: %s => %s NS     0x%04x %08x\n", dns->answers[i].question, dns->answers[i].answer->NS.name, dns->answers[i].class, dns->answers[i].ttl);
    else if(dns->answers[i].type == _DNS_TYPE_CNAME)
      fprintf(stderr, "answer: %s => %s CNAME  0x%04x %08x\n", dns->answers[i].question, dns->answers[i].answer->CNAME.name, dns->answers[i].class, dns->answers[i].ttl);
    else if(dns->answers[i].type == _DNS_TYPE_MX)
      fprintf(stderr, "answer: %s => %s (%d) MX     0x%04x 0x%08x\n", dns->answers[i].question, dns->answers[i].answer->MX.name, dns->answers[i].answer->MX.preference, dns->answers[i].class, dns->answers[i].ttl);
    else if(dns->answers[i].type == _DNS_TYPE_TEXT)
      fprintf(stderr, "answer: %s => %s TEXT   0x%04x %08x\n", dns->answers[i].question, dns->answers[i].answer->TEXT.text, dns->answers[i].class, dns->answers[i].ttl);
#ifndef WIN32
    else if(dns->answers[i].type == _DNS_TYPE_AAAA)
      fprintf(stderr, "answer: %s => %s AAAA   0x%04x %08x\n", dns->answers[i].question, dns->answers[i].answer->AAAA.address, dns->answers[i].class, dns->answers[i].ttl);
#endif
    else if(dns->answers[i].type == _DNS_TYPE_NB)
      fprintf(stderr, "answer: %s => %s (0x%04x) NB 0x%04x %08x\n", dns->answers[i].question, dns->answers[i].answer->NB.address, dns->answers[i].answer->NB.flags, dns->answers[i].class, dns->answers[i].ttl);
    else if(dns->answers[i].type == _DNS_TYPE_NBSTAT)
    {
      uint8_t j;
      NBSTAT_answer_t answer = dns->answers[i].answer->NBSTAT;

      fprintf(stderr, "answer: %s => %d names (%02x:%02x:%02x:%02x:%02x:%02x)\n", dns->answers[i].question, answer.name_count, answer.stats[0], answer.stats[1], answer.stats[2], answer.stats[3], answer.stats[4], answer.stats[5]);

      for(j = 0; j < answer.name_count; j++)
        fprintf(stderr, "    %s:%02x (%04x)\n", answer.names[j].name, answer.names[j].name_type, answer.names[j].name_flags);
    }
  }

  fprintf(stderr, "Authorities: %d\n", dns->authority_count);

  for(i = 0; i < dns->additional_count; i++)
  {
    if(dns->additionals[i].type == _DNS_TYPE_A)
      fprintf(stderr, "additional: %s => %s A      0x%04x %08x\n", dns->additionals[i].question, dns->additionals[i].additional->A.address, dns->additionals[i].class, dns->additionals[i].ttl);
    else if(dns->additionals[i].type == _DNS_TYPE_NS)
      fprintf(stderr, "additional: %s => %s NS     0x%04x %08x\n", dns->additionals[i].question, dns->additionals[i].additional->NS.name, dns->additionals[i].class, dns->additionals[i].ttl);
    else if(dns->additionals[i].type == _DNS_TYPE_CNAME)
      fprintf(stderr, "additional: %s => %s CNAME  0x%04x %08x\n", dns->additionals[i].question, dns->additionals[i].additional->CNAME.name, dns->additionals[i].class, dns->additionals[i].ttl);
    else if(dns->additionals[i].type == _DNS_TYPE_MX)
      fprintf(stderr, "additional: %s => %s (%d) MX     0x%04x 0x%08x\n", dns->additionals[i].question, dns->additionals[i].additional->MX.name, dns->additionals[i].additional->MX.preference, dns->additionals[i].class, dns->additionals[i].ttl);
    else if(dns->additionals[i].type == _DNS_TYPE_TEXT)
      fprintf(stderr, "additional: %s => %s TEXT   0x%04x %08x\n", dns->additionals[i].question, dns->additionals[i].additional->TEXT.text, dns->additionals[i].class, dns->additionals[i].ttl);
#ifndef WIN32
    else if(dns->additionals[i].type == _DNS_TYPE_AAAA)
      fprintf(stderr, "additional: %s => %s AAAA   0x%04x %08x\n", dns->additionals[i].question, dns->additionals[i].additional->AAAA.address, dns->additionals[i].class, dns->additionals[i].ttl);
#endif
    else if(dns->additionals[i].type == _DNS_TYPE_NB)
      fprintf(stderr, "additional: %s => %s (0x%04x) NB 0x%04x %08x\n", dns->additionals[i].question, dns->additionals[i].additional->NB.address, dns->additionals[i].additional->NB.flags, dns->additionals[i].class, dns->additionals[i].ttl);
    else if(dns->additionals[i].type == _DNS_TYPE_NBSTAT)
    {
      uint8_t j;
      NBSTAT_additional_t additional = dns->additionals[i].additional->NBSTAT;

      fprintf(stderr, "additional: %s => %d names (%02x:%02x:%02x:%02x:%02x:%02x)\n", dns->additionals[i].question, additional.name_count, additional.stats[0], additional.stats[1], additional.stats[2], additional.stats[3], additional.stats[4], additional.stats[5]);

      for(j = 0; j < additional.name_count; j++)
        fprintf(stderr, "    %s:%02x (%04x)\n", additional.names[j].name, additional.names[j].name_type, additional.names[j].name_flags);
    }
  }
}

dns_t *dns_create_error(uint16_t trn_id, question_t question)
{
  /* Create the DNS packet. */
  dns_t *dns = dns_create(_DNS_OPCODE_QUERY, _DNS_FLAG_QR, _DNS_RCODE_NAME_ERROR);
  dns->trn_id = trn_id;

  /* Echo back the question. */
  dns_add_question(dns, question.name, question.type, question.class);

  return dns;
}

uint8_t *dns_create_error_string(uint16_t trn_id, question_t question, size_t *length)
{
  /* Create the packet. */
  dns_t *dns = dns_create_error(trn_id, question);

  /* Convert it to a string. */
  uint8_t *packet = dns_to_packet(dns, length);
  dns_destroy(dns);

  return packet;
}

/* Gets the first system dns server. */
char *dns_get_system()
{
#ifdef WIN32
  DNS_STATUS error;

  DWORD address;

  char      *straddress = safe_malloc(16);

  /* Set the initial length to something we know is going to be wrong. */
  DWORD      length  = sizeof(IP4_ARRAY);
  IP4_ARRAY *servers = safe_malloc(length);

  /* Call the function, which will inevitably return an error but will also tell us how much memory we need. */
  error = DnsQueryConfig(DnsConfigDnsServerList, 0, NULL, NULL, servers, &length);
  if(error == ERROR_MORE_DATA)
    servers = safe_realloc(servers, length);

  /* Nowe that we have the right length, this call should succeed. */
  error = DnsQueryConfig(DnsConfigDnsServerList, 0, NULL, NULL, servers, &length);

  /* Check for an error. */
  if(error)
  {
    fprintf(stderr, "Couldn't get system DNS server: %d\n", error);
    fprintf(stderr, "You can use --dns to set a custom dns server.\n");
    exit(1);
  }

  /* Check if no servers were returned. */
  if(servers->AddrCount == 0)
  {
    fprintf(stderr, "Couldn't find any system dns servers");
    fprintf(stderr, "You can use --dns to set a custom dns server.\n");
    exit(1);
  }

  /* Take the first DNS server. */
  address = servers->AddrArray[0];

  /* Get rid of the array (we don't need it anymore). */
  safe_free(servers);

  /* Convert the address to a string representation. */
#ifdef WIN32
  sprintf_s(straddress, 16, "%d.%d.%d.%d", (address >>  0) & 0x000000FF,
                     (address >>  8) & 0x000000FF,
                     (address >> 16) & 0x000000FF,
                     (address >> 24) & 0x000000FF);
#else
  sprintf(straddress, "%d.%d.%d.%d", (address >>  0) & 0x000000FF,
                     (address >>  8) & 0x000000FF,
                     (address >> 16) & 0x000000FF,
                     (address >> 24) & 0x000000FF);
#endif

  return straddress;
#else
  FILE *file = fopen("/etc/resolv.conf", "r");
  char  buffer[1024];

  if(!file)
    return NULL;

  while(fgets(buffer, 1024, file))
  {
    if(strstr(buffer, "nameserver") == buffer)
    {
      char *address;
      char *end;

      /* Remove the first part of the string. */
      address = buffer + strlen("nameserver");

      /* Find the first character in the address. Note: the 'int' typecast is to fix a warning on cygwin. */
      while(address[0] && isspace((int)address[0]))
        address++;

      /* Make sure we didn't hit the end of the string. */
      if(!address[0])
      {
        fprintf(stderr, "Invalid record in /etc/resolv.conf: %s\n", buffer);
        continue;
      }

      /* Find the end of the string. Note: The 'int' typecast is to remove a warning on cygwin. */
      end = address;
      while(end[0] && !isspace((int)end[0]))
        end++;

      /* If we hit the end of the string, we don't have to do anything; otherwise, terminate it. */
      if(end[0])
        end[0] = '\0';


      /* In theory, we should now have the address of the first entry in resolv.conf. */
      return safe_strdup(address);
    }
  }

  return NULL;
#endif
}

int dns_is_error(dns_t *dns)
{
  return dns->rcode != 0;
}
