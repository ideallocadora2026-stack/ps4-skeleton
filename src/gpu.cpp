#include "gpu.hpp"

#include <orbis/Pigletv2VSH.h>
#include <orbis/libkernel.h>
#include <orbis/SystemService.h>
#include <orbis/Sysmodule.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace gw
{
namespace gpu
{
namespace
{
const int ATLAS_SIZE = 256;
const int MAX_COLORS = ATLAS_SIZE * ATLAS_SIZE;
const size_t MAX_VERTICES = 60000;
const float PI = 3.14159265358979323846f;

struct ShaderBlob
{
    char* ident;
    unsigned char hash[16];
    uint64_t len;
    unsigned char* code;
};

extern "C" ShaderBlob scePrecompiledShaderEntries[];

struct Vertex
{
    float x;
    float y;
    float u;
    float v;
};

int32_t pigletModule = -1;
int32_t shaderModule = -1;
EGLDisplay display = EGL_NO_DISPLAY;
EGLSurface surface = EGL_NO_SURFACE;
EGLContext context = EGL_NO_CONTEXT;
GLuint texture = GL_NONE;
GLuint program = GL_NONE;
GLint modelLocation = -1;
GLint projectionLocation = -1;
GLint textureMatrixLocation = -1;
GLint samplerLocation = -1;
GLint opacityLocation = -1;
GLint vertexLocation = -1;
int screenWidth = 1920;
int screenHeight = 1080;
bool ready = false;

std::vector<Vertex> vertices;
std::vector<uint32_t> paletteKeys;
std::vector<unsigned char> palettePixels;

const float IDENTITY[16] = {
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1
};

unsigned char* findShader(const char* name, uint64_t& size)
{
    for (ShaderBlob* entry = scePrecompiledShaderEntries; entry->ident; ++entry)
    {
        if (std::strcmp(entry->ident, name) == 0)
        {
            size = entry->len;
            return entry->code;
        }
    }
    size = 0;
    return nullptr;
}

bool loadModules()
{
    int startResult = 0;
    std::string prefix = sceKernelGetFsSandboxRandomWord();
    prefix = "/" + prefix + "/common/lib/";

    const std::string pigletPath = prefix + "libScePigletv2VSH.sprx";
    const std::string shadersPath = prefix + "libScePrecompiledShaders.sprx";
    pigletModule = static_cast<int32_t>(sceKernelLoadStartModule(pigletPath.c_str(), 0, nullptr, 0, nullptr, &startResult));
    if (pigletModule < ORBIS_OK)
    {
        std::printf("Falha ao carregar Piglet: 0x%x\n", pigletModule);
        return false;
    }
    shaderModule = static_cast<int32_t>(sceKernelLoadStartModule(shadersPath.c_str(), 0, nullptr, 0, nullptr, &startResult));
    if (shaderModule < ORBIS_OK)
    {
        std::printf("Falha ao carregar shaders: 0x%x\n", shaderModule);
        return false;
    }
    return true;
}

bool createContext()
{
    OrbisPglConfig pglConfig;
    std::memset(&pglConfig, 0, sizeof(pglConfig));
    pglConfig.size = sizeof(pglConfig);
    pglConfig.flags = ORBIS_PGL_FLAGS_USE_COMPOSITE_EXT | ORBIS_PGL_FLAGS_USE_FLEXIBLE_MEMORY | 0x60;
    pglConfig.processOrder = 1;
    pglConfig.systemSharedMemorySize = 250 * 1024 * 1024;
    pglConfig.videoSharedMemorySize = 512 * 1024 * 1024;
    pglConfig.maxMappedFlexibleMemory = 170 * 1024 * 1024;
    pglConfig.drawCommandBufferSize = 4 * 1024 * 1024;
    pglConfig.lcueResourceBufferSize = 8 * 1024 * 1024;
    pglConfig.dbgPosCmd_0x40 = screenWidth;
    pglConfig.dbgPosCmd_0x44 = screenHeight;
    pglConfig.dbgPosCmd_0x48 = 0;
    pglConfig.dbgPosCmd_0x4C = 0;
    pglConfig.unk_0x5C = 2;
    if (!scePigletSetConfigurationVSH(&pglConfig)) return false;

    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) return false;
    EGLint major = 0;
    EGLint minor = 0;
    if (!eglInitialize(display, &major, &minor)) return false;
    if (!eglBindAPI(EGL_OPENGL_ES_API)) return false;

    const EGLint configAttributes[] = {
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 0, EGL_STENCIL_SIZE, 0, EGL_SAMPLE_BUFFERS, 0, EGL_SAMPLES, 0,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_NONE
    };
    const EGLint contextAttributes[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    const EGLint windowAttributes[] = {EGL_RENDER_BUFFER, EGL_BACK_BUFFER, EGL_NONE};
    EGLConfig config = nullptr;
    EGLint configCount = 0;
    if (!eglChooseConfig(display, configAttributes, &config, 1, &configCount) || configCount < 1) return false;

    OrbisPglWindow window = {0, static_cast<khronos_uint32_t>(screenWidth), static_cast<khronos_uint32_t>(screenHeight)};
    surface = eglCreateWindowSurface(display, config, &window, windowAttributes);
    if (surface == EGL_NO_SURFACE) return false;
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttributes);
    if (context == EGL_NO_CONTEXT) return false;
    if (!eglMakeCurrent(display, surface, surface, context)) return false;

    // Piglet presents through the PS4 compositor. Interval 1 locks presentation
    // to the 60 Hz video refresh and avoids tearing.
    if (!eglSwapInterval(display, 1))
        std::printf("Aviso: eglSwapInterval(1) nao foi aceito: 0x%x\n", eglGetError());
    return true;
}

bool createProgram()
{
    uint64_t vertexSize = 0;
    uint64_t fragmentSize = 0;
    unsigned char* vertexCode = findShader("texmap/v_2.vert", vertexSize);
    unsigned char* fragmentCode = findShader("texmap/f_2.frag", fragmentSize);
    if (!vertexCode || !fragmentCode) return false;

    const GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    const GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    program = glCreateProgram();
    if (!vertexShader || !fragmentShader || !program) return false;
    glShaderBinary(1, &vertexShader, 0, vertexCode, vertexSize);
    glShaderBinary(1, &fragmentShader, 0, fragmentCode, fragmentSize);
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    modelLocation = glGetUniformLocation(program, "u_modelViewMatrix");
    projectionLocation = glGetUniformLocation(program, "u_projectionMatrix");
    textureMatrixLocation = glGetUniformLocation(program, "u_textureSpaceMatrix");
    samplerLocation = glGetUniformLocation(program, "s_sampler");
    opacityLocation = glGetUniformLocation(program, "u_opacity");
    vertexLocation = glGetAttribLocation(program, "a_vertex");
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return modelLocation >= 0 && projectionLocation >= 0 && textureMatrixLocation >= 0 &&
           samplerLocation >= 0 && opacityLocation >= 0 && vertexLocation >= 0 && glGetError() == GL_NO_ERROR;
}

bool createPaletteTexture()
{
    palettePixels.assign(MAX_COLORS * 4, 0);
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ATLAS_SIZE, ATLAS_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, palettePixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return texture != GL_NONE && glGetError() == GL_NO_ERROR;
}

void flush()
{
    if (vertices.empty()) return;
    const int rows = std::max(1, (static_cast<int>(paletteKeys.size()) + ATLAS_SIZE - 1) / ATLAS_SIZE);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ATLAS_SIZE, rows, GL_RGBA, GL_UNSIGNED_BYTE, palettePixels.data());
    glUseProgram(program);

    const float projection[16] = {
        2.0f / screenWidth, 0, 0, 0,
        0, -2.0f / screenHeight, 0, 0,
        0, 0, -1, 0,
        -1, 1, 0, 1
    };
    glUniform1f(opacityLocation, 1.0f);
    glUniform1i(samplerLocation, 0);
    glUniformMatrix4fv(modelLocation, 1, GL_FALSE, IDENTITY);
    glUniformMatrix4fv(projectionLocation, 1, GL_FALSE, projection);
    glUniformMatrix4fv(textureMatrixLocation, 1, GL_FALSE, IDENTITY);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glVertexAttribPointer(vertexLocation, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), vertices.data());
    glEnableVertexAttribArray(vertexLocation);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
    vertices.clear();
    paletteKeys.clear();
}

