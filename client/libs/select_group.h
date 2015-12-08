/* select_group.h
 * By Ron Bowes
 * Created August, 2008
 *
 * (See LICENSE.md)
 *
 * This module implements a simple interface to the select() function that
 * works across Windows, Linux, BSD, and Mac. Any (reasonable) number of
 * sockets can be added and when any one of them has data, callbacks are used
 * to notify the main program.
 *
 * This library is single-threaded (except on Windows.. I'll get to that). I've
 * compiled and tested it on Linux, FreeBSD, Mac, and Windows, and it works
 * beautifully on all of them. On any of those platforms it can select just
 * fine on stream sockets, datagram sockets, listeners, and pipes (including
 * stdin on Windows).
 *
 * Windows support for pipes is a special case. Because Windows can't select()
 * on a pipe or HANDLE, I had to implement some special code. Basically, it
 * polls -- instead of adding pipes to the select(), it times out the select
 * after a set amount of time (right now, it's 100ms). That means that every
 * 100ms, select() returns and checks if any input is waiting on the pipes. Not
 * the greatest solution, but it isn't the greatest OS for networking stuff.
 *
 * Even worse, stdin is a special case. stdin can be read through a pipe, so
 * the polling code works great -- except that Windows won't echo types
 * characters unless stdin is being actively read. Rather than introducing a
 * 100ms-delay to everything types, that would probably make me even more
 * crazy, I created the Windows-specific function get_stdin_handle(). The first
 * time it's called, it creates a thread that reads from stdin and writes to a
 * pipe. That pipe can be added to select() and everything else works the same.
 * It's an ugly hack, I know, but when writing Ncat (http://nmap.org/ncat)
 * David Fifield came up with the same solution. Apparently, it's the best
 * we've got.
 */


#ifndef __SELECT_GROUP_H__
#define __SELECT_GROUP_H__

/* Updates for dnscat2 */
#define SELECT_GROUP_VERSION "1.01"

#include <stdlib.h>

#ifdef WIN32
#include <winsock2.h>
#else
#endif

#include "types.h"

/* The maximum number of possible sockets (huge number, but I want to prevent overflows). Note that this is
 * sort of a range, because the number of sockets are doubled each time. So it's between 32768 and 65536. */
#define SOCKET_LIST_MAX_SOCKETS (65536/2)

/* The time, in milliseconds, between select() timing out and polling for pipe data (on Windows). */
#ifdef WIN32
#define TIMEOUT_INTERVAL 100
#endif

/* Different types of sockets, which will affect different aspects. */
typedef enum
{
  /* No special treatment (anything can technically use this one).  */
  SOCKET_TYPE_STREAM,

  /* Uses recvfrom() and passes along the socket address). */
  SOCKET_TYPE_DATAGRAM,

  /* Listening implies a stream. */
  SOCKET_TYPE_LISTEN,
#ifdef WIN32
  /* For use on Windows, only, is handled separately. */
  SOCKET_TYPE_PIPE
#endif
} SOCKET_TYPE_t;

/* Possible return values from callback functions. */
typedef enum
{
  SELECT_OK,           /* Everything went well. */
  SELECT_REMOVE,       /* Remove the socket from the list. */
  SELECT_CLOSE_REMOVE, /* Close the socket and remove it from the list. */
} SELECT_RESPONSE_t;

/* Define callback function types. I have to make the first parameter 'void*' because the struct hasn't been defined
 * yet, and the struct requires these typedefs to be in place. */
typedef SELECT_RESPONSE_t(select_ready)(void *group, int s, void *param);
/* 'addr' will only be filled in for datagram requests. */
typedef SELECT_RESPONSE_t(select_recv)(void *group, int s, uint8_t *data, size_t length, char *addr, uint16_t port, void *param);
typedef SELECT_RESPONSE_t(select_listen)(void *group, int s, void *param);
typedef SELECT_RESPONSE_t(select_error)(void *group, int s, int err, void *param);
typedef SELECT_RESPONSE_t(select_closed)(void *group, int s, void *param);
typedef SELECT_RESPONSE_t(select_timeout)(void *group, void *param);

