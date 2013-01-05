#ifndef __DRIVER_H__
#define __DRIVER_H__

typedef int(connect_t)(uint16_t session_id, void *param);
typedef int(bind_t)(uint16_t session_id, void *param);
typedef int(send_t)(uint16_t session_id, uint8_t *data, uint16_t length, void *param);
typedef int(recv_t)(uint16_t session_id, uint8_t buf, uint16_t max_length, void *param);
typedef int(close_t)(uint16_t session_id, void *param);

typedef struct
{
  connect_t *driver_connect;
  bind_t    *driver_bind;
  send_t    *driver_send;
  recv_t    *driver_listen;
  close_t   *driver_close;

  uint16_t default_window_size;
  uint16_t max_window_size;

} driver_t;

#endif
