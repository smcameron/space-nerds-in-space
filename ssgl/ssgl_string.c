#include <string.h>
#include "ssgl_string.h"

size_t strlcpy(char *dest, const char *src, size_t n)
{
	size_t i;

	for (i = 0;; i++) {
		if (i < n)
			dest[i] = src[i];
		else {
			dest[n - 1] = '\0';
			break;
		}
		if (src[i] == '\0')
			return i;
	}
	for (;; i++)
		if (src[i] == '\0')
			break;
	return i;
}
