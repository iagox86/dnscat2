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

#include "controller/encrypted_packet.h"
#include "controller/packet.h"
#include "libs/buffer.h"
#include "libs/crypto/sha3.h"
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

#define CLIENT_WRITE_KEY "client_write_key"
#define CLIENT_MAC_KEY   "client_mac_key"
#define SERVER_WRITE_KEY "server_write_key"
#define SERVER_MAC_KEY   "server_mac_key"

#endif

/* Define a handler function pointer. */
typedef NBBOOL(packet_handler)(session_t *session, packet_t *packet);

static NBBOOL should_we_encrypt(session_t *session)
{
  return (do_encryption && session->state != SESSION_STATE_NEW);
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

uint8_t *session_get_outgoing(session_t *session, size_t *packet_length, size_t max_length)
{
  packet_t *packet       = NULL;
  uint8_t  *packet_bytes = NULL;
  uint8_t  *data         = NULL;
  size_t    data_length  = -1;

  /* Suck in any data we can from the driver. */
  poll_for_data(session);

  /* Don't transmit too quickly without receiving anything. */
  if(!can_i_transmit_yet(session))
    return NULL;

#ifndef NO_ENCRYPTION
  /* If we're in encryption mode, we have to save 8 bytes for the encrypted_packet header. */
  if(should_we_encrypt(session))
  {
    max_length -= 8;

    if(max_length <= 0)
    {
      LOG_FATAL("There isn't enough room in this protocol to encrypt packets!\n");
      exit(1);
    }
  }
#endif

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
        packet = packet_create_syn(session->id, session->my_seq, (options_t)0);

        if(session->is_command)
          packet_syn_set_is_command(packet);

        if(session->name)
          packet_syn_set_name(packet, session->name);

#ifndef NO_ENCRYPTION
        if(do_encryption)
          packet_syn_set_encrypted(packet, 0, session->public_key);
#endif
        break;

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
    if(packet_trace)
    {
      printf("OUTGOING: ");
      packet_print(packet, session->options);
    }

    packet_bytes = packet_to_bytes(packet, packet_length, session->options);
    packet_destroy(packet);

#ifndef NO_ENCRYPTION
    if(should_we_encrypt(session))
    {
      buffer_t *packet_buffer = buffer_create(BO_LITTLE_ENDIAN);

      buffer_add_bytes(packet_buffer, packet_bytes, *packet_length);
      safe_free(packet_bytes);

      encrypt_buffer(packet_buffer, session->my_write_key, session->my_nonce++);
      sign_buffer(packet_buffer, session->my_mac_key);

      packet_bytes = buffer_create_string_and_destroy(packet_buffer, packet_length);
    }
#endif

    session->last_transmit = time_ms();
    session->missed_transmissions++;
  }

  return packet_bytes;
}

static NBBOOL _handle_syn_new(session_t *session, packet_t *packet)
{
  sha3_ctx ctx;

  session->their_seq = packet->body.syn.seq;
  session->options   = (options_t) packet->body.syn.options;

#ifndef NO_ENCRYPTION
  /* Make sure they're encrypting. */
  if(do_encryption && !(packet->body.syn.options & OPT_ENCRYPTED))
  {
    LOG_FATAL("The server doesn't want to encrypt the session (or ran into an error)!");
    packet_print(packet, session->options);
    exit(1);
  }

  print_hex("Their public key", packet->body.syn.public_key, 64);
  if(!uECC_shared_secret(packet->body.syn.public_key, session->private_key, session->shared_secret, uECC_secp256r1()))
  {
    LOG_FATAL("Failed to calculate a shared secret!");
    exit(1);
  }

  print_hex("Shared secret", session->shared_secret, 32);

  /* Generate the four keys we need. */
  sha3_256_init(&ctx);
  sha3_update(&ctx, session->shared_secret, 32);
  sha3_update(&ctx, (uint8_t*)CLIENT_WRITE_KEY, strlen(CLIENT_WRITE_KEY));
  sha3_final(&ctx, session->my_write_key);

  sha3_256_init(&ctx);
  sha3_update(&ctx, session->shared_secret, 32);
  sha3_update(&ctx, (uint8_t*)CLIENT_MAC_KEY, strlen(CLIENT_MAC_KEY));
  sha3_final(&ctx, session->my_mac_key);

  sha3_256_init(&ctx);
  sha3_update(&ctx, session->shared_secret, 32);
  sha3_update(&ctx, (uint8_t*)SERVER_WRITE_KEY, strlen(SERVER_WRITE_KEY));
  sha3_final(&ctx, session->their_write_key);

  sha3_256_init(&ctx);
  sha3_update(&ctx, session->shared_secret, 32);
  sha3_update(&ctx, (uint8_t*)SERVER_MAC_KEY, strlen(SERVER_MAC_KEY));
  sha3_final(&ctx, session->their_mac_key);
#endif

  /* We can send a response right away */
  session->last_transmit        = 0;
  session->missed_transmissions = 0;
  session->state                = SESSION_STATE_ESTABLISHED;

  return TRUE;
}

