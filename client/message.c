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

message_t *message_create_start()
{
  message_t *message = message_create();
  message->type = MESSAGE_START;

  return message;
}

message_t *message_create_session_create()
{
  message_t *message = message_create();
  message->type = MESSAGE_CREATE_SESSION;

  return message;
}

message_t *message_data_in_create(uint16_t session_id, uint8_t *data, size_t length)
{
  message_t *message = message_create();

  message->type = MESSAGE_DATA_IN;

  message->message.data.session_id = session_id;
  message->message.data.data       = data;
  message->message.data.length     = length;

  return message;
}

message_t *message_data_out_create(uint16_t session_id, uint8_t *data, size_t length)
{
  message_t *message = message_create();

  message->type = MESSAGE_DATA_OUT;

  message->message.data.session_id = session_id;
  message->message.data.data       = data;
  message->message.data.length     = length;

  return message;
}

message_t *message_destroy_session_create(uint16_t session_id)
{
  message_t *message = message_create();
  message->type = MESSAGE_DESTROY_SESSION;

  return message;
}

message_t *message_create_destroy()
{
  message_t *message = message_create();
  message->type = MESSAGE_DESTROY;

  return message;
}

void message_post_start()
{
  message_t *message = message_create_start();
  message_post(message);
  message_destroy(message);
}

void message_post_data_out(uint16_t session_id, uint8_t *data, size_t length)
{
  message_t *message = message_data_out_create(session_id, data, length);
  message_post(message);
  message_destroy(message);
}

void message_post_data_in(uint16_t session_id, uint8_t *data, size_t length)
{
  message_t *message = message_data_in_create(session_id, data, length);
  message_post(message);
  message_destroy(message);
}

void message_destroy(message_t *message)
{
  safe_free(message);
}

typedef struct _message_handler_entry_t
{
  message_handler_t *handler;
  struct _message_handler_entry_t *next;
} message_handler_entry_t;

static message_handler_entry_t *handlers[MESSAGE_MAX_MESSAGE_TYPE];

static NBBOOL is_initialized = FALSE;

/* Put the entry at the start of the linked list. */
void message_subscribe(message_type_t message_type, message_handler_t *handler)
{
  message_handler_entry_t *entry;
  if(!is_initialized)
  {
    size_t i;
    for(i = 0; i < MESSAGE_MAX_MESSAGE_TYPE; i++)
      handlers[i] = NULL;
    is_initialized = TRUE;
  }

  entry = (message_handler_entry_t *)safe_malloc(sizeof(message_handler_entry_t));
  entry->handler = handler;
  entry->next = handlers[message_type];
  handlers[message_type] = entry;
}

void message_unsubscribe(message_type_t message_type, message_handler_t *handler)
{
  /* TODO */
}

void message_post(message_t *message)
{
  message_handler_entry_t *handler;

  for(handler = handlers[message->type]; handler; handler = handler->next)
    handler->handler->callback(message, handler->handler->param);
}