void ensureVertexSpace(size_t count)
{
    if (vertices.size() + count > MAX_VERTICES) flush();
}

void colorUv(uint8_t r, uint8_t g, uint8_t b, uint8_t a, float& u, float& v)
{
    const uint32_t key = (static_cast<uint32_t>(r) << 24) | (static_cast<uint32_t>(g) << 16) |
                         (static_cast<uint32_t>(b) << 8) | a;
    int index = -1;
    for (int i = static_cast<int>(paletteKeys.size()) - 1; i >= 0; --i)
    {
        if (paletteKeys[static_cast<size_t>(i)] == key)
        {
            index = i;
            break;
        }
    }
    if (index < 0)
    {
        if (paletteKeys.size() >= MAX_COLORS) flush();
        index = static_cast<int>(paletteKeys.size());
        paletteKeys.push_back(key);
        const size_t offset = static_cast<size_t>(index) * 4;
        palettePixels[offset] = r;
        palettePixels[offset + 1] = g;
        palettePixels[offset + 2] = b;
        palettePixels[offset + 3] = a;
    }
    u = (static_cast<float>(index % ATLAS_SIZE) + 0.5f) / ATLAS_SIZE;
    v = (static_cast<float>(index / ATLAS_SIZE) + 0.5f) / ATLAS_SIZE;
}

