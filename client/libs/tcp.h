/* tcp.h
 * By Ron
 * Created August, 2008
 *
 * (See LICENSE.md)
 *
 * Platform-independent module for creating/sending TCP sockets for IPv4 TCP
 * connections.
 */

#ifndef __TCP_H__
#define __TCP_H__

#include "types.h"

/* Must be called before any other functions. */
void winsock_initialize();

/* Connect to the remote server on the given port. Prints an error to the screen and
 * returns -1 if it fails; otherwise, returns the new socket. */
int    tcp_connect(char *host, uint16_t port);

/* The same as tcp_connect, except it lets the user choose a non-blocking
 * socket. */
int tcp_connect_options(char *host, uint16_t port, int non_blocking);

/* Set a socket as non-blocking. */
void   tcp_set_nonblocking(int s);

/* Puts a socket into listening mode on the given address (use '0.0.0.0' for any).
 * Returns -1 on an error, or the socket if successful. */
int    tcp_listen(char *address, uint16_t port);

/* Accepts a connection on a listening socket. Returns the new socket if successful
 * or -1 if fails. */
int    tcp_accept(int listen, char **address, uint16_t *port);

/* Send data over the socket. Can use built-in IO functions, too. */
ssize_t tcp_send(int s, void *data, size_t length);

/* Receive data from the socket. Can use built-in IO functions, too. */
ssize_t tcp_recv(int s, void *buffer, size_t buffer_length);

/* Close the socket. */
int    tcp_close(int s);

#endif
