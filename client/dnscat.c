#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buffer.h"
#include "dns.h"
#include "tcp.h"
#include "types.h"
#include "udp.h"

#include "controller.h"
#include "driver_dns.h"
#include "driver_tcp.h"

int main(int argc, char *argv[])
{
  driver_t *driver = tcp_get_driver("localhost", 1234);
  controller_t *controller = controller_create(driver);

  

  return 0;
}
