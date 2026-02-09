#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>

#include "httplib.h"
#include "json.hpp"

#include <string>
#include <vector>
#include <unordered_set>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <cctype>

using json = nlohmann::json;

struct Earthquake {
    std::string id;
    double mag = 0.0;
    std::string place;
    long long time_ms = 0;
    double lon = 0.0, lat = 0.0, depth_km = 0.0;
};

static std::string fetch_usgs_all_day() {
    httplib::SSLClient cli("earthquake.usgs.gov", 443);
    cli.set_follow_location(true);

    auto res = cli.Get("/earthquakes/feed/v1.0/summary/all_day.geojson");
    if (!res || res->status != 200) return {};
    return res->body;
}

static std::vector<Earthquake> parse_usgs_geojson(const std::string& body) {
    std::vector<Earthquake> out;
    if (body.empty()) return out;

    json j;
    try {
        j = json::parse(body);
    } catch (...) {
        return out;
    }

    if (!j.contains("features") || !j["features"].is_array()) return out;

    for (const auto& f : j["features"]) {
        Earthquake e;

        if (f.contains("id") && f["id"].is_string())
            e.id = f["id"].get<std::string>();

        if (f.contains("properties") && f["properties"].is_object()) {
            const auto& p = f["properties"];

            if (p.contains("mag") && !p["mag"].is_null())
                e.mag = p["mag"].get<double>();

            if (p.contains("place") && p["place"].is_string())
                e.place = p["place"].get<std::string>();

            if (p.contains("time") && !p["time"].is_null())
                e.time_ms = p["time"].get<long long>();
        }

        if (f.contains("geometry") && f["geometry"].is_object()) {
            const auto& g = f["geometry"];
            if (g.contains("coordinates") && g["coordinates"].is_array()) {
                const auto& c = g["coordinates"];
                if (c.size() >= 3) {
                    if (!c[0].is_null()) e.lon = c[0].get<double>();
                    if (!c[1].is_null()) e.lat = c[1].get<double>();
                    if (!c[2].is_null()) e.depth_km = c[2].get<double>();
                }
            }
        }

        out.push_back(std::move(e));
    }

    return out;
}

static std::filesystem::path favorites_path() {
    std::filesystem::path dir = std::filesystem::current_path() / "data";
    std::filesystem::create_directories(dir);
    return dir / "favorites.json";
}

static std::unordered_set<std::string> load_favorites() {
    std::unordered_set<std::string> fav;
    std::ifstream in(favorites_path());
    if (!in) return fav;

    try {
        json j;
        in >> j;
        if (j.is_array()) {
            for (const auto& x : j) {
                if (x.is_string()) fav.insert(x.get<std::string>());
            }
        }
    } catch (...) {
        // ignore corrupted file
    }
    return fav;
}

static void save_favorites(const std::unordered_set<std::string>& fav) {
    json j = json::array();
    for (const auto& id : fav) j.push_back(id);

    std::ofstream out(favorites_path());
    out << j.dump(2);
}

static bool contains_case_insensitive(const std::string& text, const char* needle) {
    if (!needle || needle[0] == '\0') return true;

    auto lower = [](unsigned char c) { return (char)std::tolower(c); };

    std::string t = text;
    std::string n = needle;

    for (auto& ch : t) ch = lower((unsigned char)ch);
    for (auto& ch : n) ch = lower((unsigned char)ch);

    return t.find(n) != std::string::npos;
}

