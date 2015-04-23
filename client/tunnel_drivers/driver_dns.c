/* driver_dns.c
 * Created July/2013
 * By Ron Bowes
 *
 * See LICENSE.md
 */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "controller/controller.h"
#include "libs/buffer.h"
#include "libs/dns.h"
#include "libs/log.h"
#include "libs/memory.h"
#include "libs/types.h"
#include "libs/udp.h"

#include "driver_dns.h"

#define MAX_FIELD_LENGTH 62
#define MAX_DNS_LENGTH   255
#define WILDCARD_PREFIX  "dnscat"

/* The max length is a little complicated:
 * 255 because that's the max DNS length
 * Halved, because we encode in hex
 * Minus the length of the domain, which is appended
 * Minus 1, for the period right before the domain
 * Minus the number of periods that could appear within the name
 */
#define MAX_DNSCAT_LENGTH(domain) ((255/2) - (domain ? strlen(domain) : strlen(WILDCARD_PREFIX)) - 1 - ((MAX_DNS_LENGTH / MAX_FIELD_LENGTH) + 1))

static SELECT_RESPONSE_t dns_data_closed(void *group, int socket, void *param)
{
  LOG_FATAL("DNS socket closed!");
  exit(0);

  return SELECT_OK;
}

static uint8_t *remove_domain(char *str, char *domain)
{
  if(domain)
  {
    char *fixed = NULL;

    if(strlen(domain) > strlen(str))
    {
      LOG_ERROR("The string is too short to have a domain name attached: %s", str);
      return NULL;
    }

    fixed = safe_strdup(str);
    fixed[strlen(str) - strlen(domain) - 1] = '\0';

    return (uint8_t*)fixed;
  }
  else
  {
    return (uint8_t*)safe_strdup(str += strlen(WILDCARD_PREFIX));
  }
}

static uint8_t *buffer_decode_hex(uint8_t *str, size_t *length)
{
  size_t    i   = 0;
  buffer_t *out = buffer_create(BO_BIG_ENDIAN);

/*printf("Decoding: %s (%zu bytes)...\n", str, *length);*/
  while(i < *length)
  {
    uint8_t c1 = 0;
    uint8_t c2 = 0;

    /* Read the first character, ignoring periods */
    do
    {
      c1 = toupper(str[i++]);
    } while(c1 == '.' && i < *length);

    /* Make sure we aren't at the end of the buffer. */
    if(i >= *length)
    {
      LOG_ERROR("Couldn't hex-decode the name (name was an odd length): %s", str);
      return NULL;
    }

    /* Make sure we got a hex digit */
    if(!isxdigit(c1))
    {
      LOG_ERROR("Couldn't hex-decode the name (contains non-hex characters): %s", str);
      return NULL;
    }

    /* Read the second character. */
    do
    {
      c2 = toupper(str[i++]);
    } while(c2 == '.' && i < *length);

    /* Make sure we got a hex digit */
    if(!isxdigit(c2))
    {
      LOG_ERROR("Couldn't hex-decode the name (contains non-hex characters): %s", str);
      return NULL;
    }

    c1 = ((c1 < 'A') ? (c1 - '0') : (c1 - 'A' + 10));
    c2 = ((c2 < 'A') ? (c2 - '0') : (c2 - 'A' + 10));

    buffer_add_int8(out, (c1 << 4) | c2);
  }

  return buffer_create_string_and_destroy(out, length);
}

static int cmpfunc_a(const void *a, const void *b)
{
  return ((const answer_t*)a)->answer->A.bytes[0] - ((const answer_t*)b)->answer->A.bytes[0];
}

#ifndef WIN32
static int cmpfunc_aaaa(const void *a, const void *b)
{
  return ((const answer_t*)a)->answer->AAAA.bytes[0] - ((const answer_t*)b)->answer->AAAA.bytes[0];
}
#endif

