/* udp.c
 * By Ron
 * Created August, 2008
 *
 * (See LICENSE.md)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#include <winsock2.h>
#else
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif

#include "udp.h"

int udp_create_socket(uint16_t port, char *local_address)
{
  int    s;
  int    value = 1;
  struct sockaddr_in sox;

  s = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

  if(s < 0)
    nbdie("udp: couldn't create socket");

  if(setsockopt(s, SOL_SOCKET, SO_BROADCAST, (void*)&value, sizeof(int)) < 0)
    nbdie("udp: couldn't set socket to SO_BROADCAST");

  sox.sin_addr.s_addr = inet_addr(local_address);
  sox.sin_family      = AF_INET;
  sox.sin_port        = htons(port);

    if(sox.sin_addr.s_addr == INADDR_NONE)
    nbdie("udp: couldn't parse local address");

  if(bind(s, (struct sockaddr *)&sox, sizeof(struct sockaddr_in)) < 0)
    nbdie("udp: couldn't bind to port (are you running as root?)");

  return s;
}

ssize_t udp_read(int s, void *buffer, size_t buffer_length, struct sockaddr_in *from)
{
  ssize_t received;
  socklen_t fromlen = sizeof(struct sockaddr_in);

  memset(from, 0, sizeof(struct sockaddr));

  received = recvfrom(s, buffer, buffer_length, 0, (struct sockaddr *)from, &fromlen);

  if( received < 0 )
    nbdie("udp: couldn't receive data");

  return received;
}

ssize_t udp_send(int sock, char *address, uint16_t port, void *data, size_t length)
{
  int    result = -1;
  struct sockaddr_in serv_addr;
  struct hostent *server;

  /* Look up the host */
  server = gethostbyname(address);
  if(!server)
  {
    fprintf(stderr, "Couldn't find host %s\n", address);
  }
  else
  {
    /* Set up the server address */
    memset(&serv_addr, '\0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port   = htons(port);
    memcpy(&serv_addr.sin_addr, server->h_addr_list[0], server->h_length);

    result = sendto(sock, data, length, 0, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr_in));

    if( result < 0 )
      nbdie("udp: couldn't send data");
  }

  return result;
}

int udp_close(int s)
{
#ifdef WIN32
  return closesocket(s);
#else
  return close(s);
#endif
}
