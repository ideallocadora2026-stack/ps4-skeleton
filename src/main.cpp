#include <orbis/libkernel.h>
#include <orbis/SystemService.h>

int main(void)
{
    sceSystemServiceHideSplashScreen();

    while (1)
    {
        sceKernelSleep(1);
    }

    return 0;
}