int main() {
    if (!glfwInit()) return 1;

    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow* window = glfwCreateWindow(1100, 700, "Earthquake Monitor", nullptr, nullptr);
    if (!window) return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Persistent UI/app state (DO NOT reinitialize every frame)
    std::vector<Earthquake> quakes;
    std::string status = "Idle";
    int bytes = 0;
    int selected = -1;

    float minMag = 0.0f;
    char placeFilter[128] = "";

    std::unordered_set<std::string> favorites = load_favorites();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Earthquakes");

        // Fetch
        if (ImGui::Button("Fetch USGS (all_day.geojson)")) {
            status = "Fetching...";
            std::string body = fetch_usgs_all_day();
            bytes = (int)body.size();

            if (body.empty()) {
                status = "Fetch failed";
                quakes.clear();
                selected = -1;
            } else {
                quakes = parse_usgs_geojson(body);
                status = quakes.empty() ? "Parse failed" : "Fetch + Parse OK";
                selected = quakes.empty() ? -1 : 0;
            }
        }

        ImGui::SameLine();
        ImGui::Text("Status: %s | Bytes: %d | Quakes: %d", status.c_str(), bytes, (int)quakes.size());

        // Filters
        ImGui::Separator();
        ImGui::SliderFloat("Min magnitude", &minMag, 0.0f, 9.0f, "%.1f");
        ImGui::InputText("Search place", placeFilter, sizeof(placeFilter));
        ImGui::Separator();

        // Only fix selection if invalid (does not override row clicks)
        if (quakes.empty()) selected = -1;
        else if (selected < 0 || selected >= (int)quakes.size()) selected = 0;

        // Layout: list + details
        ImGui::Columns(2, nullptr, true);

        // LEFT: list
        ImGui::Text("List");
        ImGui::Separator();
        ImGui::BeginChild("list", ImVec2(0, 0), true);

        if (ImGui::BeginTable("quakes_table", 5,
                              ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Borders |
                              ImGuiTableFlags_Resizable |
                              ImGuiTableFlags_ScrollY)) {

            ImGui::TableSetupColumn("Mag", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn("Place");
            ImGui::TableSetupColumn("Lat", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Lon", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Depth", ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ImGui::TableHeadersRow();

            for (int i = 0; i < (int)quakes.size(); i++) {
                const auto& e = quakes[i];

                // Apply filters
                if (e.mag < (double)minMag) continue;
                if (!contains_case_insensitive(e.place, placeFilter)) continue;

                bool fav = favorites.count(e.id) > 0;
                if (fav) ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 215, 0, 255)); // gold

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                char label[64];
                std::snprintf(label, sizeof(label), "%.1f##%d", e.mag, i);
                if (ImGui::Selectable(label, selected == i, ImGuiSelectableFlags_SpanAllColumns)) {
                    selected = i;
                }

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(e.place.c_str());

                ImGui::TableNextColumn();
                ImGui::Text("%.4f", e.lat);

                ImGui::TableNextColumn();
                ImGui::Text("%.4f", e.lon);

                ImGui::TableNextColumn();
                ImGui::Text("%.1f", e.depth_km);

                if (fav) ImGui::PopStyleColor();
            }

            ImGui::EndTable();
        }

        ImGui::EndChild();

        ImGui::NextColumn();

        // RIGHT: details
        ImGui::Text("Details");
        ImGui::Separator();
        ImGui::BeginChild("details", ImVec2(0, 0), true);

        if (selected >= 0 && selected < (int)quakes.size()) {
            const auto& e = quakes[selected];

            bool isFav = favorites.count(e.id) > 0;

            if (ImGui::Button(isFav ? "Unfavorite" : "Favorite")) {
                if (isFav) favorites.erase(e.id);
                else favorites.insert(e.id);
                save_favorites(favorites);
            }
            ImGui::SameLine();
            ImGui::TextUnformatted(isFav ? "★" : "☆");
            ImGui::Separator();

            // Optional: tell user if selected is hidden by filters
            bool matchesFilters =
                (e.mag >= (double)minMag) &&
                contains_case_insensitive(e.place, placeFilter);

            if (!matchesFilters) {
                ImGui::TextWrapped("Note: selected quake is hidden by current filters.");
                ImGui::Separator();
            }

            ImGui::Text("ID: %s", e.id.c_str());
            ImGui::Text("Magnitude: %.1f", e.mag);
            ImGui::TextWrapped("Place: %s", e.place.c_str());
            ImGui::Text("Latitude: %.6f", e.lat);
            ImGui::Text("Longitude: %.6f", e.lon);
            ImGui::Text("Depth (km): %.2f", e.depth_km);
            ImGui::Text("Time (ms): %lld", e.time_ms);
        } else {
            ImGui::Text("No selection.");
        }

        ImGui::EndChild();

        ImGui::Columns(1);
        ImGui::End();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
