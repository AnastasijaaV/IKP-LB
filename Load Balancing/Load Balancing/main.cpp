#include "worker.h"
#include "load_balancer.h"
#include "client.h"
#include <iostream>

int main() {
    int izbor;
    std::cout << "Izaberi komponentu:\n";
    std::cout << "1. Pokreni Load Balancer\n";
    std::cout << "2. Pokreni Worker\n";
    std::cout << "3. Pokreni Klijent\n";
    std::cout << "Unos: ";
    std::cin >> izbor;
    std::cin.ignore();

    if (izbor == 1) {
        return runLoadBalancer();
    }
    else if (izbor == 2) {
        int port;
        std::cout << "Unesi port za Workera (npr. 6000): ";
        std::cin >> port;
        std::cin.ignore();
        return runWorker(port);
    }
    else if (izbor == 3) {
        return runClient();
    }
    else {
        std::cout << "Nepoznat izbor.\n";
        return 0;
    }
}
