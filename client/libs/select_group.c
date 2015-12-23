/* select_group.c
 * By Ron
 * Created August, 2008
 *
 * (See LICENSE.md)
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifdef WIN32
#include <winsock2.h>
#else
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#endif

#include "memory.h"
#include "select_group.h"
#include "tcp.h"

/* People probably won't be using more than 32 sockets, so 32 should be a good number
 * to avoid unnecessary realloc() calls. */
#define LIST_STARTING_SIZE 32
#define MAX_RECV 8192

/* Some macros to access elements within the numbered structure. */
#define SG_SOCKET(sg,i) sg->select_list[i]->s
#ifdef WIN32
#define SG_PIPE(sg,i) sg->select_list[i]->pipe
#endif
#define SG_TYPE(sg,i) sg->select_list[i]->type
#define SG_READY(sg,i) sg->select_list[i]->ready_callback
#define SG_RECV(sg,i) sg->select_list[i]->recv_callback
#define SG_LISTEN(sg,i) sg->select_list[i]->listen_callback
#define SG_ERROR(sg,i) sg->select_list[i]->error_callback
#define SG_CLOSED(sg,i) sg->select_list[i]->closed_callback
#define SG_WAITING(sg,i) sg->select_list[i]->waiting_for
#define SG_BUFFER(sg,i) sg->select_list[i]->buffer
#define SG_BUFFERED(sg,i) sg->select_list[i]->buffered
#define SG_IS_READY(sg,i) sg->select_list[i]->ready
#define SG_IS_ACTIVE(sg,i) sg->select_list[i]->active
#define SG_PARAM(sg,i) sg->select_list[i]->param

static int getlastsocketerror(int s)
{
#ifdef WIN32
  int len = 4;
  int val;
  getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&val, &len);

  return val;
#else
  return getlasterror();
#endif
}

static select_t *find_select_by_socket(select_group_t *group, int s)
{
  size_t i;
  select_t *ret = NULL;

  for(i = 0; i < group->current_size && !ret; i++)
    if(SG_IS_ACTIVE(group, i) && SG_SOCKET(group, i) == s)
      ret = group->select_list[i];

  return ret;
}

select_group_t *select_group_create()
{
  select_group_t *new_group = (select_group_t*) safe_malloc(sizeof(select_group_t));
  memset(new_group, 0, sizeof(select_group_t));

  new_group->select_list = safe_malloc(LIST_STARTING_SIZE * sizeof(select_t));
  new_group->current_size = 0;
  new_group->maximum_size = LIST_STARTING_SIZE;
  new_group->timeout_callback = NULL;
  new_group->timeout_param = NULL;

  return new_group;
}

void select_group_destroy(select_group_t *group)
{
  size_t i;

  for(i = 0; i < group->current_size; i++)
  {
    if(SG_BUFFER(group, i))
    {
      memset(SG_BUFFER(group, i), 0, SG_WAITING(group, i));
      safe_free(SG_BUFFER(group, i));
    }
    memset(group->select_list[i], 0, sizeof(select_t));
    safe_free(group->select_list[i]);
  }

  memset(group->select_list, 0, group->maximum_size * sizeof(select_t*));
  safe_free(group->select_list);

  memset(group, 0, sizeof(select_group_t));
  safe_free(group);
}

void select_group_add_socket(select_group_t *group, int s, SOCKET_TYPE_t type, void *param)
{
  select_t *new_select;

  if(find_select_by_socket(group, s))
    DIE("Tried to add same socket to select_group more than once.");

  new_select = (select_t*) safe_malloc(sizeof(select_t));
  memset(new_select, 0, sizeof(select_t));
  new_select->s = s;
  new_select->type = type;
  new_select->ready = FALSE;
  new_select->active = TRUE;
  new_select->param = param;

  group->select_list[group->current_size] = new_select;
  group->current_size++;
  if(group->current_size >= group->maximum_size)
  {
    group->maximum_size = group->maximum_size * 2;
    if(group->maximum_size > SOCKET_LIST_MAX_SOCKETS)
    {
      fprintf(stderr, "Too many sockets!\n");
      exit(1);
    }
    group->select_list = safe_realloc(group->select_list, group->maximum_size * sizeof(select_t*));
  }

  if(s > group->biggest_socket)
    group->biggest_socket = s;
}

