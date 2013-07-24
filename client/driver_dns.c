/* driver_dns.c
 * Created July/2013
 * By Ron Bowes
 */
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "dns.h"
#include "log.h"
#include "memory.h"
#include "message.h"
#include "packet.h"
#include "sessions.h"
#include "types.h"
#include "udp.h"

#include "driver_dns.h"

/* Default options */
#define DEFAULT_DNS_SERVER   "8.8.8.8"
#define DEFAULT_DNS_PORT     53
#define DEFAULT_DOMAIN       "skullseclabs.org"

#define MAX_FIELD_LENGTH 62
#define MAX_DNS_LENGTH   255 /* TODO: Define this properly again */

/* The max length is a little complicated:
 * 255 because that's the max DNS length
 * Halved, because we encode in hex
 * Minus the length of the domain, which is appended
 * Minus 1, for the period right before the domain
 * Minus the number of periods that could appear within the name
 */
#define MAX_DNSCAT_LENGTH(domain) ((255/2) - strlen(domain) - 1 - ((MAX_DNS_LENGTH / MAX_FIELD_LENGTH) + 1))

static SELECT_RESPONSE_t timeout(void *group, void *param)
{
  driver_dns_t *driver_dns = (driver_dns_t*) param;

  sessions_do_actions();

  if(driver_dns->is_closed)
  {
    if(sessions_total_bytes_queued() == 0)
    {
      sessions_close();
      sessions_destroy();
    }
  }

  return SELECT_OK;
}

