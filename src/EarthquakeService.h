#pragma once

#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include <thread>
#include <algorithm>
#include "json.hpp" 

struct Earthquake {
    std::string id;
    double mag = 0.0;
    std::string place;
    long long time_ms = 0;
    double lon = 0.0;
    double lat = 0.0;
    double depth_km = 0.0;
};

class EarthquakeService {
public:
    EarthquakeService();
    ~EarthquakeService();

    void startBackgroundService(int intervalSeconds);
    
    // NEW: API Server function
    void startAPIServer(int port);

    void stopService();
    void fetchNow();

    std::vector<Earthquake> getQuakes();
    std::string getStatus();

    void setMinMagnitude(float mag);
    void setSortByMag(bool enable);

private:
    void workerLoop(); 
    std::vector<Earthquake> parseGeoJSON(const std::string& jsonBody);

    std::mutex m_mutex;
    std::vector<Earthquake> m_quakes;
    std::string m_status = "Idle";

    std::atomic<bool> m_running{false};
    std::atomic<int> m_interval{15};
    std::atomic<float> m_minMag{0.0f};
    std::atomic<bool> m_sortByMag{true};
    
    std::thread m_thread;
};