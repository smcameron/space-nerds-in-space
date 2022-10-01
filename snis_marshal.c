#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdarg.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>

#include "stacktrace.h"

#define DEFINE_SNIS_MARSHAL_GLOBALS
#include "snis_marshal.h"
#undef DEFINE_SNIS_MARSHAL_GLOBALS

#include "stacktrace.h"

#define MAX_FRACTIONAL 0x7FFFFFFFFFFFFFFFLL /* 2^63 - 1 */
#define RADIANS_SCALE (31415927)

/* Change SNIS_MARSHAL_DETECT_NANS to 1 and then doubles and quaternions will
 * be checked before packing and after unpacking and if NaNs or infinities are
 * found, they will be logged.
 */
#define SNIS_MARSHAL_DETECT_NANS 0

static int host_is_little_endian()
{
	static int answer = -1;
	uint32_t x = 0x01020304;
	unsigned char *c = (unsigned char *) &x;

	if (answer != -1)
		return answer;

	answer = (c[0] == 0x04);
	return answer;
}

/* change a u64 to network byte order */
static uint64_t cpu_to_be64(uint64_t v)
{
	unsigned char *x, *y;
	uint64_t answer;

	if (!host_is_little_endian())
		return v;
		
	x = (unsigned char *) &v;
	y = (unsigned char *) &answer;
	y[0] = x[7];
	y[1] = x[6];
	y[2] = x[5];
	y[3] = x[4];
	y[4] = x[3];
	y[5] = x[2];
	y[6] = x[1];
	y[7] = x[0];
	return answer;
}

static uint64_t be64_to_cpu(uint64_t v)
{
	return cpu_to_be64(v);
}

static void packed_buffer_check(struct packed_buffer *pb)
{
	if ((uint32_t) pb->buffer_cursor > pb->buffer_size) {
		fprintf(stderr, "pb->buffer_cursor = %d, pb->buffer_size = %d\n",
			pb->buffer_cursor, pb->buffer_size);
		packed_buffer_print("sanity violation", pb);
		stacktrace("pb->buffer_cursor > pb->buffer_size\n");
		assert((uint32_t) (pb)->buffer_cursor <= (pb)->buffer_size);
	}
}

static void packed_buffer_check_add(struct packed_buffer *pb, int additional_bytes)
{
	if ((uint32_t) (pb->buffer_cursor + additional_bytes) > pb->buffer_size) {
		fprintf(stderr, "Buffer overflow: cursor = %d, additional bytes = %llu, buffer size = %d\n",
			pb->buffer_cursor, (unsigned long long) additional_bytes, pb->buffer_size);
		stacktrace("");
		assert(0);
	}
}

int packed_buffer_append_u16(struct packed_buffer *pb, uint16_t value)
{
	value = htons(value);
	memcpy(&pb->buffer[pb->buffer_cursor], &value, sizeof(value));
	pb->buffer_cursor += sizeof(value);
	packed_buffer_check(pb);
	return 0;
}

int packed_buffer_append_u8(struct packed_buffer *pb, uint8_t value)
{
	uint8_t *c = (uint8_t *) &pb->buffer[pb->buffer_cursor];
	*c = value;
	pb->buffer_cursor += 1;
	packed_buffer_check(pb);
	return 0;
}

int packed_buffer_append_u32(struct packed_buffer *pb, uint32_t value)
{
	value = htonl(value);
	memcpy(&pb->buffer[pb->buffer_cursor], &value, sizeof(value));
	pb->buffer_cursor += sizeof(value);
	packed_buffer_check(pb);
	return 0;
}

int packed_buffer_append_u64(struct packed_buffer *pb, uint64_t value)
{
	value = cpu_to_be64(value);
	memcpy(&pb->buffer[pb->buffer_cursor], &value, sizeof(value));
	pb->buffer_cursor += sizeof(value);
	packed_buffer_check(pb);
	return 0;
}

