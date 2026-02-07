#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "httplib/httplib.h"
#include <string>
#include <GLFW/glfw3.h>
#include <cstdio>
#include "json.hpp"
#include <vector>

static std::string fetch_usgs_all_day() {
    httplib::SSLClient cli("earthquake.usgs.gov", 443);
    cli.set_follow_location(true);

    auto res = cli.Get("/earthquakes/feed/v1.0/summary/all_day.geojson");
    if (!res || res->status != 200) return {};
    return res->body;
}

using json = nlohmann::json;

struct Earthquake {
    std::string id;
    double mag = 0.0;
    std::string place;
    long long time_ms = 0;
    double lon = 0.0, lat = 0.0, depth_km = 0.0;
};

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

        // id
        if (f.contains("id") && f["id"].is_string())
            e.id = f["id"].get<std::string>();

        // properties
        if (f.contains("properties") && f["properties"].is_object()) {
            const auto& p = f["properties"];

            if (p.contains("mag") && !p["mag"].is_null())
                e.mag = p["mag"].get<double>();

            if (p.contains("place") && p["place"].is_string())
                e.place = p["place"].get<std::string>();

            if (p.contains("time") && !p["time"].is_null())
                e.time_ms = p["time"].get<long long>();
        }

        // geometry.coordinates = [lon, lat, depth]
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

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

static std::vector<Earthquake> quakes;      
static std::string status = "Idle";
static int bytes = 0;
static int selected = -1;
static float minMag = 0.0f;
static char placeFilter[128] = "";
selected = quakes.empty() ? -1 : 0;


ImGui::Begin("Earthquakes");

// Top controls
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

ImGui::Separator();

// Two columns: list (left) + details (right)
ImGui::Columns(2, nullptr, true);

// LEFT: Table list
ImGui::Text("List");
ImGui::Separator();

ImGui::BeginChild("list", ImVec2(0, 0), true);

if (ImGui::BeginTable("quakes_table", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY)) {
    ImGui::TableSetupColumn("Mag", ImGuiTableColumnFlags_WidthFixed, 60.0f);
    ImGui::TableSetupColumn("Place");
    ImGui::TableSetupColumn("Lat", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Lon", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Depth", ImGuiTableColumnFlags_WidthFixed, 70.0f);
    ImGui::TableHeadersRow();

    for (int i = 0; i < (int)quakes.size(); i++) {
        const auto& e = quakes[i];

        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        // Selectable row (span all columns)
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
    }

    ImGui::EndTable();
}

ImGui::EndChild();

ImGui::NextColumn();

// RIGHT: Details
ImGui::Text("Details");
ImGui::Separator();
ImGui::BeginChild("details", ImVec2(0, 0), true);

if (selected >= 0 && selected < (int)quakes.size()) {
    const auto& e = quakes[selected];
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
