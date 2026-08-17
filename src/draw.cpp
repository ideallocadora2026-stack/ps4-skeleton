#include "draw.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <emmintrin.h>

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

SDL_Surface* targetSurface = nullptr;
uint32_t* pixels = nullptr;
int pitchPixels = 0;
int surfaceWidth = 0;
int surfaceHeight = 0;
int clipLeft = 0;
int clipTop = 0;
int clipRight = 0;
int clipBottom = 0;
bool surfaceLocked = false;
uint32_t redMask = 0;
uint32_t greenMask = 0;
uint32_t blueMask = 0;
uint32_t alphaMask = 0;
Uint8 redShift = 0;
Uint8 greenShift = 0;
Uint8 blueShift = 0;
Uint8 alphaShift = 0;

uint32_t packed(Color value)
{
    uint32_t result = ((static_cast<uint32_t>(value.r) << redShift) & redMask) |
                      ((static_cast<uint32_t>(value.g) << greenShift) & greenMask) |
                      ((static_cast<uint32_t>(value.b) << blueShift) & blueMask);
    if (alphaMask) result |= (static_cast<uint32_t>(value.a) << alphaShift) & alphaMask;
    return result;
}

int blendedChannel(int source, int destination, int alpha)
{
    return (source * alpha + destination * (255 - alpha) + 127) / 255;
}

void blendSpan(int y, int x1, int x2, Color value)
{
    if (!pixels || value.a == 0 || y < clipTop || y >= clipBottom) return;
    x1 = std::max(x1, clipLeft);
    x2 = std::min(x2, clipRight);
    if (x1 >= x2) return;

    uint32_t* destination = pixels + y * pitchPixels + x1;
    const int count = x2 - x1;
    if (value.a == 255)
    {
        std::fill(destination, destination + count, packed(value));
        return;
    }

    const int alpha = value.a;
    const int inverse = 255 - alpha;
    const __m128i zero = _mm_setzero_si128();
    Color opaque = value;
    opaque.a = 255;
    const __m128i sourceBytes = _mm_set1_epi32(static_cast<int>(packed(opaque)));
    const __m128i source = _mm_unpacklo_epi8(sourceBytes, zero);
    const __m128i sourceAlpha = _mm_set1_epi16(static_cast<short>(alpha));
    const __m128i destinationAlpha = _mm_set1_epi16(static_cast<short>(inverse));
    const __m128i rounding = _mm_set1_epi16(128);

    int index = 0;
    for (; index + 4 <= count; index += 4)
    {
        const __m128i bytes = _mm_loadu_si128(reinterpret_cast<const __m128i*>(destination + index));
        __m128i low = _mm_unpacklo_epi8(bytes, zero);
        __m128i high = _mm_unpackhi_epi8(bytes, zero);

        low = _mm_add_epi16(_mm_mullo_epi16(source, sourceAlpha), _mm_mullo_epi16(low, destinationAlpha));
        high = _mm_add_epi16(_mm_mullo_epi16(source, sourceAlpha), _mm_mullo_epi16(high, destinationAlpha));
        low = _mm_add_epi16(low, rounding);
        high = _mm_add_epi16(high, rounding);
        low = _mm_add_epi16(low, _mm_srli_epi16(low, 8));
        high = _mm_add_epi16(high, _mm_srli_epi16(high, 8));
        low = _mm_srli_epi16(low, 8);
        high = _mm_srli_epi16(high, 8);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(destination + index), _mm_packus_epi16(low, high));
    }

    for (; index < count; ++index)
    {
        const uint32_t old = destination[index];
        const int red = blendedChannel(value.r, (old & redMask) >> redShift, alpha);
        const int green = blendedChannel(value.g, (old & greenMask) >> greenShift, alpha);
        const int blue = blendedChannel(value.b, (old & blueMask) >> blueShift, alpha);
        destination[index] = packed({static_cast<Uint8>(red), static_cast<Uint8>(green), static_cast<Uint8>(blue), 255});
    }
}

