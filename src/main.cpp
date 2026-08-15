#include <sstream>

#include <orbis/libkernel.h>
#include <orbis/SystemService.h>

#include "graphics.h"

// graphics.cpp/log.h espera que este stream exista.
std::stringstream debugLogStream;

int main(void)
{
    sceSystemServiceHideSplashScreen();

    Scene2D scene(1920, 1080, 4);

    // 192 MiB de memória direta e dois framebuffers.
    if (!scene.Init(0x0C000000, 2))
    {
        // Mantém o processo aberto caso a inicialização do vídeo falhe.
        for (;;)
        {
            sceKernelSleep(1);
        }
    }

    const Color background = {4, 7, 18};
    const Color cyan       = {0, 220, 255};
    const Color white      = {255, 255, 255};
    const Color red        = {255, 40, 80};
    const Color purple     = {170, 60, 255};

    int frameId = 0;

    for (;;)
    {
        scene.FrameBufferFill(background);

        // Moldura.
        scene.DrawRectangle(100, 70, 1720, 8, cyan);
        scene.DrawRectangle(100, 1002, 1720, 8, cyan);
        scene.DrawRectangle(100, 70, 8, 940, cyan);
        scene.DrawRectangle(1812, 70, 8, 940, cyan);

        // Elementos do teste gráfico.
        scene.DrawRectangle(910, 780, 100, 100, white);
        scene.DrawRectangle(450, 280, 90, 90, red);
        scene.DrawRectangle(1370, 280, 90, 90, red);
        scene.DrawRectangle(700, 420, 70, 70, purple);
        scene.DrawRectangle(1150, 420, 70, 70, purple);
        scene.DrawRectangle(500, 535, 920, 4, cyan);

        scene.SubmitFlip(frameId);
        scene.FrameWait(frameId);
        scene.FrameBufferSwap();
        ++frameId;
    }
}

