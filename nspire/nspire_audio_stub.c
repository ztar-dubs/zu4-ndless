/*
 * nspire_audio_stub.c - Audio stubs for TI-Nspire CX CAS
 * Replaces zu4's cmixer/SDL audio with no-ops.
 * The TI-Nspire CX CAS has no practical audio output for games.
 */

#include <stdbool.h>

#include "music.h"
#include "sound.h"
#include "settings.h"

/* Music stubs */
void zu4_music_play(int music) { (void)music; }
void zu4_music_stop(void) {}
void zu4_music_fadeout(int msecs) { (void)msecs; }
void zu4_music_fadein(int msecs, bool loadFromMap) { (void)msecs; (void)loadFromMap; }
void zu4_music_vol(double volume) { (void)volume; }
int zu4_music_vol_inc(void) { return 0; }
int zu4_music_vol_dec(void) { return 0; }
bool zu4_music_toggle(void) { return false; }
void zu4_music_init(void) {}
void zu4_music_deinit(void) {}

/* Sound stubs */
void zu4_snd_play(int sound, bool onlyOnce, int specificDurationInTicks) {
    (void)sound; (void)onlyOnce; (void)specificDurationInTicks;
}
void zu4_snd_stop(void) {}
void zu4_snd_vol(double volume) { (void)volume; }
int zu4_snd_vol_inc(void) { return 0; }
int zu4_snd_vol_dec(void) { return 0; }
void zu4_snd_init(void) {}
void zu4_snd_deinit(void) {}
