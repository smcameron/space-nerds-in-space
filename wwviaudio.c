/*
	(C) Copyright 2007,2008, Stephen M. Cameron.

	This file is part of wordwarvi.

	wordwarvi is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	wordwarvi is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with wordwarvi; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

*/

#ifndef WWVIAUDIO_STUBS_ONLY

#include <stdio.h>
#include <inttypes.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#define WWVIAUDIO_DEFINE_GLOBALS
#include "wwviaudio.h"
#undef WWVIAUDIO_DEFINE_GLOBALS

#include "portaudio.h"
#include "ogg_to_pcm.h"

/* This must be one of 120, 240, 480, 960, 1920, and 2880, at least until
 * I write the code to resample 44100hz data to a rate opus supports */
#define FRAMES_PER_BUFFER  (1920)

static PaStream *stream = NULL;
static int audio_paused = 0;
static int music_playing = 1;
static int sound_working = 0;
static int nomusic = 0;
static int sound_effects_on = 1;
static int sound_device = -1; /* default sound device for port audio. */
static unsigned int max_concurrent_sounds = 0;
static int max_sound_clips = 0;
static int allocated_sound_clips = 0;
static int one_shot_busy = 0;
static pthread_mutex_t one_shot_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t recording_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t audio_chain_mutex[WWVIAUDIO_CHAIN_COUNT];
static int busy_recording = 0;
static unsigned int mixer_cycle_count = 0;

/* Pause all audio output, output silence. */
void wwviaudio_pause_audio(void)
{
	audio_paused = 1;
}

/* Resume playing audio previously paused. */
void wwviaudio_resume_audio(void)
{
	audio_paused = 0;
}

/* Silence the music channel */
void wwviaudio_silence_music(void)
{
	music_playing = 0;
}

/* Resume volume on the music channel. */
void wwviaudio_resume_music(void)
{
	music_playing = 1;
}

void wwviaudio_toggle_music(void)
{
	music_playing = !music_playing;
}

/* Silence the sound effects. */
void wwviaudio_silence_sound_effects(void)
{
	sound_effects_on = 0;
}

/* Resume volume on the sound effects. */
void wwviaudio_resume_sound_effects(void)
{
	sound_effects_on = 1;
}

void wwviaudio_toggle_sound_effects(void)
{
	sound_effects_on = !sound_effects_on;
}

void wwviaudio_set_nomusic(void)
{
	nomusic = 1;
}

static struct sound_clip {
	int active;
	int nsamples;
	int pos;
	int16_t *sample;
} *clip = NULL;

struct audio_queue_entry {
	int active;
	int nsamples;
	int pos;
	int16_t *sample;
	float volume;
	float delta_volume;
	void (*callback)(void *cookie);
	void *cookie;
	void *next;
};

static struct audio_queue_entry *audio_queue = NULL;
static struct audio_queue_entry *audio_chain_head[WWVIAUDIO_CHAIN_COUNT] = { 0 };
static struct audio_queue_entry *audio_chain_tail[WWVIAUDIO_CHAIN_COUNT] = { 0 };

int wwviaudio_read_ogg_clip_into_allocated_buffer(char *filename, int16_t **sample, int *nsamples)
{
	uint64_t nframes;
	char filebuf[PATH_MAX + 1];
	struct stat statbuf;
	int samplesize, sample_rate;
	int nchannels;
	int rc;

#ifndef __WIN32__
	snprintf(filebuf, PATH_MAX, "%s/%s", DATADIR, filename);
	rc = stat(filebuf, &statbuf);
	if (rc != 0) {
		strncpy(filebuf, filename, PATH_MAX);
		filebuf[PATH_MAX] = '\0';
		rc = stat(filebuf, &statbuf);
		if (rc != 0) {
			fprintf(stderr, "stat('%s') failed.\n", filebuf);
			return -1;
		}
	}
#else
	/* use relative path on win32 */
	snprintf(filebuf, PATH_MAX, "%s", filename);
#endif
/*
	printf("Reading sound file: '%s'\n", filebuf);
	printf("frames = %lld\n", sfinfo.frames);
	printf("samplerate = %d\n", sfinfo.samplerate);
	printf("channels = %d\n", sfinfo.channels);
	printf("format = %d\n", sfinfo.format);
	printf("sections = %d\n", sfinfo.sections);
	printf("seekable = %d\n", sfinfo.seekable);
*/
	rc = ogg_to_pcm(filebuf, sample, &samplesize,
		&sample_rate, &nchannels, &nframes);
	if (*sample == NULL) {
		printf("Can't get memory for sound data for %"PRIu64" frames in %s\n",
			nframes, filebuf);
		goto error;
	}

	if (rc != 0) {
		fprintf(stderr, "Error: ogg_to_pcm('%s') failed.\n",
			filebuf);
		goto error;
	}

	*nsamples = (int) nframes;
	if (*nsamples < 0)
		*nsamples = 0;

	return 0;
error:
	return -1;
}

