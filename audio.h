#pragma once

#include <cstdint>
#include "beam.h"

struct Audio;

Audio* audio_create(void);
void audio_destroy(Audio* audio);

void audio_set_volume(Audio* audio, uint8_t volume);
void audio_append_beam_data(Audio* audio, const BeamData& beam_data);
void audio_pause_set(Audio* audio, bool value);

