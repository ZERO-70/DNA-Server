// simple_server.cpp  ── a minimal cross‑platform demo
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <fstream>
#include "jobs.h"
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
ifstream virusfile;
static Job virusdata;
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

    // sending virusdata to client
    cout << "[*] Sending virus data to client\n";
    string obj = virusdata.serialize();
    size_t objsize = obj.size();
    //sending the size of the object
    int sent = send(client, (char*)&objsize, sizeof(objsize), 0);
    if (sent == SOCKET_ERROR || sent != sizeof(objsize)) {
        cerr << "[!] send() failed or incomplete\n";
        closesocket(client);
        return;
    }
    //sending the object
    if (send(client, obj.c_str(), objsize, 0) == SOCKET_ERROR) {
        cerr << "[!] send() failed\n";
        closesocket(client);
        return;
    }


    while (true) {
        if (q.empty()) {
            if (!loadJobs()) {
                cerr << "No jobs available\n";
                closesocket(client);
                return;
            }
        }

        Job job = q.back();
        q.pop_back();

        cout << "[*] Sending job to client: " << job.chrom << " " << job.start << endl;
        cout << "seqence :" << job.seq.substr(job.seq.size() - 10, 10) << endl;
        cout << "seqence len :" << job.seq.size() << endl;
        string obj = job.serialize();
        size_t objsize = obj.size();
        //sending the size of the object

        int sent = send(client, (char*)&objsize, sizeof(objsize), 0);
        if (sent == SOCKET_ERROR || sent != sizeof(objsize)) {
            cerr << "[!] send() failed or incomplete\n";
            closesocket(client);
            return;
        }

        //sending the object
        if (send(client, obj.c_str(), objsize, 0) == SOCKET_ERROR) {
            cerr << "[!] send() failed\n";
            closesocket(client);
            return;
        }

        cout << "[*] Waiting for client result...\n";

        // First receive the size of the result
        size_t resultSize;
        int n = recv(client, (char*)&resultSize, sizeof(resultSize), 0);
        if (n <= 0 || n != sizeof(resultSize)) {
            cerr << "[!] Failed to receive result size or client disconnected\n";
            break;
        }

        // Now receive the result data string
        string result(resultSize, '\0');
        size_t received = 0;
        while (received < resultSize) {
            int bytes = recv(client, &result[received], resultSize - received, 0);
            if (bytes <= 0) {
                cerr << "[!] Failed to receive full result string or client disconnected\n";
                break;
            }
            received += bytes;
        }

        cout << "[*] Received result:\n" << result << endl;
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
    cout << "[*] Listening on port " << PORT << " …" << endl;

    sockaddr_in clientAddr{};
    socklen_t addrLen = sizeof(clientAddr);

    while (true) {
        SOCKET client = accept(listener, (sockaddr*)&clientAddr, &addrLen);
        if (client == INVALID_SOCKET) {
            perror("[!] accept");
            continue;
        }

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, ip, sizeof(ip));
        cout << "[*] + connection from " << ip << ":" << ntohs(clientAddr.sin_port) << endl;

        thread(handleClient, client).detach();   // fire & forget
    }
}
void loadVirus() {
    virusfile.open("Human adenovirus 2, complete genome.fasta");
    string line;
    while (getline(virusfile, line)) {
        if (line[0] == '>') {
            virusdata.chrom = line;
            continue;
        }
        virusdata.seq += line;
    }
    virusdata.start = 0;
    cout << "-loaded virus data-" << virusdata.seq.size() << endl;
}

int main()
{
    loadVirus();
    preparejobs();
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
