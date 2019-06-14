/** extension for real-time midi playing. by @lazykuna */
#ifndef TIMIDITY_REALTIME
#define TIMIDITY_REALTIME

#include "timidity.h"

#ifdef __cplusplus
extern "C" {
#endif

TIMI_EXPORT extern void note_off_export(MidSong *song, MidEvent *e);
TIMI_EXPORT extern void note_on_export(MidSong *song, MidEvent *e);
TIMI_EXPORT extern void adjust_pressure_export(MidSong *song, MidEvent *e);
TIMI_EXPORT extern void pitch_sens_export(MidSong *song, MidEvent *e);
TIMI_EXPORT extern void pitch_wheel_export(MidSong *song, MidEvent *e);
TIMI_EXPORT extern void main_volume_export(MidSong *song, MidEvent *e);
TIMI_EXPORT extern void pan_export(MidSong *song, MidEvent *e);
TIMI_EXPORT extern void expression_export(MidSong *song, MidEvent *e);
TIMI_EXPORT extern void program_export(MidSong *song, MidEvent *e);
TIMI_EXPORT extern void sustain_export(MidSong *song, MidEvent *e);
TIMI_EXPORT extern void reset_controllers_export(MidSong *song, MidEvent *e);
TIMI_EXPORT extern void all_notes_off_export(MidSong *song, MidEvent *e);
TIMI_EXPORT extern void all_sounds_off_export(MidSong *song, MidEvent *e);
TIMI_EXPORT extern void tone_bank_export(MidSong *song, MidEvent *e);
TIMI_EXPORT extern void EOT_export(MidSong *song, MidEvent *e);
TIMI_EXPORT extern void send_event(MidSong *song, MidEvent *e);
TIMI_EXPORT extern MidSong* mid_song_for_stream(MidSongOptions *options);

#ifdef __cplusplus
}
#endif

#endif