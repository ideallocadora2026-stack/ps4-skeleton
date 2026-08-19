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

    SDL_Renderer* gpuRenderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_Texture* gpuFrame = nullptr;
    SDL_Surface* surface = nullptr;
    bool gpuPresentation = false;
    if (gpuRenderer)
    {
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
        SDL_RenderSetLogicalSize(gpuRenderer, SCREEN_WIDTH, SCREEN_HEIGHT);
        gpuFrame = SDL_CreateTexture(gpuRenderer, SDL_PIXELFORMAT_ARGB8888,
                                     SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);
        surface = SDL_CreateRGBSurface(0, SCREEN_WIDTH, SCREEN_HEIGHT, 32,
                                       0x00ff0000u, 0x0000ff00u, 0x000000ffu, 0xff000000u);
        gpuPresentation = gpuFrame != nullptr && surface != nullptr;
    }
    if (!gpuPresentation)
    {
        if (surface) SDL_FreeSurface(surface);
        surface = nullptr;
        if (gpuFrame) SDL_DestroyTexture(gpuFrame);
        gpuFrame = nullptr;
        if (gpuRenderer) SDL_DestroyRenderer(gpuRenderer);
        gpuRenderer = nullptr;
        surface = SDL_GetWindowSurface(window);
        printf("Apresentacao: framebuffer seguro\n");
    }
    else printf("Apresentacao: GPU acelerada com VSync\n");
    if (!surface) stopOnStartupError(SDL_GetError());
    if (!gw::draw::initialize(surface))
        stopOnStartupError("framebuffer 32-bit");

    gw::Game game(gpuRenderer);
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
        if (gpuPresentation)
        {
            if (SDL_UpdateTexture(gpuFrame, nullptr, surface->pixels, surface->pitch) != 0)
                stopOnStartupError(SDL_GetError());
            SDL_SetRenderDrawColor(gpuRenderer, 0, 0, 0, 255);
            SDL_RenderClear(gpuRenderer);
            SDL_RenderCopy(gpuRenderer, gpuFrame, nullptr, nullptr);
            SDL_RenderPresent(gpuRenderer);
        }
        else if (SDL_UpdateWindowSurface(window) != 0) stopOnStartupError(SDL_GetError());

        const uint64_t frameInterval = 1000000u / static_cast<uint64_t>(game.targetFps());
        nextFrame += frameInterval;
        const uint64_t now = sceKernelGetProcessTime();
        if (now < nextFrame)
            sceKernelUsleep(static_cast<uint32_t>(nextFrame - now));
        else
            nextFrame = now;
    }

    gw::draw::shutdown();
    if (gpuPresentation) SDL_FreeSurface(surface);
    if (gpuFrame) SDL_DestroyTexture(gpuFrame);
    if (gpuRenderer) SDL_DestroyRenderer(gpuRenderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
