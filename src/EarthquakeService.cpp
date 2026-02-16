#include "EarthquakeService.h"
#include "httplib.h" 
#include <iostream>
#include <chrono>

EarthquakeService::EarthquakeService() {}

EarthquakeService::~EarthquakeService() {
    stopService();
}

void EarthquakeService::startBackgroundService(int intervalSeconds) {
    if (m_running) return;
    m_interval = intervalSeconds;
    m_running = true;
    m_thread = std::thread(&EarthquakeService::workerLoop, this);
}

// NEW: API Server Implementation
void EarthquakeService::startAPIServer(int port) {
    // We detach the thread so it runs independently
    std::thread([this, port]() {
        httplib::Server svr;

        // Define /status endpoint
        svr.Get("/status", [this](const httplib::Request&, httplib::Response& res) {
            std::lock_guard<std::mutex> lock(m_mutex);
            
            // Create a JSON string manually (or use nlohmann::json)
            std::string json = "{\"status\": \"running\", \"count\": " + std::to_string(m_quakes.size());
            
            if (!m_quakes.empty()) {
                const auto& q = m_quakes[0];
                json += ", \"latest\": {\"place\": \"" + q.place + "\", \"mag\": " + std::to_string(q.mag) + "}";
            }
            json += "}";

            res.set_content(json, "application/json");
        });

        std::cout << "Starting API Server on port " << port << "..." << std::endl;
        svr.listen("0.0.0.0", port);
    }).detach();
}

void EarthquakeService::stopService() {
    m_running = false;
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void EarthquakeService::setMinMagnitude(float mag) { m_minMag = mag; }
void EarthquakeService::setSortByMag(bool enable) { m_sortByMag = enable; }

std::vector<Earthquake> EarthquakeService::getQuakes() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_quakes;
}

std::string EarthquakeService::getStatus() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_status;
}

void EarthquakeService::fetchNow() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_status = "Fetching...";
    }

    httplib::Client cli("earthquake.usgs.gov", 443);
    cli.set_follow_location(true);

    auto res = cli.Get("/earthquakes/feed/v1.0/summary/all_day.geojson");

    if (res && res->status == 200) {
        auto parsed = parseGeoJSON(res->body);
        
        if (m_sortByMag) {
            std::sort(parsed.begin(), parsed.end(), [](const Earthquake& a, const Earthquake& b) {
                return a.mag > b.mag;
            });
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        m_quakes = std::move(parsed);
        m_status = "Updated: " + std::to_string(m_quakes.size()) + " quakes";
    } else {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_status = "Error: Connection failed";
    }
}

void EarthquakeService::workerLoop() {
    while (m_running) {
        fetchNow();
        int sleepTime = m_interval.load() * 10; 
        for (int i = 0; i < sleepTime && m_running; i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

std::vector<Earthquake> EarthquakeService::parseGeoJSON(const std::string& body) {
    std::vector<Earthquake> results;
    using json = nlohmann::json;

    try {
        auto j = json::parse(body);
        if (!j.contains("features") || !j["features"].is_array()) return results;
        float minM = m_minMag.load();

        for (const auto& f : j["features"]) {
            Earthquake e;
            if (f.contains("properties")) {
                auto& p = f["properties"];
                if(!p["mag"].is_null()) e.mag = p["mag"].get<double>();
                if(!p["place"].is_null()) e.place = p["place"].get<std::string>();
                if(!p["time"].is_null()) e.time_ms = p["time"].get<long long>();
            }
            if (f.contains("geometry") && f["geometry"]["coordinates"].is_array()) {
                auto& c = f["geometry"]["coordinates"];
                if (c.size() >= 3) {
                    e.lon = c[0].get<double>();
                    e.lat = c[1].get<double>();
                    e.depth_km = c[2].get<double>();
                }
            }
            if (f.contains("id")) e.id = f["id"].get<std::string>();

            if (e.mag >= minM) results.push_back(e);
        }
    } catch (...) {}
    return results;
}