uint16_t packed_buffer_extract_u16(struct packed_buffer *pb)
{
	uint16_t value;

	memcpy(&value, &pb->buffer[pb->buffer_cursor], sizeof(value));
	pb->buffer_cursor += sizeof(value);
	packed_buffer_check(pb);
	value = ntohs(value);
	return value;
}

uint8_t packed_buffer_extract_u8(struct packed_buffer *pb)
{
	uint8_t *c = (uint8_t *) &pb->buffer[pb->buffer_cursor];
	pb->buffer_cursor += 1;
	packed_buffer_check(pb);
	return *c;
}

uint32_t packed_buffer_extract_u32(struct packed_buffer *pb)
{
	uint32_t value;

	memcpy(&value, &pb->buffer[pb->buffer_cursor], sizeof(value));
	pb->buffer_cursor += sizeof(value);
	packed_buffer_check(pb);
	value = ntohl(value);
	return value;
}

uint64_t packed_buffer_extract_u64(struct packed_buffer *pb)
{
	uint64_t value;

	memcpy(&value, &pb->buffer[pb->buffer_cursor], sizeof(value));
	pb->buffer_cursor += sizeof(value);
	packed_buffer_check(pb);
	value = be64_to_cpu(value);
	return value;
}

int packed_buffer_append_string(struct packed_buffer *pb, unsigned char *str, unsigned short len)
{
	unsigned short belen;

	packed_buffer_check_add(pb, sizeof(unsigned short) + len);
	belen = htons(len);	
	memcpy(&pb->buffer[pb->buffer_cursor], &belen, sizeof(belen));
	pb->buffer_cursor += sizeof(belen);
	packed_buffer_check(pb);
	memcpy(&pb->buffer[pb->buffer_cursor], &str, len);
	pb->buffer_cursor += len;
	packed_buffer_check(pb);
	return 0;
}

int packed_buffer_extract_string(struct packed_buffer *pb, char *buffer, int buflen)
{
	unsigned short len;

	memcpy(&len, &pb->buffer[pb->buffer_cursor], sizeof(len));
	len = ntohs(len);
	if (len < buflen)
		buflen = len;
	pb->buffer_cursor += sizeof(len);
	packed_buffer_check(pb);
	memcpy(buffer, &pb->buffer[pb->buffer_cursor], len > buflen ? buflen : len); 
	pb->buffer_cursor += len;	
	packed_buffer_check(pb);
	return len > buflen ? buflen : len;
}

int packed_buffer_append_raw(struct packed_buffer *pb, const char *buffer, unsigned short len)
{
	memcpy(&pb->buffer[pb->buffer_cursor], buffer, len);
	pb->buffer_cursor += len;
	packed_buffer_check(pb);
	return 0;
}

int packed_buffer_extract_raw(struct packed_buffer *pb, char *buffer, unsigned short len)
{
	memcpy(buffer, &pb->buffer[pb->buffer_cursor], len);
	pb->buffer_cursor += len;
	packed_buffer_check(pb);
	return 0;
}

struct packed_buffer * packed_buffer_allocate(int size)
{
	struct packed_buffer *pb;
	pb = malloc(sizeof(*pb));
	memset(pb, 0, sizeof(*pb));	
	if (size > 0) {
		pb->buffer = malloc(size);
		memset(pb->buffer, 0, size);
		pb->buffer_size = size;
	} else {
		stacktrace("Bad buffer size in packed_buffer_allocate()");
		abort();
	}
	return pb;
}

void packed_buffer_free(struct packed_buffer *pb)
{
	free(pb->buffer);
	free(pb);
}

static inline void lockmutex(pthread_mutex_t *mutex)
{
	if (mutex)
		(void) pthread_mutex_lock(mutex);
}

static inline void unlockmutex(pthread_mutex_t *mutex)
{
	if (mutex)
		(void) pthread_mutex_unlock(mutex);
}

