#include "audio.h"
#include <cstring>
#include <SDL2/SDL.h>

#define BEAM_RB_SIZE 10

struct BeamRingBuffer
{
    BeamData beam_data[BEAM_RB_SIZE];
    uint8_t write_id;
    uint8_t read_id;
};

struct Audio
{
    SDL_AudioDeviceID audio_device_id;
    BeamRingBuffer beam_buffer;
    double time_processed;
    uint8_t volume;
};

void audio_set_volume(Audio* audio, uint8_t volume)
{
    audio->volume = volume;
}

void audio_callback(void* userdata, uint8_t* stream, int len)
{
    memset(stream, 0, len);

    Audio* audio = (Audio*) userdata;
    BeamRingBuffer& beam_buffer = audio->beam_buffer;
    double volume = audio->volume / 255.0;

    float* fstream = (float*)(stream);
    const BeamData* current_data = &beam_buffer.beam_data[beam_buffer.read_id];

    for(int sid = 0; sid < (len / 8);)
    {
        if(!current_data->points) break;

        double t = audio->time_processed + sid / 44100.0;
        if(t < current_data->num_points * current_data->dt)
        {
            // if time is within the timeframe defined by the current_data points.
            size_t pid = t / current_data->dt;
            double f = (t - current_data->dt * pid) / current_data->dt;

            fstream[2 * sid + 0] = volume * (
                (1.0 - f) * current_data->points[2 * pid + 0] +
                        f * current_data->points[2 * pid + 2]
            ); /* L */

            fstream[2 * sid + 1] = volume * (
                (1.0 - f) * current_data->points[2 * pid + 1] +
                        f * current_data->points[2 * pid + 3]
            ); /* R */

            ++sid;
        }
        else
        {
            // Otherwise, we should move the beam buffer to the next data.
            size_t new_read_id = (beam_buffer.read_id + 1) % BEAM_RB_SIZE;
            double new_time_processed = audio->time_processed - current_data->num_points * current_data->dt;

            beam_buffer.read_id = new_read_id;
            audio->time_processed = new_time_processed;

            // @note: workaround for when the audio thread (consumer)
            // catches up with the graphics thread (producer).
            if(new_read_id == beam_buffer.write_id)
            {
                audio->time_processed += sid / 44100.0;
                return;
            }

            current_data = &beam_buffer.beam_data[new_read_id];
        }
    }

    audio->time_processed += (len / 8) / 44100.0;
}


Audio* audio_create(void)
{
    Audio* audio = new Audio;

    audio->volume = 64;
    {
        audio->time_processed = 0.0;
        audio->beam_buffer.read_id  = 0;
        audio->beam_buffer.write_id = 0;
        for(uint8_t i = 0; i < BEAM_RB_SIZE; ++i)
            audio->beam_buffer.beam_data[i].points = nullptr;

        {
            SDL_AudioSpec audio_spec_want;
            SDL_memset(&audio_spec_want, 0, sizeof(audio_spec_want));

            audio_spec_want.freq     = 44100;
            audio_spec_want.format   = AUDIO_F32;
            audio_spec_want.channels = 2;
            audio_spec_want.samples  = 512;
            audio_spec_want.callback = audio_callback;
            audio_spec_want.userdata = (void*) audio;

            SDL_AudioSpec audio_spec;
            audio->audio_device_id = SDL_OpenAudioDevice(NULL, 0, &audio_spec_want, &audio_spec, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
            if(!audio->audio_device_id)
            {
                fprintf(stderr, "Error creating SDL audio device. SDL_Error: %s\n", SDL_GetError());
                return nullptr;
            }

            SDL_PauseAudioDevice(audio->audio_device_id, 0);
        }
    }

    return audio;
}

void audio_destroy(Audio* audio)
{
    SDL_CloseAudioDevice(audio->audio_device_id);
    delete audio;
}

void audio_pause_set(Audio* audio, bool value)
{
    SDL_PauseAudioDevice(audio->audio_device_id, value);
}

void audio_append_beam_data(Audio* audio, const BeamData& beam_data)
{
    SDL_LockAudioDevice(audio->audio_device_id);
    {
        BeamData* new_beam_data = &audio->beam_buffer.beam_data[audio->beam_buffer.write_id];
        delete[] new_beam_data->points;
        *new_beam_data = beam_data;
        audio->beam_buffer.write_id = (audio->beam_buffer.write_id + 1) % BEAM_RB_SIZE;
    }
    SDL_UnlockAudioDevice(audio->audio_device_id);
}

