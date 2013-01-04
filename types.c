/* types.c
 * By Ron Bowes
 * Created September 1, 2008
 *
 * (See LICENSE.txt)
 */

#define _BSD_SOURCE /* For strdup(). */

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
/*			fprintf(stderr, "Dropping privileges to account %s:%d.\n", user->pw_name, user->pw_uid); */
			setuid(user->pw_uid);
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

char *nbstrcasestr(char *haystack, char *needle)
{
	size_t i;
	char *haystack_lc = safe_strdup(haystack);
	char *needle_lc   = safe_strdup(needle);
	char *result;

	/* Make them both lowercase. Note: 'int' typecast fixes compiler warning on cygwin. */
	for(i = 0; i < strlen(haystack_lc); i++)
		haystack_lc[i] = tolower((int)haystack_lc[i]);
	for(i = 0; i < strlen(needle_lc); i++)
		needle_lc[i] = tolower((int)needle_lc[i]);

	printf("Searching for '%s' in '%s'...\n", needle_lc, haystack_lc);

	result = strstr(haystack_lc, needle_lc);

	if(!result)
		return NULL;

	result = haystack + (haystack_lc - result);

	safe_free(haystack_lc);
	safe_free(needle_lc);

	return result;
}
