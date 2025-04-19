// simple_server.cpp  ── a minimal cross‑platform demo
#include <iostream>
#include <string>
#include <vector>
#include <thread>
using namespace std;
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib,"ws2_32.lib")
using socklen_t = int;          // Windows doesn’t define it
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <errno.h>
#define SOCKET         int
#define INVALID_SOCKET -1
#define SOCKET_ERROR   -1
#define closesocket    close
#endif

const uint16_t PORT = 8080;
bool initSockets()
{
#ifdef _WIN32
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
#else
    return true;                   // nothing to do on Linux / macOS
#endif
}
void cleanupSockets()
{
#ifdef _WIN32
    WSACleanup();
#endif
}
void handleClient(SOCKET client)
{
    cout << "[*] New client thread started\n";

    const char* hello = "OK 200";
    if (send(client, hello, strlen(hello), 0) == SOCKET_ERROR) {
        std::cerr << "[!] Could not send greeting, bye\n";
        closesocket(client);
        return;
    }

    int sum = 0;
    char buf[512] = { 0 };

    while (true) {
        int n = recv(client, buf, sizeof(buf) - 1, 0);
        if (n <= 0) break;                     // client left or error

        buf[n] = '\0';                         // make it a C‑string
        sum += std::atoi(buf);
        cout << "    received " << buf << "   (running sum = " << sum << ")\n";
    }

    closesocket(client);
    cout << "[*] Client thread finished\n";
}
SOCKET createListeningSocket()
{
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET) return INVALID_SOCKET;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(s, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        cerr << "[!] bind() failed\n";
        closesocket(s);
        return INVALID_SOCKET;
    }
    if (listen(s, SOMAXCONN) == SOCKET_ERROR) {
        cerr << "[!] listen() failed\n";
        closesocket(s);
        return INVALID_SOCKET;
    }
    return s;
}
void acceptClients(SOCKET listener)
{
    cout << "[*] Listening on port " << PORT << " …"<<endl;

    sockaddr_in clientAddr{};
    socklen_t addrLen = sizeof(clientAddr);

    while (true) {
        SOCKET client = accept(listener,(sockaddr*)&clientAddr,&addrLen);
        if (client == INVALID_SOCKET) {
            perror("[!] accept");
            continue;
        }

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, ip, sizeof(ip));
        cout << "[*] + connection from "<< ip << ":" << ntohs(clientAddr.sin_port) << endl;

        thread(handleClient, client).detach();   // fire & forget
    }
}

int main()
{
    if (!initSockets()) {
        cerr << "[!] Could not initialise sockets\n";
        return 1;
    }

    SOCKET listener = createListeningSocket();
    if (listener == INVALID_SOCKET) {
        cleanupSockets();
        return 1;
    }

    acceptClients(listener);          // never returns

    closesocket(listener);            // not reached, but tidy
    cleanupSockets();
    return 0;
}