static int wwviaudio_read_ogg_clip_internal(int clipnum, char *filename)
{
	if (clipnum >= allocated_sound_clips || clipnum < 0)
		return -1;

	if (clip[clipnum].sample != NULL) /* overwriting a previously read clip...? */
		free(clip[clipnum].sample);

	return wwviaudio_read_ogg_clip_into_allocated_buffer(filename, &clip[clipnum].sample,
					&clip[clipnum].nsamples);
}

int wwviaudio_read_ogg_clip(int clipnum, char *filename)
{
	if (clipnum >= max_sound_clips || clipnum < 0)
		return -1;
	return wwviaudio_read_ogg_clip_internal(clipnum, filename);
}

/* This routine will be called by the PortAudio engine when audio is needed.
** It may called at interrupt level on some machines so don't do anything
** that could mess up the system like calling malloc() or free().
*/
static int wwviaudio_mixer_loop(__attribute__ ((unused)) const void *inputBuffer,
	void *outputBuffer,
	unsigned long framesPerBuffer,
	__attribute__ ((unused)) const PaStreamCallbackTimeInfo* timeInfo,
	__attribute__ ((unused)) PaStreamCallbackFlags statusFlags,
	__attribute__ ((unused)) void *userData )
{
	unsigned int i, j;
	int sample;
	float *out = NULL;
	float output;
	out = (float*) outputBuffer;

	if (audio_paused) {
		/* output silence when paused and
		 * don't advance any sound slot pointers
		 */
		for (i = 0; i < framesPerBuffer; i++)
			*out++ = (float) 0;
		return 0;
	}

	mixer_cycle_count++;

	for (i = 0; i < framesPerBuffer; i++) {
		struct audio_queue_entry *q;
		output = 0.0;
		for (j = 0; j < max_concurrent_sounds; j++) {
			q = &audio_queue[j];
			if (!q->active || q->sample == NULL)
				continue;
			sample = i + q->pos;
			if (sample >= q->nsamples)
				continue;
			if (j != WWVIAUDIO_MUSIC_SLOT && sound_effects_on) {
				output += (float) q->sample[sample] * q->volume * 0.5 / (float) (INT16_MAX);
				q->volume += q->delta_volume;
			} else if (j == WWVIAUDIO_MUSIC_SLOT && music_playing) {
				output += (float) q->sample[sample] / (float) (INT16_MAX);
			}
		}
		/* Now mix in any audio chain... */
		for (j = 0; j < WWVIAUDIO_CHAIN_COUNT; j++) {
			pthread_mutex_lock(&audio_chain_mutex[j]);
			q = audio_chain_head[j];
			pthread_mutex_unlock(&audio_chain_mutex[j]);
			if (q && q->active && q->sample) {
				if (q && q->pos >= q->nsamples) {
					struct audio_queue_entry *oldq = q;
					pthread_mutex_lock(&audio_chain_mutex[j]);
					audio_chain_head[j] = q->next; /* Advance to next piece of the chain */
					q = audio_chain_head[j];
					pthread_mutex_unlock(&audio_chain_mutex[j]);
					if (oldq->callback)
						oldq->callback(oldq->cookie);
					free(oldq);
				}
				if (q) {
					output += (float) q->sample[q->pos] * q->volume / (float) (INT16_MAX);
					q->volume += q->delta_volume;
					q->pos++;
				}
			}
		}
		*out++ = (float) output;
        }
	for (i = 0; i < max_concurrent_sounds; i++) {
		struct audio_queue_entry *q = &audio_queue[i];
		if (!q->active)
			continue;
		q->pos += framesPerBuffer;
		if (q->pos >= q->nsamples) {
			q->active = 0;
			if (q->callback)
				q->callback(q->cookie);
		}
	}
	return 0; /* we're never finished */
}

