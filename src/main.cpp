#include <SDL2/SDL.h>

#include <orbis/libkernel.h>
#include <orbis/SystemService.h>

#include <stdio.h>

#include "draw.hpp"
#include "game.hpp"

namespace
{
const int SCREEN_WIDTH = 1920;
const int SCREEN_HEIGHT = 1080;
const uint64_t FRAME_TIME_US = 16667;

void stopOnStartupError(const char* stage)
{
    printf("Falha na inicializacao: %s\n", stage);
    for (;;) sceKernelSleep(1);
}
}

int main(int, char**)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    sceSystemServiceHideSplashScreen();

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        stopOnStartupError(SDL_GetError());

    SDL_Window* window = SDL_CreateWindow(
        "Geometric Wars", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        SCREEN_WIDTH, SCREEN_HEIGHT, 0);
    if (!window) stopOnStartupError(SDL_GetError());
    SDL_Surface* surface = SDL_GetWindowSurface(window);
    if (!surface) stopOnStartupError(SDL_GetError());
    if (!gw::draw::initialize(surface))
        stopOnStartupError("framebuffer 32-bit");

    gw::Game game(nullptr);
    if (!game.initialize())
        stopOnStartupError("Game::initialize");

    uint64_t nextFrame = sceKernelGetProcessTime();
    while (game.running())
    {
        const uint32_t tick = SDL_GetTicks();
        gw::draw::beginFrame();
        game.tick(tick);
        if (!gw::draw::present())
            stopOnStartupError("framebuffer");
        if (SDL_UpdateWindowSurface(window) != 0)
            stopOnStartupError(SDL_GetError());

        nextFrame += FRAME_TIME_US;
        const uint64_t now = sceKernelGetProcessTime();
        if (now < nextFrame)
            sceKernelUsleep(static_cast<uint32_t>(nextFrame - now));
        else if (now - nextFrame > 100000)
            nextFrame = now;
    }

    gw::draw::shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
