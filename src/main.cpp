#include <SDL2/SDL.h>

#include <orbis/libkernel.h>

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

    // SDL remains responsible only for controller input and timing. Video is
    // presented by Piglet/OpenGL ES on the PS4 GPU.
    if (SDL_Init(SDL_INIT_JOYSTICK) != 0)
        stopOnStartupError(SDL_GetError());
    if (!gw::draw::initialize(SCREEN_WIDTH, SCREEN_HEIGHT))
        stopOnStartupError("Piglet/OpenGL ES");

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
            stopOnStartupError("eglSwapBuffers");

        nextFrame += FRAME_TIME_US;
        const uint64_t now = sceKernelGetProcessTime();
        if (now < nextFrame)
            sceKernelUsleep(static_cast<uint32_t>(nextFrame - now));
        else if (now - nextFrame > 100000)
            nextFrame = now;
    }

    gw::draw::shutdown();
    SDL_Quit();
    return 0;
}