struct packed_buffer *packed_buffer_queue_combine(struct packed_buffer_queue *pbq,
	pthread_mutex_t *mutex)
{
	struct packed_buffer *answer;
	struct packed_buffer_queue_entry *i;
	uint32_t totalbytes;
	unsigned char *buf;

	/* Count total bytes in buffer queue */
	lockmutex(mutex);
	totalbytes = 0;
	for (i = pbq->head; i; i = i->next)
		totalbytes += i->buffer->buffer_cursor;

	if (totalbytes == 0) {
		unlockmutex(mutex);
		return NULL;
	}

	answer = malloc(sizeof(*answer));

	/* allocate a big buffer to hold the combined buffers... */	
	buf = malloc(totalbytes);
	answer->buffer = buf;
	answer->buffer_size = totalbytes;
	answer->buffer_cursor = 0;

	/* combine the buffers, freeing as we go. */
	for (i = pbq->head; i; i = pbq->head) {
		if (i->buffer->buffer && i->buffer->buffer_cursor) {
			memcpy(&answer->buffer[answer->buffer_cursor],
				i->buffer->buffer, i->buffer->buffer_cursor);
			answer->buffer_cursor += i->buffer->buffer_cursor;
			packed_buffer_check(answer);
			packed_buffer_free(i->buffer);
			pbq->head = i->next;
			free(i);
		}
	}
	pbq->head = NULL;
	pbq->tail = NULL;
	unlockmutex(mutex);
	return answer;
}

void packed_buffer_queue_add(struct packed_buffer_queue *pbq, struct packed_buffer *pb,
		pthread_mutex_t *mutex)
{
	struct packed_buffer_queue_entry *entry;

	entry = malloc(sizeof(*entry));
	entry->next = NULL;
	entry->buffer = pb;

	lockmutex(mutex);
	if (!pbq->tail) {
		pbq->head = entry;
		pbq->tail = entry;
		unlockmutex(mutex);
		return;
	}
	pbq->tail->next = entry;
	pbq->tail = entry;
	unlockmutex(mutex);
	return;
}	

void packed_buffer_queue_prepend(struct packed_buffer_queue *pbq, struct packed_buffer *pb,
		pthread_mutex_t *mutex)
{
	struct packed_buffer_queue_entry *entry;

	entry = malloc(sizeof(*entry));
	entry->buffer = pb;

	lockmutex(mutex);
	if (!pbq->head) {
		entry->next = NULL;
		pbq->head = entry;
		pbq->tail = entry;
		unlockmutex(mutex);
		return;
	}
	entry->next = pbq->head;
	pbq->head = entry;
	unlockmutex(mutex);
	return;
}

void packed_buffer_queue_init(struct packed_buffer_queue *pbq)
{
	pbq->head = NULL;
	pbq->tail = NULL;
}

void packed_buffer_queue_print(struct packed_buffer_queue *pbq)
{
	struct packed_buffer_queue_entry *i;
	int count;

	printf("Packed buffer queue: pbq=%p, head = %p, tail = %p\n",
		(void *) pbq, (void *) pbq->head, (void *) pbq->tail);
	count = 0;
	for (i = pbq->head; i; i = i->next) {
		printf("%d: %p: pb=%p, buffer = %p, size=%d, cursor=%d\n",
			count, (void *) i, (void *) i->buffer, (void *) i->buffer->buffer,
			i->buffer->buffer_size, i->buffer->buffer_cursor);
		count++;
	}
	printf("count = %d\n", count);
}

void packed_buffer_print(char *label, struct packed_buffer *pb)
{
	int i, len;

	len = pb->buffer_size;
	fprintf(stderr, "--- begin packed buffer: %s: cursor=%d, size=%d ---\n",
		label, pb->buffer_cursor, pb->buffer_size);
	for (i = 0; i < len; i++) {
		fprintf(stderr, "%c%02x",
			i == pb->buffer_cursor ? '>' : i == pb->buffer_cursor + 1 ? '<' : ' ',
			pb->buffer[i]);
		if (((i + 1) % 32) == 0)
			fprintf(stderr, "\n");
	}
	fprintf(stderr, "---\n");
	for (i = 0; i < len; i++) {
		fprintf(stderr, "%c%c",
			i == pb->buffer_cursor ? '>' : i == pb->buffer_cursor + 1 ? '<' : ' ',
			isgraph(pb->buffer[i]) ? pb->buffer[i] : '.');
		if (((i + 1) % 32) == 0)
			fprintf(stderr, "\n");
	}
	fprintf(stderr, "--- end packed buffer: %s: L=%d,S=%d ---\n",
		label, pb->buffer_cursor, pb->buffer_size);
}

