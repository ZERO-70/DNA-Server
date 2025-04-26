#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <fstream>
#include "TaskManager.h"
#include <mutex>
#include <memory>
using namespace std;
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib,"ws2_32.lib")
using socklen_t = int;          // Windows doesn't define it
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

// Socket class to wrap socket functionality
class Socket {
private:
    SOCKET socketFd;
    string ipAddress;
    uint16_t port;

public:
    Socket(SOCKET fd, const string& ip, uint16_t p) : socketFd(fd), ipAddress(ip), port(p) {}
    
    ~Socket() {
        // Close socket if still open
        if (socketFd != INVALID_SOCKET) {
            closesocket(socketFd);
            socketFd = INVALID_SOCKET;
        }
    }

    SOCKET getSocket() const { return socketFd; }
    string getIpAddress() const { return ipAddress; }
    uint16_t getPort() const { return port; }
    
    // Prevent copying
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
};

// Functions to manage the socket vector
vector<shared_ptr<Socket>> socketRefs;
mutex socketRefs_mutex;

void addSocket(shared_ptr<Socket> socket) {
    lock_guard<mutex> lock(socketRefs_mutex);
    socketRefs.push_back(socket);
    cout << "[*] Device connected! Total connected devices: " << socketRefs.size() << endl;
}

void removeSocket(shared_ptr<Socket> socket) {
    lock_guard<mutex> lock(socketRefs_mutex);
    auto it = find_if(socketRefs.begin(), socketRefs.end(),
        [&socket](const shared_ptr<Socket>& s) {
            return s->getSocket() == socket->getSocket();
        });
    
    if (it != socketRefs.end()) {
        socketRefs.erase(it);
        cout << "[*] Device disconnected! Total connected devices: " << socketRefs.size() << endl;
    }
}

size_t getConnectedDeviceCount() {
    lock_guard<mutex> lock(socketRefs_mutex);
    return socketRefs.size();
}

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

void handleClient(shared_ptr<Socket> clientSocket)
{
    cout << "[*] New client thread started\n";

    while (true) {
        if (taskQ.empty()) { // agar task queue khali ho jaye to or task load karo 
            if (!loadTasks()) {// or agar or tasks hi nhi bache to return kar do, matlab client se disconnect
                cerr << "No jobs available\n";
                removeSocket(clientSocket);
                return;
            }
        }

        Task task; // task ka object
        {
            lock_guard<std::mutex> lk(jobs_mutex); // mutex lock karo
            task = taskQ.front();  // taskQ se front wala task lo
            taskQ.pop(); // taskQ se front wala task hata do
        }   
        cout << "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@" << endl;
        cout << "Sending task to client: " << task.human.chrom << " " << task.human.start << endl;
        cout << "seqence :" << task.human.seq.substr(task.human.seq.size() - 10, 10) << endl;
        cout << "seqence len :" << task.human.seq.size() << endl;
        cout << "virus chunk :" << task.vIndex << endl;
        cout << "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@" << endl;

        string obj = task.serialize(); // task ko serialize karo
        size_t objsize = obj.size();

        // task ka size send karo
        int sent = send(clientSocket->getSocket(), (char*)&objsize, sizeof(objsize), 0);
        if (sent == SOCKET_ERROR || sent != sizeof(objsize)) {
            cerr << "[!] send() failed or incomplete\n";
            removeSocket(clientSocket);
            return;
        }

        // task ka data send karo
        if (send(clientSocket->getSocket(), obj.c_str(), objsize, 0) == SOCKET_ERROR) {
            cerr << "[!] send() failed\n";
            removeSocket(clientSocket);
            return;
        }

        cout << "[*] Waiting for client result...\n";

        // result ka size recieve karo
        uint32_t netLen;
        int n = recv(clientSocket->getSocket(), (char*)&netLen, sizeof(netLen), MSG_WAITALL);
        if (n <= 0 || n != sizeof(netLen)) { // agar task ka size nahi mila ya client disconnect ho gaya
            cerr << "[!] Failed to receive result size or client disconnected\n";
            {
                lock_guard<std::mutex> lk(jobs_mutex); // mutex lock karo
                taskQ.emplace(task); // task ko wapas queue me daal do
                cerr << "[!] placing back the pending task\n";
            }
            break;
        }

        uint32_t resultSize = ntohl(netLen); // convert to host byte order
        // kya kuch or ajeed data to nhi wapis aaya
        if (resultSize > 1000000) { // sanity check (1MB max)
            cerr << "[!] Suspicious result size, aborting\n";
            cout << "result size : " << resultSize << endl;
            {
                lock_guard<std::mutex> lk(jobs_mutex); // mutex lock karo
                taskQ.emplace(task); // task ko wapas queue me daal do
            }
            break;
        }   

        // result ka data recieve karo
        string result(resultSize, '\0');
        size_t received = 0;
        bool success = true;
        while (received < resultSize) {
            int bytes = recv(clientSocket->getSocket(), &result[received], resultSize - received, 0);
            if (bytes <= 0) {
                cerr << "[!] Failed to receive full result string or client disconnected\n";
                success = false;
                {
                    lock_guard<std::mutex> lk(jobs_mutex); // mutex lock karo
                    taskQ.emplace(task); // task ko wapas queue me daal do
                    cerr << "[!] placing back the pending task -----\n";
                }
                // breaking from innner loop
                break;
            }
            received += bytes;
        }
        // breaking from outer loop
        if (!success) { // safety ke liye dobara se task ko queue me daal do
            lock_guard<std::mutex> lk(jobs_mutex);
            taskQ.emplace(task);
            cerr << "[!] placing back the pending task -----\n";
            break;
        }       // now breaks outer loop too

        cout << "[*] Received result:\n" << result << endl;
    }
    removeSocket(clientSocket);
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
        perror("[!] bind");             
        std::cerr << "errno = " << errno << '\n';
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

        auto clientSocket = make_shared<Socket>(client, ip, ntohs(clientAddr.sin_port));
        addSocket(clientSocket);

        thread(handleClient, clientSocket).detach();   // fire & forget
    }
}

int main()
{
    loadVirus();
    makeVirusChunks();
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

    acceptClients(listener);          

    closesocket(listener);            
    cleanupSockets();
    return 0;
}
