#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "load_balancer.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <sstream>
#include <map>
#include <algorithm>
#include "common.h"
#include "thread_pool.h"


#include <psapi.h>  // ⬅️ Dodato

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "psapi.lib")  // ⬅️ Dodato

const int LB_PORT = 5059;


void redistribuirajPodatke(const std::vector<std::string>& podaci) {
    for (const auto& podatak : podaci) {
        if (podatak.empty()) continue;
        if (registeredWorkers.empty()) return;

        WorkerInfo target = registeredWorkers[0];
        int minLoad = workerLoad[target.port];

        for (const auto& wr : registeredWorkers) {
            int load = workerLoad[wr.port];
            if (load < minLoad) {
                minLoad = load;
                target = wr;
            }
        }

        SOCKET syncSock = socket(AF_INET, SOCK_STREAM, 0);
        if (syncSock == INVALID_SOCKET) continue;

        sockaddr_in wrAddr;
        wrAddr.sin_family = AF_INET;
        wrAddr.sin_port = htons(target.port);
        wrAddr.sin_addr.s_addr = inet_addr(target.ip.c_str());

        if (connect(syncSock, (sockaddr*)&wrAddr, sizeof(wrAddr)) != SOCKET_ERROR) {
            std::string syncMsg = "SYNC " + podatak;
            send(syncSock, syncMsg.c_str(), static_cast<int>(syncMsg.length()), 0);
            logMessage("?? Redistribuiran podatak Workeru " + std::to_string(target.port) + ": " + podatak);

            workerLoad[target.port]++;
            workerDataMap[target.port].push_back(podatak);
        }

        closesocket(syncSock);
    }
}

