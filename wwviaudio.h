#ifndef _WWVIAUDIO_H_
#define _WWVIAUDIO_H_
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

#ifdef WWVIAUDIO_DEFINE_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

#define WWVIAUDIO_MUSIC_SLOT (0)
#define WWVIAUDIO_SAMPLE_RATE   (48000)
#define WWVIAUDIO_ANY_SLOT (-1)
#define WWVIAUDIO_CHAIN_COUNT 4

/*
 *             Configuration functions.
 */
/* Disables the music channel.  Meant to be called prior to
 * wwviaudio_initialize_portaudio
 */
GLOBAL void wwviaudio_set_nomusic(void);

/* Set the audio device number to the given value
 * This is meant to be called prior to calling
 * wwviaudio_initialize_portaudio.  If you don't use
 * this function, portaudio's default device will be
 * used.  This function provides a way to use a
 * device other than the default
 */
GLOBAL int wwviaudio_set_sound_device(int device);

/* Returns the audio device number as set when
 * wwviaudio_initialize_portaudio() (see below) ran.
 * This is meant to be called after calling
 * wwviaudio_initialize_portaudio().
 */
GLOBAL int wwviaudio_get_sound_device(void);

/* Initialize portaudio and start the audio engine.
 * Space will be allocated to allow for the specified
 * number of concurrently playing sounds.  The 2nd parameter
 * indicates how many different sound clips are to be held
 * in memory at once.  e.g. if your app has five different
 * sounds that it plays, then this would be 5.
 * 0 is returned on success, -1 otherwise.
 */
GLOBAL int wwviaudio_initialize_portaudio(int maximum_concurrent_sounds,
	int maximum_sound_clips);

/* Stop portaudio and the audio engine. Space allocated
 * during initialization is freed.
 */
GLOBAL void wwviaudio_stop_portaudio(void);

/*
 *             Audio data functions
 */

/* Read and decode an ogg vorbis audio file into a newly allocated buffer
 * 0 is returned on success, -1 otherwise.
 * Audio files should be 44100Hz, MONO.
 * sample[] will contain the data, *nsamples the number of samples in sample[].
 */
GLOBAL int wwviaudio_read_ogg_clip_into_allocated_buffer(char *filename, int16_t **sample, int *nsamples);

/* Read and decode an ogg vorbis audio file into a numbered buffer
 * The sound_number parameter is used later with wwviaudio_play_music and
 * wwviaudio_add_sound.  0 is returned on success, -1 otherwise.
 * Audio files should be 44100Hz, MONO.  The sound number is one you
 * provide which will then be associated with that sound.
 */
GLOBAL int wwviaudio_read_ogg_clip(int sound_number, char *filename);

/*
 *             Global sound control functions.
 */

/* Suspend all audio playback.  Silence is output. */
GLOBAL void wwviaudio_pause_audio(void);

/* Resume all previously playing audio from whence it was previusly paused. */
GLOBAL void wwviaudio_resume_audio(void);

/*
 *             Music channel related functions
 */

/* Begin playing a numbered buffer into the mix on the music channel
 * The channel number of the music channel is returned.
 */
GLOBAL int wwviaudio_play_music(int sound_number);

/* Output silence on the music channel (pointer still advances though.) */
GLOBAL void wwviaudio_silence_music(void);

/* Unsilence the music channel */
GLOBAL void wwviaudio_resume_music(void);

/* Silence or unsilence the music channe. */
GLOBAL void wwviaudio_toggle_music(void);

/* Stop playing the playing buffer from the music channel */
GLOBAL void wwviaudio_cancel_music(void);

/*
 *             Sound effect (not music) related functions
 */

/* Begin playing a sound on a non-music channel.  The channel is returned.
 * sound_number refers to a sound previously associated with the number by
 * wwviaudio_read_ogg_clip()
 */
GLOBAL /* channel */ int wwviaudio_add_sound(int sound_number);