struct packed_buffer *packed_buffer_copy(struct packed_buffer *pb)
{
	struct packed_buffer *pbc;

	pbc = packed_buffer_allocate(pb->buffer_size);
	memcpy(pbc->buffer, pb->buffer, pb->buffer_size);
	pbc->buffer_cursor = pb->buffer_cursor;
	return pbc;
}

void packed_buffer_init(struct packed_buffer * pb, void *buffer, int size)
{
	pb->buffer = buffer;
	pb->buffer_size = size;
	pb->buffer_cursor = 0;
}

uint32_t dtou32(double d, uint32_t scale)
{
	return (uint32_t) ((d / scale) * (double) UINT32_MAX);
}

double u32tod(uint32_t u, uint32_t scale)
{
	return ((double) u * (double) scale) / (double) UINT32_MAX;
}

int32_t dtos32(double d, int32_t scale)
{
	return (int32_t) ((d / scale) * (double) INT32_MAX);
}

/* Q for quaternion */
int32_t Qtos32(float q)
{
	/* q must be between -1.0 and 1.0 */
	return (int32_t) (q * (float) (INT32_MAX - 1));
}

/* Q for quaternion */
float s32toQ(int32_t i)
{
	return ((float) i) / (float) (INT32_MAX - 1);
}

/* Q for quaternion */
int16_t Qtos16(float q)
{
	/* q must be between -1.0 and 1.0 */
	return (int16_t) (q * (float) (INT16_MAX - 1));
}

/* Q for quaternion */
float s16toQ(int16_t i)
{
	return ((float) i) / (float) (INT16_MAX - 1);
}

double s32tod(int32_t u, int32_t scale)
{
	return ((double) u * (double) scale) / (double) INT32_MAX;
}

int packed_buffer_append_du32(struct packed_buffer *pb, double d, uint32_t scale)
{
	return packed_buffer_append_u32(pb, dtou32(d, scale));	
}

int packed_buffer_append_ds32(struct packed_buffer *pb, double d, int32_t scale)
{
	return packed_buffer_append_u32(pb, (uint32_t) dtos32(d, scale));
}

double packed_buffer_extract_du32(struct packed_buffer *pb, uint32_t scale)
{
	return u32tod(packed_buffer_extract_u32(pb), scale);
}

double packed_buffer_extract_ds32(struct packed_buffer *pb, int32_t scale)
{
	return s32tod(packed_buffer_extract_u32(pb), scale);
}

int packed_buffer_append_quat(struct packed_buffer *pb, float q[])
{
	int i;
	int16_t v[4];

	packed_buffer_check_add(pb, sizeof(v));
	for (i = 0; i < 4; i++)
		v[i] = htons(Qtos16(q[i]));
	memcpy(&pb->buffer[pb->buffer_cursor], v, sizeof(v));
	pb->buffer_cursor += sizeof(v);
	packed_buffer_check(pb);
	return 0;
}

void packed_buffer_extract_quat(struct packed_buffer *pb, float q[])
{
	int i;
	int16_t v;

	for (i = 0; i < 4; i++) {
		/* need the memcpy for alignment reasons (not on x86, but for others) */
		memcpy(&v, &pb->buffer[pb->buffer_cursor], sizeof(v));
		q[i] = s16toQ(ntohs(v));
		pb->buffer_cursor += sizeof(v);
		packed_buffer_check(pb);
	}
}