#ifdef WIN32
void select_group_add_pipe(select_group_t *group, int identifier, HANDLE pipe, void *param)
{
  select_t *new_select;

  if(find_select_by_socket(group, identifier))
    DIE("Tried to add same pipe to select_group more than once (or choose a poor identifier).");

  new_select = (select_t*) safe_malloc(sizeof(select_t));
  memset(new_select, 0, sizeof(select_t));
  new_select->s      = identifier;
  new_select->pipe   = pipe;
  new_select->type   = SOCKET_TYPE_PIPE;
  new_select->active = TRUE;
  new_select->param  = param;

  group->select_list[group->current_size] = new_select;
  group->current_size++;
  if(group->current_size >= group->maximum_size)
  {
    group->maximum_size = group->maximum_size * 2;
    if(group->maximum_size > SOCKET_LIST_MAX_SOCKETS)
    {
      fprintf(stderr, "Too many sockets!\n");
      exit(1);
    }
    group->select_list = safe_realloc(group->select_list, group->maximum_size * sizeof(select_t*));
  }
}
#endif

select_ready *select_set_ready(select_group_t *group, int s, select_ready *callback)
{
  select_t *select = find_select_by_socket(group, s);
  select_ready *old = NULL;

  if(select)
  {
    old = select->ready_callback;
    select->ready_callback = callback;
  }
  return old;
}

select_recv *select_set_recv(select_group_t *group, int s, select_recv *callback)
{
  select_t *select = find_select_by_socket(group, s);
  select_recv *old = NULL;

  if(select)
  {
    old = select->recv_callback;
    select->recv_callback = callback;
  }
  return old;
}

select_listen *select_set_listen(select_group_t *group, int s, select_listen *callback)
{
  select_t *select = find_select_by_socket(group, s);
  select_listen *old = NULL;

  if(select)
  {
    old = select->listen_callback;
    select->listen_callback = callback;
  }
  return old;
}

select_error *select_set_error(select_group_t *group, int s, select_error *callback)
{
  select_t *select = find_select_by_socket(group, s);
  select_error *old = NULL;

  if(select)
  {
    old = select->error_callback;
    select->error_callback = callback;
  }
  return old;
}

select_closed *select_set_closed(select_group_t *group, int s, select_closed *callback)
{
  select_t *select = find_select_by_socket(group, s);
  select_closed *old = NULL;

  if(select)
  {
    old = select->closed_callback;
    select->closed_callback = callback;
  }
  return old;
}

select_timeout *select_set_timeout(select_group_t *group, select_timeout *callback, void *param)
{
  select_timeout *old;
  old = group->timeout_callback;
  group->timeout_callback = callback;
  group->timeout_param = param;

  return old;
}

NBBOOL select_group_remove_socket(select_group_t *group, int s)
{
  select_t *socket = find_select_by_socket(group, s);

  if(socket)
    socket->active = FALSE;

  return (socket ? TRUE : FALSE);
}

NBBOOL select_group_remove_and_close_socket(select_group_t *group, int s)
{
  tcp_close(s);
  return select_group_remove_socket(group, s);
}

static SELECT_RESPONSE_t select_handle_response(select_group_t *group, int s, SELECT_RESPONSE_t response)
{
  if(response == SELECT_OK)
  {
    /* printf("A-ok\n"); */
  }
  else if(response == SELECT_REMOVE)
  {
    select_group_remove_socket(group, s);
  }
  else if(response == SELECT_CLOSE_REMOVE)
  {
    select_group_remove_and_close_socket(group, s);
  }
  else
  {
    DIE("Unknown SELECT result was returned by a callback.");
  }

  return response;
}

