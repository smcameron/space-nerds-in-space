#ifndef __SNIS_MARSHAL_C__
#define __SNIS_MARSHAL_C__

#include <stdarg.h>
#include <pthread.h>

#include "quat.h"

struct packed_buffer
{
	unsigned char *buffer;
	uint32_t buffer_size; /* size of space allocated for buffer */
	int buffer_cursor;
};

struct packed_buffer_queue_entry {
	struct packed_buffer *buffer;
	struct packed_buffer_queue_entry *next;
};

struct packed_buffer_queue {
	struct packed_buffer_queue_entry *head, *tail;
};

#ifdef DEFINE_SNIS_MARSHAL_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

GLOBAL struct packed_buffer * packed_buffer_allocate(int size); 
GLOBAL void packed_buffer_free(struct packed_buffer *pb);

/* For packed_buffer_append/extract, format is like:
 * "b" = u8 (byte)
 * "h" = u16 (half-word) ( ugh, I hate propagating the notion that "word == 16 bits")
 * "w" = u32 (word)
 * "q" = u64 (quad word)
 * "s" = string (char *)
 * "r" = raw (char *) (unsigned short len)
 * "S" = 32-bit signed integer encoded double (takes 2 params, double + scale )
 * "U" = 32-bit unsigned integer encoded double (takes 2 params, double + scale )
 * "Q" = 4 16-bit signed integer encoded floats representing a quaternion axis + angle
 * "R" = 32-bit signed integer encoded double radians representing an angle
 *       (-2 * M_PI <= angle <= 2 * M_PI must hold.)
 * "Bx" = up to 8 bits, where x denotes how many bits (e.g. B1, B2, B3, B4, B5, B6, B7 or B8)
 */

GLOBAL int packed_buffer_append(struct packed_buffer *pb, const char *format, ...);
GLOBAL int packed_buffer_append_va(struct packed_buffer *pb, const char *format,
					va_list ap);
GLOBAL int packed_buffer_unpack(void*, const char *format, ...);
GLOBAL int packed_buffer_unpack_raw(void *buffer, int size, const char *format, ...);
GLOBAL int packed_buffer_extract(struct packed_buffer *pb, const char *format, ...);
GLOBAL int packed_buffer_extract_va(struct packed_buffer *pb, const char *format,
					va_list ap);
GLOBAL struct packed_buffer *packed_buffer_new(const char *format, ...);
GLOBAL int packed_buffer_append_u16(struct packed_buffer *pb, uint16_t value);
GLOBAL int packed_buffer_append_u8(struct packed_buffer *pb, uint8_t value);
GLOBAL int packed_buffer_append_u32(struct packed_buffer *pb, uint32_t value);
GLOBAL int packed_buffer_append_u64(struct packed_buffer *pb, uint64_t value);
GLOBAL int packed_buffer_append_quat(struct packed_buffer *pb, float q[]);
GLOBAL uint16_t packed_buffer_extract_u16(struct packed_buffer *pb);
GLOBAL uint8_t packed_buffer_extract_u8(struct packed_buffer *pb);
GLOBAL uint32_t packed_buffer_extract_u32(struct packed_buffer *pb);
GLOBAL uint64_t packed_buffer_extract_u64(struct packed_buffer *pb);
GLOBAL void packed_buffer_extract_quat(struct packed_buffer *pb, float q[]);
GLOBAL int packed_buffer_append_string(struct packed_buffer *pb, unsigned char *str, unsigned short len);
GLOBAL int packed_buffer_extract_string(struct packed_buffer *pb, char *buffer, int buflen);
GLOBAL int packed_buffer_append_raw(struct packed_buffer *pb, const char *buffer, unsigned short len);
GLOBAL int packed_buffer_extract_raw(struct packed_buffer *pb, char *buffer, unsigned short len);
GLOBAL struct packed_buffer *packed_buffer_queue_combine(struct packed_buffer_queue *pbqh, pthread_mutex_t *mutex);
GLOBAL void packed_buffer_queue_add(struct packed_buffer_queue *pbqh, struct packed_buffer *pb,
		pthread_mutex_t *mutex);
GLOBAL void packed_buffer_queue_prepend(struct packed_buffer_queue *pbqh, struct packed_buffer *pb,
		pthread_mutex_t *mutex);
GLOBAL void packed_buffer_queue_init(struct packed_buffer_queue *pbq);
GLOBAL void packed_buffer_queue_print(struct packed_buffer_queue *pbg);
GLOBAL void packed_buffer_print(char *label, struct packed_buffer *pb);
GLOBAL struct packed_buffer *packed_buffer_copy(struct packed_buffer *pb);
GLOBAL void packed_buffer_init(struct packed_buffer * pb, void *buffer, int size);

GLOBAL uint32_t dtou32(double d, uint32_t scale);
GLOBAL double u32tod(uint32_t u, uint32_t scale);
GLOBAL int32_t dtos32(double d, int32_t scale);
GLOBAL double s32tod(int32_t u, int32_t scale);
GLOBAL int16_t Qtos16(float q); /* for quaternion elements. (-1.0 <= q <= 1.0) must hold */
GLOBAL int32_t Qtos32(float q);
GLOBAL float s16toQ(int16_t i);
GLOBAL float s32toQ(int32_t i);

GLOBAL int packed_buffer_append_du32(struct packed_buffer *pb, double d, uint32_t scale);
GLOBAL int packed_buffer_append_ds32(struct packed_buffer *pb, double d, int32_t scale);
GLOBAL double packed_buffer_extract_du32(struct packed_buffer *pb, uint32_t scale);
GLOBAL double packed_buffer_extract_ds32(struct packed_buffer *pb, int32_t scale);
GLOBAL int calculate_buffer_size(const char *format);
GLOBAL int packed_buffer_queue_length(struct packed_buffer_queue *pbq, pthread_mutex_t *mutex);
GLOBAL int packed_buffer_length(struct packed_buffer *pb);

#undef GLOBAL
#endif