#if SNIS_MARSHAL_DETECT_NANS
#warning "snis_marshal.c: NaN detection enabled"
static void check_double(char *where, double d)
{
	int c = fpclassify(d);

	switch (c) {
	case FP_NORMAL:
	case FP_SUBNORMAL:
	case FP_ZERO:
		return;
	case FP_NAN:
	case FP_INFINITE:
	default:
		break;
	}
	fprintf(stderr, "Bad float encountered in snis_marshal %s\n", where);
	stacktrace("Bad float encountered in snis_marshal\n");
}

static void check_quaternion(char *where, float *q)
{
	int i, c;

	for (i = 0; i < 4; i++) {
		c = fpclassify(q[i]);
		switch (c) {
		case FP_NORMAL:
		case FP_SUBNORMAL:
		case FP_ZERO:
			continue;
		case FP_NAN:
		case FP_INFINITE:
		default:
			break;
		}
		fprintf(stderr, "Bad quaternion encountered in snis_marshal %s\n", where);
		fprintf(stderr, "q[0,1,2,3] = %f, %f, %f, %f\n", q[0], q[1], q[2], q[3]);
		stacktrace("Bad quaternion encountered in snis_marshal\n");
		return;
	}
	return;
}
#else
#define check_double(a, b)
#define check_quaternion(a, b)
#endif

int packed_buffer_append_va(struct packed_buffer *pb, const char *format, va_list ap)
{
	uint8_t b, bits;
	uint16_t h;
	uint32_t w;
	uint64_t q;
	int32_t uscale;
	uint32_t sscale;
	char *r;
	unsigned char *s;
	unsigned short len;
	int i, n, rc = 0;
	double d;
	float *quaternion;

	while (*format) {	
		switch(*format++) {
		case 'b':
			b = (uint8_t) va_arg(ap, int);
			packed_buffer_append_u8(pb, b);
			break;
		case 'h':
			h = (uint16_t) va_arg(ap, int);
			packed_buffer_append_u16(pb, h);
			break;
		case 'w':
			w = va_arg(ap, uint32_t);
			packed_buffer_append_u32(pb, w);
			break;
		case 'q':
			q = va_arg(ap, uint64_t);
			packed_buffer_append_u64(pb, q);
			break;
		case 's':
			s = va_arg(ap, unsigned char *);
			packed_buffer_append_string(pb, s, (unsigned short) strlen((char *) s));
			break;
		case 'r':
			r = va_arg(ap, char *);
			len = (unsigned short) va_arg(ap, int);
			packed_buffer_append_raw(pb, r, len);
			break;
		case 'S':
			d = va_arg(ap, double);
			check_double("packed_buffer_append_va:'S'", d);
			sscale = va_arg(ap, int32_t); 
			packed_buffer_append_ds32(pb, d, sscale);
			break;
		case 'R':
			d = va_arg(ap, double);
			check_double("packed_buffer_append_va:'R'", d);
			sscale = INT32_MAX / RADIANS_SCALE;
			if (d < -2.0 * M_PI || d > 2.0 * M_PI) {
				printf("out of range angle %d\n", (int) (d * 180.0 / M_PI));
				stacktrace("out of range angle in packed_buffer_append_va");
			}
			packed_buffer_append_ds32(pb, d, sscale);
			break;
		case 'U':
			d = va_arg(ap, double);
			check_double("packed_buffer_append_va:'U'", d);
			uscale = va_arg(ap, uint32_t); 
			packed_buffer_append_du32(pb, d, uscale);
			break;
		case 'Q':
			quaternion = va_arg(ap, float *);
			check_quaternion("packed_buffer_append_va:'Q'", quaternion);
			packed_buffer_append_quat(pb, quaternion);
			break;
		case 'B':
			bits = 0;
			n = *format++ - '0';
			if (n > 8 || n < 1) {
				fprintf(stderr, "Bad bitfield length in packed_buffer_append_va(): '%c'\n", n);
				stacktrace("Bad bitfield length in packed_buffer_append_va");
				n = 1;
			}
			for (i = 0; i < n; i++) {
				b = (uint8_t) va_arg(ap, int);
				b = !!b;
				bits = (bits << 1) | b;
			}
			packed_buffer_append_u8(pb, bits);
			break;
		default:
			fprintf(stderr, "Bad format char in packed_buffer_append_va(): '%c'\n", *(format - 1));
			stacktrace("Bad format string in packed_buffer_append_va()");
			abort();
		}
	}
	return rc;
}

