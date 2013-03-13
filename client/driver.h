#ifndef __DRIVER_H__
#define __DRIVER_H__

#include <stdint.h>

typedef void(init_t)(void *driver);
typedef int(connect_t)(void *driver);
typedef int(send_t)(void *driver, uint8_t *data, size_t length);
typedef int(recv_t)(void *driver, uint8_t *buf, size_t buf_length);
typedef void(close_t)(void *driver);
typedef void(cleanup_t)(void *driver);

typedef struct
{
  void      *driver;

  init_t    *driver_init;
  connect_t *driver_connect;
  send_t    *driver_send;
  recv_t    *driver_recv;
  close_t   *driver_close;
  cleanup_t *driver_cleanup;

  uint16_t default_window_size;
  uint16_t max_window_size;
  size_t max_packet_size;

} driver_t;

#endif