void addVertex(float x, float y, float u, float v)
{
    vertices.push_back({x, y, u, v});
}

void quad(float x1, float y1, float x2, float y2, float u, float v)
{
    ensureVertexSpace(6);
    addVertex(x1, y1, u, v);
    addVertex(x2, y1, u, v);
    addVertex(x1, y2, u, v);
    addVertex(x1, y2, u, v);
    addVertex(x2, y1, u, v);
    addVertex(x2, y2, u, v);
}
}

bool initialize(int width, int height)
{
    screenWidth = width;
    screenHeight = height;
    vertices.reserve(65536);
    paletteKeys.reserve(256);
    if (!loadModules() || !createContext() || !createProgram() || !createPaletteTexture()) return false;

    glViewport(0, 0, screenWidth, screenHeight);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SYSTEM_SERVICE);
    sceSystemServiceHideSplashScreen();
    ready = glGetError() == GL_NO_ERROR;
    return ready;
}

void shutdown()
{
    if (texture != GL_NONE) glDeleteTextures(1, &texture);
    if (program != GL_NONE) glDeleteProgram(program);
    if (display != EGL_NO_DISPLAY)
    {
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (surface != EGL_NO_SURFACE) eglDestroySurface(display, surface);
        if (context != EGL_NO_CONTEXT) eglDestroyContext(display, context);
        eglTerminate(display);
    }
    int stopResult = 0;
    if (shaderModule >= 0) sceKernelStopUnloadModule(shaderModule, 0, nullptr, 0, nullptr, &stopResult);
    if (pigletModule >= 0) sceKernelStopUnloadModule(pigletModule, 0, nullptr, 0, nullptr, &stopResult);
    ready = false;
}