/* For packed_buffer_append/extract, format is like:
 * "b" = u8 (byte)
 * "h" = u16 (half-word) ( ugh, I hate propagating the notion that "word == 16 bits")
 * "w" = u32 (word)
 * "s" = string
 * "r" = raw
 * "d" = double
 * "S" = 32-bit signed integer encoded double (takes 2 params, double + scale )
 * "U" = 32-bit unsigned integer encoded double (takes 2 params, double + scale )
 * "Q" = 4 16-bit signed integer encoded floats representing a quaternion axis + angle
 * "R" = 32-bit signed integer encoded double radians representing an angle
 *       (-2 * M_PI <= angle <= 2 * M_PI must hold.)
 * "Bx" = Where ('1' <= x <= '8'). Up to 8 bits in 8 separate arguments to be
 *  packed into/out of a single byte.
 */

int packed_buffer_append(struct packed_buffer *pb, const char *format, ...)
{
	va_list ap;
	int rc = 0;

	va_start(ap, format);
	rc = packed_buffer_append_va(pb, format, ap);
	va_end(ap);
	return rc;
}

int calculate_buffer_size(const char *format)
{
	int i, size = 0;

	for (i = 0; format[i]; i++) {
		switch (format[i]) {
		case 'B':
			i++; /* Skip length that follows 'B' */
			size += 1;
			break;
		case 'b':
			size += 1;
			break;
		case 'h':
			size += 2;
			break;
		case 'w':
		case 'S':
		case 'U':
		case 'R':
			size += 4;
			break;
		case 'q':
			size += 8;
			break;
		case 'Q':
			size += 8;
			break;
		default:
			fprintf(stderr, "Bad format string '%s' (at %c)\n", format, format[i]);
			stacktrace("Bad format string");
			assert(0);
			return -1;
		}
	}
	return size;
}

struct packed_buffer *packed_buffer_new(const char *format, ...)
{
	va_list ap;
	struct packed_buffer *pb;
	int size = calculate_buffer_size(format);
	
	if (size < 0)
		return NULL;
	pb = packed_buffer_allocate(size);
	if (!pb)
		return NULL;
	va_start(ap, format);
	if (packed_buffer_append_va(pb, format, ap)) {
		packed_buffer_free(pb);
		return NULL;
	}
	va_end(ap);
	return pb;
}

