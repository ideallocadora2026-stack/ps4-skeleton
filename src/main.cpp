#include <SDL2/SDL.h>

#include <orbis/libkernel.h>
#include <orbis/SystemService.h>

#include <stdio.h>

#include "game.hpp"

namespace
{
const int SCREEN_WIDTH = 1920;
const int SCREEN_HEIGHT = 1080;
const int RENDER_WIDTH = 960;
const int RENDER_HEIGHT = 540;

void stopOnStartupError(const char* stage)
{
    printf("Falha na inicializacao (%s): %s\n", stage, SDL_GetError());
    for (;;) sceKernelSleep(1);
}
}

int main(int, char**)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    sceSystemServiceHideSplashScreen();

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    // Keep this identical to the OpenOrbis SDL2 sample. The PS4 SDL build
    // disables the generic haptic backend and initializes its timer internally.
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) != 0)
        stopOnStartupError("SDL_Init");

    SDL_Window* window = SDL_CreateWindow(
        "Geometric Wars", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        SCREEN_WIDTH, SCREEN_HEIGHT, 0);
    if (!window)
        stopOnStartupError("SDL_CreateWindow");

    SDL_Surface* surface = SDL_GetWindowSurface(window);
    if (!surface)
        stopOnStartupError("SDL_GetWindowSurface");

    // OpenOrbis uses a software renderer. Draw at half resolution and scale the
    // finished frame to 1080p; this removes most of the per-pixel CPU cost.
    SDL_Surface* frameSurface = SDL_CreateRGBSurfaceWithFormat(
        0, RENDER_WIDTH, RENDER_HEIGHT, 32, surface->format->format);
    if (!frameSurface)
        stopOnStartupError("SDL_CreateRGBSurfaceWithFormat");

    SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(frameSurface);
    if (!renderer)
        stopOnStartupError("SDL_CreateSoftwareRenderer");

    SDL_RenderSetScale(renderer, 0.5f, 0.5f);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    gw::Game game(renderer);
    if (!game.initialize())
        stopOnStartupError("Game::initialize");

    while (game.running())
    {
        const uint32_t frameStart = SDL_GetTicks();
        game.tick(frameStart);
        if (SDL_BlitScaled(frameSurface, nullptr, surface, nullptr) != 0)
            stopOnStartupError("SDL_BlitScaled");
        SDL_UpdateWindowSurface(window);

        const uint32_t elapsed = SDL_GetTicks() - frameStart;
        if (elapsed < 16) SDL_Delay(16 - elapsed);
    }

    SDL_DestroyRenderer(renderer);
    SDL_FreeSurface(frameSurface);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