static void handle_incoming_data(select_group_t *group, size_t i)
{
  int s = SG_SOCKET(group, i);
  uint8_t buffer[MAX_RECV];

  /* SG_WAITING is set when we're buffering data. Doesn't work with Windows pipes. */
  if(SG_WAITING(group, i))
  {
    /* Figure out how many bytes we're waiting on. */
    size_t require = SG_WAITING(group, i) - SG_BUFFERED(group, i);

    /* Read no more than what we need. */
    int size = recv(s, buffer, require, 0);

    if(SG_TYPE(group, i) == SOCKET_TYPE_DATAGRAM)
      DIE("Tried to treat a DATAGRAM socket like a stream.");

    /* Check for error */
    if(size < 0)
    {
      if(SG_ERROR(group, i))
        select_handle_response(group, s, SG_ERROR(group, i)(group, s, getlastsocketerror(SG_SOCKET(group, i)), SG_PARAM(group, i)));
      else
        select_group_remove_and_close_socket(group, s);
    }
    else if(size == 0)
    {
      if(SG_CLOSED(group, i))
        select_handle_response(group, s, SG_CLOSED(group, i)(group, s, SG_PARAM(group, i)));
      else
        select_group_remove_and_close_socket(group, s);
    }
    else
    {
      /* Copy the bytes just read into the buffer */
      memcpy(SG_BUFFER(group, i) + SG_BUFFERED(group, i), buffer, size);

      /* Increment the counter. */
      SG_BUFFERED(group, i) = SG_BUFFERED(group, i) + size;

      /* If we're finished buffering data, call the callback function and clear the buffer. */
      if(SG_BUFFERED(group, i) > SG_WAITING(group, i))
        DIE("Something caused data corruption (overflow?)");

      if(SG_BUFFERED(group, i) == SG_WAITING(group, i))
      {
        select_handle_response(group, s, SG_RECV(group, i)(group, s, SG_BUFFER(group, i), SG_BUFFERED(group, i), NULL, -1, SG_PARAM(group, i)));
        memset(SG_BUFFER(group, i), 0, SG_BUFFERED(group, i));
        SG_BUFFERED(group, i) = 0;
      }
    }
  }
  else
  {
#ifdef WIN32
    if(SG_TYPE(group, i) == SOCKET_TYPE_STREAM || SG_TYPE(group, i) == SOCKET_TYPE_PIPE)
#else
    if(SG_TYPE(group, i) == SOCKET_TYPE_STREAM)
#endif
    {
      ssize_t size;
      NBBOOL success = TRUE;

#ifdef WIN32
      /* If it's a stream, use tcp_recv; if it's a pipe, use ReadFile. */
      if(SG_TYPE(group, i) == SOCKET_TYPE_STREAM)
      {
        size = tcp_recv(s, buffer, MAX_RECV);
      }
      else if(SG_TYPE(group, i) == SOCKET_TYPE_PIPE)
      {
        success = ReadFile(SG_PIPE(group, i), buffer, MAX_RECV, &size, NULL);
      }
#else
      size = read(s, buffer, MAX_RECV); /* read is better than recv, because it can handle stdin */
#endif

/*fprintf(stderr, "Read %d bytes from socket %d\n", size, s); */

      /* Handle error conditions. */
      if(size < 0 || !success)
      {
        if(SG_ERROR(group, i))
          select_handle_response(group, s, SG_ERROR(group, i)(group, s, getlastsocketerror(SG_SOCKET(group, i)), SG_PARAM(group, i)));
        else
          select_group_remove_and_close_socket(group, s);
      }
      else if(size == 0)
      {
/* fprintf(stderr, "Closed!\n"); */
        if(SG_CLOSED(group, i))
          select_handle_response(group, s, SG_CLOSED(group, i)(group, s, SG_PARAM(group, i)));
        else
          select_group_remove_and_close_socket(group, s);
      }
      else
      {
        /* Send the recv()'d data to the callback, handling the response appropriately. */
        if(SG_RECV(group, i))
          select_handle_response(group, s, SG_RECV(group, i)(group, s, buffer, size, NULL, -1, SG_PARAM(group, i)));
      }
    }
    else
    {
      /* It's a datagram socket, so use recvfrom. */
      struct sockaddr_in addr;
      socklen_t addr_size = sizeof(struct sockaddr_in);
      ssize_t size;

      memset(&addr, 0, sizeof(struct sockaddr_in));
      size = recvfrom(s, buffer, MAX_RECV, 0, (struct sockaddr *)&addr, &addr_size);

      /* Handle error conditions. */
      if(size < 0 || size == (size_t)-1)
      {
        if(SG_ERROR(group, i))
          select_handle_response(group, s, SG_ERROR(group, i)(group, s, getlastsocketerror(SG_SOCKET(group, i)), SG_PARAM(group, i)));
        else
          select_group_remove_and_close_socket(group, s);
      }
      else if(size == 0)
      {
        if(SG_CLOSED(group, i))
          select_handle_response(group, s, SG_CLOSED(group, i)(group, s, SG_PARAM(group, i)));
        else
          select_group_remove_and_close_socket(group, s);
      }
      else
      {
        /* Send the recv()'d data to the callback, handling the response appropriately. */
        if(SG_RECV(group, i))
          select_handle_response(group, s, SG_RECV(group, i)(group, s, buffer, size, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), SG_PARAM(group, i)));
      }
    }
  }
}

