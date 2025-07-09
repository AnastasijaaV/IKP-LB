#pragma once
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

class ThreadPool {
public:
    ThreadPool(int brojNiti = 4);
    ~ThreadPool();

    void dodajZadatak(std::function<void()> zadatak);

private:
    std::vector<std::thread> niti;
    std::queue<std::function<void()>> redZadataka;
    std::mutex queueMutex;
    std::condition_variable queueCV;
    std::atomic<bool> stop;

    void workerLoop();
};
