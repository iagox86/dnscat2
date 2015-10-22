/* session.c
 * By Ron Bowes
 *
 * See LICENSE.md
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef WIN32
#include <unistd.h>
#include <sys/time.h>
#endif

#include "controller/packet.h"
#include "libs/buffer.h"
#include "libs/log.h"
#include "libs/memory.h"
#include "libs/select_group.h"

#ifndef NO_ENCRYPTION
#include "libs/crypto/micro-ecc/uECC.h"
#endif

#include "session.h"

/* Allow the user to override the initial sequence number. */
static uint32_t isn = 0xFFFFFFFF;

/* Enable/disable packet tracing. */
static NBBOOL packet_trace;

/* The amount of delay between packets. */
static int packet_delay = 1000;

/* Transmit instantly when data is received. */
static NBBOOL transmit_instantly_on_data = TRUE;

#ifndef NO_ENCRYPTION
/* Should we set up encryption? */
static NBBOOL do_encryption = TRUE;

/* Pre-shared secret (used for authentication) */
/*static char *preshared_secret = NULL;*/
#endif

/* Define a handler function pointer. */
typedef NBBOOL(packet_handler)(session_t *session, packet_t *packet);

/* TODO: Delete this. */
static void print_hex(char *label, uint8_t *data, size_t length)
{
  size_t i;

  printf("%s: ", label);
  for(i = 0; i < length; i++)
    printf("%02x", data[i] & 0x0FF);
  printf("\n");
}

static uint64_t time_ms()
{
#ifdef WIN32
  FILETIME ft;
  GetSystemTimeAsFileTime(&ft);

  return ((uint64_t)ft.dwLowDateTime + ((uint64_t)(ft.dwHighDateTime) << 32LL)) / 10000;
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000 ;
#endif
}

/* Wait for a delay or incoming data before retransmitting. Call this after transmitting data. */
static void update_counter(session_t *session)
{
  session->last_transmit = time_ms();
  session->missed_transmissions++;
}

/* Decide whether or not we should transmit data yet. */
static NBBOOL can_i_transmit_yet(session_t *session)
{
  if(time_ms() - session->last_transmit > packet_delay)
    return TRUE;
  return FALSE;
}

/* Polls the driver for data and puts it in our own buffer. This is necessary
 * because the session needs to ACK data and such. */
static void poll_for_data(session_t *session)
{
  size_t length = -1;

  /* Read all the data we can. */
  uint8_t *data = driver_get_outgoing(session->driver, &length, -1);

  /* If a driver returns NULL, it means it's done - once the driver is
   * done and all our data is sent, go into 'shutdown' mode. */
  if(!data)
  {
    if(buffer_get_remaining_bytes(session->outgoing_buffer) == 0)
      session_kill(session);
  }
  else
  {
    if(length)
      buffer_add_bytes(session->outgoing_buffer, data, length);

    safe_free(data);
  }
}

static packet_t *create_syn(session_t *session)
{
  packet_t *packet = packet_create_syn(session->id, session->my_seq, (options_t)0);

  if(session->is_command)
    packet_syn_set_is_command(packet);

  if(session->name)
    packet_syn_set_name(packet, session->name);

  return packet;
}

uint8_t *session_get_outgoing(session_t *session, size_t *length, size_t max_length)
{
  packet_t *packet      = NULL;
  uint8_t  *result      = NULL;
  uint8_t  *data        = NULL;
  size_t    data_length = -1;

  /* Suck in any data we can from the driver. */
  poll_for_data(session);

  /* Don't transmit too quickly without receiving anything. */
  if(!can_i_transmit_yet(session))
    return NULL;

  /* It's pretty ugly, but I don't see any other way, since ping requires
   * special packets we have to handle it separately. */
  if(session->is_ping)
  {
    /* Read data without consuming it (ie, leave it in the buffer till it's ACKed) */
    data = buffer_read_remaining_bytes(session->outgoing_buffer, &data_length, max_length - packet_get_ping_size(), FALSE);
    packet = packet_create_ping(session->id, (char*)data);
    safe_free(data);

    LOG_INFO("In PING, sending a PING packet (%zd bytes of data...)", data_length);
  }
  else
  {
    switch(session->state)
    {
      case SESSION_STATE_NEW:
#ifndef NO_ENCRYPTION
        if(do_encryption)
        {
          packet = packet_create_negenc(session->id, 0, session->public_key);
        }
        else
        {
          packet = create_syn(session);
        }
#else
        packet = create_syn(session);
#endif
        break;

#ifndef NO_ENCRYPTION
      case SESSION_STATE_READY:
        packet = create_syn(session);
        break;
#endif

      case SESSION_STATE_ESTABLISHED:
        /* Read data without consuming it (ie, leave it in the buffer till it's ACKed) */
        data = buffer_read_remaining_bytes(session->outgoing_buffer, &data_length, max_length - packet_get_msg_size(session->options), FALSE);
        LOG_INFO("In SESSION_STATE_ESTABLISHED, sending a MSG packet (SEQ = 0x%04x, ACK = 0x%04x, %zd bytes of data...)", session->my_seq, session->their_seq, data_length);

        if(data_length == 0 && session->is_shutdown)
          packet = packet_create_fin(session->id, "Stream closed");
        else
          packet = packet_create_msg(session->id, session->my_seq, session->their_seq, data, data_length);

        safe_free(data);

        break;

      default:
        LOG_FATAL("Wound up in an unknown state: 0x%x", session->state);
        exit(1);
    }
  }

  if(packet)
  {
  /* Print packet data if we're supposed to. */
    if(packet_trace)
    {
      printf("OUTGOING: ");
      packet_print(packet, session->options);
    }

    update_counter(session);
    result = packet_to_bytes(packet, length, session->options);
    packet_destroy(packet);
  }

  return result;
}