int packed_buffer_extract_va(struct packed_buffer *pb, const char *format, va_list ap)
{
	uint8_t *b;
	uint8_t bits;
	uint16_t *h;
	uint32_t *w;
	uint64_t *q;
	int32_t uscale;
	uint32_t sscale;
	char *r;
	unsigned char *s;
	unsigned short len;
	int ilen;
	int i, n, rc = 0;
	double *d;
	float *quaternion;

	while (*format) {	
		switch(*format++) {
		case 'b':
			b = va_arg(ap, uint8_t *);
			*b = packed_buffer_extract_u8(pb);
			break;
		case 'h':
			h = va_arg(ap, uint16_t *);
			*h = packed_buffer_extract_u16(pb);
			break;
		case 'w':
			w = va_arg(ap, uint32_t *);
			*w = packed_buffer_extract_u32(pb);
			break;
		case 'q':
			q = va_arg(ap, uint64_t *);
			*q = packed_buffer_extract_u64(pb);
			break;
		case 's':
			s = va_arg(ap, unsigned char *);
			ilen = va_arg(ap, int);
			packed_buffer_extract_string(pb, (char *) s, ilen);
			break;
		case 'r':
			r = va_arg(ap, char *);
			len = va_arg(ap, int);
			packed_buffer_extract_raw(pb, r, len);
			break;
		case 'S':
			d = va_arg(ap, double *);
			sscale = va_arg(ap, int32_t); 
			*d = packed_buffer_extract_ds32(pb, sscale);
			check_double("packed_buffer_extract_va:'S'", *d);
			break;
		case 'R':
			d = va_arg(ap, double *);
			sscale = INT32_MAX / RADIANS_SCALE;
			*d = packed_buffer_extract_ds32(pb, sscale);
			check_double("packed_buffer_extract_va:'R'", *d);
			break;
		case 'U':
			d = va_arg(ap, double *);
			uscale = va_arg(ap, uint32_t); 
			*d = packed_buffer_extract_du32(pb, uscale);
			check_double("packed_buffer_extract_va:'U'", *d);
			break;
		case 'Q':
			quaternion = va_arg(ap, float *);
			packed_buffer_extract_quat(pb, quaternion);
			check_quaternion("packed_buffer_extract_va:'Q'", quaternion);
			break;
		case 'B':
			bits = packed_buffer_extract_u8(pb);
			n = *format++ - '0';
			if (n > 8 || n < 1) {
				fprintf(stderr, "Bad bitfield length in packed_buffer_extract_va(): '%c'\n", n);
				stacktrace("Bad bitfield length in packed_buffer_extract_va()");
				n = 1;
			}
			bits = bits << (8 - n);
			for (i = 0; i < n; i++) {
				b = va_arg(ap, uint8_t *);
				*b = (bits & 0x80) >> 7;
				bits = bits << 1;
			}
			break;
		default:
			rc = -EINVAL;
			break;
		}
	}
	return rc;
}

int packed_buffer_extract(struct packed_buffer *pb, const char *format, ...)
{
	va_list ap;
	int rc = 0;

	va_start(ap, format);
	rc = packed_buffer_extract_va(pb, format, ap);
	va_end(ap);
	return rc;
}

int packed_buffer_unpack(void * buffer, const char *format, ...)
{
	va_list ap;
	struct packed_buffer pb;
	int rc, size = calculate_buffer_size(format);

	packed_buffer_init(&pb, buffer, size);
	va_start(ap, format);
	rc = packed_buffer_extract_va(&pb, format, ap);
	va_end(ap);
	return rc;
}

int packed_buffer_unpack_raw(void *buffer, int size, const char *format, ...)
{
	va_list ap;
	struct packed_buffer pb;
	int rc;

	packed_buffer_init(&pb, buffer, size);
	va_start(ap, format);
	rc = packed_buffer_extract_va(&pb, format, ap);
	va_end(ap);
	return rc;
}

int packed_buffer_queue_length(struct packed_buffer_queue *pbq, pthread_mutex_t *mutex)
{
	int totalbytes;
	struct packed_buffer_queue_entry *i;

	/* Count total bytes in buffer queue */
	lockmutex(mutex);
	totalbytes = 0;
	for (i = pbq->head; i; i = i->next)
		totalbytes += i->buffer->buffer_cursor;
	unlockmutex(mutex);
	return totalbytes;
}

int packed_buffer_length(struct packed_buffer *pb)
{
	return pb->buffer_cursor;
}