static void do_send(driver_dns_t *driver)
{
  size_t        i;
  dns_t        *dns;
  buffer_t     *buffer;
  uint8_t      *encoded_bytes;
  size_t        encoded_length;
  uint8_t      *dns_bytes;
  size_t        dns_length;
  size_t        section_length;

  size_t length;
  uint8_t *data = controller_get_outgoing((size_t*)&length, (size_t)MAX_DNSCAT_LENGTH(driver->domain));

  /* If we aren't supposed to send anything (like we're waiting for a timeout),
   * data is NULL. */
  if(!data)
    return;

  assert(driver->s != -1); /* Make sure we have a valid socket. */
  assert(data); /* Make sure they aren't trying to send NULL. */
  assert(length > 0); /* Make sure they aren't trying to send 0 bytes. */
  assert(length <= MAX_DNSCAT_LENGTH(driver->domain));

  buffer = buffer_create(BO_BIG_ENDIAN);

  /* If no domain is set, add the wildcard prefix at the start. */
  if(!driver->domain)
  {
    buffer_add_bytes(buffer, (uint8_t*)WILDCARD_PREFIX, strlen(WILDCARD_PREFIX));
    buffer_add_int8(buffer, '.');
  }

  section_length = 0;
  /* TODO: I don't much care for this loop... */
  for(i = 0; i < length; i++)
  {
    char hex_buf[3];

#ifdef WIN32
    sprintf_s(hex_buf, 3, "%02x", data[i]);
#else
    sprintf(hex_buf, "%02x", data[i]);
#endif
    buffer_add_bytes(buffer, hex_buf, 2);

    /* Add periods when we need them. */
    section_length += 2;
    if(i + 1 != length && section_length + 2 >= MAX_FIELD_LENGTH)
    {
      section_length = 0;
      buffer_add_int8(buffer, '.');
    }
  }

  /* If a domain is set, instead of the wildcard prefix, add the domain to the end. */
  if(driver->domain)
  {
    buffer_add_int8(buffer, '.');
    buffer_add_bytes(buffer, driver->domain, strlen(driver->domain));
  }
  buffer_add_int8(buffer, '\0');

  /* Get the result out. */
  encoded_bytes = buffer_create_string_and_destroy(buffer, &encoded_length);

  /* Double-check we didn't mess up the length. */
  assert(encoded_length <= MAX_DNS_LENGTH);

  dns = dns_create(_DNS_OPCODE_QUERY, _DNS_FLAG_RD, _DNS_RCODE_SUCCESS);
  dns_add_question(dns, (char*)encoded_bytes, driver->type, _DNS_CLASS_IN);
  dns_bytes = dns_to_packet(dns, &dns_length);

  LOG_INFO("Sending DNS query for: %s to %s:%d", encoded_bytes, driver->dns_server, driver->dns_port);
  udp_send(driver->s, driver->dns_server, driver->dns_port, dns_bytes, dns_length);

  safe_free(dns_bytes);
  safe_free(encoded_bytes);
  safe_free(data);

  dns_destroy(dns);
}

static SELECT_RESPONSE_t timeout_callback(void *group, void *param)
{
  do_send((driver_dns_t*)param);

  return SELECT_OK;
}

