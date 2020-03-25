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

#define FRAMES_PER_BUFFER  (1024)

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
};

static struct audio_queue_entry *audio_queue = NULL;

static int wwviaudio_read_ogg_clip_internal(int clipnum, char *filename)
{
	uint64_t nframes;
	char filebuf[PATH_MAX + 1];
	struct stat statbuf;
	int samplesize, sample_rate;
	int nchannels;
	int rc;

	if (clipnum >= allocated_sound_clips || clipnum < 0)
		return -1;

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
	if (clip[clipnum].sample != NULL)
		/* overwriting a previously read clip... */
		free(clip[clipnum].sample);

	rc = ogg_to_pcm(filebuf, &clip[clipnum].sample, &samplesize,
		&sample_rate, &nchannels, &nframes);
	if (clip[clipnum].sample == NULL) {
		printf("Can't get memory for sound data for %"PRIu64" frames in %s\n",
			nframes, filebuf);
		goto error;
	}

	if (rc != 0) {
		fprintf(stderr, "Error: ogg_to_pcm('%s') failed.\n",
			filebuf);
		goto error;
	}

	clip[clipnum].nsamples = (int) nframes;
	if (clip[clipnum].nsamples < 0)
		clip[clipnum].nsamples = 0;

	return 0;
error:
	return -1;
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
	int sample, count;
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

	for (i = 0; i < framesPerBuffer; i++) {
		output = 0.0;
		count = 0;
		for (j = 0; j < max_concurrent_sounds; j++) {
			struct audio_queue_entry *q = &audio_queue[j];
			if (!q->active || q->sample == NULL)
				continue;
			sample = i + q->pos;
			count++;
			if (sample >= q->nsamples)
				continue;
			if (j != WWVIAUDIO_MUSIC_SLOT && sound_effects_on) {
				output += (float) q->sample[sample] * q->volume * 0.5 / (float) (INT16_MAX);
				q->volume += q->delta_volume;
			} else if (j == WWVIAUDIO_MUSIC_SLOT && music_playing) {
				output += (float) q->sample[sample] / (float) (INT16_MAX);
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

	if (maximum_concurrent_sounds < 0)
		return -1;

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

	c = malloc(sizeof(*c));
	c->callback = callback;
	c->cookie = cookie;
	one_shot_busy = 1;
	s = &clip[allocated_sound_clips - 1];
	if (s->sample)
		free(s->sample);
	s->pos = 0;
	s->nsamples = nsamples;
	s->sample = malloc(sizeof(int16_t) * nsamples);
	memcpy(s->sample, samples, sizeof(int16_t) * nsamples);
	s->active = 1;
	pthread_mutex_unlock(&one_shot_mutex);
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

#endif
