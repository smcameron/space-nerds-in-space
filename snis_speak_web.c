/*
	This file (snis_speak_web.c) is entirely machine-generated. To the extent
	possible under law, the author(s) have dedicated all copyright and related and
	neighboring rights to this software to the public domain worldwide.  This
	software is distributed without any warranty.

	You should have received a copy of the CC0 Public Domain Dedication along with
	this software. If not, see <http://creativecommons.org>.
*/

/*
 * Browser implementation of snis_speak.h, using the browser's own speech
 * synthesis (the Web Speech API) in place of snis_text_to_speech.sh.
 *
 * Speech synthesis belongs to the page, not to worker threads, so the call
 * is run on the main thread.  speak() only queues the text, so this returns
 * at once, and the browser says each line in turn.
 */

#include <emscripten.h>
#include <emscripten/proxying.h>
#include <emscripten/threading.h>

#define DEFINE_SNIS_SPEAK_GLOBALS
#include "snis_speak.h"

struct speech {
	const char *text;
	float volume;
};

EM_JS(void, js_speak, (const char *text, float volume), {
	var u;

	if (!self.speechSynthesis)
		return;
	u = new SpeechSynthesisUtterance(UTF8ToString(text));
	u.volume = volume;
	speechSynthesis.speak(u);
})

static void speak_on_main_thread(void *arg)
{
	struct speech *s = arg;

	js_speak(s->text, s->volume);
}

void snis_speak(const char *text, float volume)
{
	struct speech s = { text, volume };

	if (emscripten_is_main_runtime_thread())
		speak_on_main_thread(&s);
	else
		emscripten_proxy_sync(emscripten_proxy_get_system_queue(),
			emscripten_main_runtime_thread_id(), speak_on_main_thread, &s);
}
