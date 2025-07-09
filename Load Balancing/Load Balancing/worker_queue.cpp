#include "worker_queue.h"
#include "worker.h"  // da može koristiti posaljiSyncDrugima itd.
#include <iostream>

#include <algorithm> // zbog std::find


std::queue<std::string> syncQueue;
std::mutex syncQueueMutex;
std::condition_variable syncQueueCV;

void workerThreadLoop(int port) {
    while (true) {
        std::string podatak;

        {
            std::unique_lock<std::mutex> lock(syncQueueMutex);
            syncQueueCV.wait(lock, [] { return !syncQueue.empty(); });

            podatak = syncQueue.front();
            syncQueue.pop();
        }

        {
            std::lock_guard<std::mutex> lock(bazaMutex);
            if (std::find(lokalnaBaza.begin(), lokalnaBaza.end(), podatak) == lokalnaBaza.end()) {
                lokalnaBaza.push_back(podatak);
                std::cout << "Saèuvan podatak: " << podatak << "\n";
            }
        }

        std::cout << " Poslata SYNC poruka iz workerThreadLoop za: " << podatak << "\n";
        posaljiSyncDrugima(podatak);

    }
}
