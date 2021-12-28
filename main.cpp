#include <cstdint>
#include <SDL2/SDL.h>

#include "beam.h"
#include "audio.h"
#include "renderer.h"

void sinusoidal_tone_generator(double t, double* x, double* y)
{
    static const double frequency = 200.0;
    static const double radius = 0.8;
    double u = frequency * 2.0 * M_PI * t;
    double radius_ramp = t > 1.0? 1.0: t*t*t;
    *x = radius * radius_ramp * cos(u);
    *y = radius * radius_ramp * sin(u);
}

int main(int argc, char* argv[])
{
    int window_width  = 600;
    int window_height = 600;
    SDL_Window* window;
    SDL_GLContext gl_context;
    {
        if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0)
        {
            fprintf(stderr, "Error initializing SDL. SDL_Error: %s\n", SDL_GetError());
            return -1;
        }

        // Use OpenGL 3.1 core
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
        SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);

        window = SDL_CreateWindow(
            "tone oscillator",
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            window_width, window_height,
            SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI |
            SDL_WINDOW_RESIZABLE
        );

        if(!window)
        {
            fprintf(stderr, "Error creating SDL window. SDL_Error: %s\n", SDL_GetError());
            SDL_Quit();
            return -1;
        }

        gl_context = SDL_GL_CreateContext(window);
        if(!gl_context)
        {
            fprintf(stderr, "Error creating SDL GL context. SDL_Error: %s\n", SDL_GetError());
            SDL_DestroyWindow(window);
            SDL_Quit();
            return -1;
        }

        int r, g, b, a, m, s;
        SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &r);
        SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &g);
        SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &b);
        SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, &a);
        SDL_GL_GetAttribute(SDL_GL_MULTISAMPLESAMPLES, &m);
        SDL_GL_GetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, &s);

        SDL_GL_SetSwapInterval(1);
    }

    int drawable_width, drawable_height;
    SDL_GL_GetDrawableSize(window, &drawable_width, &drawable_height);

    Renderer renderer;
    gl_renderer_init(&renderer, window_width, window_height);

    Beam beam;
    {
        beam.num_edges  = 5000;
        beam.decay_time = 4e-2;
        beam.radius     = 1.0e-2;
        beam.intensity  = 25.0 * (1 << 5);
        beam.color[0]   = 0.05f;
        beam.color[1]   = 1.0f;
        beam.color[2]   = 0.05f;

        beam.sim_time = 0.0;

        beam.x = 0.0;
        beam.y = 0.0;
    }
    gl_renderer_set_beam_parameters(renderer, beam);

    Audio* audio = audio_create();
    audio_set_volume(audio, 120);

    uint64_t cpu_frequency = SDL_GetPerformanceFrequency();
    uint64_t current_clock = SDL_GetPerformanceCounter();

    bool running = true;
    while(running)
    {
        // Process input
        SDL_Event sdl_event;
        while(SDL_PollEvent(&sdl_event) != 0)
        {
            if(sdl_event.type == SDL_QUIT)
                running = false;
            else if(sdl_event.type == SDL_KEYDOWN)
            {
                if(sdl_event.key.keysym.sym == SDLK_ESCAPE)
                {
                    running = false;
                }
            }
            else if(sdl_event.type == SDL_WINDOWEVENT)
            {
                if(sdl_event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                {
                    int new_drawable_width, new_drawable_height;
                    SDL_GL_GetDrawableSize(window, &new_drawable_width, &new_drawable_height);

                    if(
                        (new_drawable_width != drawable_width) || 
                        (new_drawable_height != drawable_height)
                    ){
                        new_drawable_width = drawable_width;
                        new_drawable_height = drawable_height;

                        gl_renderer_resize(&renderer, drawable_width, drawable_height);

                        // Keep beam at fixed size regardless of resolution.
                        beam.radius = 1e-2 * 700.0 / fmin(drawable_width, drawable_height);
                        gl_renderer_set_beam_parameters(renderer, beam);
                    }
                }
            }
        }

        uint64_t new_clock = SDL_GetPerformanceCounter();
        double frame_sec = double(new_clock - current_clock) / cpu_frequency;
        current_clock = new_clock;

        BeamData beam_data;
        beam_simulate(&beam, &beam_data, sinusoidal_tone_generator, frame_sec);
        gl_renderer_draw_beam_points(renderer, beam, beam_data);

        audio_append_beam_data(audio, beam_data);

        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    audio_destroy(audio);
    SDL_Quit();

    return 0;
}
