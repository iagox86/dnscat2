#include "memory.h"
#include "types.h"

#include "message.h"

message_handler_t *message_handler_create(message_callback_t *callback, void *param)
{
  message_handler_t *handler = (message_handler_t *)safe_malloc(sizeof(message_handler_t));
  handler->callback = callback;
  handler->param    = param;

  return handler;
}

void message_handler_destroy(message_handler_t *handler)
{
  safe_free(handler);
}

static message_t *message_create()
{
  return (message_t *) safe_malloc(sizeof(message_t));
}

message_t *message_create_create()
{
  message_t *message = message_create();
  message->type = MESSAGE_CREATE;

  return message;
}

message_t *message_data_create(uint16_t session_id, uint8_t *data, size_t length)
{
  message_t *message = message_create();

  message->type = MESSAGE_DATA;

  message->message.data.session_id = session_id;
  message->message.data.data       = data;
  message->message.data.length     = length;

  return message;
}

message_t *message_destroy_create(uint16_t session_id)
{
  message_t *message = message_create();
  message->type = MESSAGE_DESTROY;
  message->message.destroy.session_id = session_id;

  return message;
}

void message_destroy(message_t *message)
{
  safe_free(message);
}

void message_pass(message_handler_t *handler, message_t *message)
{
  handler->callback(message, handler->param);
}
