#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "client.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>

#pragma comment(lib, "ws2_32.lib")

const std::string LB_IP = "127.0.0.1";
const int LB_PORT = 5059;
int runClient() {
    WSADATA wsaData;
    int wsaInit = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsaInit != 0) {
        std::cerr << "WSAStartup greška: " << wsaInit << "\n";
        return 1;
    }

    std::string input;
    std::cout << "Unesi podatak za slanje (ili 'exit' za kraj):\n";

    while (true) {
        std::cout << "> ";
        std::getline(std::cin, input);
        if (input == "exit") break;

        SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == INVALID_SOCKET) {
            std::cerr << "Greška pri kreiranju socket-a\n";
            continue;
        }

        sockaddr_in lbAddr;
        lbAddr.sin_family = AF_INET;
        lbAddr.sin_port = htons(LB_PORT);
        lbAddr.sin_addr.s_addr = inet_addr(LB_IP.c_str());

        if (connect(sock, (sockaddr*)&lbAddr, sizeof(lbAddr)) == SOCKET_ERROR) {
            std::cerr << "? Ne mogu da se povežem sa Load Balancer-om\n";
            closesocket(sock);
            continue;
        }

        std::string message = "STORE " + input;
        send(sock, message.c_str(), static_cast<int>(message.length()), 0);

        closesocket(sock);
    }

    WSACleanup();
    return 0;
}
