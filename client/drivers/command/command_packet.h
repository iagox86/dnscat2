/* command_packet.h
 * By Ron Bowes
 * Created May, 2014
 *
 * See LICENSE.md
 *
 * A class for creating and parsing dnscat command protocol packets.
 */
#ifndef __COMMAND_PACKET_H__
#define __COMMAND_PACKET_H__

#include <stdlib.h>

#include "libs/buffer.h"
#include "libs/types.h"

#ifdef WIN32
#include "libs/pstdint.h"
#else
#include <stdint.h>
#endif

/* Just make sure it doesn't overflow, basically */
#define MAX_COMMAND_PACKET_SIZE 0x7FFFFF00

typedef enum
{
  COMMAND_PING      = 0x0000,
  COMMAND_SHELL     = 0x0001,
  COMMAND_EXEC      = 0x0002,
  COMMAND_DOWNLOAD  = 0x0003,
  COMMAND_UPLOAD    = 0x0004,
  COMMAND_SHUTDOWN  = 0x0005,

  TUNNEL_CONNECT    = 0x1000,
  TUNNEL_DATA       = 0x1001,
  TUNNEL_CLOSE      = 0x1002,

  COMMAND_ERROR     = 0xFFFF,
} command_packet_type_t;

typedef enum
{
  TUNNEL_STATUS_FAIL = 0x8000,
} tunnel_status_t;

typedef struct
{
  uint16_t request_id;
  command_packet_type_t command_id;
  NBBOOL is_request;

  union
  {
    struct
    {
      union {
        struct { char *data; } ping;
        struct { char *name; } shell;
        struct { char *name; char *command; } exec;
        struct { char *filename; } download;
        struct { char *filename; uint8_t *data; uint32_t length; } upload;
        struct { int dummy; } shutdown;
        struct { uint32_t options; char *host; uint16_t port; } tunnel_connect;
        struct { uint32_t tunnel_id; uint8_t *data; size_t length; } tunnel_data;
        struct { uint32_t tunnel_id; char *reason; } tunnel_close;
        struct { uint16_t status; char *reason; } error;
      } body;
    } request;

    struct
    {
      union {
        struct { char *data; } ping;
        struct { uint16_t session_id; } shell;
        struct { uint16_t session_id; } exec;
        struct { uint8_t *data; uint32_t length; } download;
        struct { int dummy; } upload;
        struct { int dummy; } shutdown;
        struct { uint16_t status; uint32_t tunnel_id; } tunnel_connect;
        struct { int dummy; } tunnel_data;
        struct { int dummy; } tunnel_close;
        struct { uint16_t status; char *reason; } error;
      } body;
    } response;
  } r;
} command_packet_t;

/* Parse a packet from a byte stream. */
command_packet_t *command_packet_read(buffer_t *buffer);

/* Create a packet with the given characteristics. */
command_packet_t *command_packet_create_ping_request(uint16_t request_id, char *data);
command_packet_t *command_packet_create_ping_response(uint16_t request_id, char *data);

command_packet_t *command_packet_create_shell_request(uint16_t request_id, char *name);
command_packet_t *command_packet_create_shell_response(uint16_t request_id, uint16_t session_id);

command_packet_t *command_packet_create_exec_request(uint16_t request_id, char *name, char *command);
command_packet_t *command_packet_create_exec_response(uint16_t request_id, uint16_t session_id);

command_packet_t *command_packet_create_download_request(uint16_t request_id, char *filename);
command_packet_t *command_packet_create_download_response(uint16_t request_id, uint8_t *data, uint32_t length);

command_packet_t *command_packet_create_upload_request(uint16_t request_id, char *filename, uint8_t *data, uint32_t length);
command_packet_t *command_packet_create_upload_response(uint16_t request_id);

command_packet_t *command_packet_create_shutdown_response(uint16_t request_id);

command_packet_t *command_packet_create_tunnel_connect_request(uint16_t request_id, uint32_t options, char *host, uint16_t port);
command_packet_t *command_packet_create_tunnel_connect_response(uint16_t request_id, uint32_t tunnel_id);

command_packet_t *command_packet_create_tunnel_data_request(uint16_t request_id, uint32_t tunnel_id, uint8_t *data, uint32_t length);

command_packet_t *command_packet_create_tunnel_close_request(uint16_t request_id, uint32_t tunnel_id, char *reason);

command_packet_t *command_packet_create_error_request(uint16_t request_id, uint16_t status, char *reason);
command_packet_t *command_packet_create_error_response(uint16_t request_id, uint16_t status, char *reason);


/* Free the packet data structures. */
void command_packet_destroy(command_packet_t *packet);

/* Print the packet (debugging, mostly) */
void command_packet_print(command_packet_t *packet);

/* Needs to be freed with safe_free() */
uint8_t *command_packet_to_bytes(command_packet_t *packet, size_t *length);

#endif
