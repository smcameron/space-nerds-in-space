#ifndef OPEN_SIMPLEX_NOISE_H__
#define OPEN_SIMPLEX_NOISE_H__

/*
 * OpenSimplex (Simplectic) Noise in C.
 * Ported to C from Kurt Spencer's java implementation by Stephen M. Cameron
 *
 * v1.1 (October 6, 2014) 
 * - Ported to C
 * 
 * v1.1 (October 5, 2014)
 * - Added 2D and 4D implementations.
 * - Proper gradient sets for all dimensions, from a
 *   dimensionally-generalizable scheme with an actual
 *   rhyme and reason behind it.
 * - Removed default permutation array in favor of
 *   default seed.
 * - Changed seed-based constructor to be independent
 *   of any particular randomization library, so results
 *   will be the same when ported to other languages.
 */

struct osn_context;
 
void open_simplex_noise_init(uint16_t perm[], int nelements);
void open_simplex_noise_free(struct osn_context *ctx);
int open_simplex_noise(int64_t seed, struct osn_context **ctx);
double open_simplex_noise2(struct osn_context *ctx, double x, double y);
double open_simplex_noise3(struct osn_context *ctx, double x, double y, double z);
double open_simplex_noise4(struct osn_context *ctx, double x, double y, double z, double w);

#endif
