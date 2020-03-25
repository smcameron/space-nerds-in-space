/*
	Copyright (C) 2020 Stephen M. Cameron
	Author: Stephen M. Cameron

	This file is part of Spacenerds In Space.

	Spacenerds in Space is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Spacenerds in Space is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Spacenerds in Space; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#ifdef WITHVOICECHAT
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <string.h>
#include <arpa/inet.h>

#include "opus.h"
#include "pthread_util.h"
#include "snis_voice_chat.h"
#include "wwviaudio.h"
#define SNIS_CLIENT_DATA
#include "snis.h"
#include "snis_packet.h"
#include "snis_marshal.h"

#define VC_BUFFER_SIZE 1920 /* This has to equal FRAMES_PER_BUFFER in wwviaudio.c */
#define SAMPLE_RATE 48000
#define CHANNELS 1
#define OPUS_PACKET_SIZE 4000
#define SWAP_BYTES 0

static pthread_t voice_chat_encoding_thread;
static pthread_t voice_chat_decoding_thread;
static int recording_audio = 0;

static struct audio_buffer {
	int16_t audio_buffer[VC_BUFFER_SIZE];
	uint8_t opus_buffer[OPUS_PACKET_SIZE];
	int nsamples;
	int nopus_bytes;
	struct audio_buffer *next, *prev;
	uint32_t snis_radio_channel;
	uint8_t destination; /* VOICE_CHAT_DESTINATION_CREW, _ALL, or _CHANNEL */
} recording_buffer;

struct audio_queue {
	struct audio_buffer *head;
	struct audio_buffer *tail;
	pthread_mutex_t mutex;
	pthread_mutex_t event_mutex;
	pthread_cond_t event_cond;
	int time_to_stop;
} incoming, outgoing;

static void audio_queue_init(struct audio_queue *q)
{
	q->head = NULL;
	q->tail = NULL;
	pthread_mutex_init(&q->mutex, NULL);
	pthread_cond_init(&q->event_cond, NULL);
	q->time_to_stop = 0;
}

/* Must hold q->mutex */
static void enqueue_audio_buffer(struct audio_queue *q, struct audio_buffer *b)
{
	b->prev = NULL;

	if (!q->head) {
		b->next = NULL;
		q->head = b;
		q->tail = b;
		return;
	}
	b->next = q->head;
	q->head->prev = b;
	q->head = b;
	pthread_cond_signal(&q->event_cond);
}

/* Must hold q->mutex */
static void enqueue_audio_data(struct audio_queue *q,
		int16_t *buffer, int nsamples, uint8_t destination, uint32_t channel)
{
	struct audio_buffer *b = calloc(1, sizeof(*b));
	memcpy(b->audio_buffer, buffer, sizeof(b->audio_buffer[0]) * nsamples);
	b->nsamples = nsamples;
	b->destination = destination;
	b->snis_radio_channel = channel;
	enqueue_audio_buffer(q, b);
}

/* Must hold q->mutex */
static void enqueue_opus_audio(struct audio_queue *q, uint8_t *opus_buffer, int nopus_bytes)
{
	struct audio_buffer *b = calloc(1, sizeof(*b));
	memcpy(b->opus_buffer, opus_buffer, sizeof(b->opus_buffer[0]) * nopus_bytes);
	b->nopus_bytes = nopus_bytes;
	enqueue_audio_buffer(q, b);
}

/* Must hold q->mutex */
static struct audio_buffer *dequeue_audio_buffer(struct audio_queue *q)
{
	struct audio_buffer *b;

	if (!q->tail) {
		return NULL;
	}
	b = q->tail;
	if (!b->prev) {
		q->head = NULL;
		q->tail = NULL;
		return b;
	}
	q->tail = b->prev;
	q->tail->next = NULL;
	b->prev = NULL;
	return b;
}

static void free_audio_buffer(void *x)
{
	struct audio_buffer *b = x;
	if (b)
		free(b);
}

static void transmit_opus_packet_to_server(uint8_t *opus_packet, int packet_len,
						uint8_t destination, uint32_t snis_radio_channel)
{
	struct packed_buffer *pb;
	uint16_t len = (uint16_t) packet_len;

	pb = packed_buffer_allocate(8 + packet_len);
	packed_buffer_append(pb, "bbwhr", OPCODE_OPUS_AUDIO_DATA,
				destination, snis_radio_channel, len, opus_packet, len);
	queue_to_server(pb);
}

static void *voice_chat_encode_thread_fn(void *arg)
{
	struct audio_queue *q = arg;
	struct audio_buffer *b;
	OpusEncoder *encoder;
	int len, rc;

	/* Set up Opus encoder */
	encoder = opus_encoder_create(SAMPLE_RATE, CHANNELS, OPUS_APPLICATION_VOIP, &rc);
	if (rc < 0) {
		fprintf(stderr, "Failed to create Opus encoder: %s\n", opus_strerror(rc));
		return NULL;
	}

	while (1) {

		/* Get an audio buffer from the queue */
		pthread_mutex_lock(&q->mutex);
		b = dequeue_audio_buffer(q);
		if (!b) {
			rc = pthread_cond_wait(&q->event_cond, &q->mutex);
			if (q->time_to_stop) {
				pthread_mutex_unlock(&q->mutex);
				goto quit;
			}
			pthread_mutex_unlock(&q->mutex);
			if (rc != 0)
				fprintf(stderr, "pthread_cond_wait failed %s:%d.\n", __FILE__, __LINE__);
			continue;
		}
		pthread_mutex_unlock(&q->mutex);
#if SWAP_BYTES
		/* TODO: supporting a mixture of little/big endian clients and servers is not
		 * something I am willing to do without machines to test it on.
		 * It's hard enough to get it right with only intel stuff.
		 */
		/* Convert to network byte order (is this right?) */
		for (int i = 0; i < VC_BUFFER_SIZE; i++)
			b->audio_buffer[i] = htons(b->audio_buffer[i]);
#endif
		/* Encode audio buffer */
		len = opus_encode(encoder, b->audio_buffer, VC_BUFFER_SIZE, b->opus_buffer, OPUS_PACKET_SIZE);
		if (len < 0) { /* Error */
			fprintf(stderr, "opus_encode failed: %s\n", opus_strerror(len));
			goto quit;
		}

		/* Transmit audio buffer to server */
		transmit_opus_packet_to_server(b->opus_buffer, len, b->destination, b->snis_radio_channel);
		free(b);
	}
quit:
	if (encoder)
		opus_encoder_destroy(encoder);
	return NULL;
}