static void decode_paerror(PaError rc)
{
	if (rc == paNoError)
		return;
	fprintf(stderr, "An error occurred while using the portaudio stream\n");
	fprintf(stderr, "Error number: %d\n", rc);
	fprintf(stderr, "Error message: %s\n", Pa_GetErrorText(rc));
}

static void wwviaudio_terminate_portaudio(PaError rc)
{
	Pa_Terminate();
	decode_paerror(rc);
}

int wwviaudio_initialize_portaudio(int maximum_concurrent_sounds, int maximum_sound_clips)
{
	PaStreamParameters outparams;
	PaError rc;
	PaDeviceIndex device_count;
	int i;

	if (maximum_concurrent_sounds < 0)
		return -1;

	for (i = 0; i < WWVIAUDIO_CHAIN_COUNT; i++)
		pthread_mutex_init(&audio_chain_mutex[i], NULL);

	max_concurrent_sounds = (unsigned int) maximum_concurrent_sounds;
	max_sound_clips = maximum_sound_clips;

	audio_queue = malloc(max_concurrent_sounds * sizeof(audio_queue[0]));
	clip = malloc((max_sound_clips + 1) * sizeof(clip[0])); /* +1 for the "one shot" sound */
	if (audio_queue == NULL || clip == NULL)
		return -1;

	allocated_sound_clips = max_sound_clips + 1;
	memset(audio_queue, 0, sizeof(audio_queue[0]) * max_concurrent_sounds);
	memset(clip, 0, sizeof(clip[0]) * allocated_sound_clips);

	rc = Pa_Initialize();
	if (rc != paNoError)
		goto error;

	device_count = Pa_GetDeviceCount();
	printf("Portaudio reports %d sound devices.\n", device_count);

	if (device_count == 0) {
		printf("There will be no audio.\n");
		goto error;
		rc = 0;
	}
	sound_working = 1;

	outparams.device = Pa_GetDefaultOutputDevice();  /* default output device */

	printf("Portaudio says the default device is: %d\n", outparams.device);

	if (sound_device != -1) {
		if (sound_device >= device_count)
			fprintf(stderr, "wordwarvi:  Invalid sound device "
				"%d specified, ignoring.\n", sound_device);
		else
			outparams.device = sound_device;
		printf("Using sound device %d\n", outparams.device);
	}
	sound_device = outparams.device;

	if (outparams.device < 0 && device_count > 0) {
		printf("Hmm, that's strange, portaudio says the default device is %d,\n"
			" but there are %d devices\n",
			outparams.device, device_count);
		printf("I think we'll just skip sound for now.\n");
		printf("You might try the '--sounddevice' option and see if that helps.\n");
		sound_working = 0;
		return -1;
	}

	outparams.channelCount = 1;                      /* mono output */
	outparams.sampleFormat = paFloat32;              /* 32 bit floating point output */
	outparams.suggestedLatency =
		Pa_GetDeviceInfo(outparams.device)->defaultLowOutputLatency;
	outparams.hostApiSpecificStreamInfo = NULL;

	rc = Pa_OpenStream(&stream,
		NULL,         /* no input */
		&outparams, WWVIAUDIO_SAMPLE_RATE, FRAMES_PER_BUFFER,
		paNoFlag, /* paClipOff, */   /* we won't output out of range samples so don't bother clipping them */
		wwviaudio_mixer_loop, NULL /* cookie */);
	if (rc != paNoError)
		goto error;
	if ((rc = Pa_StartStream(stream)) != paNoError)
		goto error;
	return rc;
error:
	wwviaudio_terminate_portaudio(rc);
	return rc;
}


void wwviaudio_stop_portaudio(void)
{
	int i, rc;
	
	if (!sound_working)
		return;
	if ((rc = Pa_StopStream(stream)) != paNoError)
		goto error;
	rc = Pa_CloseStream(stream);
error:
	wwviaudio_terminate_portaudio(rc);
	if (audio_queue) {
		free(audio_queue);
		audio_queue = NULL;
		max_concurrent_sounds = 0;
	}
	if (clip) {
		for (i = 0; i < allocated_sound_clips; i++) {
			if (clip[i].sample)
				free(clip[i].sample);
		}
		free(clip);
		clip = NULL;
		max_sound_clips = 0;
		allocated_sound_clips = 0;
	}
	return;
}