static void handle_incoming_connection(select_group_t *group, size_t i)
{
  int s = SG_SOCKET(group, i);

  if(SG_LISTEN(group, i))
    select_handle_response(group, s, SG_LISTEN(group, i)(group, s, SG_PARAM(group, i)));
}

void select_group_do_select(select_group_t *group, int timeout_ms)
{
  fd_set read_set;
  fd_set write_set;
  fd_set error_set;
  int select_return;
  size_t i;
  struct timeval select_timeout;

#ifdef WIN32
  size_t count = 0;
#endif

  /* Always time out after an interval (like Ncat does) -- this lets us poll for non-Internet sockets on Windows. */
#ifdef WIN32
  select_timeout.tv_sec = 0;
  select_timeout.tv_usec = TIMEOUT_INTERVAL * 1000;
#else
  select_timeout.tv_sec = timeout_ms / 1000;
  select_timeout.tv_usec = (timeout_ms % 1000) * 1000;
#endif

  /* Clear the current socket set */
  FD_ZERO(&read_set);
  FD_ZERO(&write_set);
  FD_ZERO(&error_set);

  /* Crawl over the list, adding the sockets. */
  for(i = 0; i < group->current_size; i++)
  {
#ifdef WIN32
    /* On Windows, don't add pipes. */
    if(SG_IS_ACTIVE(group, i) && SG_TYPE(group, i) != SOCKET_TYPE_PIPE)
    {
      if(SG_IS_READY(group, i))
        FD_SET(SG_SOCKET(group, i), &read_set);
      else
        FD_SET(SG_SOCKET(group, i), &write_set);

      FD_SET(SG_SOCKET(group, i), &error_set);

      /* Count is only used to check if there are any sockets in the set; if
       * there aren't, then sleep() is used instead of select(). */
      count++;
    }
#else
    if(SG_IS_ACTIVE(group, i))
    {
      if(SG_IS_READY(group, i))
        FD_SET(SG_SOCKET(group, i), &read_set);
      else
        FD_SET(SG_SOCKET(group, i), &write_set);

      FD_SET(SG_SOCKET(group, i), &error_set);
    }
#endif
  }

#ifdef WIN32
  /* If no sockets are added, then use the Sleep() function here. */
  if(count == 0)
    Sleep(TIMEOUT_INTERVAL);
  else
    select_return = select(group->biggest_socket + 1, &read_set, &write_set, &error_set, &select_timeout);
#else
  select_return = select(group->biggest_socket + 1, &read_set, &write_set, &error_set, timeout_ms < 0 ? NULL : &select_timeout);
#endif
/*  fprintf(stderr, "Select returned %d\n", select_return); */

  if(select_return == -1)
    nbdie("select_group: couldn't select()");

#ifdef WIN32
  /* Handle pipes on every run, whether it's a timeout or data arrived. */
  for(i = 0; i < group->current_size; i++)
  {
    if(SG_IS_ACTIVE(group, i) && SG_TYPE(group, i) == SOCKET_TYPE_PIPE)
    {
      /* Check if the handle is ready. */
      DWORD n;

      /* See if there's any data in the pipe. */
      BOOL result = PeekNamedPipe(SG_PIPE(group, i), NULL, 0, NULL, &n, NULL);

      /* If there is, call the incoming_data function. */
      if(result)
      {
        if(n > 0)
          handle_incoming_data(group, i);
      }
      else
      {
        int s = SG_SOCKET(group, i);

        if(GetLastError() == ERROR_BROKEN_PIPE) /* Pipe closed */
        {
          if(SG_CLOSED(group, i))
            select_handle_response(group, s, SG_CLOSED(group, i)(group, s, SG_PARAM(group, i)));
          else
            select_group_remove_and_close_socket(group, s);
        }
        else
        {
          if(SG_ERROR(group, i))
            select_handle_response(group, s, SG_ERROR(group, i)(group, s, getlastsocketerror(SG_SOCKET(group, i)), SG_PARAM(group, i)));
          else
            select_group_remove_and_close_socket(group, s);
        }
      }
    }
  }
#endif

  /* select_return is 0 when there's a timeout -- but because there's a timeout
   * callback, we have to check if we crossed it. */
  if(select_return == 0)
  {
    if(timeout_ms >= 0)
    {
#ifdef WIN32
      /* On Windows, check if we've overflowed our elapsed time. */
      if((group->elapsed_time / timeout_ms) != ((group->elapsed_time + TIMEOUT_INTERVAL) / timeout_ms))
      {
        /* Timeout elapsed with no events, inform the callbacks. */
        if(group->timeout_callback)
          group->timeout_callback(group, group->timeout_param);
      }

      /* Increment the elapsed time. We don't really care if this overflows. */
      group->elapsed_time = (group->elapsed_time + TIMEOUT_INTERVAL);
#else
      /* Timeout elapsed with no events, inform the callbacks. */
    if(group->timeout_callback)
      group->timeout_callback(group, group->timeout_param);
#endif
    }
  }
  else
  {
    /* Loop through the sockets to find the one that had activity. */
    for(i = 0; i < group->current_size; i++)
    {
      /* If the socket is active and it has data waiting to be read, process it. */
      if(SG_IS_ACTIVE(group, i) && FD_ISSET(SG_SOCKET(group, i), &read_set))
      {
        if(SG_TYPE(group, i) == SOCKET_TYPE_LISTEN)
        {
          handle_incoming_connection(group, i);
        }
        else
        {
          handle_incoming_data(group, i);
        }
      }

      /* If the socket became writable, update as appropriate. */
      if(SG_IS_ACTIVE(group, i) && FD_ISSET(SG_SOCKET(group, i), &write_set))
      {
        /* Call the connect callback. */
        if(SG_READY(group, i))
          select_handle_response(group, SG_SOCKET(group, i), SG_READY(group, i)(group, SG_SOCKET(group, i), SG_PARAM(group, i)));

        /* Mark the socket as ready. */
        SG_IS_READY(group, i) = TRUE;
      }

      /* If there's an error, handle it. */
      if(SG_IS_ACTIVE(group, i) && FD_ISSET(SG_SOCKET(group, i), &error_set))
      {
        /* If there's no handler defined, default to closing and removing the
         * socket. */
        if(SG_ERROR(group, i))
          select_handle_response(group, SG_SOCKET(group, i), SG_ERROR(group, i)(group, SG_SOCKET(group, i), getlastsocketerror(SG_SOCKET(group, i)), SG_PARAM(group, i)));
        else
          select_handle_response(group, SG_SOCKET(group, i), SELECT_CLOSE_REMOVE);
      }
    }
  }
}