static SELECT_RESPONSE_t recv_socket_callback(void *group, int s, uint8_t *data, size_t length, char *addr, uint16_t port, void *param)
{
  /*driver_dns_t *driver_dns = param;*/
  dns_t        *dns    = dns_create_from_packet(data, length);
  driver_dns_t *driver = (driver_dns_t*) param;

  LOG_INFO("DNS response received (%d bytes)", length);

  /* TODO */
  if(dns->rcode != _DNS_RCODE_SUCCESS)
  {
    /* TODO: Handle errors more gracefully */
    switch(dns->rcode)
    {
      case _DNS_RCODE_FORMAT_ERROR:
        LOG_ERROR("DNS: RCODE_FORMAT_ERROR");
        break;
      case _DNS_RCODE_SERVER_FAILURE:
        LOG_ERROR("DNS: RCODE_SERVER_FAILURE");
        break;
      case _DNS_RCODE_NAME_ERROR:
        LOG_ERROR("DNS: RCODE_NAME_ERROR");
        break;
      case _DNS_RCODE_NOT_IMPLEMENTED:
        LOG_ERROR("DNS: RCODE_NOT_IMPLEMENTED");
        break;
      case _DNS_RCODE_REFUSED:
        LOG_ERROR("DNS: RCODE_REFUSED");
        break;
      default:
        LOG_ERROR("DNS: Unknown error code (0x%04x)", dns->rcode);
        break;
    }
  }
  else if(dns->question_count != 1)
  {
    LOG_ERROR("DNS returned the wrong number of response fields (question_count should be 1, was instead %d).", dns->question_count);
    LOG_ERROR("This is probably due to a DNS error");
  }
  else if(dns->answer_count < 1)
  {
    LOG_ERROR("DNS didn't return an answer");
    LOG_ERROR("This is probably due to a DNS error");
  }
  else
  {
    size_t    i;

    uint8_t   *answer = NULL;
    size_t     answer_length = 0;
    dns_type_t type = dns->answers[0].type;

    if(type == _DNS_TYPE_TEXT)
    {
      /* Get the answer. */
      answer        = dns->answers[0].answer->TEXT.text;
      answer_length = dns->answers[0].answer->TEXT.length;
      LOG_INFO("Received a TXT response (%zu bytes)", answer_length);

      /* Decode it. */
      answer = buffer_decode_hex(answer, &answer_length);
    }
    else if(type == _DNS_TYPE_CNAME)
    {
      /* Get the answer. */
      answer = remove_domain((char*)dns->answers[0].answer->CNAME.name, driver->domain);
      answer_length = strlen((char*)answer);
      LOG_INFO("Received a CNAME response (%zu bytes)", answer_length);

      /* Decode it. */
      answer = buffer_decode_hex(answer, &answer_length);
    }
    else if(type == _DNS_TYPE_MX)
    {
      /* Get the answer. */
      answer = remove_domain((char*)dns->answers[0].answer->MX.name, driver->domain);
      answer_length = strlen((char*)answer);
      LOG_INFO("Received a MX response (%zu bytes)", answer_length);

      /* Decode it. */
      answer = buffer_decode_hex(answer, &answer_length);
    }
    else if(type == _DNS_TYPE_A)
    {
      buffer_t *buf = buffer_create(BO_BIG_ENDIAN);

      qsort(dns->answers, dns->answer_count, sizeof(answer_t), cmpfunc_a);

      for(i = 0; i < dns->answer_count; i++)
        buffer_add_bytes(buf, dns->answers[i].answer->A.bytes + 1, 3);

      answer_length = buffer_read_next_int8(buf);
      LOG_INFO("Received an A response (%zu bytes)", answer_length);

      answer = safe_malloc(answer_length);
      buffer_read_bytes_at(buf, 1, answer, answer_length);
    }
#ifndef WIN32
    else if(type == _DNS_TYPE_AAAA)
    {
      buffer_t *buf = buffer_create(BO_BIG_ENDIAN);

      qsort(dns->answers, dns->answer_count, sizeof(answer_t), cmpfunc_aaaa);

      for(i = 0; i < dns->answer_count; i++)
        buffer_add_bytes(buf, dns->answers[i].answer->AAAA.bytes + 1, 15);

      answer_length = buffer_read_next_int8(buf);
      LOG_INFO("Received an AAAA response (%zu bytes)", answer_length);

      answer = safe_malloc(answer_length);
      buffer_read_bytes_at(buf, 1, answer, answer_length);
    }
#endif
    else
    {
      LOG_ERROR("Unknown DNS type returned: %d", type);
      answer = NULL;
    }

    if(answer)
    {
      /*LOG_WARNING("Received a %zu-byte DNS response: %s [0x%04x]", answer_length, answer, type);*/

      /* Pass the buffer to the caller */
      if(answer_length > 0)
      {
        /* Pass the data elsewhere. */
        if(controller_data_incoming(answer, answer_length))
          do_send(driver);
      }

      safe_free(answer);
    }
  }

  dns_destroy(dns);

  return SELECT_OK;
}

driver_dns_t *driver_dns_create(select_group_t *group, char *domain, char *host, uint16_t port, dns_type_t type, char *server)
{
  driver_dns_t *driver_dns = (driver_dns_t*) safe_malloc(sizeof(driver_dns_t));

  /* Create the actual DNS socket. */
  LOG_INFO("Creating UDP (DNS) socket");
  driver_dns->s = udp_create_socket(0, host);
  if(driver_dns->s == -1)
  {
    LOG_FATAL("Couldn't create UDP socket!");
    exit(1);
  }

  /* Set the domain and stuff. */
  driver_dns->domain     = domain;
  driver_dns->type       = type;
  driver_dns->dns_port   = port;
  driver_dns->dns_server = server;

  /* If it succeeds, add it to the select_group */
  select_group_add_socket(group, driver_dns->s, SOCKET_TYPE_STREAM, driver_dns);
  select_set_recv(group, driver_dns->s, recv_socket_callback);
  select_set_timeout(group, timeout_callback, driver_dns);
  select_set_closed(group, driver_dns->s, dns_data_closed);

  return SELECT_OK;
}

void driver_dns_destroy(driver_dns_t *driver)
{
  safe_free(driver);
}