void handleClient(SOCKET clientSock, sockaddr_in clientAddr) {
    LARGE_INTEGER start, end, freq;  // ⬅️ Dodato
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);

    std::cout << " Nit " << std::this_thread::get_id() << " je započela obradu\n";

    char buffer[1024];
    int bytesReceived = recv(clientSock, buffer, sizeof(buffer) - 1, 0);
    if (bytesReceived <= 0) {
        closesocket(clientSock);
        return;
    }

    buffer[bytesReceived] = '\0';
    std::string msg(buffer);
    logMessage("?? Primljeno od klijenta: " + msg);


    if (msg.rfind("REGISTER ", 0) == 0) {
        int port = std::stoi(msg.substr(9));
        WorkerInfo newWorker;
        newWorker.ip = inet_ntoa(clientAddr.sin_addr);
        newWorker.port = port;

        registeredWorkers.push_back(newWorker);
        logMessage("? Worker registrovan: IP=" + newWorker.ip + ", Port=" + std::to_string(port));


        workerLoad.clear();
        workerDataMap.clear();
        std::map<int, std::vector<std::string>> novaPodela;

        int brojWorkera = registeredWorkers.size();
        int validIndex = 0;
        for (const auto& podatak : sviPrimljeniPodaci) {
            if (podatak.empty()) continue;
            int workerIndex = validIndex % brojWorkera;
            int targetPort = registeredWorkers[workerIndex].port;
            novaPodela[targetPort].push_back(podatak);
            validIndex++;
        }

        for (const auto& wr : registeredWorkers) {
            int portWR = wr.port;
            workerLoad[portWR] = 0;

            for (const auto& podatak : novaPodela[portWR]) {
                SOCKET syncSock = socket(AF_INET, SOCK_STREAM, 0);
                if (syncSock == INVALID_SOCKET) continue;

                sockaddr_in wrAddr;
                wrAddr.sin_family = AF_INET;
                wrAddr.sin_port = htons(portWR);
                wrAddr.sin_addr.s_addr = inet_addr(wr.ip.c_str());

                if (connect(syncSock, (sockaddr*)&wrAddr, sizeof(wrAddr)) != SOCKET_ERROR) {
                    std::string syncMsg = "SYNC " + podatak;
                    send(syncSock, syncMsg.c_str(), static_cast<int>(syncMsg.length()), 0);
                    logMessage("?? Redistribuiran podatak Workeru " + std::to_string(portWR) + ": " + podatak);


                    workerLoad[portWR]++;
                    workerDataMap[portWR].push_back(podatak);
                }

                closesocket(syncSock);
            }
        }
    }

    else if (msg.rfind("DEREGISTER ", 0) == 0) {
        int port = std::stoi(msg.substr(11));
        registeredWorkers.erase(std::remove_if(registeredWorkers.begin(), registeredWorkers.end(),
            [port](const WorkerInfo& wr) { return wr.port == port; }), registeredWorkers.end());

        workerLoad.erase(port);
        if (lastStorePort == port) lastStorePort = 0;

        logMessage("? Worker odjavljen sa porta: " + std::to_string(port));


        auto it = workerDataMap.find(port);
        if (it != workerDataMap.end()) {
            redistribuirajPodatke(it->second);
            workerDataMap.erase(it);
        }
    }

    else if (msg.rfind("STORED ", 0) == 0) {
        std::istringstream iss(msg);
        std::string keyword, podatak;
        int port;
        iss >> keyword >> podatak >> port;

        if (workerLoad.count(port) && !podatak.empty()) {
            workerLoad[port]++;
            workerDataMap[port].push_back(podatak);
            logMessage("? Ažuriran broj podataka za Worker port " + std::to_string(port) + ": " + std::to_string(workerLoad[port]));

        }
    }

    else if (msg.rfind("STORE ", 0) == 0) {
        std::string podatak = msg.substr(6);
        if (podatak.empty()) {
            std::cerr << "?? Ignorisana STORE poruka bez podatka!\n";
            closesocket(clientSock);
            return;
        }

        sviPrimljeniPodaci.push_back(podatak);

        if (registeredWorkers.empty()) {
            std::cerr << "?? Nema dostupnih Workera!\n";
            closesocket(clientSock);
            return;
        }

        WorkerInfo target = registeredWorkers[0];
        int minLoad = workerLoad[target.port];

        for (const auto& wr : registeredWorkers) {
            int load = workerLoad[wr.port];
            if (load < minLoad) {
                minLoad = load;
                target = wr;
            }
        }

        SOCKET workerSock = socket(AF_INET, SOCK_STREAM, 0);
        if (workerSock == INVALID_SOCKET) {
            std::cerr << "? Greška pri otvaranju socket-a ka Workeru\n";
            closesocket(clientSock);
            return;
        }

        sockaddr_in workerAddr;
        workerAddr.sin_family = AF_INET;
        workerAddr.sin_port = htons(target.port);
        workerAddr.sin_addr.s_addr = inet_addr(target.ip.c_str());

        if (connect(workerSock, (sockaddr*)&workerAddr, sizeof(workerAddr)) == SOCKET_ERROR) {
            std::cerr << "? Ne mogu da se povežem sa Worker-om " << target.ip << ":" << target.port << "\n";
            closesocket(workerSock);
        }
        else {
            std::string forwardMsg = "STORE " + podatak;
            send(workerSock, forwardMsg.c_str(), static_cast<int>(forwardMsg.length()), 0);
            logMessage("?? Prosleđeno Worker-u " + std::to_string(target.port) + ": " + forwardMsg);

            lastStorePort = target.port;
        }

        closesocket(workerSock);
    }

    std::cout << "?? Trenutno stanje opterećenja:\n";
    for (auto& pair : workerLoad) {
        logMessage("  ? Worker " + std::to_string(pair.first) + ": " + std::to_string(pair.second) + " podat(a)ka");

    }
    std::cout << " Nit " << std::this_thread::get_id() << " je završila obradu\n";

    // ⬇️ Izmereno vreme i RAM
    QueryPerformanceCounter(&end);
    double elapsedTime = (double)(end.QuadPart - start.QuadPart) * 1000000 / freq.QuadPart;
    std::cout << "🕒 Vreme obrade klijenta: " << elapsedTime << " mikrosekundi\n";

    PROCESS_MEMORY_COUNTERS memInfo;
    GetProcessMemoryInfo(GetCurrentProcess(), &memInfo, sizeof(memInfo));
    std::cout << "💾 Load Balancer RAM: " << memInfo.WorkingSetSize / 1024 << " KB\n";
    closesocket(clientSock);
}

void startLoadBalancer() {
    ThreadPool pool(4);  // Pokrećemo thread pool sa 4 niti

    SOCKET serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock == INVALID_SOCKET) {
        std::cerr << "Greška pri otvaranju socket-a\n";
        return;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(LB_PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind greška\n";
        closesocket(serverSock);
        return;
    }

    listen(serverSock, SOMAXCONN);
    std::cout << "?? Load Balancer sluša na portu " << LB_PORT << "\n";

    while (true) {
        sockaddr_in clientAddr;
        int clientSize = sizeof(clientAddr);
        SOCKET clientSock = accept(serverSock, (sockaddr*)&clientAddr, &clientSize);

        if (clientSock != INVALID_SOCKET) {
            pool.dodajZadatak([clientSock, clientAddr]() {
                handleClient(clientSock, clientAddr);
                });
        }
    }

    closesocket(serverSock);
}


int runLoadBalancer() {
    WSADATA wsaData;
    int wsaInit = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsaInit != 0) {
        std::cerr << "WSAStartup greška: " << wsaInit << "\n";
        return 1;
    }

    startLoadBalancer();
    WSACleanup();
    return 0;
}
