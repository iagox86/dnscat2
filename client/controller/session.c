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

#include "libs/log.h"
#include "libs/memory.h"
#include "libs/select_group.h"
#include "packet.h"

#include "session.h"

/* Allow the user to override the initial sequence number. */
static uint32_t isn = 0xFFFFFFFF;

/* Enable/disable packet tracing. */
static NBBOOL packet_trace;

/* The amount of delay between packets. */
static int packet_delay = 1000;

/* Transmit instantly when data is received. */
static NBBOOL transmit_instantly_on_data = TRUE;

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
        LOG_INFO("In SESSION_STATE_NEW, sending a SYN packet (SEQ = 0x%04x)...", session->my_seq);

        packet = packet_create_syn(session->id, session->my_seq, (options_t)0);

        if(session->is_command)
          packet_syn_set_is_command(packet);

        if(session->name)
          packet_syn_set_name(packet, session->name);

        break;

      case SESSION_STATE_ESTABLISHED:
        /* Read data without consuming it (ie, leave it in the buffer till it's ACKed) */
        data = buffer_read_remaining_bytes(session->outgoing_buffer, &data_length, max_length - packet_get_msg_size(session->options), FALSE);
        LOG_INFO("In SESSION_STATE_ESTABLISHED, sending a MSG packet (SEQ = 0x%04x, ACK = 0x%04x, %zd bytes of data...)", session->my_seq, session->their_seq, data_length);

        if(data_length == 0 && session->is_shutdown)
          packet = packet_create_fin(session->id, "Stream closed");
        else
          packet = packet_create_msg_normal(session->id, session->my_seq, session->their_seq, data, data_length);

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

  if(session->is_ping)
  {
    /* This only returns if the receive is bad. */
    driver_data_received(session->driver, (uint8_t*)packet->body.ping.data, strlen(packet->body.ping.data));
  }
  else
  {
    switch(session->state)
    {
      case SESSION_STATE_NEW:
        if(packet->packet_type == PACKET_TYPE_SYN)
        {
          LOG_INFO("In SESSION_STATE_NEW, received SYN (ISN = 0x%04x)", packet->body.syn.seq);
          session->their_seq = packet->body.syn.seq;
          session->options   = (options_t) packet->body.syn.options;
          session->state = SESSION_STATE_ESTABLISHED;

          /* Since we established a valid session, we can send stuff right away. */
          session->last_transmit = 0;
          session->missed_transmissions = 0;
          send_right_away = TRUE;
        }
        else if(packet->packet_type == PACKET_TYPE_MSG)
        {
          LOG_WARNING("In SESSION_STATE_NEW, received unexpected MSG (ignoring)");
        }
        else if(packet->packet_type == PACKET_TYPE_FIN)
        {
          /* TODO: I shouldn't exit here. */
          LOG_FATAL("In SESSION_STATE_NEW, received FIN: %s", packet->body.fin.reason);

          exit(0);
        }
        else
        {
          /* TODO: I shouldn't exit here. */
          LOG_FATAL("Unknown packet type: 0x%02x", packet->packet_type);
          exit(1);
        }

        break;
      case SESSION_STATE_ESTABLISHED:
        if(packet->packet_type == PACKET_TYPE_SYN)
        {
          LOG_WARNING("In SESSION_STATE_ESTABLISHED, recieved SYN (ignoring)");
        }
        else if(packet->packet_type == PACKET_TYPE_MSG)
        {
          LOG_INFO("In SESSION_STATE_ESTABLISHED, received a MSG");

          /* Validate the SEQ */
          if(packet->body.msg.options.normal.seq == session->their_seq)
          {
            /* Verify the ACK is sane */
            uint16_t bytes_acked = packet->body.msg.options.normal.ack - session->my_seq;

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
            LOG_WARNING("Bad SEQ received (Expected %d, received %d)", session->their_seq, packet->body.msg.options.normal.seq);
          }
        }
        else if(packet->packet_type == PACKET_TYPE_FIN)
        {
          LOG_FATAL("In SESSION_STATE_ESTABLISHED, received FIN: %s - closing session", packet->body.fin.reason);
          session->last_transmit = 0;
          session->missed_transmissions = 0;
          session_kill(session);
        }
        else
        {
          LOG_FATAL("Unknown packet type: 0x%02x - closing session", packet->packet_type);
          session_kill(session);
        }

        break;
      default:
        LOG_FATAL("Wound up in an unknown state: 0x%x", session->state);
        packet_destroy(packet);
        session_kill(session);
        exit(1);
    }
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

  session->name = NULL;
  if(name)
  {
    session->name = safe_strdup(name);
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
