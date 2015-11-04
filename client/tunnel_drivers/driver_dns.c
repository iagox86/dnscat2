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

#define HEXCHAR(c) ((c) < 10 ? ((c)+'0') : (((c)-10) + 'a'))

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

    if(!strstr(str, domain))
    {
      LOG_ERROR("The response didn't contain the domain name: %s", str);
      return NULL;
    }

    /* The server returns an empty domain name for all errors. */
    if(!strcmp(str, domain))
    {
      LOG_INFO("The response was just the domain name: %s", str);
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

static dns_type_t get_type(driver_dns_t *driver)
{
  return driver->types[rand() % driver->type_count];
}

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

  /* Keep track of the length of the current section (the characters between two periods). */
  section_length = 0;
  for(i = 0; i < length; i++)
  {
    buffer_add_int8(buffer, HEXCHAR((data[i] >> 4) & 0x0F));
    buffer_add_int8(buffer, HEXCHAR((data[i] >> 0) & 0x0F));

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
  dns_add_question(dns, (char*)encoded_bytes, get_type(driver), _DNS_CLASS_IN);
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
  controller_heartbeat();

  return SELECT_OK;
}

static SELECT_RESPONSE_t recv_socket_callback(void *group, int s, uint8_t *data, size_t length, char *addr, uint16_t port, void *param)
{
  /*driver_dns_t *driver_dns = param;*/
  dns_t        *dns    = dns_create_from_packet(data, length);
  driver_dns_t *driver = (driver_dns_t*) param;

  LOG_INFO("DNS response received (%d bytes)", length);

  if(dns->rcode != _DNS_RCODE_SUCCESS)
  {
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
    uint8_t   *tmp_answer = NULL;
    size_t     answer_length = 0;
    dns_type_t type = dns->answers[0].type;

    if(type == _DNS_TYPE_TEXT)
    {
      LOG_INFO("Received a TXT response: %s", dns->answers[0].answer->TEXT.text);

      /* Get the answer. */
      tmp_answer    = dns->answers[0].answer->TEXT.text;
      answer_length = dns->answers[0].answer->TEXT.length;

      /* Decode it. */
      answer = buffer_decode_hex(tmp_answer, &answer_length);
    }
    else if(type == _DNS_TYPE_CNAME)
    {
      LOG_INFO("Received a CNAME response: %s", (char*)dns->answers[0].answer->CNAME.name);

      /* Get the answer. */
      tmp_answer = remove_domain((char*)dns->answers[0].answer->CNAME.name, driver->domain);
      if(!tmp_answer)
      {
        answer = NULL;
      }
      else
      {
        answer_length = strlen((char*)tmp_answer);

        /* Decode it. */
        answer = buffer_decode_hex(tmp_answer, &answer_length);
        safe_free(tmp_answer);
      }
    }
    else if(type == _DNS_TYPE_MX)
    {
      LOG_INFO("Received a MX response: %s", (char*)dns->answers[0].answer->MX.name);

      /* Get the answer. */
      tmp_answer = remove_domain((char*)dns->answers[0].answer->MX.name, driver->domain);
      if(!tmp_answer)
      {
        answer = NULL;
      }
      else
      {
        answer_length = strlen((char*)tmp_answer);
        LOG_INFO("Received a MX response (%zu bytes)", answer_length);

        /* Decode it. */
        answer = buffer_decode_hex(tmp_answer, &answer_length);
        safe_free(tmp_answer);
      }
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

driver_dns_t *driver_dns_create(select_group_t *group, char *domain, char *host, uint16_t port, char *types, char *server)
{
  driver_dns_t *driver = (driver_dns_t*) safe_malloc(sizeof(driver_dns_t));
  char *token = NULL;

  /* Create the actual DNS socket. */
  LOG_INFO("Creating UDP (DNS) socket on %s", host);
  driver->s = udp_create_socket(0, host);
  if(driver->s == -1)
  {
    LOG_FATAL("Couldn't create UDP socket!");
    exit(1);
  }

  /* Set the domain and stuff. */
  driver->group      = group;
  driver->domain     = domain;
  driver->dns_port   = port;
  driver->dns_server = server;

  /* Allow the user to choose 'any' protocol. */
  if(!strcmp(types, "ANY"))
    types = DNS_TYPES;

  /* Make a copy of types, since strtok() changes it. */
  types = safe_strdup(types);

  driver->type_count = 0;
  for(token = strtok(types, ", "); token && driver->type_count < DNS_MAX_TYPES; token = strtok(NULL, ", "))
  {
    if(!strcmp(token, "TXT") || !strcmp(token, "TEXT"))
      driver->types[driver->type_count++] = _DNS_TYPE_TEXT;
    else if(!strcmp(token, "MX"))
      driver->types[driver->type_count++] = _DNS_TYPE_MX;
    else if(!strcmp(token, "CNAME"))
      driver->types[driver->type_count++] = _DNS_TYPE_CNAME;
    else if(!strcmp(token, "A"))
      driver->types[driver->type_count++] = _DNS_TYPE_A;
#ifndef WIN32
    else if(!strcmp(token, "AAAA"))
      driver->types[driver->type_count++] = _DNS_TYPE_AAAA;
#endif
  }

  /* Now that we no longer need types. */
  safe_free(types);

  if(driver->type_count == 0)
  {
    LOG_FATAL("You didn't pass any valid DNS types to use! Allowed types are "DNS_TYPES);
    exit(1);
  }

  /* If it succeeds, add it to the select_group */
  select_group_add_socket(group, driver->s, SOCKET_TYPE_STREAM, driver);
  select_set_recv(group, driver->s, recv_socket_callback);
  select_set_timeout(group, timeout_callback, driver);
  select_set_closed(group, driver->s, dns_data_closed);

  return driver;
}

void driver_dns_destroy(driver_dns_t *driver)
{
  safe_free(driver);
}

void driver_dns_go(driver_dns_t *driver)
{
  /* Do a fake timeout at the start so we can get going more quickly. */
  timeout_callback(driver->group, driver);

  /* Loop forever and poke the socket. */
  while(TRUE)
    select_group_do_select(driver->group, 50);
}
