#pragma once
// common.h
#pragma once
#include <mutex>
#include <map>
#include <vector>
#include <string>
#include <queue>
#include <condition_variable>
#include <WinSock2.h> // za SOCKET

#include <fstream>

struct WorkerInfo {
    std::string ip;
    int port;
};

// Globalne strukture
extern std::vector<WorkerInfo> registeredWorkers;
extern std::map<int, int> workerLoad;
extern std::map<int, std::vector<std::string>> workerDataMap;
extern std::vector<std::string> sviPrimljeniPodaci;

// Thread pool
extern std::queue<SOCKET> taskQueue;


extern std::mutex queueMutex;
extern std::condition_variable queueCV;
extern int lastStorePort;


extern std::mutex logMutex;
void logMessage(const std::string& message);


