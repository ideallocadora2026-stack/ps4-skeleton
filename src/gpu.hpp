#pragma once

#include <stdint.h>

namespace gw
{
namespace gpu
{
bool initialize(int width, int height);
void shutdown();
void beginFrame();
bool present();
void setClip(int x, int y, int w, int h);
void clearClip();

void fillRect(float x, float y, float w, float h, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void line(float x1, float y1, float x2, float y2, float thickness, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void fillCircle(float cx, float cy, float radius, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void circle(float cx, float cy, float radius, float thickness, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void triangle(float x1, float y1, float x2, float y2, float x3, float y3, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
}
}
