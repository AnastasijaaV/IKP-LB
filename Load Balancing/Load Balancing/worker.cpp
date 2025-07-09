#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "worker.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <string>
#include <sstream>
#include <vector>
#include <mutex>
#include <set>
#include <windows.h>
#include "worker_queue.h"
#include <condition_variable>

#pragma comment(lib, "ws2_32.lib")

std::vector<std::string> lokalnaBaza;
std::mutex bazaMutex;

std::set<int> poznatiPortovi;
int mojPort = 0;

const std::string LB_IP = "127.0.0.1";
const int LB_PORT = 5059;

BOOL WINAPI ConsoleHandler(DWORD signal) {
    if (signal == CTRL_CLOSE_EVENT || signal == CTRL_SHUTDOWN_EVENT || signal == CTRL_LOGOFF_EVENT) {
        SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == INVALID_SOCKET) return TRUE;

        sockaddr_in lbAddr;
        lbAddr.sin_family = AF_INET;
        lbAddr.sin_port = htons(LB_PORT);
        lbAddr.sin_addr.s_addr = inet_addr(LB_IP.c_str());

        if (connect(sock, (sockaddr*)&lbAddr, sizeof(lbAddr)) != SOCKET_ERROR) {
            std::string msg = "DEREGISTER " + std::to_string(mojPort);
            send(sock, msg.c_str(), static_cast<int>(msg.length()), 0);
            std::cout << "? Poslata odjava sa porta " << mojPort << "\n";
        }

        closesocket(sock);
    }
    return TRUE;
}

void posaljiSyncDrugima(const std::string& podatak) {
    for (int port : poznatiPortovi) {
        if (port == mojPort) continue;

        SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == INVALID_SOCKET) continue;

        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        if (connect(sock, (sockaddr*)&addr, sizeof(addr)) != SOCKET_ERROR) {
            std::string poruka = "SYNC " + podatak;
            send(sock, poruka.c_str(), static_cast<int>(poruka.length()), 0);
            std::cout << "?? Poslat SYNC podatak Workeru " << port << ": " << podatak << "\n";
        }

        closesocket(sock);
    }
}

void registerToLoadBalancer(int port) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) return;

    sockaddr_in lbAddr;
    lbAddr.sin_family = AF_INET;
    lbAddr.sin_port = htons(LB_PORT);
    lbAddr.sin_addr.s_addr = inet_addr(LB_IP.c_str());

    if (connect(sock, (sockaddr*)&lbAddr, sizeof(lbAddr)) == SOCKET_ERROR) {
        closesocket(sock);
        return;
    }

    std::ostringstream oss;
    oss << "REGISTER " << port;
    std::string message = oss.str();

    send(sock, message.c_str(), static_cast<int>(message.length()), 0);
    closesocket(sock);
}

void startWorkerServer(int port) {
    SOCKET serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock == INVALID_SOCKET) return;

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "? Bind gre\u0161ka\n";
        closesocket(serverSock);
        return;
    }

    listen(serverSock, SOMAXCONN);
    std::cout << "?? Worker slu\u0161a na portu " << port << "\n";

    while (true) {
        sockaddr_in clientAddr;
        int clientSize = sizeof(clientAddr);
        SOCKET clientSock = accept(serverSock, (sockaddr*)&clientAddr, &clientSize);

        if (clientSock != INVALID_SOCKET) {
            char buffer[1024] = { 0 };
            int bytesReceived = recv(clientSock, buffer, sizeof(buffer), 0);
            if (bytesReceived > 0) {
                buffer[bytesReceived] = '\0';
                std::string poruka(buffer);
                std::cout << "?? Primljeno: " << poruka << "\n";

                if (poruka.rfind("STORE ", 0) == 0 || poruka.rfind("SYNC ", 0) == 0) {
                    std::string podatak = poruka.substr(poruka.find(" ") + 1);

                    {
                        std::lock_guard<std::mutex> lock(bazaMutex);
                        if (std::find(lokalnaBaza.begin(), lokalnaBaza.end(), podatak) == lokalnaBaza.end()) {
                            lokalnaBaza.push_back(podatak);
                            std::cout << "? Sa\u010duvan podatak: " << podatak << "\n";
                        }
                    }

                    if (poruka.rfind("STORE ", 0) == 0) {
                        SOCKET lbSock = socket(AF_INET, SOCK_STREAM, 0);
                        if (lbSock != INVALID_SOCKET) {
                            sockaddr_in lbAddr;
                            lbAddr.sin_family = AF_INET;
                            lbAddr.sin_port = htons(LB_PORT);
                            lbAddr.sin_addr.s_addr = inet_addr(LB_IP.c_str());

                            if (connect(lbSock, (sockaddr*)&lbAddr, sizeof(lbAddr)) != SOCKET_ERROR) {
                                std::string potvrda = "STORED " + podatak + " " + std::to_string(port);
                                send(lbSock, potvrda.c_str(), static_cast<int>(potvrda.length()), 0);
                                std::cout << "?? Poslata potvrda LB-u: " << potvrda << "\n";
                            }

                            closesocket(lbSock);
                        }

                        {
                            std::lock_guard<std::mutex> lock(syncQueueMutex);
                            syncQueue.push(podatak);
                            syncQueueCV.notify_one();
                        }
                    }
                }
            }

            closesocket(clientSock);
        }
    }

    closesocket(serverSock);
}

int runWorker(int port) {
    mojPort = port;

    for (int p = 6000; p <= 6005; ++p) {
        poznatiPortovi.insert(p);
    }

    SetConsoleCtrlHandler(ConsoleHandler, TRUE);

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        return 1;

    std::thread t1(registerToLoadBalancer, port);
    std::thread t2(startWorkerServer, port);
    std::thread t3(workerThreadLoop, port);
    t3.detach();

    t1.join();
    t2.join();

    WSACleanup();
    return 0;
}
