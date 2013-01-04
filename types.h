/* types.h
 * By Ron Bowes
 * Created September 1, 2008
 *
 * (See LICENSE.txt)
 *
 * Defines (or includes libraries that define) various required datatypes.
 *
 * Additionally, this module implements several miscellanious functions in a
 * platform-independent way.
 */

#ifndef __TYPES_H__
#define __TYPES_H__

#ifdef WIN32
#include "pstdint.h"
#else
#include <stdint.h>
#endif

#define NBTOOL_NAME         "nbtool"
#define NBTOOL_VERSION      "0.05alpha2"
#define NBTOOL_NAME_VERSION NBTOOL_NAME" v"NBTOOL_VERSION

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

#endif

