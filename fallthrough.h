#ifndef FALLTHROUGH_H__
#define FALLTHROUGH_H__
#if defined(__GNUC__) && __GNUC__ >= 7 || defined(__clang__)
 #define FALLTHROUGH __attribute__ ((fallthrough))
#else
 #define FALLTHROUGH ((void)0)
#endif /* __GNUC__ >= 7 */

#endif