NBBOOL select_group_wait_for_bytes(select_group_t *group, int s, size_t bytes)
{
  select_t *socket = find_select_by_socket(group, s);

  if(bytes > MAX_RECV)
    DIE("Tried to wait for too many bytes at once.");

  if(socket)
  {
    if(socket->type != SOCKET_TYPE_STREAM)
      DIE("Tried to buffer bytes on the wrong type of socket");

    /* Already waiting, free bytes. */
    if(socket->waiting_for)
      safe_free(socket->buffer);

    socket->buffer = safe_malloc(sizeof(uint8_t) * bytes);
    socket->waiting_for = bytes;
    socket->buffered = 0;
  }

  return (socket ? TRUE : FALSE);
}


size_t select_group_get_active_count(select_group_t *group)
{
  size_t i;
  size_t count = 0;

  for(i = 0; i < group->current_size; i++)
  {
    if(SG_IS_ACTIVE(group, i))
      count++;
  }

  return count;
}

#ifdef WIN32
typedef struct
{
  HANDLE stdin_read; /* Probably don't need this, but whatever. */
  HANDLE stdin_write;
} stdin_thread_param;

static DWORD WINAPI stdin_thread(void *param)
{
  char buffer[1024];
  DWORD bytes_read;
  DWORD bytes_written;
  HANDLE stdin_write = ((stdin_thread_param*)param)->stdin_write;

  /* Don't need the param anymore. */
  safe_free(param);

  while(1)
  {
    int i = 0;

    do
    {
      if(!ReadFile(GetStdHandle(STD_INPUT_HANDLE), buffer, 1024, &bytes_read, NULL))
      {
        fprintf(stderr, "No more data from stdin\n");
        CloseHandle(stdin_write);
        return 0;
      }

      if(!WriteFile(stdin_write, buffer, bytes_read, &bytes_written, NULL))
        nbdie("stdin: Couldn't write to stdin pipe");

    } while(1);
  }

  return 0;
}

