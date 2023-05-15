#ifndef SNIS_VOICE_CHAT_H_
#define SNIS_VOICE_CHAT_H_

#include "snis_marshal.h"

#define VOICE_CHAT_DESTINATION_CREW 0
#define VOICE_CHAT_DESTINATION_ALL 1
#define VOICE_CHAT_DESTINATION_CHANNEL 2
#define OPUS_PACKET_SIZE 4000

extern void print_demon_console_msg(const char *fmt, ...); /* Defined in snis_client.c */
extern void queue_to_server(struct packed_buffer *pb); /* Defined in snis_client.c */
extern void ssgl_msleep(int milliseconds); /* Defined in ssgl/ssgl_sleep.c */


void voice_chat_setup_threads(void);
void voice_chat_stop_threads(void);
void voice_chat_start_recording(uint8_t destination, uint32_t user_channel /* not the voip channel */);
void voice_chat_stop_recording(void);
void voice_chat_play_opus_packet(uint8_t *buffer, int buflen, int audio_chain);
int voice_chat_recording_level(void);
int voice_chat_playback_level(void);

#endif
