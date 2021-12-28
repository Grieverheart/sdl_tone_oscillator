#pragma once

#include <cstddef>

struct BeamData
{
    size_t num_points;
    double* points;
    double dt;
};

struct Beam
{
    size_t num_edges;
    double decay_time;
    double radius;

    float intensity;
    float color[3];

    double sim_time;

    double x, y;
};

typedef void (*SoundGenerator)(double t, double *x, double *y);

void beam_simulate(Beam* beam, BeamData* beam_data, SoundGenerator ps, double frame_sec);