#ifndef NO_ENCRYPTION
NBBOOL _handle_bad_new(session_t *session, packet_t *packet)
{
  LOG_FATAL("The server failed to respond with a NEGENC packet");
  LOG_FATAL("This could indicate a version mismatch!");
  exit(1);
}

NBBOOL _handle_negenc_new(session_t *session, packet_t *packet)
{
  print_hex("Their public key", packet->body.negenc.public_key, 64);

  if(!uECC_shared_secret(packet->body.negenc.public_key, session->private_key, session->shared_secret, uECC_secp256r1()))
  {
    LOG_FATAL("Failed to calculate a shared secret!");
    exit(1);
  }

  session->state = SESSION_STATE_READY;

  print_hex("Shared secret", session->shared_secret, 32);

  /* We can send a response right away */
  session->last_transmit = 0;
  session->missed_transmissions = 0;

  return TRUE;
}
#endif

NBBOOL _handle_syn_ready(session_t *session, packet_t *packet)
{
  session->their_seq = packet->body.syn.seq;
  session->options   = (options_t) packet->body.syn.options;
  session->state = SESSION_STATE_ESTABLISHED;

  /* Since we established a valid session, we can send stuff right away. */
  session->last_transmit = 0;
  session->missed_transmissions = 0;

  return TRUE;
}

NBBOOL _handle_msg_ready(session_t *session, packet_t *packet)
{
  LOG_WARNING("In SESSION_STATE_READY, received unexpected MSG (ignoring)");
  return FALSE;
}

#ifndef NO_ENCRYPTION
NBBOOL _handle_negenc_ready(session_t *session, packet_t *packet)
{
  LOG_FATAL("Re-negotiate isn't implemented yet!");
  exit(1);
}

NBBOOL _handle_auth_ready(session_t *session, packet_t *packet)
{
  LOG_FATAL("Auth isn't implemented yet!");
  exit(1);
}
#endif

NBBOOL _handle_syn_established(session_t *session, packet_t *packet)
{
  LOG_WARNING("Received a SYN in the middle of a session (ignoring)");
  return FALSE;
}

NBBOOL _handle_msg_established(session_t *session, packet_t *packet)
{
  NBBOOL send_right_away = FALSE;

  LOG_INFO("In SESSION_STATE_ESTABLISHED, received a MSG");

  /* Validate the SEQ */
  if(packet->body.msg.seq == session->their_seq)
  {
    /* Verify the ACK is sane */
    uint16_t bytes_acked = packet->body.msg.ack - session->my_seq;

    /* If there's still bytes waiting in the buffer.. */
    if(bytes_acked <= buffer_get_remaining_bytes(session->outgoing_buffer))
    {
      /* Since we got a valid response back, the connection isn't dying. */
      session->missed_transmissions = 0;

      /* Reset the retransmit counter since we got some valid data. */
      if(bytes_acked > 0)
      {
        /* Only reset the counter if we want to re-transmit
         * right away. */
        if(transmit_instantly_on_data)
        {
          session->last_transmit = 0;
          session->missed_transmissions = 0;
          send_right_away = TRUE;
        }
      }

      /* Increment their sequence number */
      session->their_seq = (session->their_seq + packet->body.msg.data_length) & 0xFFFF;

      /* Remove the acknowledged data from the buffer */
      buffer_consume(session->outgoing_buffer, bytes_acked);

      /* Increment my sequence number */
      if(bytes_acked != 0)
      {
        session->my_seq = (session->my_seq + bytes_acked) & 0xFFFF;
      }

      /* Print the data, if we received any, and then immediately receive more. */
      if(packet->body.msg.data_length > 0)
      {
        driver_data_received(session->driver, packet->body.msg.data, packet->body.msg.data_length);
      }
    }
    else
    {
      LOG_WARNING("Bad ACK received (%d bytes acked; %d bytes in the buffer)", bytes_acked, buffer_get_remaining_bytes(session->outgoing_buffer));
    }
  }
  else
  {
    LOG_WARNING("Bad SEQ received (Expected %d, received %d)", session->their_seq, packet->body.msg.seq);
  }

  return send_right_away;
}

