#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "memory.h"
#include "packet.h"
#include "session.h"

int main(int argc, const char *argv[])
{
#if 0
  controller_t *c = controller_create(tcp_get_driver("www.javaop.com", 80));
  size_t waiting;

  printf("Connecting...\n");
  controller_connect(c);

  printf("Sending data...\n");
  controller_send(c, "GET / HTTP/1.0\r\n\r\n", 18);

  printf("Calling do_actions...\n");
  waiting = controller_do_actions(c);
  printf("There are %zu bytes waiting to be read...\n", waiting);
  waiting = controller_do_actions(c);
  printf("There are %zu bytes waiting to be read...\n", waiting);
  sleep(1);
  waiting = controller_do_actions(c);
  printf("There are %zu bytes waiting to be read...\n", waiting);
  waiting = controller_do_actions(c);
  printf("There are %zu bytes waiting to be read...\n", waiting);

  /*buffer_print(c->incoming_data);*/

  controller_cleanup(c);
#endif

  printf("Done!\n");

  print_memory();

  return 0;
}
