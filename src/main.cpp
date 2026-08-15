#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>

#include <orbis/libkernel.h>

// A assinatura do navegador existe no PS4, mas o header do OpenOrbis 0.5.4
// ainda a declara sem os argumentos que já foram documentados pela comunidade.
extern "C" int32_t sceSystemServiceHideSplashScreen(void);
extern "C" int32_t sceSystemServiceLaunchWebBrowser(const char* uri, void* param);

namespace
{
    const int SERVER_PORT = 8088;
    const char* WEB_ROOT = "/app0/Boyceta-ps4";

    bool writeAll(int fd, const void* data, size_t size)
    {
        const char* bytes = static_cast<const char*>(data);
        size_t sent = 0;
        while (sent < size)
        {
            ssize_t result = write(fd, bytes + sent, size - sent);
            if (result <= 0) return false;
            sent += static_cast<size_t>(result);
        }
        return true;
    }

    void sendError(int client, int status, const char* reason)
    {
        char body[256];
        int bodyLength = snprintf(body, sizeof(body),
            "<!doctype html><html><body style='background:#03050a;color:#fff'>"
            "Erro %d: %s</body></html>", status, reason);

        char header[512];
        int headerLength = snprintf(header, sizeof(header),
            "HTTP/1.1 %d %s\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Content-Length: %d\r\n"
            "Cache-Control: no-store\r\n"
            "Connection: close\r\n\r\n",
            status, reason, bodyLength);

        writeAll(client, header, static_cast<size_t>(headerLength));
        writeAll(client, body, static_cast<size_t>(bodyLength));
    }

    const char* getMimeType(const char* path)
    {
        const char* extension = strrchr(path, '.');
        if (!extension) return "application/octet-stream";
        if (strcmp(extension, ".html") == 0) return "text/html; charset=utf-8";
        if (strcmp(extension, ".css") == 0) return "text/css; charset=utf-8";
        if (strcmp(extension, ".js") == 0) return "application/javascript; charset=utf-8";
        return "application/octet-stream";
    }

    const char* resolveAsset(const char* uri)
    {
        if (strcmp(uri, "/") == 0 || strcmp(uri, "/index.html") == 0)
            return "/app0/Boyceta-ps4/index.html";
        if (strcmp(uri, "/style.css") == 0)
            return "/app0/Boyceta-ps4/style.css";
        if (strcmp(uri, "/script.js") == 0)
            return "/app0/Boyceta-ps4/script.js";
        return nullptr;
    }

    void serveFile(int client, const char* path)
    {
        struct stat info;
        if (stat(path, &info) != 0 || info.st_size <= 0)
        {
            sendError(client, 404, "Arquivo nao encontrado");
            return;
        }

        int file = open(path, O_RDONLY);
        if (file < 0)
        {
            sendError(client, 500, "Falha ao abrir arquivo");
            return;
        }

        char header[768];
        int headerLength = snprintf(header, sizeof(header),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %lld\r\n"
            "Cache-Control: no-cache, no-store, must-revalidate\r\n"
            "Pragma: no-cache\r\n"
            "Expires: 0\r\n"
            "Connection: close\r\n\r\n",
            getMimeType(path), static_cast<long long>(info.st_size));

        if (!writeAll(client, header, static_cast<size_t>(headerLength)))
        {
            close(file);
            return;
        }

        char buffer[16384];
        for (;;)
        {
            ssize_t count = read(file, buffer, sizeof(buffer));
            if (count <= 0) break;
            if (!writeAll(client, buffer, static_cast<size_t>(count))) break;
        }
        close(file);
    }

    void handleClient(int client)
    {
        char request[4096];
        ssize_t count = read(client, request, sizeof(request) - 1);
        if (count <= 0) return;
        request[count] = '\0';

        char method[16] = {};
        char uri[512] = {};
        if (sscanf(request, "%15s %511s", method, uri) != 2)
        {
            sendError(client, 400, "Requisicao invalida");
            return;
        }

        if (strcmp(method, "GET") != 0 && strcmp(method, "HEAD") != 0)
        {
            sendError(client, 405, "Metodo nao permitido");
            return;
        }

        char* query = strchr(uri, '?');
        if (query) *query = '\0';

        const char* path = resolveAsset(uri);
        if (!path)
        {
            sendError(client, 404, "Arquivo nao encontrado");
            return;
        }

        serveFile(client, path);
    }

    int createServer()
    {
        int server = socket(AF_INET, SOCK_STREAM, 0);
        if (server < 0) return -1;

        int enabled = 1;
        setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled));

        struct sockaddr_in address;
        memset(&address, 0, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(SERVER_PORT);

        if (bind(server, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) != 0)
        {
            close(server);
            return -1;
        }
        if (listen(server, 8) != 0)
        {
            close(server);
            return -1;
        }
        return server;
    }
}

int main(void)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    sceSystemServiceHideSplashScreen();

    // Confirma que os tres arquivos do jogo realmente foram instalados.
    struct stat assetInfo;
    if (stat("/app0/Boyceta-ps4/index.html", &assetInfo) != 0 ||
        stat("/app0/Boyceta-ps4/style.css", &assetInfo) != 0 ||
        stat("/app0/Boyceta-ps4/script.js", &assetInfo) != 0)
    {
        for (;;) sceKernelSleep(1);
    }

    int server = createServer();
    if (server < 0)
    {
        for (;;) sceKernelSleep(1);
    }

    // O servidor ja esta escutando antes de o navegador interno ser aberto.
    sceSystemServiceLaunchWebBrowser("http://127.0.0.1:8088/index.html", nullptr);

    for (;;)
    {
        struct sockaddr_in clientAddress;
        socklen_t clientLength = sizeof(clientAddress);
        int client = accept(server, reinterpret_cast<struct sockaddr*>(&clientAddress), &clientLength);
        if (client < 0)
        {
            sceKernelUsleep(10000);
            continue;
        }

        handleClient(client);
        shutdown(client, SHUT_RDWR);
        close(client);
    }
}

