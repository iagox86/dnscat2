#ifndef __MESSAGE_H__
#define __MESSAGE_H__

#include "types.h"

typedef enum
{
  MESSAGE_START,
  MESSAGE_CREATE_SESSION,
  MESSAGE_DATA_OUT,
  MESSAGE_DATA_IN,
  MESSAGE_DESTROY_SESSION,
  MESSAGE_DESTROY,

  MESSAGE_MAX_MESSAGE_TYPE,
} message_type_t;

typedef struct
{
} message_start_t;

typedef struct
{
  struct
  {
    uint16_t session_id;
  } out;
} message_create_t;

typedef struct
{
  uint16_t   session_id;
  uint8_t   *data;
  size_t     length;
} message_data_t;

typedef struct
{
  uint16_t   session_id;
} message_destroy_session_t;

typedef struct
{
} message_destroy_t;

typedef struct
{
  message_type_t type;
  union
  {
    message_create_t  create;
    message_data_t    data;
    message_destroy_t destroy;
  } message;
} message_t;

typedef void(message_callback_t)(message_t *message, void *param);

typedef struct
{
  message_callback_t *callback;
  void *param;
} message_handler_t;

message_handler_t *message_handler_create(message_callback_t *callback, void *param);
void message_handler_destroy(message_handler_t *handler);

message_t *message_create_start();
message_t *message_create_session_create();
message_t *message_data_in_create(uint16_t session_id, uint8_t *data, size_t length);
message_t *message_data_out_create(uint16_t session_id, uint8_t *data, size_t length);
message_t *message_destroy_session_create(uint16_t session_id);
message_t *message_create_destroy();
void message_destroy(message_t *message);

void message_subscribe(message_type_t message_type, message_handler_t *handler);
void message_unsubscribe(message_type_t message_type, message_handler_t *handler);
void message_post(message_t *message);

#endif