NBBOOL _handle_fin(session_t *session, packet_t *packet)
{
  LOG_FATAL("Received FIN: (reason: '%s') - closing session", packet->body.fin.reason);
  session->last_transmit = 0;
  session->missed_transmissions = 0;
  session_kill(session);

  return TRUE;
}

#ifndef NO_ENCRYPTION
NBBOOL _handle_negenc_established(session_t *session, packet_t *packet)
{
  LOG_FATAL("Re-negotiate isn't implemented yet!");
  exit(1);
}
NBBOOL _handle_auth_established(session_t *session, packet_t *packet)
{
  LOG_FATAL("Auth isn't implemented yet!");
  exit(1);
}
#endif

NBBOOL session_data_incoming(session_t *session, uint8_t *data, size_t length)
{
  /* Parse the packet to get the session id */
  packet_t *packet = packet_parse(data, length, session->options);

  /* Set to TRUE if data was properly ACKed and we should send more right away. */
  NBBOOL send_right_away = FALSE;

  /* Suck in any data we can from the driver. */
  poll_for_data(session);

  /* Print packet data if we're supposed to. */
  if(packet_trace)
  {
    printf("INCOMING: ");
    packet_print(packet, session->options);
  }

  if(session->is_ping && packet->packet_type == PACKET_TYPE_PING)
  {
    /* This only returns if the receive is bad. */
    driver_data_received(session->driver, (uint8_t*)packet->body.ping.data, strlen(packet->body.ping.data));
  }
  else
  {
    /* Handlers for the various states / messages */
    packet_handler *handlers[PACKET_TYPE_COUNT_NOT_PING][SESSION_STATE_COUNT];

#ifndef NO_ENCRYPTION
    handlers[PACKET_TYPE_SYN][SESSION_STATE_NEW]            = _handle_bad_new;
#endif
    handlers[PACKET_TYPE_SYN][SESSION_STATE_READY]          = _handle_syn_ready;
    handlers[PACKET_TYPE_SYN][SESSION_STATE_ESTABLISHED]    = _handle_syn_established;

#ifndef NO_ENCRYPTION
    handlers[PACKET_TYPE_MSG][SESSION_STATE_NEW]            = _handle_bad_new;
#endif
    handlers[PACKET_TYPE_MSG][SESSION_STATE_READY]          = _handle_msg_ready;
    handlers[PACKET_TYPE_MSG][SESSION_STATE_ESTABLISHED]    = _handle_msg_established;

#ifndef NO_ENCRYPTION
    handlers[PACKET_TYPE_FIN][SESSION_STATE_NEW]            = _handle_fin;
#endif
    handlers[PACKET_TYPE_FIN][SESSION_STATE_READY]          = _handle_fin;
    handlers[PACKET_TYPE_FIN][SESSION_STATE_ESTABLISHED]    = _handle_fin;

#ifndef NO_ENCRYPTION
    handlers[PACKET_TYPE_NEGENC][SESSION_STATE_NEW]         = _handle_negenc_new;
    handlers[PACKET_TYPE_NEGENC][SESSION_STATE_READY]       = _handle_negenc_ready;
    handlers[PACKET_TYPE_NEGENC][SESSION_STATE_ESTABLISHED] = _handle_negenc_established;

    handlers[PACKET_TYPE_AUTH][SESSION_STATE_NEW]           = _handle_bad_new;
    handlers[PACKET_TYPE_AUTH][SESSION_STATE_READY]         = _handle_auth_ready;
    handlers[PACKET_TYPE_AUTH][SESSION_STATE_ESTABLISHED]   = _handle_auth_established;
#endif

    /* Be extra cautious. */
    if(packet->packet_type < 0 || packet->packet_type >= PACKET_TYPE_COUNT_NOT_PING)
    {
      char *packet_str = packet_to_s(packet, session->options);
      LOG_FATAL("Received an illegal packet: %s", packet_str);
      safe_free(packet_str);
      exit(1);
    }

    if(session->state < 0 || session->state >= SESSION_STATE_COUNT)
    {
      LOG_FATAL("We ended up in an illegal state: 0x%x", session->state);
    }

    send_right_away = handlers[packet->packet_type][session->state](session, packet);
  }

  packet_destroy(packet);

  return send_right_away;
}

