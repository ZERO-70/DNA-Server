#include <iostream>
#include <string>
#include <vector>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
// Link with the ws2_32.lib library
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <errno.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

using namespace std;

#define PORT 8080

// This function handles each connected client in a separate thread.
void handleClient(SOCKET clientSocket) {
    cout << "[*] Handling client connection." << endl;

    // 1. Send "OK 200" so the client knows to start sending numbers.
    const char* okMsg = "OK 200";
    int sendResult = send(clientSocket, okMsg, strlen(okMsg), 0);
    if (sendResult == SOCKET_ERROR) {
        cerr << "[!] Failed to send initial message to client." << endl;
        closesocket(clientSocket);
        return;
    }
    cout << "[*] Sent 'OK 200' to client." << endl;

    // 2. Continuously receive numbers from the client, sum them, and print them.
    int runningSum = 0;  // Holds the sum of all received numbers.
    char recvBuf[512];
    while (true) {
        memset(recvBuf, 0, sizeof(recvBuf));
        int bytesReceived = recv(clientSocket, recvBuf, sizeof(recvBuf) - 1, 0);
        if (bytesReceived <= 0) {
            // If recv returns 0 or negative, the client disconnected or an error occurred.
            cout << "[!] Client disconnected or read error." << endl;
            break;
        }
        recvBuf[bytesReceived] = '\0'; // Null-terminate the buffer.

        // 3. Convert the received data to an integer and add to our running sum.
        int receivedNumber = atoi(recvBuf); // naive parsing (works if each recv is a single integer).
        runningSum += receivedNumber;
        cout << "[*] Received: " << receivedNumber
            << " | Current sum: " << runningSum << endl;
    }

    // 4. Close this client's connection socket.
    closesocket(clientSocket);
    cout << "[*] Client handler thread exiting." << endl;
}

int main() {
#ifdef _WIN32
    // Initialize Winsock on Windows.
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        cerr << "[!] WSAStartup failed: " << iResult << endl;
        return 1;
    }
#endif

    // Create a socket for listening.
    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket == INVALID_SOCKET) {
#ifdef _WIN32
        cerr << "[!] Error at socket(): " << WSAGetLastError() << endl;
        WSACleanup();
#else
        perror("[!] socket failed");
#endif
        return 1;
    }

    // Set up the server address structure.
    sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;   // Listen on any network interface
    server.sin_port = htons(PORT);

    // Bind the socket to the address and port.
    if (bind(listenSocket, (sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
#ifdef _WIN32
        cerr << "[!] Bind failed with error: " << WSAGetLastError() << endl;
        closesocket(listenSocket);
        WSACleanup();
#else
        perror("[!] bind failed");
        closesocket(listenSocket);
#endif
        return 1;
    }

    // Start listening.
    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
#ifdef _WIN32
        cerr << "[!] Listen failed with error: " << WSAGetLastError() << endl;
        closesocket(listenSocket);
        WSACleanup();
#else
        perror("[!] listen failed");
        closesocket(listenSocket);
#endif
        return 1;
    }

    cout << "[*] Server listening on port " << PORT << "..." << endl;

    // Container for client handling threads.
    vector<thread> clientThreads;
    sockaddr_in client;
#ifdef _WIN32
    int clientSize = sizeof(client);
#else
    socklen_t clientSize = sizeof(client);
#endif

    // Main accept loop: accept new clients and spawn a handler thread for each.
    while (true) {
        SOCKET clientSocket = accept(listenSocket, (sockaddr*)&client, &clientSize);
        if (clientSocket == INVALID_SOCKET) {
#ifdef _WIN32
            cerr << "[!] Accept failed: " << WSAGetLastError() << endl;
#else
            perror("[!] accept failed");
#endif
            continue;
        }

        // Get the client's IP address.
        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client.sin_addr, clientIP, INET_ADDRSTRLEN);
        string clientAddress = string(clientIP) + ":" + to_string(ntohs(client.sin_port));
        cout << "[*] Accepted connection from " << clientAddress << endl;

        // Spawn a new thread to handle this client connection.
        clientThreads.emplace_back(thread(handleClient, clientSocket));
        clientThreads.back().detach();  // Detach so it runs independently.
    }

    // Clean up the listening socket (though on most servers, this line is never reached).
    closesocket(listenSocket);

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}




//ssh -i "D:\download stuff\dinodna_keypair.pem" ubuntu@56.228.11.151
