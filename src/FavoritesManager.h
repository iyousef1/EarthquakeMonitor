#pragma once
#include <unordered_set>
#include <string>
#include <fstream>
#include <filesystem>
#include "json.hpp" // Make sure this is available

class FavoritesManager {
public:
    // Load favorites from disk
    static std::unordered_set<std::string> Load() {
        std::unordered_set<std::string> favs;
        if (!std::filesystem::exists("favorites.json")) return favs;
        
        try {
            std::ifstream f("favorites.json");
            nlohmann::json j;
            f >> j;
            if (j.is_array()) {
                for (const auto& id : j) {
                    favs.insert(id.get<std::string>());
                }
            }
        } catch(...) {} // Ignore errors for simplicity
        return favs;
    }

    // Save favorites to disk
    static void Save(const std::unordered_set<std::string>& favs) {
        nlohmann::json j = nlohmann::json::array();
        for (const auto& id : favs) {
            j.push_back(id);
        }
        std::ofstream f("favorites.json");
        f << j.dump(4);
    }
};