/* Begin playing a segment of a sound at "begin" until "end" at "volume" (all between 0.0 and 1.0).
 * Upon reaching "end", call "callback" with "cookie" as a parameter.
 */
static int wwviaudio_add_sound_segment_to_slot(int which_sound, int which_slot, float initial_volume,
		float target_volume, float begin, float end, void (*callback)(void *cookie), void *cookie)
{
	unsigned int i;
	int start, stop;

	if (!sound_working)
		return 0;

	if (nomusic && which_slot == WWVIAUDIO_MUSIC_SLOT)
		return 0;

	if (begin > 1.0)
		return 0;
	if (end < 0.0)
		return 0;
	if (end <= begin)
		return 0;
	if (begin < 0.0)
		begin = 0.0;
	if (end > 1.0)
		end = 1.0;

	if (which_slot != WWVIAUDIO_ANY_SLOT) {
		if (audio_queue[which_slot].active)
			audio_queue[which_slot].active = 0;
		audio_queue[which_slot].pos = 0;
		audio_queue[which_slot].nsamples = 0;
		/* would like to put a memory barrier here. */
		start = clip[which_sound].nsamples * begin;
		stop = clip[which_sound].nsamples * end;
		audio_queue[which_slot].pos = start;
		audio_queue[which_slot].sample = clip[which_sound].sample;
		audio_queue[which_slot].nsamples = stop;
		audio_queue[which_slot].callback = callback;
		audio_queue[which_slot].cookie = cookie;
		audio_queue[which_slot].volume = initial_volume;
		audio_queue[which_slot].delta_volume = (target_volume - initial_volume) / (stop - start);
		audio_queue[which_slot].next = NULL;
		/* would like to put a memory barrier here. */
		audio_queue[which_slot].active = 1;
		return which_slot;
	}
	for (i=1;i<max_concurrent_sounds;i++) {
		if (audio_queue[i].active == 0) {
			start = clip[which_sound].nsamples * begin;
			stop = clip[which_sound].nsamples * end;
			audio_queue[i].nsamples = stop;
			audio_queue[i].pos = start;
			audio_queue[i].sample = clip[which_sound].sample;
			audio_queue[i].callback = callback;
			audio_queue[i].cookie = cookie;
			audio_queue[i].volume = initial_volume;
			audio_queue[i].delta_volume = (target_volume - initial_volume) / (stop - start);
			audio_queue[i].next = NULL;
			audio_queue[i].active = 1;
			break;
		}
	}
	return (i >= max_concurrent_sounds) ? -1 : (int) i;
}

/* Begin playing a segment of a sound at "begin" until "end" at "volume" (all between 0.0 and 1.0).
 * Upon reaching "end", call "callback" with "cookie" as a parameter.
 */
int wwviaudio_add_sound_segment(int which_sound, float initial_volume, float target_volume,
		float begin, float end, void (*callback)(void *cookie), void *cookie)
{
	return wwviaudio_add_sound_segment_to_slot(which_sound, WWVIAUDIO_ANY_SLOT,
					initial_volume, target_volume, begin, end, callback, cookie);
}

static int wwviaudio_add_sound_to_slot(int which_sound, int which_slot)
{
	return wwviaudio_add_sound_segment_to_slot(which_sound, which_slot, 1.0, 1.0, 0.0, 1.0, NULL, NULL);
}

int wwviaudio_add_sound(int which_sound)
{
	return wwviaudio_add_sound_to_slot(which_sound, WWVIAUDIO_ANY_SLOT);
}

struct one_shot_pcm_cb_data {
	void (*callback)(void *);
	void *cookie;
};

static void one_shot_sound_cb(void *x)
{
	struct one_shot_pcm_cb_data *c = x;
	pthread_mutex_lock(&one_shot_mutex);
	one_shot_busy = 0;
	pthread_mutex_unlock(&one_shot_mutex);
	if (c) {
		if (c->callback)
			c->callback(c->cookie);
		free(c);
	}
}

int wwviaudio_add_one_shot_sound(char *filename)
{
	int rc;

	pthread_mutex_lock(&one_shot_mutex);
	if (one_shot_busy) {
		pthread_mutex_unlock(&one_shot_mutex);
		return -1;
	}
	one_shot_busy = 1;
	pthread_mutex_unlock(&one_shot_mutex);
	rc = wwviaudio_read_ogg_clip_internal(allocated_sound_clips - 1, filename);
	if (rc)
		return rc;
	return wwviaudio_add_sound_segment_to_slot(allocated_sound_clips - 1, WWVIAUDIO_ANY_SLOT,
				1.0, 1.0, 0.0, 1.0, one_shot_sound_cb, NULL);
}

