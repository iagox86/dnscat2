/* udp.h
 * By Ron
 * Created August, 2008
 *
 * (See LICENSE.md)
 *
 * Platform-independent module for creating/sending IPv4 UDP packets.
 */

#ifndef __UDP_H__
#define __UDP_H__

/*#ifdef WIN32
#include <winsock2.h>
#else
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#endif*/

#include "types.h"

/* Must be called before any other functions. This is actually defined in tcp.c. */
void winsock_initialize();

/* Create a UDP socket on the given port. */
int udp_create_socket(uint16_t port, char *local_address);

/* Read from the new socket, filling in the 'from' field if given. Not currently being used. */
/*ssize_t udp_read(int s, void *buffer, size_t buffer_length, struct sockaddr_in *from);*/

/* Send data to the given address on the given port. */
ssize_t udp_send(int sock, char *address, uint16_t port, void *data, size_t length);

/* Close the UDP socket. */
int    udp_close(int s);

#endif