HANDLE get_stdin_handle()
{
  HANDLE              stdin_read;
  HANDLE              stdin_write;
  HANDLE              new_thread;
  stdin_thread_param *param = (stdin_thread_param*) safe_malloc(sizeof(stdin_thread_param));
  static HANDLE       handle = NULL;

  /* Check if we already have the handle open. */
  if(handle)
    return handle;

  CreatePipe(&stdin_read, &stdin_write, NULL, 0);
  param->stdin_read  = stdin_read;
  param->stdin_write = stdin_write;

  /* Create the new stdin thread. */
  new_thread = CreateThread(NULL, 0, stdin_thread, param, 0, NULL);

  if(!new_thread)
    nbdie("stdin: Couldn't create thread");

  /* This will let us reference this file later, if this function is called again. */
  handle = stdin_read;

  /* Return the fake stdin. */
  return stdin_read;
}
#endif

#if 0
#include <stdio.h>
/*#define _POSIX_C_SOURCE*/
#include "udp.h"
#include "tcp.h"
SELECT_RESPONSE_t test_timeout(void *group, int s, void *param)
{
  printf("Timeout called for socket %d\n", s);

  return SELECT_OK;
}

SELECT_RESPONSE_t test_recv(void *group, int s, uint8_t *data, size_t length, char *addr, uint16_t port, void *param)
{
  int i;

  if(addr)
    printf("Socket %d received %d bytes from %s:%d\n", s, length, addr, port);

  if(data[0] == 'q')
    return SELECT_REMOVE;
  else if(data[0] == 'x')
    return SELECT_CLOSE_REMOVE;
  else if(data[0] == 'c')
    close(s);

  for(i = 0; i < length; i++)
  {
    printf("%c", data[i] < 0x20 ? '.' : data[i]);
  }
  printf("\n");

  return SELECT_OK;
}

SELECT_RESPONSE_t test_error(void *group, int s, int err, void *param)
{
  fprintf(stderr, "Error in socket %d: %s\n", s, strerror(err));
  return SELECT_CLOSE_REMOVE;
}

SELECT_RESPONSE_t test_closed(void *group, int s, void *param)
{
  printf("Socket %d: connection closed.\n", s);
  return SELECT_CLOSE_REMOVE;
}

SELECT_RESPONSE_t test_listen(void *group, int s, void *param)
{
  char *address;
  uint16_t port;
  int new = tcp_accept(s, &address, &port);

  printf("Accepting connection from %s:%d\n", address, port);

  select_group_add_socket(group, new, SOCKET_TYPE_STREAM, NULL);
  select_set_recv(group, new, test_recv);
  select_set_error(group, new, test_error);
/*  select_set_closed(group, new, test_closed); */

  return SELECT_OK;
}

int main(int argc, char **argv)
{
  select_group_t *group;

  int udp = udp_create_socket(2222, "0.0.0.0");
  int tcp = tcp_connect("www.google.ca", 80);
  int listen = tcp_listen("0.0.0.0", 4444);

  char *test = "GET / HTTP/1.0\r\nHost: www.google.com\r\n\r\n";

  printf("Listening on udp/2222\n");
  printf("Connecting to Google on tcp/80\n");
  printf("Listening on tcp/4444\n\n");

  /* Check if everything was created right. */
  if(udp < 0)
  {
    printf("UDP socket failed to create!\n");
    exit(1);
  }
  if(tcp < 0)
  {
    printf("TCP socket failed to create!\n");
    exit(1);
  }
  if(listen < 0)
  {
    printf("Listen socket failed to create!\n");
    exit(1);
  }

  tcp_send(tcp, test, strlen(test));

  group = select_group_create();
  select_group_add_socket(group, udp,           SOCKET_TYPE_DATAGRAM, NULL);
  select_group_add_socket(group, tcp,           SOCKET_TYPE_STREAM, NULL);
  select_group_add_socket(group, listen,        SOCKET_TYPE_LISTEN, NULL);
  select_group_add_socket(group, STDIN_FILENO,  SOCKET_TYPE_STREAM, NULL);

  select_set_recv(group, udp,           test_recv);
  select_set_recv(group, tcp,           test_recv);
  select_set_recv(group, STDIN_FILENO,  test_recv);

  select_set_listen(group, listen, test_listen);

  select_set_error(group, udp, test_error);
  select_set_error(group, tcp, test_error);
  select_set_error(group, listen, test_error);
  select_set_error(group, STDIN_FILENO,  test_error);

  select_set_closed(group, udp, test_closed);
  select_set_closed(group, tcp, test_closed);
  select_set_closed(group, listen, test_closed);
  select_set_closed(group, STDIN_FILENO,  test_closed);

  select_group_wait_for_bytes(group, tcp, 25);

  while(1)
  {
    select_group_do_select(group, -1, -1);
  }

  select_group_destroy(group);

  return 0;
}
#endif
