#include "draw.hpp"

#include <algorithm>
#include <cmath>

namespace gw
{
namespace
{
struct Glyph
{
    char character;
    Uint8 rows[7];
};

const Glyph GLYPHS[] = {
    {'A', {0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11}},
    {'B', {0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e}},
    {'C', {0x0f, 0x10, 0x10, 0x10, 0x10, 0x10, 0x0f}},
    {'D', {0x1e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1e}},
    {'E', {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f}},
    {'F', {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10}},
    {'G', {0x0f, 0x10, 0x10, 0x17, 0x11, 0x11, 0x0f}},
    {'H', {0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11}},
    {'I', {0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1f}},
    {'J', {0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0e}},
    {'K', {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}},
    {'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f}},
    {'M', {0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11}},
    {'N', {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}},
    {'O', {0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e}},
    {'P', {0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10}},
    {'Q', {0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d}},
    {'R', {0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11}},
    {'S', {0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e}},
    {'T', {0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
    {'U', {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e}},
    {'V', {0x11, 0x11, 0x11, 0x11, 0x11, 0x0a, 0x04}},
    {'W', {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0a}},
    {'X', {0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11}},
    {'Y', {0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04}},
    {'Z', {0x1f, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1f}},
    {'0', {0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e}},
    {'1', {0x04, 0x0c, 0x14, 0x04, 0x04, 0x04, 0x1f}},
    {'2', {0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f}},
    {'3', {0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e}},
    {'4', {0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02}},
    {'5', {0x1f, 0x10, 0x10, 0x1e, 0x01, 0x01, 0x1e}},
    {'6', {0x0e, 0x10, 0x10, 0x1e, 0x11, 0x11, 0x0e}},
    {'7', {0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}},
    {'8', {0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e}},
    {'9', {0x0e, 0x11, 0x11, 0x0f, 0x01, 0x01, 0x0e}},
    {'-', {0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00}},
    {':', {0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00}},
    {'.', {0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x0c}},
    {'/', {0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10}},
    {'+', {0x00, 0x04, 0x04, 0x1f, 0x04, 0x04, 0x00}},
    {'%', {0x19, 0x19, 0x02, 0x04, 0x08, 0x13, 0x13}},
    {'!', {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04}},
    {'?', {0x0e, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04}},
    {'(', {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02}},
    {')', {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08}},
    {'[', {0x0e, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0e}},
    {']', {0x0e, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0e}},
    {'=', {0x00, 0x00, 0x1f, 0x00, 0x1f, 0x00, 0x00}},
    {'_', {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f}},
    {'<', {0x01, 0x02, 0x04, 0x08, 0x04, 0x02, 0x01}},
    {'>', {0x10, 0x08, 0x04, 0x02, 0x04, 0x08, 0x10}},
    {'*', {0x00, 0x11, 0x0a, 0x1f, 0x0a, 0x11, 0x00}},
};

const Uint8* glyph(char value)
{
    if (value >= 'a' && value <= 'z') value = static_cast<char>(value - 'a' + 'A');
    for (unsigned i = 0; i < sizeof(GLYPHS) / sizeof(GLYPHS[0]); ++i)
        if (GLYPHS[i].character == value) return GLYPHS[i].rows;
    return nullptr;
}
}

namespace draw
{
void color(SDL_Renderer* renderer, Color value)
{
    SDL_SetRenderDrawColor(renderer, value.r, value.g, value.b, value.a);
}

void fillRect(SDL_Renderer* renderer, int x, int y, int w, int h, Color value)
{
    if (w <= 0 || h <= 0) return;
    color(renderer, value);
    SDL_Rect rectangle = {x, y, w, h};
    SDL_RenderFillRect(renderer, &rectangle);
}

void outlineRect(SDL_Renderer* renderer, int x, int y, int w, int h, Color value, int thickness)
{
    for (int i = 0; i < thickness; ++i)
    {
        color(renderer, value);
        SDL_Rect rectangle = {x + i, y + i, w - i * 2, h - i * 2};
        if (rectangle.w > 0 && rectangle.h > 0) SDL_RenderDrawRect(renderer, &rectangle);
    }
}

void line(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, Color value, int thickness)
{
    color(renderer, value);
    const int half = std::max(0, thickness / 2);
    for (int offset = -half; offset <= half; ++offset)
    {
        SDL_RenderDrawLine(renderer, x1 + offset, y1, x2 + offset, y2);
        if (thickness > 2) SDL_RenderDrawLine(renderer, x1, y1 + offset, x2, y2 + offset);
    }
}

void circle(SDL_Renderer* renderer, int cx, int cy, int radius, Color value)
{
    if (radius <= 0) return;
    color(renderer, value);
    int x = radius;
    int y = 0;
    int error = 1 - radius;
    while (x >= y)
    {
        SDL_RenderDrawPoint(renderer, cx + x, cy + y);
        SDL_RenderDrawPoint(renderer, cx + y, cy + x);
        SDL_RenderDrawPoint(renderer, cx - y, cy + x);
        SDL_RenderDrawPoint(renderer, cx - x, cy + y);
        SDL_RenderDrawPoint(renderer, cx - x, cy - y);
        SDL_RenderDrawPoint(renderer, cx - y, cy - x);
        SDL_RenderDrawPoint(renderer, cx + y, cy - x);
        SDL_RenderDrawPoint(renderer, cx + x, cy - y);
        ++y;
        if (error < 0) error += 2 * y + 1;
        else
        {
            --x;
            error += 2 * (y - x) + 1;
        }
    }
}

void fillCircle(SDL_Renderer* renderer, int cx, int cy, int radius, Color value)
{
    if (radius <= 0) return;
    color(renderer, value);
    SDL_Rect spans[512];
    int count = 0;
    for (int y = -radius; y <= radius; ++y)
    {
        const int span = static_cast<int>(std::sqrt(static_cast<float>(radius * radius - y * y)));
        if (count == 512)
        {
            SDL_RenderFillRects(renderer, spans, count);
            count = 0;
        }
        spans[count++] = {cx - span, cy + y, span * 2 + 1, 1};
    }
    if (count > 0) SDL_RenderFillRects(renderer, spans, count);
}

void glowCircle(SDL_Renderer* renderer, int cx, int cy, int radius, Color value, int glow)
{
    if (glow > 0)
    {
        Color haze = value;
        haze.a = static_cast<Uint8>(std::max(10, static_cast<int>(value.a) / 9));
        fillCircle(renderer, cx, cy, radius + glow, haze);
    }
    fillCircle(renderer, cx, cy, radius, value);
}

void triangle(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, int x3, int y3, Color value)
{
    const int minY = std::min(y1, std::min(y2, y3));
    const int maxY = std::max(y1, std::max(y2, y3));
    color(renderer, value);
    for (int y = minY; y <= maxY; ++y)
    {
        int intersections[3];
        int count = 0;
        const int xs[3] = {x1, x2, x3};
        const int ys[3] = {y1, y2, y3};
        for (int edge = 0; edge < 3; ++edge)
        {
            const int next = (edge + 1) % 3;
            const int ya = ys[edge];
            const int yb = ys[next];
            if ((y >= ya && y < yb) || (y >= yb && y < ya))
            {
                const float t = static_cast<float>(y - ya) / static_cast<float>(yb - ya);
                intersections[count++] = static_cast<int>(xs[edge] + (xs[next] - xs[edge]) * t);
            }
        }
        if (count >= 2)
        {
            if (intersections[0] > intersections[1]) std::swap(intersections[0], intersections[1]);
            SDL_RenderDrawLine(renderer, intersections[0], y, intersections[1], y);
        }
    }
}

void panel(SDL_Renderer* renderer, int x, int y, int w, int h, Color border, Color background)
{
    fillRect(renderer, x, y, w, h, background);
    outlineRect(renderer, x, y, w, h, {border.r, border.g, border.b, 42}, 10);
    outlineRect(renderer, x, y, w, h, border, 2);
}

int textWidth(const std::string& value, int scale)
{
    if (value.empty()) return 0;
    return static_cast<int>(value.size()) * 6 * scale - scale;
}

void text(SDL_Renderer* renderer, const std::string& value, int x, int y, int scale, Color valueColor, bool centered)
{
    if (scale <= 0) return;
    if (centered) x -= textWidth(value, scale) / 2;
    color(renderer, valueColor);
    SDL_Rect blocks[256];
    int blockCount = 0;
    for (unsigned index = 0; index < value.size(); ++index)
    {
        const Uint8* rows = glyph(value[index]);
        if (rows)
        {
            for (int row = 0; row < 7; ++row)
                for (int column = 0; column < 5; ++column)
                    if ((rows[row] & (1 << (4 - column))) != 0)
                    {
                        if (blockCount == 256)
                        {
                            SDL_RenderFillRects(renderer, blocks, blockCount);
                            blockCount = 0;
                        }
                        blocks[blockCount++] = {x + column * scale, y + row * scale, scale, scale};
                    }
        }
        x += 6 * scale;
    }
    if (blockCount > 0) SDL_RenderFillRects(renderer, blocks, blockCount);
}
}
}
