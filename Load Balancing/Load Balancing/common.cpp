// common.cpp
#include "common.h"
#include <iostream>
#include <fstream>
#include <mutex>

// Globalne strukture definisane ovde
std::vector<WorkerInfo> registeredWorkers;
std::map<int, int> workerLoad;
std::map<int, std::vector<std::string>> workerDataMap;
std::vector<std::string> sviPrimljeniPodaci;

// Thread pool
std::queue<SOCKET> taskQueue;



std::mutex queueMutex;
std::condition_variable queueCV;

int lastStorePort = 0;

std::mutex logMutex;

void logMessage(const std::string& message) {
    std::lock_guard<std::mutex> lock(logMutex);
    std::ofstream logFile("log.txt", std::ios::app);  // append mode
    if (logFile.is_open()) {
        logFile << message << std::endl;
    }
    std::cout << message << std::endl; // i dalje prikazuje u konzoli
}