static SELECT_RESPONSE_t recv_socket_callback(void *group, int s, uint8_t *data, size_t length, char *addr, uint16_t port, void *param)
{
  driver_dns_t *driver_dns = param;
  dns_t    *dns      = dns_create_from_packet(data, length);

  LOG_INFO("DNS response received (%d bytes)", length);

  /* TODO */
  if(dns->rcode != DNS_RCODE_SUCCESS)
  {
    /* TODO: Handle errors more gracefully */
    switch(dns->rcode)
    {
      case DNS_RCODE_FORMAT_ERROR:
        LOG_ERROR("DNS: RCODE_FORMAT_ERROR");
        break;
      case DNS_RCODE_SERVER_FAILURE:
        LOG_ERROR("DNS: RCODE_SERVER_FAILURE");
        break;
      case DNS_RCODE_NAME_ERROR:
        LOG_ERROR("DNS: RCODE_NAME_ERROR");
        break;
      case DNS_RCODE_NOT_IMPLEMENTED:
        LOG_ERROR("DNS: RCODE_NOT_IMPLEMENTED");
        break;
      case DNS_RCODE_REFUSED:
        LOG_ERROR("DNS: RCODE_REFUSED");
        break;
      default:
        LOG_ERROR("DNS: Unknown error code (0x%04x)", dns->rcode);
        break;
    }
  }
  else if(dns->question_count != 1)
  {
    LOG_ERROR("DNS returned the wrong number of response fields.");
  }
  else if(dns->answer_count != 1)
  {
    LOG_ERROR("DNS returned the wrong number of response fields.");
  }
  else if(dns->answers[0].type == DNS_TYPE_TEXT)
  {
    char *answer;
    char buf[3];
    size_t i;

    /* TODO: We don't actually need the .domain suffix if we use TEXT records */
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

          buffer_add_int8(incoming_data, strtol(buf, NULL, 16));
        }
      }

      /* Pass the buffer to the caller */
      if(buffer_get_length(incoming_data) > 0)
      {
        size_t length;
        uint8_t *data = buffer_create_string(incoming_data, &length);

        /* Parse the dnscat packet. */
        packet_t *packet = packet_parse(data, length);
        session_t *session = sessions_get_by_id(packet->session_id);

        if(session == NULL)
        {
          LOG_WARNING("Received %d bytes of data to a non-existant session", length);
        }
        else if(session->is_closed)
        {
          LOG_WARNING("Received %d bytes of data to a closed session", length);
        }
        else
        {
          LOG_INFO("Received %d bytes of data", length);
          session_recv(session, packet);
        }

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

/* This function expects to receive the proper length of data. */
static void dns_send(uint16_t session_id, uint8_t *data, size_t length, void *d)
{
  size_t        i;
  dns_t        *dns;
  buffer_t     *buffer;
  uint8_t      *encoded_bytes;
  size_t        encoded_length;
  uint8_t      *dns_bytes;
  size_t        dns_length;
  size_t        section_length;

  driver_dns_t *driver_dns = (driver_dns_t*) d;

  LOG_INFO("Entering DNS: queuing %d bytes of data to be sent", length);

  assert(driver_dns->s != -1); /* Make sure we have a valid socket. */
  assert(data); /* Make sure they aren't trying to send NULL. */
  assert(length > 0); /* Make sure they aren't trying to send 0 bytes. */
  assert(length <= MAX_DNSCAT_LENGTH(driver_dns->domain));

  buffer = buffer_create(BO_BIG_ENDIAN);
  section_length = 0;
  for(i = 0; i < length; i++)
  {
    char hex_buf[3];
    sprintf(hex_buf, "%02x", data[i]);
    buffer_add_bytes(buffer, hex_buf, 2);

    /* Add periods when we need them. */
    section_length += 2;
    if(section_length + 2 >= MAX_FIELD_LENGTH)
    {
      section_length = 0;
      buffer_add_int8(buffer, '.');
    }
  }
  buffer_add_ntstring(buffer, ".skullseclabs.org");
  encoded_bytes = buffer_create_string_and_destroy(buffer, &encoded_length);

  /* Double-check we didn't mess up the length. */
  assert(encoded_length <= MAX_DNS_LENGTH);

  dns = dns_create(DNS_OPCODE_QUERY, DNS_FLAG_RD, DNS_RCODE_SUCCESS);
  dns_add_question(dns, (char*)encoded_bytes, DNS_TYPE_TEXT, DNS_CLASS_IN);
  dns_bytes = dns_to_packet(dns, &dns_length);

  LOG_INFO("Sending DNS query for: %s", encoded_bytes);
  udp_send(driver_dns->s, driver_dns->dns_host, driver_dns->dns_port, dns_bytes, dns_length);

  safe_free(dns_bytes);
  safe_free(encoded_bytes);
  dns_destroy(dns);
}

/* Called by the session object when incoming data arrives. */
/* Just send it to the other side. */
static void dns_recv(uint16_t session_id, uint8_t *data, size_t length, void *d)
{
  message_post_data_in(session_id, data, length);
}

static SELECT_RESPONSE_t dns_data_closed(void *group, int socket, void *param)
{
  LOG_FATAL("DNS socket closed");

  message_post_destroy();
  sessions_destroy();

  return SELECT_OK;
}

/*********** ***********/

static void handle_data(uint16_t session_id, uint8_t *data, size_t length, driver_dns_t *driver_dns)
{
  /* Queue data into the session. */
  session_t *session = sessions_get_by_id(session_id);

  if(!session)
  {
    LOG_FATAL("Unknown session id: %d\n", session_id);
    exit(1);
  }
  if(session->is_closed)
  {
    LOG_ERROR("Tried to send data to a closed session: %d", session_id);
    return;
  }

  session_send(session, data, length);
}

static void handle_destroy(driver_dns_t *driver)
{
  driver->is_closed = TRUE;
}

static void handle_message(message_t *message, void *d)
{
  driver_dns_t *driver_dns = (driver_dns_t*) d;

  switch(message->type)
  {
    case MESSAGE_START:
      /* Not sure if I need to handler this... */
      break;

    case MESSAGE_DATA_OUT:
      handle_data(message->message.data.session_id, message->message.data.data, message->message.data.length, driver_dns);
      break;

    case MESSAGE_SESSION_CREATED:
      message->message.session_created.out.outgoing_callback = dns_send;
      message->message.session_created.out.incoming_callback = dns_recv;
      message->message.session_created.out.callback_param    = driver_dns;
      message->message.session_created.out.max_size          = 10; /* TODO: Define this properly based on the domain. */
      break;

    case MESSAGE_DESTROY:
      handle_destroy(driver_dns);
      break;

    default:
      LOG_FATAL("driver_dns received an invalid message!");
      abort();
  }
}

void dns_close_message_handler(message_handler_t *handler)
{
  message_handler_destroy(handler);
}

driver_dns_t *driver_dns_create(select_group_t *group)
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

  /* Set the defaults. */
  driver_dns->domain   = DEFAULT_DOMAIN;
  driver_dns->dns_host = DEFAULT_DNS_SERVER;
  driver_dns->dns_port = DEFAULT_DNS_PORT;

  /* If it succeeds, add it to the select_group */
  select_group_add_socket(group, driver_dns->s, SOCKET_TYPE_STREAM, driver_dns);
  select_set_recv(group, driver_dns->s, recv_socket_callback);
  select_set_timeout(group, timeout, driver_dns);
  select_set_closed(group, driver_dns->s, dns_data_closed);

  /* Save the socket group in case we need it later. */
  driver_dns->group = group;

  /* Create and save a message handler. */
  driver_dns->my_message_handler = message_handler_create(handle_message, driver_dns);

  /* Subscribe to the messages we care about. */
  message_subscribe(MESSAGE_START,           driver_dns->my_message_handler);
  message_subscribe(MESSAGE_DATA_OUT,        driver_dns->my_message_handler);
  message_subscribe(MESSAGE_SESSION_CREATED, driver_dns->my_message_handler);
  message_subscribe(MESSAGE_DESTROY,         driver_dns->my_message_handler);

  return driver_dns;
}

