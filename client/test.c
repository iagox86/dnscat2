/* test.c
 * By Ron Bowes
 *
 * See LICENSE.md
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "memory.h"
#include "packet.h"
#include "session.h"

int main(int argc, const char *argv[])
{
  packet_t *packet;

  uint8_t  *bytes;
  size_t    length;

  /* Create a SYN */
  packet = packet_create_syn(0x1234, 0x0000, 0x0000);
  packet_print(packet);

  /* Convert it to bytes and free the original */
  bytes = packet_to_bytes(packet, &length);
  packet_destroy(packet);

  /* Parse the bytes from the old packet to create a new one */
  packet = packet_parse(bytes, length);
  packet_print(packet);
  packet_destroy(packet);
  safe_free(bytes);

  /* Create a MSG */
  packet = packet_create_msg(0x1234, 0x0000, 0x0001, (uint8_t*)"AAAAA", 5);
  packet_print(packet);

  /* Convert it to bytes and free the orignal */
  bytes = packet_to_bytes(packet, &length);
  packet_destroy(packet);

  /* Parse the bytes from the old packet to create a new one */
  packet = packet_parse(bytes, length);
  packet_print(packet);
  packet_destroy(packet);
  safe_free(bytes);

  /* Create a FIN */
  packet = packet_create_fin(0x1234);
  packet_print(packet);

  /* Convert it to bytes and free the orignal */
  bytes = packet_to_bytes(packet, &length);
  packet_destroy(packet);
  safe_free(bytes);

  /* Parse the bytes from the old packet to create a new one */
  packet = packet_parse(bytes, length);
  packet_print(packet);
  packet_destroy(packet);

  print_memory();

  return 0;
}
