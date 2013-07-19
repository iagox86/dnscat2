#ifndef __MESSAGE_H__
#define __MESSAGE_H__

#include "types.h"

typedef enum
{
  MESSAGE_CREATE,
  MESSAGE_DATA,
  MESSAGE_DESTROY,
} message_type_t;

typedef struct
{
  uint16_t   out_session_id;
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

message_t *message_create_create();
message_t *message_data_create(uint16_t session_id, uint8_t *data, size_t length);
message_t *message_destroy_create(uint16_t session_id);
void message_destroy(message_t *message);

void message_pass(message_handler_t *handler, message_t *message);

#endif