void beginFrame()
{
    vertices.clear();
    paletteKeys.clear();
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, screenWidth, screenHeight);
    glClearColor(3.0f / 255.0f, 5.0f / 255.0f, 15.0f / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

bool present()
{
    flush();
    if (glGetError() != GL_NO_ERROR) return false;
    return eglSwapBuffers(display, surface) == EGL_TRUE;
}

void setClip(int x, int y, int w, int h)
{
    flush();
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, screenHeight - y - h, w, h);
}

void clearClip()
{
    flush();
    glDisable(GL_SCISSOR_TEST);
}

void fillRect(float x, float y, float w, float h, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (w <= 0 || h <= 0) return;
    ensureVertexSpace(6);
    float u, v;
    colorUv(r, g, b, a, u, v);
    quad(x, y, x + w, y + h, u, v);
}

void line(float x1, float y1, float x2, float y2, float thickness, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    const float dx = x2 - x1;
    const float dy = y2 - y1;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length <= 0.001f) return;
    const float half = std::max(0.5f, thickness * 0.5f);
    const float px = -dy / length * half;
    const float py = dx / length * half;
    ensureVertexSpace(6);
    float u, v;
    colorUv(r, g, b, a, u, v);
    addVertex(x1 + px, y1 + py, u, v);
    addVertex(x2 + px, y2 + py, u, v);
    addVertex(x1 - px, y1 - py, u, v);
    addVertex(x1 - px, y1 - py, u, v);
    addVertex(x2 + px, y2 + py, u, v);
    addVertex(x2 - px, y2 - py, u, v);
}

void fillCircle(float cx, float cy, float radius, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (radius <= 0) return;
    const int segments = std::max(16, std::min(64, static_cast<int>(radius * 1.6f)));
    ensureVertexSpace(static_cast<size_t>(segments) * 3);
    float u, v;
    colorUv(r, g, b, a, u, v);
    for (int i = 0; i < segments; ++i)
    {
        const float a1 = PI * 2.0f * i / segments;
        const float a2 = PI * 2.0f * (i + 1) / segments;
        addVertex(cx, cy, u, v);
        addVertex(cx + std::cos(a1) * radius, cy + std::sin(a1) * radius, u, v);
        addVertex(cx + std::cos(a2) * radius, cy + std::sin(a2) * radius, u, v);
    }
}

void circle(float cx, float cy, float radius, float thickness, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (radius <= 0) return;
    const int segments = std::max(20, std::min(72, static_cast<int>(radius * 1.8f)));
    const float inner = std::max(0.0f, radius - std::max(1.0f, thickness));
    ensureVertexSpace(static_cast<size_t>(segments) * 6);
    float u, v;
    colorUv(r, g, b, a, u, v);
    for (int i = 0; i < segments; ++i)
    {
        const float a1 = PI * 2.0f * i / segments;
        const float a2 = PI * 2.0f * (i + 1) / segments;
        const float ox1 = cx + std::cos(a1) * radius;
        const float oy1 = cy + std::sin(a1) * radius;
        const float ox2 = cx + std::cos(a2) * radius;
        const float oy2 = cy + std::sin(a2) * radius;
        const float ix1 = cx + std::cos(a1) * inner;
        const float iy1 = cy + std::sin(a1) * inner;
        const float ix2 = cx + std::cos(a2) * inner;
        const float iy2 = cy + std::sin(a2) * inner;
        addVertex(ox1, oy1, u, v);
        addVertex(ox2, oy2, u, v);
        addVertex(ix1, iy1, u, v);
        addVertex(ix1, iy1, u, v);
        addVertex(ox2, oy2, u, v);
        addVertex(ix2, iy2, u, v);
    }
}

void triangle(float x1, float y1, float x2, float y2, float x3, float y3, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    ensureVertexSpace(3);
    float u, v;
    colorUv(r, g, b, a, u, v);
    addVertex(x1, y1, u, v);
    addVertex(x2, y2, u, v);
    addVertex(x3, y3, u, v);
}
}
}