int wwviaudio_add_one_shot_pcm_data(int16_t *samples, int nsamples,
				void (*callback)(void *), void *cookie)
{
	struct one_shot_pcm_cb_data *c;
	struct sound_clip *s;


	pthread_mutex_lock(&one_shot_mutex);
	if (one_shot_busy) {
		pthread_mutex_unlock(&one_shot_mutex);
		return -1;
	}
	one_shot_busy = 1;
	pthread_mutex_unlock(&one_shot_mutex);

	c = malloc(sizeof(*c));
	c->callback = callback;
	c->cookie = cookie;
	s = &clip[allocated_sound_clips - 1];
	if (s->sample)
		free(s->sample);
	s->pos = 0;
	s->nsamples = nsamples;
	s->sample = malloc(sizeof(int16_t) * nsamples);
	memcpy(s->sample, samples, sizeof(int16_t) * nsamples);
	s->active = 1;
	return wwviaudio_add_sound_segment_to_slot(allocated_sound_clips - 1, WWVIAUDIO_ANY_SLOT,
				1.0, 1.0, 0.0, 1.0, one_shot_sound_cb, c);
}

int wwviaudio_play_music(int which_sound)
{
	return wwviaudio_add_sound_to_slot(which_sound, WWVIAUDIO_MUSIC_SLOT);
}


void wwviaudio_add_sound_low_priority(int which_sound)
{

	/* adds a sound if there are at least 5 empty sound slots. */
	unsigned int i;
	int empty_slots = 0;
	int last_slot;

	if (!sound_working)
		return;
	last_slot = -1;
	for (i = 1; i < max_concurrent_sounds; i++)
		if (audio_queue[i].active == 0) {
			last_slot = i;
			empty_slots++;
			if (empty_slots >= 5)
				break;
	}

	if (empty_slots < 5)
		return;
	
	i = (unsigned int) last_slot;

	if (audio_queue[i].active == 0) {
		audio_queue[i].nsamples = clip[which_sound].nsamples;
		audio_queue[i].pos = 0;
		audio_queue[i].sample = clip[which_sound].sample;
		audio_queue[i].volume = 1.0;
		audio_queue[i].delta_volume = 0.0;
		audio_queue[i].callback = NULL;
		audio_queue[i].cookie = NULL;
		audio_queue[i].active = 1;
	}
	return;
}


void wwviaudio_cancel_sound(int queue_entry)
{
	if (!sound_working)
		return;
	audio_queue[queue_entry].active = 0;
}

void wwviaudio_cancel_music(void)
{
	wwviaudio_cancel_sound(WWVIAUDIO_MUSIC_SLOT);
}

void wwviaudio_cancel_all_sounds(void)
{
	unsigned int i;
	if (!sound_working)
		return;
	for (i = 0; i < max_concurrent_sounds; i++)
		audio_queue[i].active = 0;
}

int wwviaudio_set_sound_device(int device)
{
	sound_device = device;
	return 0;
}

int wwviaudio_get_sound_device(void)
{
	return sound_device;
}

void wwviaudio_list_devices(void)
{
	int i, device_count;

	device_count = Pa_GetDeviceCount();

	printf("Portaudio reports %d devices\n", device_count);
	for (i = 0; i < device_count; i++) {
		const struct PaDeviceInfo *devinfo = Pa_GetDeviceInfo(i);
		if (!devinfo) {
			printf("%d: No device info\n", i);
			continue;
		}
		printf("%d: %s\n", i, devinfo->name);
	}
}

static struct wwviaudio_record_data {
	PaStream *stream;
	void *cookie;
	void (*callback)(void *, int16_t *, int);
	int16_t *sample;
	int pos;
	int total_samples;
	int stop_recording;
} record_data;

