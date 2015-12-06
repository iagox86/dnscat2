/* commands_tunnel.c
 * By Ron Bowes
 * Created December, 2015
 *
 * See LICENSE.md
 *
 * Despite the name, this isn't realllly a header file. I moved some of
 * the functions into it to keep them better organized.
 */

static uint32_t g_tunnel_id = 0;

typedef struct
{
  uint32_t          tunnel_id;
  int               s;
  driver_command_t *driver;
} tunnel_t;

static SELECT_RESPONSE_t tunnel_data_in(void *group, int s, uint8_t *data, size_t length, char *addr, uint16_t port, void *param)
{
  tunnel_t         *tunnel   = (tunnel_t*) param;
  command_packet_t *out      = NULL;
  uint8_t          *out_data = NULL;
  size_t            out_length;

  printf("Received data from the socket!\n");

  out = command_packet_create_tunnel_data_request(request_id(), tunnel->tunnel_id, data, length);
  printf("Sending data across tunnel: ");
  command_packet_print(out);

  out_data = command_packet_to_bytes(out, &out_length);
  buffer_add_bytes(tunnel->driver->outgoing_data, out_data, out_length);
  safe_free(out_data);
  command_packet_destroy(out);

  return SELECT_OK;
}

static SELECT_RESPONSE_t tunnel_data_closed(void *group, int s, void *param)
{
  tunnel_t         *tunnel   = (tunnel_t*) param;
  command_packet_t *out      = NULL;
  uint8_t          *out_data = NULL;
  size_t            out_length;

  printf("Socket was closed!\n");

  out = command_packet_create_tunnel_close_request(request_id(), tunnel->tunnel_id);
  printf("Sending close across tunnel: ");
  command_packet_print(out);

  out_data = command_packet_to_bytes(out, &out_length);
  buffer_add_bytes(tunnel->driver->outgoing_data, out_data, out_length);
  safe_free(out_data);
  command_packet_destroy(out);

  return SELECT_CLOSE_REMOVE;
}

static command_packet_t *handle_tunnel_connect(driver_command_t *driver, command_packet_t *in)
{
  command_packet_t *out    = NULL;
  tunnel_t         *tunnel = NULL;

  if(!in->is_request)
    return NULL;

  LOG_WARNING("Connecting to %s:%d...", in->r.request.body.tunnel_connect.host, in->r.request.body.tunnel_connect.port);

  tunnel = (tunnel_t*)safe_malloc(sizeof(tunnel_t));
  tunnel->tunnel_id = g_tunnel_id++;
  /* TODO: The connect should be done asynchronously, if possible. */
  tunnel->s         = tcp_connect(in->r.request.body.tunnel_connect.host, in->r.request.body.tunnel_connect.port);
  tunnel->driver    = driver;

  /* TODO: This global tunnels thing is ugly. */
  LOG_FATAL("Adding the tunnel to an array. THIS IS TERRIBLE. FIX BEFORE REMOVING THIS!");
  ll_add(driver->tunnels, ll_32(tunnel->tunnel_id), tunnel);

  printf("tunnel = %p\n", tunnel);
  select_group_add_socket(driver->group, tunnel->s, SOCKET_TYPE_STREAM, tunnel);
  select_set_recv(driver->group, tunnel->s, tunnel_data_in);
  select_set_closed(driver->group, tunnel->s, tunnel_data_closed);

  out = command_packet_create_tunnel_connect_response(in->request_id, tunnel->tunnel_id);

  return out;
}

static command_packet_t *handle_tunnel_data(driver_command_t *driver, command_packet_t *in)
{
  /* TODO: Find socket by tunnel_id */
  tunnel_t *tunnel = (tunnel_t *)ll_find(driver->tunnels, ll_32(in->r.request.body.tunnel_data.tunnel_id));
  if(!tunnel)
  {
    LOG_ERROR("Couldn't find tunnel: %d", in->r.request.body.tunnel_data.tunnel_id);
    return NULL;
  }
  printf("Received data to tunnel %d (%zd bytes)\n", in->r.request.body.tunnel_data.tunnel_id, in->r.request.body.tunnel_data.length);
  tcp_send(tunnel->s, in->r.request.body.tunnel_data.data, in->r.request.body.tunnel_data.length);

  return NULL;
}

static command_packet_t *handle_tunnel_close(driver_command_t *driver, command_packet_t *in)
{
  tunnel_t *tunnel = (tunnel_t *)ll_remove(driver->tunnels, ll_32(in->r.request.body.tunnel_data.tunnel_id));

  if(!tunnel)
  {
    LOG_WARNING("The server tried to close a tunnel that we don't know about: %d", in->r.request.body.tunnel_data.tunnel_id);
    return NULL;
  }

  select_group_remove_socket(driver->group, tunnel->s);
  tcp_close(tunnel->s);
  LOG_WARNING("Closed tunnel %d", tunnel->tunnel_id);

  return NULL;
}