void session_kill(session_t *session)
{
  if(session->is_shutdown)
  {
    LOG_WARNING("Tried to kill a session that's already dead: %d\n", session->id);
    return;
  }

  session->is_shutdown = TRUE;
  driver_close(session->driver);
}

void session_destroy(session_t *session)
{
  if(!session->is_shutdown)
    session_kill(session);

  if(session->name)
    safe_free(session->name);

  if(session->driver)
    driver_destroy(session->driver);

  if(session->outgoing_buffer)
    buffer_destroy(session->outgoing_buffer);

  safe_free(session);
}

static char *get_name(char *base)
{
  /* This code isn't beautiful, but it does the job and it's only local
   * stuff anyway (no chance of being exploited). */
  char hostname[128];
  buffer_t *buffer = NULL;

  /* Read the hostname */
  gethostname(hostname, 128);
  hostname[127] = '\0';

  buffer = buffer_create(BO_BIG_ENDIAN);
  buffer_add_string(buffer, base);
  buffer_add_string(buffer, " (");
  buffer_add_string(buffer, hostname);
  buffer_add_string(buffer, ")");
  buffer_add_int8(buffer, 0);

  return (char*)buffer_create_string_and_destroy(buffer, NULL);
}

static session_t *session_create(char *name)
{
  session_t *session     = (session_t*)safe_malloc(sizeof(session_t));

  session->id            = rand() % 0xFFFF;

  /* Check if it's a 16-bit value (I set it to a bigger value to set a random isn) */
  if(isn == (isn & 0xFFFF))
    session->my_seq        = (uint16_t) isn; /* Use the hardcoded one. */
  else
    session->my_seq        = rand() % 0xFFFF; /* Random isn */

#ifndef NO_ENCRYPTION
  session->state         = SESSION_STATE_NEW;
#else
  session->state         = SESSION_STATE_READY;
#endif
  session->their_seq     = 0;
  session->is_shutdown   = FALSE;

  session->last_transmit = 0;
  session->missed_transmissions = 0;
  session->outgoing_buffer = buffer_create(BO_LITTLE_ENDIAN);

#ifndef NO_ENCRYPTION
  if(do_encryption)
  {
    if(!uECC_make_key(session->public_key, session->private_key, uECC_secp256r1()))
    {
      LOG_FATAL("Failed to generate a keypair!");
      exit(1);
    }

    print_hex("My private key", session->private_key, 32);
    print_hex("My public key", session->public_key, 64);
  }
#endif
  session->name = NULL;
  if(name)
  {
    session->name = get_name(name);
    LOG_INFO("Setting session->name to %s", session->name);
  }

  return session;
}

session_t *session_create_console(select_group_t *group, char *name)
{
  session_t *session = session_create(name);

  session->driver = driver_create(DRIVER_TYPE_CONSOLE, driver_console_create(group));

  return session;
}

session_t *session_create_exec(select_group_t *group, char *name, char *process)
{
  session_t *session = session_create(name);

  session->driver = driver_create(DRIVER_TYPE_EXEC, driver_exec_create(group, process));

  return session;
}

session_t *session_create_command(select_group_t *group, char *name)
{
  session_t *session = session_create(name);

  session->driver = driver_create(DRIVER_TYPE_COMMAND, driver_command_create(group));
  session->is_command = TRUE;

  return session;
}

session_t *session_create_ping(select_group_t *group, char *name)
{
  session_t *session = session_create(name);

  session->driver = driver_create(DRIVER_TYPE_PING, driver_ping_create(group));
  session->is_ping = TRUE;

  return session;
}

void debug_set_isn(uint16_t value)
{
  isn = value;

  LOG_WARNING("WARNING: Setting a custom ISN can be dangerous!");
}

void session_enable_packet_trace()
{
  packet_trace = TRUE;
}

void session_set_delay(int delay_ms)
{
  packet_delay = delay_ms;
}

void session_set_transmit_immediately(NBBOOL transmit_immediately)
{
  transmit_instantly_on_data = transmit_immediately;
}

NBBOOL session_is_shutdown(session_t *session)
{
  return session->is_shutdown;
}