static int wwviaudio_record_cb(const void *input_buffer,
		__attribute__((unused)) void *outputBuffer,
		unsigned long frames_per_buffer,
		const PaStreamCallbackTimeInfo *time_info,
		PaStreamCallbackFlags status_flags,
		void *dataptr)
{
	struct wwviaudio_record_data *d = dataptr;
	int left = d->total_samples - d->pos;
	int count = left < frames_per_buffer ?  left : frames_per_buffer;
	if (!input_buffer)
		memset(&d->sample[d->pos], 0, sizeof(int16_t) * count);
	else
		memcpy(&d->sample[d->pos], input_buffer, sizeof(int16_t) * count);
	d->pos += count;

	if (d->pos >= d->total_samples) {
		if (d->callback)
			d->callback(d->cookie, d->sample, d->total_samples);
		d->pos = 0; /* Start over */
		if (d->stop_recording)
			return paComplete;
	}
	return paContinue;
}

int wwviaudio_start_audio_capture(int16_t *buffer, int nsamples,
				void (*callback)(void *, int16_t *, int), void *cookie)
{
	PaStreamParameters params;
	PaError rc = paNoError;
	static PaDeviceIndex default_input = -1;

	pthread_mutex_lock(&recording_mutex);
	if (busy_recording) {
		pthread_mutex_unlock(&recording_mutex);
		return -1;
	}
	busy_recording = 1;
	pthread_mutex_unlock(&recording_mutex);

	if (default_input == -1) {
		default_input = Pa_GetDefaultInputDevice();
		if (default_input == paNoDevice) {
			fprintf(stderr, "No default input audio device\n");
			busy_recording = 0;
			pthread_mutex_unlock(&recording_mutex);
			return -1;
		}
	}
	params.device = default_input;
	params.channelCount = 1;
	params.sampleFormat = paInt16;
	params.suggestedLatency = Pa_GetDeviceInfo(params.device)->defaultLowInputLatency;
	params.hostApiSpecificStreamInfo = NULL;

	record_data.sample = buffer;
	record_data.pos = 0;
	record_data.total_samples = nsamples;
	record_data.cookie = cookie;
	record_data.callback = callback;
	record_data.stop_recording = 0;
	record_data.stream = NULL;

	rc = Pa_OpenStream(&record_data.stream, &params, NULL, WWVIAUDIO_SAMPLE_RATE, FRAMES_PER_BUFFER, paClipOff,
				wwviaudio_record_cb, &record_data);
	if (rc != paNoError)
		goto error;
	rc = Pa_StartStream(record_data.stream);
	if (rc != paNoError)
		goto error;
	return 0;
error:
	return -1;
}

int wwviaudio_stop_audio_capture()
{
	PaError rc = paNoError;

	pthread_mutex_lock(&recording_mutex);
	busy_recording = 0;
	pthread_mutex_unlock(&recording_mutex);
	record_data.stop_recording = 1;
	if (!record_data.stream)
		return 0;
	while ((rc = Pa_IsStreamActive(record_data.stream)) == 1) {
		Pa_Sleep(1);
	}
	if (rc < 0)
		goto error;
	rc = Pa_CloseStream(record_data.stream);
	if (rc != paNoError)
		goto error;
	return 0;
error:
	return -1;
}

unsigned int wwviaudio_get_mixer_cycle_count(void) {
	return mixer_cycle_count;
}

void wwviaudio_append_to_audio_chain(int16_t *samples, int nsamples, int chain, void (*callback)(void *), void *cookie)
{
	struct audio_queue_entry *entry = malloc(sizeof(*entry));

	if (chain < 0 || chain >= WWVIAUDIO_CHAIN_COUNT)
		return;

	entry->active = 1;
	entry->pos = 0;
	entry->nsamples = nsamples;
	entry->pos = 0;
	entry->sample = samples;
	entry->nsamples = nsamples;
	entry->callback = callback;
	entry->cookie = cookie;
	entry->volume = 1.0;
	entry->delta_volume = 0.0;
	entry->next = NULL;

	pthread_mutex_lock(&audio_chain_mutex[chain]);
	if (!audio_chain_head[chain]) {
		audio_chain_head[chain] = entry;
		audio_chain_tail[chain] = entry;
		pthread_mutex_unlock(&audio_chain_mutex[chain]);
		return;
	}
	audio_chain_tail[chain]->next = entry;
	audio_chain_tail[chain] = entry;
	pthread_mutex_unlock(&audio_chain_mutex[chain]);
}

#else /* stubs only... */

int wwviaudio_initialize_portaudio() { return 0; }
void wwviaudio_stop_portaudio() { return; }
void wwviaudio_set_nomusic() { return; }
int wwviaudio_read_ogg_clip(int clipnum, char *filename) { return 0; }

