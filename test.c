#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "controller.h"
#include "driver_tcp.h"

int main(int argc, const char *argv[])
{
  controller_t *c = controller_create(tcp_get_driver("www.javaop.com", 80));

  printf("Connecting...\n");
  controller_connect(c);

  printf("Sending data...\n");
  controller_send(c, "GET / HTTP/1.0\r\n\r\n", 18);

  printf("Calling do_actions...\n");
  controller_do_actions(c);
  controller_do_actions(c);
  sleep(1);
  controller_do_actions(c);
  controller_do_actions(c);

  buffer_print(c->incoming_data);

  printf("Done!\n");

  return 0;
}
