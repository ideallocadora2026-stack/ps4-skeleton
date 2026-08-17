#include <SDL2/SDL.h>

#include <orbis/SystemService.h>

#include <stdio.h>

#include "game.hpp"

namespace
{
const int SCREEN_WIDTH = 1920;
const int SCREEN_HEIGHT = 1080;
}

int main(int, char**)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    sceSystemServiceHideSplashScreen();

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC | SDL_INIT_TIMER) != 0)
    {
        printf("SDL_Init falhou: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Geometric Wars", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        SCREEN_WIDTH, SCREEN_HEIGHT, 0);
    if (!window)
    {
        printf("SDL_CreateWindow falhou: %s\n", SDL_GetError());
        SDL_Quit();
        return 2;
    }

    SDL_Surface* surface = SDL_GetWindowSurface(window);
    SDL_Renderer* renderer = surface ? SDL_CreateSoftwareRenderer(surface) : nullptr;
    if (!renderer)
    {
        printf("SDL_CreateSoftwareRenderer falhou: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 3;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    gw::Game game(renderer);
    if (!game.initialize())
    {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 4;
    }

    while (game.running())
    {
        const uint32_t frameStart = SDL_GetTicks();
        game.tick(frameStart);
        SDL_UpdateWindowSurface(window);

        const uint32_t elapsed = SDL_GetTicks() - frameStart;
        if (elapsed < 16) SDL_Delay(16 - elapsed);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