void wwviaudio_pause_audio() { return; }
void wwviaudio_resume_audio() { return; }

void wwviaudio_silence_music() { return; }
void wwviaudio_resume_music() { return; }
void wwviaudio_silence_sound_effects() { return; }
void wwviaudio_resume_sound_effects() { return; }
void wwviaudio_toggle_sound_effects() { return; }

int wwviaudio_play_music(int which_sound) { return 0; }
void wwviaudio_cancel_music() { return; }
void wwviaudio_toggle_music() { return; }
int wwviaudio_add_sound(int which_sound) { return 0; }
void wwviaudio_add_sound_low_priority(int which_sound) { return; }
void wwviaudio_cancel_sound(int queue_entry) { return; }
void wwviaudio_cancel_all_sounds() { return; }
int wwviaudio_set_sound_device(int device) { return 0; }
void wwviaudio_list_devices(void) {}
void wwviaudio_append_to_audio_chain(int16_t *samples, int nsamples) {}
int wwviaudio_get_mixer_cycle_count(void) { return 0; }

#endif

#ifdef WWVIAUDIO_BASIC_TEST

#include <stdio.h>
#include <stdlib.h>

struct cb_data {
	int16_t *audio;
	int position;
	int nsamples;
};

void my_callback(void *cookie)
{
	int count;
	struct cb_data *d = cookie;

	if (d->position >= d->nsamples)
		return;
	count = 4096;
	if (count > d->nsamples - d->position)
		count = d->nsamples - d->position;
	wwviaudio_add_one_shot_pcm_data(&d->audio[d->position], count, my_callback, cookie);
	d->position += count;
}

int main(int argc, char *argv[])
{
	int16_t *audio;
	int rc, nsamples;
	struct cb_data c;

	if (argc < 2) {
		fprintf(stderr, "usage: wwviaudio_basic_test file.ogg\n");
		exit(1);
	}
	printf("wwviaudio basic test, reading ogg file %s\n", argv[1]);
	rc = wwviaudio_initialize_portaudio(10, 10);
	rc = wwviaudio_read_ogg_clip_into_allocated_buffer(argv[1], &audio, &nsamples);
	if (rc != 0) {
		wwviaudio_stop_portaudio();
		return rc;
	}
	c.audio = audio;
	c.nsamples = nsamples;
	c.position = 0;
	my_callback(&c);

	sleep(10);
	wwviaudio_stop_portaudio();
	return rc;
}
#endif

#ifdef WWVIAUDIO_RECORDING_TEST

#include <stdio.h>
#include <stdlib.h>

int16_t audio2[441000];
int nsamples2;

void recording_callback(void *cookie, int16_t *buffer, int nsamples)
{
	printf("recording callback, nsamples = %d\n", nsamples);
	memcpy(audio2, buffer, nsamples);
	nsamples2 = nsamples;
	printf("Saved recorded audio to buffer n = %d\n", nsamples);
}

int main(int argc, char *argv[])
{
	int16_t audio[441000];
	double average;
	int16_t max, val;
	int i, rc;

	printf("wwviaudio recording test\n");
	rc = wwviaudio_initialize_portaudio(10, 10);
	if (rc != 0) {
		wwviaudio_stop_portaudio();
		return rc;
	}
	memset(audio2, 0, sizeof(audio2));
	nsamples2 = 0;
	printf("Recording 10 secs of audio...\n");
	rc = wwviaudio_start_audio_capture(audio, 441000, recording_callback, NULL);
	printf("Recording started, sleeping, rc = .%d\n", rc);
	sleep(11);

	max = 0;
	average = 0.0;
	for (i = 0; i < nsamples2; i++) {
		val = audio2[i];
		if (val < 0)
			val = -val;
		if (val > max)
			max = val;
		average += val;
	}
	average = average / (double) nsamples2;
	printf("max = %d, average = %f, nsamples2 = %d\n", max, average, nsamples2);

	rc = wwviaudio_add_one_shot_pcm_data(audio2, nsamples2, NULL, NULL);
	printf("Playing back recorded audio (nsamples2=%d) rc = %d\n", nsamples2, rc);
	sleep(11);
	printf("Finished playing back recorded audio, rc = %d.\n", rc);
	wwviaudio_stop_portaudio();
	return rc;
}
#endif