/* This struct is for internal use. */
typedef struct
{
  /* The socket. */
  int             s;
#ifdef WIN32
  /* A pipe (used for Windows' named pipes. */
  HANDLE          pipe;
#endif
  /* Datagram, stream, etc. */
  SOCKET_TYPE_t   type;

  /* The function to call when the socket is ready for data. */
  select_ready   *ready_callback;

  /* The function to call when data arrives. */
  select_recv    *recv_callback;

  /* The function to call when a connection arrives. */
  select_listen  *listen_callback;

  /* The function to call when there's an error. */
  select_error   *error_callback;

  /* The function to call when the connection is closed. */
  select_closed  *closed_callback;

  /* The number of bytes being waited on. If set to 0, will trigger on all
   * incoming data. */
  size_t          waiting_for;

  /* The buffer that holds the current bytes. */
  uint8_t        *buffer;

  /* The number of bytes currently stored in the buffer. */
  size_t          buffered;

  /* This is set after the socket has received the signal that it's ready to
   * receive data. */
  NBBOOL         ready;

  /* Set to 'false' when the socket is 'deleted'. It's easier than physically
   * removing it from the list, so until I implement something heavy weight
   * this will work. */
  NBBOOL         active;

  /* Stores a piece of arbitrary data that's sent to the callbacks. */
  void           *param;
} select_t;

/* This is the primary struct for this module. */
typedef struct
{
  /* A list of the select_t objects. */
  select_t **select_list;

  /* The current number of "select_t"s in the list. */
  size_t current_size;

  /* The maximum number of "select_t"s in the list before realloc() has to expand it. */
  size_t maximum_size;
#ifdef WIN32
  /* The number of milliseconds that have elapsed; used for timeouts. */
  uint32_t elapsed_time;
#endif

  /* The handle to the highest-numbered socket in the list (required for select() call). */
  int biggest_socket;

  /* The function to call when the timeout time expires. */
  select_timeout *timeout_callback;

  /* A parameter that is passed to the callback function. */
  void *timeout_param;
} select_group_t;

/* Allocate memory for a select group */
select_group_t *select_group_create();

/* Destroy and cleanup the group. */
void select_group_destroy(select_group_t *group);

/* Add a socket to the group. */
void select_group_add_socket(select_group_t *group, int s, SOCKET_TYPE_t type, void *param);

#ifdef WIN32
/* Add a pipe to the group. The 'identifier' is treated as a socket and is used in place of a socket
 * to look up the pipe. */
void select_group_add_pipe(select_group_t *group, int identifier, HANDLE pipe, void *param);
#endif

/* Set a callback that's called when the socket becomes ready to send data. */
select_ready   *select_set_ready(select_group_t *group, int s, select_ready *callback);

/* Set the recv() callback. This will return with as much data as comes in, or with the number of bytes set by
 * set_group_wait_for_bytes(), if that's set. Returns the old callback, if set. */
select_recv    *select_set_recv(select_group_t *group, int s, select_recv *callback);

/* Set the listen() callback for incoming connections. It's up to the callback to perform the accept() to
 * get the new socket. */
select_listen  *select_set_listen(select_group_t *group, int s, select_listen *callback);

/* Set the error callback, for socket errors. If SELECT_OK is returned, it assumes the error's been handled
 * and will continue to select() on the socket. In almost every case, SELECT_OK is the wrong thing to return.
 * If this isn't defined, the socket is automatically closed/removed. */
select_error   *select_set_error(select_group_t *group, int s, select_error *callback);

/* Set the closed callback. This is called when the connection is gracefully terminated. Like with errors,
 * SELECT_OK is probably not what you want. If this isn't handled, the socket is automatically removed from
 * the list. */
select_closed  *select_set_closed(select_group_t *group, int s, select_closed *callback);

/* Set the timeout callback, for when the time specified in select_group_do_select() elapses. */
select_timeout *select_set_timeout(select_group_t *group, select_timeout *callback, void *param);

/* Remove a socket from the group. Returns non-zero if successful. */
NBBOOL select_group_remove_socket(select_group_t *group, int s);

/* Remove a socket from the group, and close it. */
NBBOOL select_group_remove_and_close_socket(select_group_t *group, int s);

/* Perform the select() call across the various sockets. with the given timeout in milliseconds.
 * Note that the timeout (and therefore the timeout callback) only fires if _every_ socket is idle.
 * If timeout_ms < 0, it will block indefinitely (till data arrives on any socket). Because of polling,
 * on Windows, timeout_ms actually has a resolution defined by TIMEOUT_INTERVAL. */
void select_group_do_select(select_group_t *group, int timeout_ms);

/* Wait for the given number of bytes to arrive on the socket, rather than any number of bytes. This doesn't
 * work for datagram sockets.
 * Note: any data already queued up will be whacked. */
NBBOOL select_group_wait_for_bytes(select_group_t *group, int s, size_t bytes);

/* Check how many active sockets are left. */
size_t select_group_get_active_count(select_group_t *group);

#ifdef WIN32
/* Get a handle to stdin. This handle can be added to a select_group as a pipe. Behind the scenes,
 * it uses a thread. Don't ask. */
HANDLE get_stdin_handle();
#endif

#endif
