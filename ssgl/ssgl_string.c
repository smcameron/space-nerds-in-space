#include <string.h>
#include "ssgl_string.h"

#if USE_CUSTOM_STRLCPY
size_t strlcpy(char *dest, const char *src, size_t n)
{
	size_t rc;
	int i;

	for (i = 0;; i++) {
		if (i < n)
			dest[i] = src[i];
		else {
			dest[n - 1] = '\0';
			if (src[i])
				rc = i + 1 + strlen(&src[i + 1]);
			else
				rc = i;
			break;
		}
		if (!src[i]) {
			rc = i;
			break;
		}
	}
	return rc;
}
#endif
