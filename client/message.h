#ifndef __MESSAGE_H__
#define __MESSAGE_H__

#include "packet.h"
#include "types.h"

typedef enum
{
  /* This is used to set a configuration value in another listener. */
  MESSAGE_CONFIG           = 0x00,

  /* This is posted once, when the dnscat process starts, and is when any
   * initial connections should be made. */
  /* DEPRECATED */
  /*MESSAGE_START            = 0x01,*/

  /* This begins the "shutdown" process - closing all sessions and eventually
   * terminating the process. */
  MESSAGE_SHUTDOWN         = 0x02,

  /* Requests the session library create a new session. This will cause a
   * SESSION_CREATED message to be posted out that contains the new session
   * id value. */
  MESSAGE_CREATE_SESSION   = 0x03,

  /* This is posted by the session library when a new session has been
   * created. */
  MESSAGE_SESSION_CREATED  = 0x04,

  /* Requests the session library close the session. The actual closing will
   * potentially happen later, at which point a SESSION_CLOSED message will
   * be posted. */
  MESSAGE_CLOSE_SESSION    = 0x05,

  /* Posted by the session library to let listeners know that the session has
   * been closed. */
  MESSAGE_SESSION_CLOSED   = 0x06,

  /* This is posted by the input driver, and injects data into the session to
   * be sent out when the session sees fit. */
  MESSAGE_DATA_OUT         = 0x07,

  /* Posted by the session library when data is ready to be sent directly to the
   * wire */
  MESSAGE_PACKET_OUT       = 0x08,

  /* Raw dnscat packet data posted by the output driver, directly from the wire. */
  MESSAGE_PACKET_IN        = 0x09,

  /* This is the plaintext data after it's been handled by the input driver. */
  MESSAGE_DATA_IN          = 0x0a,

  /* Sent once every second or so. */
  MESSAGE_HEARTBEAT        = 0x0b,

  /* Used when requesting a ping. */
  MESSAGE_PING_REQUEST     = 0x0c,

  /* Used when a PING response comes back. */
  MESSAGE_PING_RESPONSE    = 0x0d,

  /***********************************/
  /* Used to create arrays and such. */
  /***********************************/
  MESSAGE_MAX_MESSAGE_TYPE = 0x0e,
  /***********************************/
  /* Used to create arrays and such. */
  /***********************************/
} message_type_t;

typedef struct {
  char *name;
  union {
    char *s;
    uint32_t i;
  } value;
} message_options_t;

typedef enum
{
  CONFIG_INT,
  CONFIG_STRING
} config_type_t;

/* This defines a message that can be passed around. It's basically a big
 * union across all the message types. */
typedef struct
{
  message_type_t type;
  union
  {
    struct
    {
      char *name;
      config_type_t type;
      union
      {
        int   int_value;
        char *string_value;
      } value;
    } config;

    struct
    {
      int dummy; /* WIN32 doesn't allow empty structs/unions */
    } shutdown;

    struct
    {
      char *name;
      char *download;
      uint32_t first_chunk;
      NBBOOL is_command;

      struct
      {
        uint16_t session_id;
      } out;
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
      uint8_t *data;
      size_t   length;
    } packet_out;

    struct
    {
      uint8_t *data;
      size_t   length;
    } packet_in;

    struct
    {
      uint16_t   session_id;
      uint8_t   *data;
      size_t     length;
    } data_in;

    struct
    {
      char *data;
    } ping_request;

    struct
    {
      char *data;
    } ping_response;

    struct
    {
      int dummy; /* WIN32 doesn't allow empty structs/unions */
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

void message_post_config_int(char *name, int value);
void message_post_config_string(char *name, char *value);

void message_post_shutdown();
/* options must either be NULL, or terminated with a NULL entry */
uint16_t message_post_create_session(message_options_t options[]);
void message_post_session_created(uint16_t session_id);
void message_post_close_session(uint16_t session_id);
void message_post_session_closed(uint16_t session_id);

void message_post_data_out(uint16_t session_id, uint8_t *data, size_t length);
void message_post_packet_out(uint8_t *data, size_t length);
void message_post_packet_in(uint8_t *data, size_t length);
void message_post_data_in(uint16_t session_id, uint8_t *data, size_t length);

void message_post_heartbeat();

void message_post_ping_request(char *data);
void message_post_ping_response(char *data);

#endif