void blendRing(int cx, int cy, int innerRadius, int outerRadius, Color value)
{
    if (outerRadius <= innerRadius || outerRadius <= 0 || value.a == 0) return;
    const int outerSquared = outerRadius * outerRadius;
    const int innerSquared = innerRadius * innerRadius;
    for (int y = -outerRadius; y <= outerRadius; ++y)
    {
        const int outerSpan = static_cast<int>(std::sqrt(static_cast<float>(std::max(0, outerSquared - y * y))));
        if (std::abs(y) >= innerRadius)
        {
            blendSpan(cy + y, cx - outerSpan, cx + outerSpan + 1, value);
            continue;
        }
        const int innerSpan = static_cast<int>(std::sqrt(static_cast<float>(std::max(0, innerSquared - y * y))));
        blendSpan(cy + y, cx - outerSpan, cx - innerSpan, value);
        blendSpan(cy + y, cx + innerSpan + 1, cx + outerSpan + 1, value);
    }
}
}

namespace draw
{
bool initialize(SDL_Surface* surface)
{
    if (!surface || !surface->pixels || !surface->format || surface->format->BytesPerPixel != 4 || !surface->format->Rmask || !surface->format->Gmask || !surface->format->Bmask) return false;
    targetSurface = surface;
    pixels = static_cast<uint32_t*>(surface->pixels);
    pitchPixels = surface->pitch / 4;
    surfaceWidth = surface->w;
    surfaceHeight = surface->h;
    redMask = surface->format->Rmask;
    greenMask = surface->format->Gmask;
    blueMask = surface->format->Bmask;
    alphaMask = surface->format->Amask;
    redShift = surface->format->Rshift;
    greenShift = surface->format->Gshift;
    blueShift = surface->format->Bshift;
    alphaShift = surface->format->Ashift;
    clipLeft = 0;
    clipTop = 0;
    clipRight = surfaceWidth;
    clipBottom = surfaceHeight;
    return true;
}

void shutdown()
{
    if (surfaceLocked && targetSurface) SDL_UnlockSurface(targetSurface);
    surfaceLocked = false;
    targetSurface = nullptr;
    pixels = nullptr;
}

void beginFrame()
{
    if (targetSurface && SDL_MUSTLOCK(targetSurface))
    {
        if (SDL_LockSurface(targetSurface) == 0)
        {
            surfaceLocked = true;
            pixels = static_cast<uint32_t*>(targetSurface->pixels);
            pitchPixels = targetSurface->pitch / 4;
        }
    }
    clipLeft = 0;
    clipTop = 0;
    clipRight = surfaceWidth;
    clipBottom = surfaceHeight;
}

bool present()
{
    if (surfaceLocked && targetSurface)
    {
        SDL_UnlockSurface(targetSurface);
        surfaceLocked = false;
    }
    return pixels != nullptr;
}

void setClipRect(int x, int y, int w, int h)
{
    clipLeft = std::max(0, x);
    clipTop = std::max(0, y);
    clipRight = std::min(surfaceWidth, x + std::max(0, w));
    clipBottom = std::min(surfaceHeight, y + std::max(0, h));
}

void clearClipRect()
{
    clipLeft = 0;
    clipTop = 0;
    clipRight = surfaceWidth;
    clipBottom = surfaceHeight;
}

void color(SDL_Renderer* renderer, Color value)
{
    (void)renderer;
    (void)value;
}

void fillRect(SDL_Renderer* renderer, int x, int y, int w, int h, Color value)
{
    (void)renderer;
    if (w <= 0 || h <= 0) return;
    const int firstY = std::max(y, clipTop);
    const int lastY = std::min(y + h, clipBottom);
    for (int row = firstY; row < lastY; ++row)
        blendSpan(row, x, x + w, value);
}

void outlineRect(SDL_Renderer* renderer, int x, int y, int w, int h, Color value, int thickness)
{
    if (w <= 0 || h <= 0 || thickness <= 0) return;
    fillRect(renderer, x, y, w, thickness, value);
    fillRect(renderer, x, y + h - thickness, w, thickness, value);
    fillRect(renderer, x, y + thickness, thickness, h - thickness * 2, value);
    fillRect(renderer, x + w - thickness, y + thickness, thickness, h - thickness * 2, value);
}

void line(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, Color value, int thickness)
{
    (void)renderer;
    const int dx = x2 - x1;
    const int dy = y2 - y1;
    const int steps = std::max(std::abs(dx), std::abs(dy));
    const int size = std::max(1, thickness);
    const int half = size / 2;
    if (y1 == y2)
    {
        fillRect(renderer, std::min(x1, x2), y1 - half, std::abs(dx) + 1, size, value);
        return;
    }
    if (x1 == x2)
    {
        fillRect(renderer, x1 - half, std::min(y1, y2), size, std::abs(dy) + 1, value);
        return;
    }
    if (steps == 0)
    {
        fillRect(renderer, x1 - half, y1 - half, size, size, value);
        return;
    }
    const float stepX = static_cast<float>(dx) / steps;
    const float stepY = static_cast<float>(dy) / steps;
    float x = static_cast<float>(x1);
    float y = static_cast<float>(y1);
    for (int i = 0; i <= steps; ++i)
    {
        const int px = static_cast<int>(x + 0.5f) - half;
        const int py = static_cast<int>(y + 0.5f) - half;
        for (int row = 0; row < size; ++row) blendSpan(py + row, px, px + size, value);
        x += stepX;
        y += stepY;
    }
}

void circle(SDL_Renderer* renderer, int cx, int cy, int radius, Color value)
{
    (void)renderer;
    if (radius <= 0) return;
    int x = radius;
    int y = 0;
    int error = 1 - radius;
    while (x >= y)
    {
        blendSpan(cy + y, cx + x, cx + x + 2, value);
        blendSpan(cy + x, cx + y, cx + y + 2, value);
        blendSpan(cy + x, cx - y, cx - y + 2, value);
        blendSpan(cy + y, cx - x, cx - x + 2, value);
        blendSpan(cy - y, cx - x, cx - x + 2, value);
        blendSpan(cy - x, cx - y, cx - y + 2, value);
        blendSpan(cy - x, cx + y, cx + y + 2, value);
        blendSpan(cy - y, cx + x, cx + x + 2, value);
        ++y;
        if (error < 0) error += 2 * y + 1;
        else
        {
            --x;
            error += 2 * (y - x) + 1;
        }
    }
}

void ellipse(SDL_Renderer* renderer, int cx, int cy, int radiusX, int radiusY, Color value, int thickness)
{
    if (radiusX <= 0 || radiusY <= 0) return;
    const int segments = std::max(24, std::min(96, (radiusX + radiusY) / 2));
    int previousX = cx + radiusX;
    int previousY = cy;
    for (int i = 1; i <= segments; ++i)
    {
        const float angle = static_cast<float>(i) * 6.28318530718f / segments;
        const int nextX = cx + static_cast<int>(std::cos(angle) * radiusX);
        const int nextY = cy + static_cast<int>(std::sin(angle) * radiusY);
        line(renderer, previousX, previousY, nextX, nextY, value, thickness);
        previousX = nextX;
        previousY = nextY;
    }
}

void fillCircle(SDL_Renderer* renderer, int cx, int cy, int radius, Color value)
{
    (void)renderer;
    if (radius <= 0) return;
    for (int y = -radius; y <= radius; ++y)
    {
        const int span = static_cast<int>(std::sqrt(static_cast<float>(radius * radius - y * y)));
        blendSpan(cy + y, cx - span, cx + span + 1, value);
    }
}

void glowCircle(SDL_Renderer* renderer, int cx, int cy, int radius, Color value, int glow)
{
    const int step = 3;
    for (int inner = radius; inner < radius + glow; inner += step)
    {
        Color haze = value;
        const int distance = inner - radius;
        haze.a = static_cast<Uint8>(std::max(3, static_cast<int>(value.a) * (glow - distance) / std::max(1, glow * 7)));
        blendRing(cx, cy, inner, std::min(radius + glow, inner + step), haze);
    }
    fillCircle(renderer, cx, cy, radius, value);
}

void triangle(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, int x3, int y3, Color value)
{
    (void)renderer;
    const int minY = std::min(y1, std::min(y2, y3));
    const int maxY = std::max(y1, std::max(y2, y3));
    const int xs[3] = {x1, x2, x3};
    const int ys[3] = {y1, y2, y3};
    for (int y = minY; y <= maxY; ++y)
    {
        int intersections[3];
        int count = 0;
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
            blendSpan(y, intersections[0], intersections[1] + 1, value);
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
    for (unsigned index = 0; index < value.size(); ++index)
    {
        const Uint8* rows = glyph(value[index]);
        if (rows)
        {
            for (int row = 0; row < 7; ++row)
                for (int column = 0; column < 5; ++column)
                    if ((rows[row] & (1 << (4 - column))) != 0)
                        fillRect(renderer, x + column * scale, y + row * scale, scale, scale, valueColor);
        }
        x += 6 * scale;
    }
}
}
}
