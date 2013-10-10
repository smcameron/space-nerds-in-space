#ifndef BUILD_ASSERT

/* Cribbed from ccan.org build_assert.h
 * See: https://github.com/rustyrussell/stats/blob/master/ccan/build_assert/build_assert.h
 */
#define BUILD_ASSERT(cond) \
do { (void) sizeof(char [1 - 2*!(cond)]); } while(0)

#endif
