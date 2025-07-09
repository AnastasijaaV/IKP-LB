#pragma once
#include <vector>
#include <string>
#include <mutex>
#include <set>

// Promenljive koje su definisane u worker.cpp i koje treba deliti
extern std::vector<std::string> lokalnaBaza;
extern std::mutex bazaMutex;
extern std::set<int> poznatiPortovi;
extern int mojPort;

// Funkcije koje koristiš i u drugim fajlovima
void posaljiSyncDrugima(const std::string& podatak);
void registerToLoadBalancer(int port);
void startWorkerServer(int port);
int runWorker(int port);
