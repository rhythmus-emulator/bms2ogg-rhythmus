/** extension for real-time midi playing. by @lazykuna */
#ifndef TIMIDITY_REALTIME
#define TIMIDITY_REALTIME

#include "timidity.h"

#ifdef __cplusplus
extern "C" {
#endif

TIMI_EXPORT extern void note_off_export(MidSong *song);
TIMI_EXPORT extern void note_on_export(MidSong *song);
TIMI_EXPORT extern MidSong* mid_song_for_stream(MidSongOptions *options);

#ifdef __cplusplus
}
#endif

#endif