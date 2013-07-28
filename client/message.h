#ifndef __MESSAGE_H__
#define __MESSAGE_H__

#include "packet.h"
#include "types.h"

typedef enum
{
  /* This is posted once, when the dnscat process starts, and is when any
   * initial connections should be made. */
  MESSAGE_START,

  /* This begins the "shutdown" process - closing all sessions and eventually
   * terminating the process. */
  MESSAGE_SHUTDOWN,

  /* Requests the session library create a new session. This will cause a
   * SESSION_CREATED message to be posted out that contains the new session
   * id value. */
  MESSAGE_CREATE_SESSION,

  /* This is posted by the session library when a new session has been
   * created. */
  MESSAGE_SESSION_CREATED,

  /* Requests the session library close the session. The actual closing will
   * potentially happen later, at which point a SESSION_CLOSED message will
   * be posted. */
  MESSAGE_CLOSE_SESSION,

  /* Posted by the session library to let listeners know that the session has
   * been closed. */
  MESSAGE_SESSION_CLOSED,

  /* This is posted by the input driver, and injects data into the session to
   * be sent out when the session sees fit. */
  MESSAGE_DATA_OUT,

  /* Posted by the session library when data is ready to be sent directly to the
   * wire */
  MESSAGE_PACKET_OUT,

  /* Raw dnscat packet data posted by the output driver, directly from the wire. */
  MESSAGE_PACKET_IN,

  /* This is the plaintext data after it's been handled by the input driver. */
  MESSAGE_DATA_IN,

  /* Sent once every second or so. */
  MESSAGE_HEARTBEAT,

  /* Used to create arrays and such. */
  MESSAGE_MAX_MESSAGE_TYPE,
} message_type_t;

/* This defines a message that can be passed around. It's basically a big
 * union across all the message types. */
typedef struct
{
  message_type_t type;
  union
  {
    struct
    {
    } start;

    struct
    {
    } shutdown;

    struct
    {
    } create_session;

    struct
    {
      uint16_t session_id;
    } session_created;

    struct
    {
      uint16_t session_id;
    } close_session;

    struct
    {
      uint16_t session_id;
    } session_closed;

    struct
    {
      uint16_t   session_id;
      uint8_t   *data;
      size_t     length;
    } data_out;

    struct
    {
      packet_t  *packet;
    } packet_out;

    struct
    {
      packet_t  *packet;
    } packet_in;

    struct
    {
      uint16_t   session_id;
      uint8_t   *data;
      size_t     length;
    } data_in;

    struct
    {
    } heartbeat;
  } message;
} message_t;

/* Define the callback function for messages. */
typedef void(message_callback_t)(message_t *message, void *param);

/* Define the message handler type, which is basically a callback function
 * and the corresponding parameter to send with it. */
typedef struct
{
  message_callback_t *callback;
  void *param;
} message_handler_t;

void message_subscribe(message_type_t message_type, message_callback_t *callback, void *param);
void message_unsubscribe(message_type_t message_type, message_callback_t *callback); /* TODO */
void message_cleanup();

void message_post_start();
void message_post_shutdown();
void message_post_create_session();
void message_post_session_created(uint16_t session_id);
void message_post_close_session(uint16_t session_id);
void message_post_session_closed(uint16_t session_id);

void message_post_data_out(uint16_t session_id, uint8_t *data, size_t length);
void message_post_packet_out(packet_t *packet);
void message_post_packet_in(packet_t *packet);
void message_post_data_in(uint16_t session_id, uint8_t *data, size_t length);

void message_post_heartbeat();

#endif
