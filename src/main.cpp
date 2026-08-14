#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <orbis/libkernel.h>
#include <orbis/SystemService.h>

int main(void) {
    const char *args[] = {
        "com.playstation.webkit",
        "file:///app0/index.html",
        NULL
    };
    sceSystemServiceLoadExec("com.playstation.webkit", args);
    while(1) { sceKernelSleep(1); }
    return 0;
}
