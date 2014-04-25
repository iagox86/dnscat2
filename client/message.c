#include "memory.h"
#include "types.h"

#include "message.h"

typedef struct _message_handler_entry_t
{
  message_handler_t *handler;
  struct _message_handler_entry_t *next;
} message_handler_entry_t;

static message_handler_entry_t *handlers[MESSAGE_MAX_MESSAGE_TYPE];

static NBBOOL is_initialized = FALSE;

static message_handler_t *message_handler_create(message_callback_t *callback, void *param)
{
  message_handler_t *handler = (message_handler_t *)safe_malloc(sizeof(message_handler_t));
  handler->callback = callback;
  handler->param    = param;

  return handler;
}

/* Put the entry at the start of the linked list. */
void message_subscribe(message_type_t message_type, message_callback_t *callback, void *param)
{
  message_handler_t *handler = message_handler_create(callback, param);
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

void message_unsubscribe(message_type_t message_type, message_callback_t *callback)
{
  /* TODO */
}

void message_cleanup()
{
  message_handler_entry_t *this;
  message_handler_entry_t *next;
  size_t type;

  for(type = 0; type < MESSAGE_MAX_MESSAGE_TYPE; type++)
  {
    for(this = handlers[type]; this; this = next)
    {
      next = this->next;
      safe_free(this->handler);
      safe_free(this);
    }
  }
}

void message_handler_destroy(message_handler_t *handler)
{
  safe_free(handler);
}

static message_t *message_create(message_type_t message_type)
{
  message_t *message = (message_t *) safe_malloc(sizeof(message_t));
  message->type = message_type;
  return message;
}

void message_destroy(message_t *message)
{
  safe_free(message);
}

void message_post(message_t *message)
{
  message_handler_entry_t *handler;

  for(handler = handlers[message->type]; handler; handler = handler->next)
    handler->handler->callback(message, handler->handler->param);
}

void message_post_config_int(char *name, int value)
{
  message_t *message = message_create(MESSAGE_CONFIG);
  message->message.config.name = name;
  message->message.config.type = CONFIG_INT;
  message->message.config.value.int_value = value;
  message_post(message);
  message_destroy(message);
}

void message_post_config_string(char *name, char *value)
{
  message_t *message = message_create(MESSAGE_CONFIG);
  message->message.config.name = name;
  message->message.config.type = CONFIG_STRING;
  message->message.config.value.string_value = value;
  message_post(message);
  message_destroy(message);
}

void message_post_start()
{
  message_t *message = message_create(MESSAGE_START);
  message_post(message);
  message_destroy(message);
}

void message_post_shutdown()
{
  message_t *message = message_create(MESSAGE_SHUTDOWN);
  message_post(message);
  message_destroy(message);
}

uint16_t message_post_create_session()
{
  uint16_t session_id;

  message_t *message = message_create(MESSAGE_CREATE_SESSION);
  message_post(message);

  session_id = message->message.create_session.out.session_id;

  message_destroy(message);

  return session_id;
}

void message_post_session_created(uint16_t session_id)
{
  message_t *message = message_create(MESSAGE_SESSION_CREATED);
  message->message.session_created.session_id = session_id;
  message_post(message);
  message_destroy(message);
}

void message_post_close_session(uint16_t session_id)
{
  message_t *message = message_create(MESSAGE_CLOSE_SESSION);
  message->message.session_created.session_id = session_id;
  message_post(message);
  message_destroy(message);
}

void message_post_session_closed(uint16_t session_id)
{
  message_t *message = message_create(MESSAGE_CLOSE_SESSION);
  message->message.session_created.session_id = session_id;
  message_post(message);
  message_destroy(message);
}

void message_post_data_out(uint16_t session_id, uint8_t *data, size_t length)
{
  message_t *message = message_create(MESSAGE_DATA_OUT);
  message->message.data_out.session_id = session_id;
  message->message.data_out.data = data;
  message->message.data_out.length = length;
  message_post(message);
  message_destroy(message);
}

void message_post_packet_out(packet_t *packet)
{
  message_t *message = message_create(MESSAGE_PACKET_OUT);
  message->message.packet_out.packet = packet;
  message_post(message);
  message_destroy(message);
}

void message_post_packet_in(packet_t *packet)
{
  message_t *message = message_create(MESSAGE_PACKET_IN);
  message->message.packet_in.packet = packet;
  message_post(message);
  message_destroy(message);
}

void message_post_data_in(uint16_t session_id, uint8_t *data, size_t length)
{
  message_t *message = message_create(MESSAGE_DATA_IN);
  message->message.data_in.session_id = session_id;
  message->message.data_in.data = data;
  message->message.data_in.length = length;
  message_post(message);
  message_destroy(message);
}

void message_post_heartbeat()
{
  message_t *message = message_create(MESSAGE_HEARTBEAT);
  message_post(message);
  message_destroy(message);
}