/* Read an ogg file, play it once. Only one one-shot sound can be played
 * concurrently. Returns 0 on success, -1 on failure (e.g. file does not
 * exist) or -2 if the one shot slot is busy (currently playing another
 * one-shot sound.)  The memory allocated for the one-shot sound will be
 * reclaimed the next time any one-shot sound is played.
 */
GLOBAL int wwviaudio_add_one_shot_sound(char *filename);

/* Play one-shot sound from pcm data in samples, nsamples. When sound finishes
 * callback will be called, passing the cookie along.
 * You can stream audio data to be played by making each callback submit
 * more data to play.
 *
 * Returns 0 on success, -1 means previous sound is still playing.
 * Shares an audio slot with wwviaudio_add_one_shot_sound, above, so the
 * the two may not play sounds concurrently.
 */
GLOBAL int wwviaudio_add_one_shot_pcm_data(int16_t *samples, int nsamples,
				void (*callback)(void *), void *cookie);

/* Add to the end of single audio chain that is playing (if not empty).
 * This is for e.g. voice chat. The callback function is called with cookie
 * as a parameter, useful for e.g. freeing the used audio buffer.
 * The chain parameter specifies which of the channels for VOIP should
 * be used. There are WWVIAUDIO_CHAIN_COUNT such channels starting with 0
 * and incrementing by 1 up to WWVIAUDIO_CHAIN_COUNT - 1.
 */
GLOBAL void wwviaudio_append_to_audio_chain(int16_t *samples, int nsamples, int chain,
		void (*callback)(void *), void *cookie);

/* Begin playing a segment of a sound at "begin" until "end" at "volume" (all between 0.0 and 1.0).
 * Upon reaching "end", call "callback" with "cookie" as a parameter.
 */
GLOBAL /* channel */ int wwviaudio_add_sound_segment(int sound_number, float initial_volume,
			float target_volume, float begin, float end, void (*callback)(void *cookie),
			void *cookie);

/* Begin playing a sound on a non-music channel.  The channel is returned.
 * If fewer than five channels are open, the sound is not played, and -1
 * is returned.
 */
GLOBAL void wwviaudio_add_sound_low_priority(int sound_number);

/* Silence all channels but the music channel (pointers still advance though) */
GLOBAL void wwviaudio_silence_sound_effects(void);

/* Unsilence all channels but the music channel */
GLOBAL void wwviaudio_resume_sound_effects(void);

/* Either silence or unsilence all but the music channel */
GLOBAL void wwviaudio_toggle_sound_effects(void);

/* Stop playing the playing buffer from the given channel */
GLOBAL void wwviaudio_cancel_sound(int channel);


/* Stop playing the playing buffer from all channels */
GLOBAL void wwviaudio_cancel_all_sounds(void);

/* List available devices */
GLOBAL void wwviaudio_list_devices(void);

GLOBAL int wwviaudio_start_audio_capture(int16_t *buffer, int nsamples,
	void (*callback)(void *, int16_t *, int), void *cookie);

GLOBAL int wwviaudio_stop_audio_capture();

GLOBAL unsigned int wwviaudio_get_mixer_cycle_count();

/*
	Example usage, something along these lines:

	if (wwviaudio_initialize_portaudio() != 0)
		bail_out_and_die();

	You would probably use #defines or enums rather than bare ints...
	wwviaudio_read_ogg_clip(1, "mysound1.ogg");
	wwviaudio_read_ogg_clip(2, "mysound2.ogg");
	wwviaudio_read_ogg_clip(3, "mysound3.ogg");
	wwviaudio_read_ogg_clip(4, "mymusic.ogg");

	...

	wwviaudio_play_music(4); <-- begins playing music in background, returns immediately

	while (program isn't done) {
		do_stuff();
		if (something happened)
			wwviaudio_add_sound(1);
		if (something else happened)
			wwviaudio_add_sound(2);
		time_passes();
	}
	
	wwviaudio_cancel_all_sounds();
	wwviaduio_stop_portaudio();
*/
#undef GLOBAL
#endif
