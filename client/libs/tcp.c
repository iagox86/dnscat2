/* tcp.c
 * By Ron
 * Created August, 2008
 *
 * (See LICENSE.md)
 */

#include <stdio.h>
#include <string.h>

#ifdef WIN32
#include <winsock2.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif

#include "tcp.h"

void winsock_initialize()
{
#ifdef WIN32
  WORD wVersionRequested = MAKEWORD(2, 2);
  WSADATA wsaData;

  int error = WSAStartup(wVersionRequested, &wsaData);

  switch(error)
  {
  case WSASYSNOTREADY:
    fprintf(stderr, "The underlying network subsystem is not ready for network communication.\n");
    exit(1);
    break;

  case WSAVERNOTSUPPORTED:
    fprintf(stderr, "The version of Windows Sockets support requested is not provided by this particular Windows Sockets implementation.\n");
    exit(1);
    break;

  case WSAEINPROGRESS:
    fprintf(stderr, "A blocking Windows Sockets 1.1 operation is in progress.\n");
    exit(1);
    break;

  case WSAEPROCLIM:
    fprintf(stderr, "A limit on the number of tasks supported by the Windows Sockets implementation has been reached.\n");
    exit(1);
    break;

  case WSAEFAULT:
    fprintf(stderr, "The lpWSAData parameter is not a valid pointer.\n");
    exit(1);
    break;
  }
#endif
}

int tcp_connect_options(char *host, uint16_t port, int non_blocking)
{
  struct sockaddr_in serv_addr;
  struct hostent *server;
  int s;
  int status;

  /* Create the socket */
  s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

  if (s == -1)
    nbdie("tcp: couldn't create socket");

  if(non_blocking)
  {
#ifdef WIN32
    unsigned long mode = 1;
    ioctlsocket(s, FIONBIO, &mode);
#else
    int flags = fcntl(s, F_GETFL, 0);
    flags = flags | O_NONBLOCK;
    fcntl(s, F_SETFL, flags);
#endif
  }

  /* Look up the host */
  server = gethostbyname(host);
  if(!server)
  {
    fprintf(stderr, "Couldn't find host %s\n", host);
    return -1;
  }

  /* Set up the server address */
  memset(&serv_addr, '\0', sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port   = htons(port);
  memcpy(&serv_addr.sin_addr, server->h_addr_list[0], server->h_length);

  /* Connect */
  status = connect(s, (struct sockaddr*)&serv_addr, sizeof(serv_addr));

#ifdef WIN32
  if(status < 0 && GetLastError() != WSAEWOULDBLOCK)
#else
  if(status < 0 && errno != EINPROGRESS)
#endif
  {
    nberror("tcp: couldn't connect to host");

    return -1;
  }

  return s;
}

int tcp_connect(char *host, uint16_t port)
{
  return tcp_connect_options(host, port, 0);
}

void tcp_set_nonblocking(int s)
{
#ifdef WIN32
  /* TODO: This */
  fprintf(stderr, "Don't know how to do nonblocking on Windows\n");
  exit(1);
#else
  fcntl(s, F_SETFL, O_NONBLOCK);
#endif
}

int tcp_listen(char *address, uint16_t port)
{
  int s;
  struct sockaddr_in serv_addr;

  /* Get the server address */
  memset((char *) &serv_addr, '\0', sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(address);
  serv_addr.sin_port = htons(port);

  if(serv_addr.sin_addr.s_addr == INADDR_NONE)
    nbdie("tcp: couldn't parse local address");

  /* Create a socket */
  s = socket(AF_INET, SOCK_STREAM, 0);
  if(s < 0)
  {
    nbdie("tcp: couldn't create socket");
  }
  else
  {
    /* Bind the socket */
    if (bind(s, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
      nbdie("tcp: couldn't bind to socket");

    /* Switch the socket to listen mode. TODO: why 20? */
    if(listen(s, 20) < 0)
      nbdie("tcp: couldn't listen on socket");
  }

  return s;
}

int tcp_accept(int listen, char **address, uint16_t *port)
{
  struct sockaddr_in addr;
  socklen_t sockaddr_len = sizeof(struct sockaddr_in);
  int s;

  s = accept(listen, (struct sockaddr *) &addr, &sockaddr_len);

  if(s < 0)
    nbdie("tcp: couldn't accept connection");

  *address = inet_ntoa(addr.sin_addr);
  *port    = ntohs(addr.sin_port);

  return s;
}

ssize_t tcp_send(int s, void *data, size_t length)
{
  return send(s, data, length, 0);
}

ssize_t tcp_recv(int s, void *buffer, size_t buffer_length)
{
  return recv(s, buffer, buffer_length, 0);
}

int tcp_close(int s)
{
#ifdef WIN32
  return closesocket(s);
#else
  return close(s);
#endif
}

#if 0
int main(int argc, char *argv[])
{
  char buffer[1024];
  int s;
  int listener;
  struct sockaddr_in addr;
  int port;
  size_t len;

  memset(buffer, 0, 1024);

  winsock_initialize();

  s = tcp_connect("www.google.ca", 80);
  if(s < 0)
    DIE("Fail");
  tcp_send(s, "GET / HTTP/1.0\r\nHost: www.google.com\r\n\r\n", 41);
  tcp_recv(s, buffer, 1024);
  tcp_close(s);

  printf("%s\n", buffer);

  printf("Listening on TCP/6666\n");
  listener = tcp_listen("0.0.0.0", 6666);
  s = tcp_accept(listener, &addr, &port);

  if(s < 0)
    DIE("Fail");

  printf("Connection accepted from %s:%d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

  memset(buffer, 0, 1024);
  strcpy(buffer, "HELLO!");
  len = 7;
  while(len > 0 && tcp_send(s, buffer, len) >= 0)
  {
    memset(buffer, 0, 1024);
    len = tcp_recv(s, buffer, 1024);
    printf("Received: %s [%d]\n", buffer, len);
  }

  printf("%s\n", buffer);
  return 1;
}
#endif
