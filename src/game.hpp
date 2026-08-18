#pragma once

#include <SDL2/SDL.h>

#include <stdint.h>

namespace gw
{
class Game
{
public:
    explicit Game(SDL_Renderer* renderer);
    ~Game();

    bool initialize();
    void tick(uint32_t now);
    bool running() const;
    int targetFps() const;

private:
    Game(const Game&);
    Game& operator=(const Game&);

    struct Impl;
    Impl* impl_;
};
}
