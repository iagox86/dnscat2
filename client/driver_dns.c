/* driver_dns.c
 * Created July/2013
 * By Ron Bowes
 */
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "buffer.h"
#include "dns.h"
#include "log.h"
#include "memory.h"
#include "message.h"
#include "types.h"
#include "udp.h"

#include "driver_dns.h"

#define MAX_FIELD_LENGTH 62
#define MAX_DNS_LENGTH   255

/* The max length is a little complicated:
 * 255 because that's the max DNS length
 * Halved, because we encode in hex
 * Minus the length of the domain, which is appended
 * Minus 1, for the period right before the domain
 * Minus the number of periods that could appear within the name
 */
#define MAX_DNSCAT_LENGTH(domain) ((255/2) - strlen(domain) - 1 - ((MAX_DNS_LENGTH / MAX_FIELD_LENGTH) + 1))

static SELECT_RESPONSE_t dns_data_closed(void *group, int socket, void *param)
{
  LOG_FATAL("DNS socket closed!");
  exit(0);

  return SELECT_OK;
}

static SELECT_RESPONSE_t recv_socket_callback(void *group, int s, uint8_t *data, size_t length, char *addr, uint16_t port, void *param)
{
  driver_dns_t *driver_dns = param;
  dns_t        *dns        = dns_create_from_packet(data, length);

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
  }
  else if(dns->answer_count != 1)
  {
    LOG_ERROR("DNS returned the wrong number of response fields (answer_count should be 1, was instead %d).", dns->answer_count);
  }
  else if(dns->answers[0].type == _DNS_TYPE_TEXT)
  {
    char *answer;
    char buf[3];
    size_t i;

    answer = (char*)dns->answers[0].answer->TEXT.text;
    LOG_INFO("Received a DNS TXT response: %s", answer);

    if(!strcmp(answer, driver_dns->domain))
    {
      LOG_INFO("Received a 'nil' answer; ignoring (usually this is due to caching/re-sends and doesn't matter)");
    }
    else
    {
      buffer_t *incoming_data = buffer_create(BO_BIG_ENDIAN);

      /* Loop through the part of the answer before the 'domain' */
      for(i = 0; i < dns->answers[0].answer->TEXT.length; i += 2)
      {
        /* Validate the answer */
        if(answer[i] == '.')
        {
          /* ignore */
        }
        else if(answer[i+1] == '.')
        {
          LOG_ERROR("Answer contained an odd number of digits");
        }
        else if(!isxdigit((int)answer[i]))
        {
          LOG_ERROR("Answer contained an invalid digit: '%c'", answer[i]);
        }
        else if(!isxdigit((int)answer[i+1]))
        {
          LOG_ERROR("Answer contained an invalid digit: '%c'", answer[i+1]);
        }
        else
        {
          buf[0] = answer[i];
          buf[1] = answer[i + 1];
          buf[2] = '\0';

          buffer_add_int8(incoming_data, (uint8_t)strtol(buf, NULL, 16));
        }
      }

      /* Pass the buffer to the caller */
      if(buffer_get_length(incoming_data) > 0)
      {
        size_t length;
        uint8_t *data = buffer_create_string(incoming_data, &length);

        /* Pass the data elsewhere. */
        message_post_packet_in(data, length);

        safe_free(data);
      }
      buffer_destroy(incoming_data);
    }
  }
  else
  {
    LOG_ERROR("Unknown DNS type returned");
  }

  dns_destroy(dns);

  return SELECT_OK;
}

static void handle_start(driver_dns_t *driver)
{
  message_post_config_int("max_packet_length", MAX_DNSCAT_LENGTH(driver->domain));
}

/* This function expects to receive the proper length of data. */
static void handle_packet_out(driver_dns_t *driver, uint8_t *data, size_t length)
{
  size_t        i;
  dns_t        *dns;
  buffer_t     *buffer;
  uint8_t      *encoded_bytes;
  size_t        encoded_length;
  uint8_t      *dns_bytes;
  size_t        dns_length;
  size_t        section_length;

  assert(driver->s != -1); /* Make sure we have a valid socket. */
  assert(data); /* Make sure they aren't trying to send NULL. */
  assert(length > 0); /* Make sure they aren't trying to send 0 bytes. */
  assert(length <= MAX_DNSCAT_LENGTH(driver->domain));

  buffer = buffer_create(BO_BIG_ENDIAN);
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

  buffer_add_int8(buffer, '.');
  buffer_add_ntstring(buffer, driver->domain); /* TODO: Hardcoded domain name! */
  encoded_bytes = buffer_create_string_and_destroy(buffer, &encoded_length);

  /* Double-check we didn't mess up the length. */
  assert(encoded_length <= MAX_DNS_LENGTH);

  dns = dns_create(_DNS_OPCODE_QUERY, _DNS_FLAG_RD, _DNS_RCODE_SUCCESS);
  dns_add_question(dns, (char*)encoded_bytes, _DNS_TYPE_TEXT, _DNS_CLASS_IN);
  dns_bytes = dns_to_packet(dns, &dns_length);

  LOG_INFO("Sending DNS query for: %s to %s:%d", encoded_bytes, driver->dns_host, driver->dns_port);
  udp_send(driver->s, driver->dns_host, driver->dns_port, dns_bytes, dns_length);

  safe_free(dns_bytes);
  safe_free(encoded_bytes);
  dns_destroy(dns);
}

static void handle_message(message_t *message, void *d)
{
  driver_dns_t *driver_dns = (driver_dns_t*) d;

  switch(message->type)
  {
    case MESSAGE_START:
      handle_start(driver_dns);
      break;

    case MESSAGE_PACKET_OUT:
      handle_packet_out(driver_dns, message->message.packet_out.data, message->message.packet_out.length);
      break;

    default:
      LOG_FATAL("driver_dns received an invalid message!");
      abort();
  }
}

driver_dns_t *driver_dns_create(select_group_t *group, char *domain)
{
  driver_dns_t *driver_dns = (driver_dns_t*) safe_malloc(sizeof(driver_dns_t));

  /* Create the actual DNS socket. */
  LOG_INFO("Creating UDP (DNS) socket");
  driver_dns->s = udp_create_socket(0, "0.0.0.0");
  if(driver_dns->s == -1)
  {
    LOG_FATAL("Couldn't create UDP socket!");
    exit(1);
  }

  /* Set the domain. */
  driver_dns->domain   = domain;

  /* If it succeeds, add it to the select_group */
  select_group_add_socket(group, driver_dns->s, SOCKET_TYPE_STREAM, driver_dns);
  select_set_recv(group, driver_dns->s, recv_socket_callback);
  select_set_closed(group, driver_dns->s, dns_data_closed);

  /* Subscribe to the messages we care about. */
  message_subscribe(MESSAGE_START,      handle_message, driver_dns);
  message_subscribe(MESSAGE_PACKET_OUT, handle_message, driver_dns);

  return driver_dns;
}

void driver_dns_destroy(driver_dns_t *driver)
{
  if(driver->dns_host)
    safe_free(driver->dns_host);
  safe_free(driver);
}
