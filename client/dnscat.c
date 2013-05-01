#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "select_group.h"

#include "driver.h"

#include "memory.h"
#include "packet.h"
#include "session.h"
#include "time.h"
#include "types.h"

#if 0
void cleanup()
{
  fprintf(stderr, "[[dnscat]] :: Terminating\n");

  if(options)
  {
    session_destroy(options->session);
    safe_free(options);
    options = NULL;
  }

  print_memory();
}
#endif

int main(int argc, char *argv[])
{
  driver_create(argc, argv);
}
