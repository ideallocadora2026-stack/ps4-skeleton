#pragma once

#include <SDL2/SDL.h>

#include <string>

namespace gw
{
struct Color
{
    Uint8 r;
    Uint8 g;
    Uint8 b;
    Uint8 a;
};

namespace draw
{
void color(SDL_Renderer* renderer, Color value);
void fillRect(SDL_Renderer* renderer, int x, int y, int w, int h, Color value);
void outlineRect(SDL_Renderer* renderer, int x, int y, int w, int h, Color value, int thickness = 1);
void line(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, Color value, int thickness = 1);
void circle(SDL_Renderer* renderer, int cx, int cy, int radius, Color value);
void fillCircle(SDL_Renderer* renderer, int cx, int cy, int radius, Color value);
void glowCircle(SDL_Renderer* renderer, int cx, int cy, int radius, Color value, int glow);
void triangle(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, int x3, int y3, Color value);
void panel(SDL_Renderer* renderer, int x, int y, int w, int h, Color border, Color background);
int textWidth(const std::string& text, int scale);
void text(SDL_Renderer* renderer, const std::string& value, int x, int y, int scale, Color textColor, bool centered = false);
}
}

