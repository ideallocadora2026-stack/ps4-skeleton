#include <sstream>

#include <orbis/libkernel.h>
#include <orbis/SystemService.h>

#include "graphics.h"

std::stringstream debugLogStream;

int frameID = 0;

int main(void)
{
    sceSystemServiceHideSplashScreen();

    // Tela Full HD do PS4
    Scene2D scene(1920, 1080, 4);

    // 192 MB de memória direta e dois framebuffers
    if (!scene.Init(0xC000000, 2))
    {
        // Se falhar, mantém aberto para não voltar ao menu
        while (1)
        {
            sceKernelSleep(1);
        }
    }

    Color fundo     = { 4, 7, 18 };
    Color azul      = { 0, 220, 255 };
    Color branco    = { 255, 255, 255 };
    Color vermelho  = { 255, 40, 80 };
    Color roxo      = { 170, 60, 255 };

    while (1)
    {
        // Fundo
        scene.FrameBufferFill(fundo);

        // Moldura
        scene.DrawRectangle(100, 70, 1720, 8, azul);
        scene.DrawRectangle(100, 1002, 1720, 8, azul);
        scene.DrawRectangle(100, 70, 8, 940, azul);
        scene.DrawRectangle(1812, 70, 8, 940, azul);

        // Jogador
        scene.DrawRectangle(910, 780, 100, 100, branco);

        // "Inimigos" apenas para testar desenho
        scene.DrawRectangle(450, 280, 90, 90, vermelho);
        scene.DrawRectangle(1370, 280, 90, 90, vermelho);

        scene.DrawRectangle(700, 420, 70, 70, roxo);
        scene.DrawRectangle(1150, 420, 70, 70, roxo);

        // Linha central
        scene.DrawRectangle(500, 535, 920, 4, azul);

        // Envia a imagem para a TV
        scene.SubmitFlip(frameID);
        scene.FrameWait(frameID);
        scene.FrameBufferSwap();

        frameID++;
    }

    return 0;
}
