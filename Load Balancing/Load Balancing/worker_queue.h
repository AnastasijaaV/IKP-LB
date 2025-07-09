#pragma once
#pragma once
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>

extern std::queue<std::string> syncQueue;
extern std::mutex syncQueueMutex;
extern std::condition_variable syncQueueCV;

void workerThreadLoop(int port);
