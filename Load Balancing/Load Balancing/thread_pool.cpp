#include "thread_pool.h"

ThreadPool::ThreadPool(int brojNiti) : stop(false) {
    for (int i = 0; i < brojNiti; ++i) {
        niti.emplace_back(&ThreadPool::workerLoop, this);
    }
}

ThreadPool::~ThreadPool() {
    stop = true;
    queueCV.notify_all();

    for (auto& t : niti) {
        if (t.joinable()) t.join();
    }
}

void ThreadPool::dodajZadatak(std::function<void()> zadatak) {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        redZadataka.push(zadatak);
    }
    queueCV.notify_one();
}

void ThreadPool::workerLoop() {
    while (!stop) {
        std::function<void()> zadatak;

        {
            std::unique_lock<std::mutex> lock(queueMutex);
            queueCV.wait(lock, [&] { return stop || !redZadataka.empty(); });

            if (stop && redZadataka.empty()) return;

            zadatak = redZadataka.front();
            redZadataka.pop();
        }

        if (zadatak) {
            zadatak();
        }
    }
}