#ifdef TEST_MARSHAL
static void test_bit_fields(void)
{
	int i, j;
	uint8_t b[8];
	struct packed_buffer *pb;
	int err_count = 0;

	for (i = 0; i < 8; i++) {
		memset(b, 0, sizeof(b));
		b[i] = '1';
		pb = packed_buffer_new("B8", b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);
		pb->buffer_cursor = 0;
		memset(b, 255, sizeof(b));
		packed_buffer_extract(pb, "B8", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5], &b[6], &b[7]);
		for (j = 0; j < 8; j++) {
			if (j == i) {
				if (b[j] == 1)
					continue;
				fprintf(stderr, "B8 bit field failure, i = %d, j = %d, expected 1, got %d\n",
						i, j, b[j]);
				err_count++;
			} else {
				if (b[j] == 0)
					continue;
				fprintf(stderr, "B8 bit field failure, i = %d, j = %d, expected 0, got %d\n",
						i, j, b[j]);
				err_count++;
			}
		}
		packed_buffer_free(pb);
	}

	/* Test some that don't use the entire byte. */
	for (i = 0; i < 7; i++) {
		memset(b, 0, sizeof(b));
		b[i] = '1';
		pb = packed_buffer_new("B7", b[0], b[1], b[2], b[3], b[4], b[5], b[6]);
		pb->buffer_cursor = 0;
		memset(b, 255, sizeof(b));
		packed_buffer_extract(pb, "B7", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5], &b[6]);
		for (j = 0; j < 7; j++) {
			if (j == i) {
				if (b[j] == 1)
					continue;
				fprintf(stderr, "B7 bit field failure, i = %d, j = %d, expected 1, got %d\n",
						i, j, b[j]);
				err_count++;
			} else {
				if (b[j] == 0)
					continue;
				fprintf(stderr, "B7 bit field failure, i = %d, j = %d, expected 0, got %d\n",
						i, j, b[j]);
				err_count++;
			}
		}
		packed_buffer_free(pb);
	}

	if (err_count == 0)
		printf("Bit fields passed all tests\n");
	else
		fprintf(stderr, "Bit fields failed %d tests\n", err_count);
}

int main(int argc, char *argv[])
{

	float x;
	int i, j;

	for (x = -1.0; x <= 1.0; x += 0.0001) {
		printf("Qtos32(%f) = %d\n", x, Qtos32(x));
		printf("s32toQ(%f) = %f\n", x, s32toQ(x));
		printf("s32toQ(Qtos32(%f) = %f\n", x, s32toQ(Qtos32(x)));
		printf("Qtos32(s32toQ(%f) = %d\n", x, Qtos32(s32toQ(x)));
		if (fabs(s32toQ(Qtos32(x)) - x) > 0.00001) {
			printf("FAIL!!!\n");
			return -1;
		}
	}

	double angle, outangle;
	int32_t min_diff, max_diff, diff;
	uint8_t b[8];
	max_diff = -100000;
	min_diff = 100000;
	uint32_t last_value = 0x7fffffff;
	double angle_increment = M_PI / 18000000.0;
	double error, max_error = 0;
	struct packed_buffer *pb;
	for (angle = -2.0 * M_PI; angle < 2.0 * M_PI; angle = angle + angle_increment) {
		uint32_t value;
		uint32_t scale = INT32_MAX / RADIANS_SCALE;
		value = (uint32_t) dtos32(angle, scale);
		if (last_value != 0x7fffffff) {
			diff = value - last_value;
			if (diff < 0)
				diff = -diff;
			if (diff > max_diff)
				max_diff = diff;
			if (diff < min_diff)
				min_diff = diff;
		}
		last_value = value;
		/* printf("%lf --> %u\n", angle * 180.0 / M_PI, value);  */

		/* Test radians */
		pb = packed_buffer_new("R", angle);
		pb->buffer_cursor = 0;
		packed_buffer_extract(pb, "R", &outangle);
		packed_buffer_free(pb);
		error = fabs(outangle - angle);
		if (error > max_error)
			max_error = error;

	}
	printf("For radians conversions, max_diff = %d / %g deg, min_diff = %d / %g deg\n",
		max_diff, angle_increment * 180.0 / M_PI, min_diff, angle_increment * 180.0 / M_PI);
	printf("Max radians error = %g\n", max_error);
	if (max_diff > 6 || min_diff < 5) { /* these limits determined empirically */
		printf("Something's wrong with the radians conversions.\n");
	}

	/* Test bit fields */
	test_bit_fields();
	return 0;
}
#endif
