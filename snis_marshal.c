#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>

#define DEFINE_SNIS_MARSHAL_GLOBALS
#include "snis_marshal.h"
#undef DEFINE_SNIS_MARSHAL_GLOBALS

#define MAX_FRACTIONAL 0x7FFFFFFFFFFFFFFFLL /* 2^63 - 1 */

static void pack_double(double value, struct packed_double *pd)
{
	double fractional = fabs(frexp(value, &pd->exponent)) - 0.5;

	if (fractional < 0.0) {
		pd->fractional = 0;
		return;
	}

	pd->fractional = 1 + (int64_t) (fractional * 2.0 * (MAX_FRACTIONAL - 1));
	if (value < 0.0)
		pd->fractional = -pd->fractional;
}

static double unpack_double(const struct packed_double *pd)
{
	double fractional, value;

	if (pd->fractional == 0)
		return 0.0;

	fractional = ((double) (llabs(pd->fractional) - 1) / (MAX_FRACTIONAL - 1)) / 2.0;
	value = ldexp(fractional + 0.5, pd->exponent);
	if (pd->fractional < 0)
		return -value;
	return value;
}

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

/* change a packed double to network byte order. */
static inline void packed_double_nbo(struct packed_double *pd)
{
	if (!host_is_little_endian())
		return;
	unsigned char *a, *b;
	a = (unsigned char *) &pd->exponent;
	b = (unsigned char *) &pd->exponent;
	a[0] = b[3];
	a[1] = b[2];
	a[2] = b[1];
	a[3] = b[0];
	a = (unsigned char *) &pd->fractional;
	b = (unsigned char *) &pd->fractional;
	a[0] = b[7];
	a[1] = b[6];
	a[2] = b[5];
	a[3] = b[4];
	a[4] = b[3];
	a[5] = b[2];
	a[6] = b[1];
	a[7] = b[0];
}

/* Change a packed double to host byte order. */
static inline void packed_double_to_host(struct packed_double *pd)
{
	packed_double_nbo(pd);
}

int packed_buffer_append_double(struct packed_buffer *pb, double value)
{
	struct packed_double pd;
	if (pb->buffer_cursor + sizeof(pd) > pb->buffer_size)
		return -1; /* no room */
	pack_double(value, &pd);
	packed_double_nbo(&pd);
	memcpy(&pb->buffer[pb->buffer_cursor], &pd, sizeof(pd));
	pb->buffer_cursor += sizeof(pd);
	return 0;
}

double packed_buffer_extract_double(struct packed_buffer *pb)
{
	struct packed_double pd;

	memcpy(&pb->buffer[pb->buffer_cursor], &pd, sizeof(pd));
	pb->buffer_cursor += sizeof(pd);
	packed_double_to_host(&pd);
	return unpack_double(&pd);	
}

int packed_buffer_append_u16(struct packed_buffer *pb, uint16_t value)
{
	value = htons(value);
	memcpy(&pb->buffer[pb->buffer_cursor], &value, sizeof(value));
	pb->buffer_cursor += sizeof(value);
	return 0;
}

int packed_buffer_append_u8(struct packed_buffer *pb, uint8_t value)
{
	uint8_t *c = (uint8_t *) &pb->buffer[pb->buffer_cursor];
	*c = value;
	pb->buffer_cursor += 1;
	return 0;
}

int packed_buffer_append_u32(struct packed_buffer *pb, uint32_t value)
{
	value = htonl(value);
	memcpy(&pb->buffer[pb->buffer_cursor], &value, sizeof(value));
	pb->buffer_cursor += sizeof(value);
	return 0;
}

uint16_t packed_buffer_extract_u16(struct packed_buffer *pb)
{
	uint16_t value;

	memcpy(&value, &pb->buffer[pb->buffer_cursor], sizeof(value));
	pb->buffer_cursor += sizeof(value);
	value = ntohs(value);
	return value;
}

uint8_t packed_buffer_extract_u8(struct packed_buffer *pb)
{
	uint8_t *c = (uint8_t *) &pb->buffer[pb->buffer_cursor];
	pb->buffer_cursor += 1;
	return *c;
}

uint32_t packed_buffer_extract_u32(struct packed_buffer *pb)
{
	uint32_t value;

	memcpy(&value, &pb->buffer[pb->buffer_cursor], sizeof(value));
	pb->buffer_cursor += sizeof(value);
	value = ntohl(value);
	return value;
}

int packed_buffer_append_string(struct packed_buffer *pb, unsigned char *str, unsigned short len)
{
	unsigned short belen;
	if (pb->buffer_size < pb->buffer_cursor + len + sizeof(unsigned short))
		return -1; /* not enough room */
	belen = htons(len);	
	memcpy(&pb->buffer[pb->buffer_cursor], &belen, sizeof(belen));
	pb->buffer_cursor += sizeof(belen);
	memcpy(&pb->buffer[pb->buffer_cursor], &str, len);
	pb->buffer_cursor += len;
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
	memcpy(buffer, &pb->buffer[pb->buffer_cursor], len > buflen ? buflen : len); 
	pb->buffer_cursor += len;	
	return len > buflen ? buflen : len;
}

int packed_buffer_append_raw(struct packed_buffer *pb, char *buffer, unsigned short len)
{
	memcpy(&pb->buffer[pb->buffer_cursor], buffer, len);
	pb->buffer_cursor += len;
	return 0;
}

int packed_buffer_extract_raw(struct packed_buffer *pb, char *buffer, unsigned short len)
{
	memcpy(buffer, &pb->buffer[pb->buffer_cursor], len);
	pb->buffer_cursor += len;
	return 0;
}

struct packed_buffer * packed_buffer_allocate(int size)
{
	struct packed_buffer *pb;
	pb = malloc(sizeof(*pb));
	memset(pb, 0, sizeof(*pb));	
	if (size != 0) {
		pb->buffer = malloc(size);
		memset(pb->buffer, 0, size);
		pb->buffer_size = size;
	} else
		pb->buffer = NULL;
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

	answer = malloc(sizeof(*answer));

	/* Count total bytes in buffer queue */
	lockmutex(mutex);
	totalbytes = 0;
	for (i = pbq->head; i; i = i->next)
		totalbytes += i->buffer->buffer_cursor;

	if (totalbytes == 0) {
		unlockmutex(mutex);
		answer->buffer = NULL;
		answer->buffer_size = 0;
		answer->buffer_cursor = 0;
		return answer;	
	}

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

struct packed_buffer *packed_buffer_copy(struct packed_buffer *pb)
{
	struct packed_buffer *pbc;

	pbc = packed_buffer_allocate(pb->buffer_size);
	memcpy(pbc->buffer, pb->buffer, pb->buffer_size);
	pbc->buffer_cursor = pb->buffer_cursor;
	return pbc;
}
