#include <stdlib.h>

#include "buffer.h"
#include "memory.h"
#include "session.h"

session_t *session_create()
{
  session_t *session = (session_t *) safe_malloc(sizeof(session_t));

  session->id = rand() & 0xFFFF;
  session->my_seq = rand() & 0xFFFF;

  return session;
}

void session_destroy(session_t *session)
{
  buffer_destroy(session->incoming_data);
  buffer_destroy(session->outgoing_data);
  safe_free(session);
}