static NBBOOL _handle_syn_established(session_t *session, packet_t *packet)
{
  LOG_WARNING("Received a SYN in the middle of a session (ignoring)");
  return FALSE;
}

static NBBOOL _handle_msg_new(session_t *session, packet_t *packet)
{
  LOG_WARNING("Received an unexpected message; very likely the remains of an old session");
  return FALSE;
}

static NBBOOL _handle_msg_established(session_t *session, packet_t *packet)
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

static NBBOOL _handle_fin(session_t *session, packet_t *packet)
{
  LOG_FATAL("Received FIN: (reason: '%s') - closing session", packet->body.fin.reason);
  session->last_transmit = 0;
  session->missed_transmissions = 0;
  session_kill(session);

  return TRUE;
}

static NBBOOL _handle_auth(session_t *session, packet_t *packet)
{
  LOG_FATAL("Received AUTH; we haven't implemented that yet!");
  exit(1);
}

NBBOOL session_data_incoming(session_t *session, uint8_t *data, size_t length)
{
  uint8_t *packet_bytes;

  /* Parse the packet to get the session id */
  packet_t *packet;

  /* Set to TRUE if data was properly ACKed and we should send more right away. */
  NBBOOL send_right_away = FALSE;

  /* Suck in any data we can from the driver. */
  /* TODO: I'm not 100% sure what this does, I should check it out. */
  poll_for_data(session);

  /* Make a copy of the data so we can mess around with it. */
  packet_bytes = safe_malloc(length);
  memcpy(packet_bytes, data, length);

#ifndef NO_ENCRYPTION
  if(should_we_encrypt(session))
  {
    buffer_t *packet_buffer = buffer_create_with_data(BO_BIG_ENDIAN, packet_bytes, length);
    safe_free(packet_bytes);

    if(!check_signature(packet_buffer, session->their_mac_key))
    {
      LOG_FATAL("Server's signature was wrong!");
      exit(1);
    }
    printf("SIGNATURE VERIFIED!!\n");

    /* TODO: Verify their nonce */
    decrypt_buffer(packet_buffer, session->their_write_key, NULL);

    /* Switch to the decrypted data. */
    packet_bytes = buffer_create_string_and_destroy(packet_buffer, &length);
  }
#endif

  /* Parse the packet. */
  packet = packet_parse(data, length, session->options);

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

    handlers[PACKET_TYPE_SYN][SESSION_STATE_NEW]            = _handle_syn_new;
    handlers[PACKET_TYPE_SYN][SESSION_STATE_ESTABLISHED]    = _handle_syn_established;

    handlers[PACKET_TYPE_MSG][SESSION_STATE_NEW]            = _handle_msg_new;
    handlers[PACKET_TYPE_MSG][SESSION_STATE_ESTABLISHED]    = _handle_msg_established;

    handlers[PACKET_TYPE_FIN][SESSION_STATE_NEW]            = _handle_fin;
    handlers[PACKET_TYPE_FIN][SESSION_STATE_ESTABLISHED]    = _handle_fin;

    handlers[PACKET_TYPE_AUTH][SESSION_STATE_NEW]           = _handle_auth;
    handlers[PACKET_TYPE_AUTH][SESSION_STATE_ESTABLISHED]   = _handle_auth;

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

  session->state         = SESSION_STATE_NEW;
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

    session->my_nonce = 0;
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
