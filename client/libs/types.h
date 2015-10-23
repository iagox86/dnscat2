/* types.h
 * By Ron Bowes
 * Created September 1, 2008
 *
 * (See LICENSE.md)
 *
 * Defines (or includes libraries that define) various required datatypes.
 *
 * Additionally, this module implements several miscellanious functions in a
 * platform-independent way.
 *
 * You'll notice things with nb* names - that dates back to the
 * heritage, a lot of dnscat2 stuff came from dnscat1, which was part of
 * my 'nbtool' library for hacking netbios. History lesson!
 */

#ifndef __TYPES_H__
#define __TYPES_H__

#ifndef WIN32
/* OS X doesn't seem to have INADDR_NONE defined in all cases. */
/* If this causes a compile error on some system, try putting "#ifdef __APPLE__"
 * around it. */
#ifndef INADDR_NONE
#define INADDR_NONE ((in_addr_t) -1)
#endif
#endif

#ifdef WIN32
#include "pstdint.h"

/* Define ssize_t because Windows doesn't. */
#ifndef _SSIZE_T_DEFINED
#ifdef  _WIN64
typedef unsigned __int64    ssize_t;
#else
typedef _W64 unsigned int   ssize_t;
#endif
#define _SSIZE_T_DEFINED
#endif

#else
#include <stdint.h>
#endif

#include <stdlib.h>

#ifndef TRUE
typedef enum
{
  FALSE,
  TRUE
} NBBOOL;
#else
typedef int NBBOOL;
#endif

#ifdef WIN32
typedef int socklen_t;
#define strcasecmp _strcmpi
#define strcasestr nbstrcasestr
#define fileno     _fileno
#define write      _write
#endif

#ifndef MIN
#define MIN(a,b) (a < b ? a : b)
#endif

#ifndef MAX
#define MAX(a,b) (a > b ? a : b)
#endif

#define DIE(a) {fprintf(stderr, "Unrecoverable error in %s(%d): %s\n\n", __FILE__, __LINE__, a); abort();}
#define DIE_MEM() {DIE("Out of memory.");}

/* Drop privileges to the chosen user, then verify that root can't be re-enabled. */
void drop_privileges(char *username);

/* Get the last error, independent of platform. */
int getlasterror();

/* Displays an error and doesn't die. The getlasterror() function is used as well as the appropriate
 * error-message-display function for the platform. If str is non-NULL, it's also displayed. */
void nberror(char *str);

/* Displays an error using nberror(), then dies. */
void nbdie(char *str);

/* Implementation of strcasestr() for Windows. */
char *nbstrcasestr(char *haystack, char *needle);

/* Print a hex string, comes in handy a lot! */
void print_hex(char *label, uint8_t *data, size_t length);

#endif

