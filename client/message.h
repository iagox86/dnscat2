#ifndef __MESSAGE_H__
#define __MESSAGE_H__

#include "types.h"

typedef enum
{
  /* This is called once, at the beginning of the process, and is used for
   * initialization. */
  MESSAGE_START,
  MESSAGE_CREATE_SESSION,
  MESSAGE_SESSION_CREATED,
  MESSAGE_DATA_OUT,
  MESSAGE_DATA_IN,
  MESSAGE_DESTROY_SESSION,
  MESSAGE_DESTROY,

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
      struct
      {
        uint16_t session_id;
      } out;
    } create_session;

    struct
    {
      struct
      {
        session_data_callback_t *outgoing_callback;
        session_data_callback_t *incoming_callback;
        void                    *callback_param;
        size_t                   max_size;
      } out;
    } session_created;

    struct
    {
      uint16_t   session_id;
      uint8_t   *data;
      size_t     length;
    } data;

    struct
    {
      uint16_t session_id;
    } destroy_session;

    struct
    {
    } destroy;
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
void message_post_create_session(uint16_t *session_id);
void message_post_session_created(session_data_callback_t **outgoing_callback, session_data_callback_t **incoming_callback, void **callback_param, size_t *max_size);
void message_post_data_in(uint16_t session_id, uint8_t *data, size_t length);
void message_post_data_out(uint16_t session_id, uint8_t *data, size_t length);
void message_post_destroy_session(uint16_t session_id);
void message_post_destroy();

#endif
