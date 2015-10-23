/* types.c
 * By Ron Bowes
 * Created September 1, 2008
 *
 * (See LICENSE.md)
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#include <winsock2.h>
#else
#include <pwd.h> /* Required for dropping privileges. */
#include <unistd.h>
#endif

#include "log.h"
#include "memory.h"
#include "types.h"

void drop_privileges(char *username)
{
#ifdef WIN32
  fprintf(stderr, "Skipping privilege drop since we're on Windows.\n");
#else
  /* Drop privileges. */
  if(getuid() == 0)
  {
    /* Get the user id for the given user. */
    struct passwd *user = getpwnam(username);

    if(!user)
    {
      fprintf(stderr, "Error: couldn't drop privileges to '%s': user not found. Please create the\n", username);
      fprintf(stderr, "user or specify a better one with -u.\n");
    }
    else if(user->pw_uid == 0)
    {
      fprintf(stderr, "Error: dropped user account has root privileges; please specify a better\n");
      fprintf(stderr, "one with -u.\n");
    }
    else
    {
/*      fprintf(stderr, "Dropping privileges to account %s:%d.\n", user->pw_name, user->pw_uid); */
      if(setuid(user->pw_uid))
      {
        LOG_FATAL("Failed to drop privileges to %s!", username);
        exit(1);
      }
    }

    /* Ensure it succeeded. */
    if(setuid(0) == 0)
    {
      fprintf(stderr, "Privilege drop failed, sorry!\n");
      exit(1);
    }
  }
#endif
}

int getlasterror()
{
#ifdef WIN32
  if(errno)
    return errno;
  if(GetLastError())
    return GetLastError();
  return WSAGetLastError();
#else
  return errno;
#endif
}

/* Displays an error and doesn't die. */
void nberror(char *str)
{
  int error = getlasterror();

#ifdef WIN32
  char error_str[1024];
  FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, error, 0, error_str, 1024, NULL);

  if(str)
    fprintf(stderr, "%s\n", str);

  fprintf(stderr, "Error %d: %s", error, error_str);

#else
  char *error_str = strerror(error);
  if(str)
    fprintf(stderr, "%s (error %d: %s)\n", str, error, error_str);
  else
    fprintf(stderr, "Error %d: %s\n", error, error_str);

#endif
}

void nbdie(char *str)
{
  nberror(str);
  exit(EXIT_FAILURE);
}

void print_hex(char *label, uint8_t *data, size_t length)
{
  size_t i;

  printf("%s: ", label);
  for(i = 0; i < length; i++)
    printf("%02x", data[i] & 0x0FF);
  printf("\n");
}