static void *voice_chat_decode_thread_fn(void *arg)
{
	struct audio_queue *q = arg;
	struct audio_buffer *b;
	int len, rc;
	OpusDecoder *opus_decoder;

	/* Set up opus decoder */
	opus_decoder = opus_decoder_create(SAMPLE_RATE, CHANNELS, &rc);
	if (rc < 0) {
		fprintf(stderr, "Failed to create Opus decoder: %s\n", opus_strerror(rc));
		return NULL;
	}

	while (1) {

		/* Get an audio buffer from the queue */
		pthread_mutex_lock(&q->mutex);
		b = dequeue_audio_buffer(q);
		if (!b) {
			rc = pthread_cond_wait(&q->event_cond, &q->mutex);
			if (q->time_to_stop) {
				pthread_mutex_unlock(&q->mutex);
				goto quit;
			}
			pthread_mutex_unlock(&q->mutex);
			if (rc != 0)
				fprintf(stderr, "pthread_cond_wait failed %s:%d.\n", __FILE__, __LINE__);
			continue;
		}
		pthread_mutex_unlock(&q->mutex);

		/* decode audio buffer */
		len = opus_decode(opus_decoder, b->opus_buffer, b->nopus_bytes, b->audio_buffer, VC_BUFFER_SIZE, 0);
		if (len < 0) {
			fprintf(stderr, "opus_decode failed\n");
			goto quit;
		}
#if SWAP_BYTES
		/* TODO: supporting a mixture of little/big endian clients and servers is not
		 * something I am willing to do without machines to test it on.
		 * It's hard enough to get it right with only intel stuff.
		 */
		/* Convert to host byte order (is this right?) */
		for (int i = 0; i < len; i++)
			b->audio_buffer[i] = ntohs(b->audio_buffer[i]);
#endif
		wwviaudio_append_to_audio_chain(b->audio_buffer, len, free_audio_buffer, b);
	}
quit:
	if (opus_decoder)
		opus_decoder_destroy(opus_decoder);
	return NULL;
}

void voice_chat_setup_threads(void)
{
	int rc;

	printf("voice_chat: setup threads\n");
	audio_queue_init(&incoming);
	audio_queue_init(&outgoing);

	rc = create_thread(&voice_chat_encoding_thread, voice_chat_encode_thread_fn, &outgoing, "snisc-vce", 0);
	if (rc)
		fprintf(stderr, "Failed to create voice chat encoding thread.\n");
	rc = create_thread(&voice_chat_decoding_thread, voice_chat_decode_thread_fn, &incoming, "snisc-vcd", 0);
	if (rc)
		fprintf(stderr, "Failed to create voice chat playing thread.\n");
}

static void voice_chat_stop_thread(struct audio_queue *q)
{
	pthread_mutex_lock(&q->mutex);
	q->time_to_stop = 1;
	pthread_cond_signal(&q->event_cond);
	pthread_mutex_unlock(&q->mutex);
}

void voice_chat_stop_threads(void)
{
	voice_chat_stop_thread(&incoming);
	voice_chat_stop_thread(&outgoing);
}

static void recording_callback(void *cookie, int16_t *buffer, int nsamples)
{
	if (nsamples != VC_BUFFER_SIZE)
		return;
	recording_buffer.nsamples = nsamples;
	pthread_mutex_lock(&outgoing.mutex);
	enqueue_audio_data(&outgoing, recording_buffer.audio_buffer, recording_buffer.nsamples,
			recording_buffer.destination, recording_buffer.snis_radio_channel);
	pthread_mutex_unlock(&outgoing.mutex);
}

void voice_chat_start_recording(uint8_t destination, uint32_t channel)
{
	if (recording_audio)
		return;
	recording_audio = 1;
	recording_buffer.destination = destination;
	recording_buffer.snis_radio_channel = channel;
	wwviaudio_start_audio_capture(recording_buffer.audio_buffer, VC_BUFFER_SIZE, recording_callback, NULL);
}

void voice_chat_stop_recording(void)
{
	recording_audio = 0;
	wwviaudio_stop_audio_capture();
}

void voice_chat_play_opus_packet(uint8_t *opus_buffer, int buflen)
{
	if (buflen > VC_BUFFER_SIZE)
		buflen = VC_BUFFER_SIZE;
	pthread_mutex_lock(&incoming.mutex);
	enqueue_opus_audio(&incoming, opus_buffer, buflen);
	pthread_mutex_unlock(&incoming.mutex);
}

#else
#include <stdio.h>
#include <stdint.h>
void voice_chat_setup_threads(void)
{
}

void voice_chat_start_recording(void)
{
}

void voice_chat_stop_recording(void)
{
}

void voice_chat_stop_threads(void)
{
}

void voice_chat_play_opus_packet(uint8_t *opus_buffer, int buflen)
{
}
#endif

