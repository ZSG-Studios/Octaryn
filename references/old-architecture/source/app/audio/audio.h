#pragma once

typedef enum oct_audio_event
{
    OCT_AUDIO_EVENT_PLACE,
    OCT_AUDIO_EVENT_BREAK,
    OCT_AUDIO_EVENT_SELECT,
    OCT_AUDIO_EVENT_CHANGE,
}
oct_audio_event_t;

void oct_audio_init(void);
void oct_audio_shutdown(void);
void oct_audio_play_event(oct_audio_event_t event);
