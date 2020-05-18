#ifndef SSGL_STRING_H__
#define SSGL_STRING_H__

#if USE_CUSTOM_STRLCPY == 1
extern size_t strlcpy(char *dest, const char *src, size_t n);
#else
#include <bsd/string.h>
#endif

#endif
