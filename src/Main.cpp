#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <vector>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <map>
#include <unordered_set>

// --- IMAGE LOADING LIBRARY ---
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h" 

#include "EarthquakeService.h"
#include "MapWidget.h"
#include "FavoritesManager.h"
#include "httplib.h" 

// --- GLOBAL FLAG CACHE ---
std::map<std::string, GLuint> flagCache;

// --- HELPER: Load Texture ---
GLuint LoadTextureFromMemory(const std::string& imageData) {
    int width, height, channels;
    unsigned char* data = stbi_load_from_memory((const unsigned char*)imageData.data(), (int)imageData.size(), &width, &height, &channels, 4); 
    if (!data) return 0;
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);
    return textureID;
}

// --- HELPER: Flag Loader with Comprehensive ISO Mapping ---
void EnsureFlagLoaded(const std::string& region) {
    if (flagCache.count(region)) return;

    static const std::map<std::string, std::string> isoCodes = {
        {"USA", "us"}, {"Japan", "jp"}, {"Mexico", "mx"}, {"Indonesia", "id"}, {"Chile", "cl"},
        {"Philippines", "ph"}, {"Canada", "ca"}, {"New Zealand", "nz"}, {"Italy", "it"},
        {"Greece", "gr"}, {"China", "cn"}, {"Turkey", "tr"}, {"Taiwan", "tw"}, {"Iceland", "is"},
        {"Papua New Guinea", "pg"}, {"Fiji", "fj"}, {"Russia", "ru"}, {"Argentina", "ar"},
        {"Peru", "pe"}, {"Afghanistan", "af"}, {"Albania", "al"}, {"Algeria", "dz"},
        {"Australia", "au"}, {"Austria", "at"}, {"Bolivia", "bo"}, {"Brazil", "br"},
        {"Colombia", "co"}, {"Costa Rica", "cr"}, {"Dominican Republic", "do"}, {"Ecuador", "ec"},
        {"El Salvador", "sv"}, {"France", "fr"}, {"Guatemala", "gt"}, {"Haiti", "ht"},
        {"Honduras", "hn"}, {"India", "in"}, {"Iran", "ir"}, {"Iraq", "iq"}, {"Morocco", "ma"},
        {"Myanmar", "mm"}, {"Nicaragua", "ni"}, {"Pakistan", "pk"}, {"Panama", "pa"},
        {"Portugal", "pt"}, {"Spain", "es"}, {"South Africa", "za"}, {"South Korea", "kr"},
        {"Vanuatu", "vu"}, {"Vietnam", "vn"}
    };

    if (!isoCodes.count(region)) return;

    httplib::SSLClient cli("flagcdn.com");
    cli.set_follow_location(true);
    auto res = cli.Get(("/w80/" + isoCodes.at(region) + ".png").c_str());

    if (res && res->status == 200) {
        flagCache[region] = LoadTextureFromMemory(res->body);
    }
}

// --- HELPER: Download Map ---
GLuint DownloadMapFromAPI() {
    httplib::SSLClient cli("upload.wikimedia.org");
    cli.set_follow_location(true);
    auto res = cli.Get("/wikipedia/commons/8/83/Equirectangular_projection_SW.jpg");
    return (res && res->status == 200) ? LoadTextureFromMemory(res->body) : 0;
}

bool ContainsCaseInsensitive(const std::string& text, const std::string& query) {
    if (query.empty()) return true;
    auto it = std::search(text.begin(), text.end(), query.begin(), query.end(), [](char a, char b) { return std::tolower(a) == std::tolower(b); });
    return it != text.end();
}

std::string FormatTime(long long time_ms) {
    std::time_t temp = time_ms / 1000;
    std::tm* t = std::localtime(&temp);
    std::stringstream ss;
    ss << std::put_time(t, "%Y-%m-%d %H:%M:%S"); 
    return ss.str();
}

int main(int, char**) {
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Earthquake Monitor - Pro", nullptr, nullptr);
    if (!window) return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); 

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 150");

    GLuint mapTextureID = DownloadMapFromAPI();
    EarthquakeService service;
    service.startBackgroundService(15);
    service.startAPIServer(8080); 

    std::unordered_set<std::string> favorites = FavoritesManager::Load();
    float minMagFilter = 0.0f;
    bool showMap = true, showFavoritesOnly = false;
    char searchBuffer[128] = "";
    std::string selectedID = ""; 

    // All US States for grouping
    static const std::unordered_set<std::string> usStates = {
        "AL", "AK", "AZ", "AR", "CA", "CO", "CT", "DE", "FL", "GA", "HI", "ID", "IL", "IN", "IA", "KS", "KY", "LA", "ME", "MD", 
        "MA", "MI", "MN", "MS", "MO", "MT", "NE", "NV", "NH", "NJ", "NM", "NY", "NC", "ND", "OH", "OK", "OR", "PA", "RI", "SC", 
        "SD", "TN", "TX", "UT", "VT", "VA", "WA", "WV", "WI", "WY", "Alabama", "Alaska", "Arizona", "Arkansas", "California", 
        "Colorado", "Connecticut", "Delaware", "Florida", "Georgia", "Hawaii", "Idaho", "Illinois", "Indiana", "Iowa", "Kansas", 
        "Kentucky", "Louisiana", "Maine", "Maryland", "Massachusetts", "Michigan", "Minnesota", "Mississippi", "Missouri", "Montana", 
        "Nebraska", "Nevada", "New Hampshire", "New Jersey", "New Mexico", "New York", "North Carolina", "North Dakota", "Ohio", 
        "Oklahoma", "Oregon", "Pennsylvania", "Rhode Island", "South Carolina", "South Dakota", "Tennessee", "Texas", "Utah", 
        "Vermont", "Virginia", "Washington", "West Virginia", "Wisconsin", "Wyoming", "Puerto Rico"
    };

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0)); 
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("Dashboard", nullptr, ImGuiWindowFlags_NoDecoration);

        // --- SIDEBAR ---
        ImGui::BeginChild("Sidebar", ImVec2(320, 0), true);
        ImGui::Text("Global Earthquake Monitor");
        ImGui::Separator();
        if (ImGui::Button("Refresh Now", ImVec2(-1, 30))) service.fetchNow();
        
        auto allQuakes = service.getQuakes();
        std::vector<Earthquake> filtered;
        struct RegionStats { std::string name; int count = 0; double maxMag = 0.0; };
        std::map<std::string, RegionStats> statsMap;

        for (const auto& q : allQuakes) {
            if (q.mag < minMagFilter) continue;
            if (!ContainsCaseInsensitive(q.place, searchBuffer)) continue;
            if (showFavoritesOnly && favorites.find(q.id) == favorites.end()) continue;
            filtered.push_back(q);

            // Robust Region Extraction
            size_t lastComma = q.place.find_last_of(',');
            std::string region = (lastComma == std::string::npos) ? q.place : q.place.substr(lastComma + 2);
            
            // Normalize US locations
            bool isUSA = false;
            for (const auto& s : usStates) {
                if (region == s || region.find(s) != std::string::npos) { isUSA = true; break; }
            }
            if (isUSA) region = "USA";

            // Cleanup trailing junk
            size_t extra = region.find(" region");
            if (extra != std::string::npos) region = region.substr(0, extra);
            extra = region.find(" offshore");
            if (extra != std::string::npos) region = region.substr(0, extra);

            auto& entry = statsMap[region];
            entry.name = region; entry.count++;
            if (q.mag > entry.maxMag) entry.maxMag = q.mag;
        }

        std::vector<RegionStats> sortedCount, sortedMag;
        for (auto const& [n, s] : statsMap) { sortedCount.push_back(s); sortedMag.push_back(s); }
        std::sort(sortedCount.begin(), sortedCount.end(), [](auto& a, auto& b){ return a.count > b.count; });
        std::sort(sortedMag.begin(), sortedMag.end(), [](auto& a, auto& b){ return a.maxMag > b.maxMag; });

        ImGui::Separator();
        ImGui::TextColored(ImVec4(1, 0.8f, 0, 1), "Top 3 Active Regions");
        for (int i = 0; i < std::min((int)sortedCount.size(), 3); i++) {
            EnsureFlagLoaded(sortedCount[i].name);
            if (flagCache.count(sortedCount[i].name)) {
                ImGui::Image((ImTextureID)(intptr_t)flagCache[sortedCount[i].name], ImVec2(24, 16));
                ImGui::SameLine();
            }
            ImGui::Text("%d. %s (%d events)", i+1, sortedCount[i].name.c_str(), sortedCount[i].count);
        }

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Top 3 Strongest Regions");
        for (int i = 0; i < std::min((int)sortedMag.size(), 3); i++) {
            ImGui::BulletText("%s: Mag %.1f", sortedMag[i].name.c_str(), (float)sortedMag[i].maxMag);
        }

        ImGui::Separator();
        ImGui::Text("Magnitude Distribution");
        float histogram[10] = {0.0f}; float maxH = 0.0f;
        for(const auto& q : filtered) {
            int bin = (int)q.mag;
            if(bin >= 0 && bin < 10) { histogram[bin]++; if(histogram[bin] > maxH) maxH = histogram[bin]; }
        }
        ImGui::PlotHistogram("##H", histogram, 10, 0, nullptr, 0.0f, maxH, ImVec2(-1, 60));

        ImGui::Separator();
        ImGui::InputText("Filter", searchBuffer, 128);
        ImGui::SliderFloat("Mag", &minMagFilter, 0.0f, 9.0f);
        ImGui::Checkbox("Favs Only", &showFavoritesOnly);
        ImGui::EndChild();

        ImGui::SameLine();

        // --- CONTENT ---
        ImGui::BeginGroup();
        if (showMap) MapWidget::Draw(filtered, (ImTextureID)(intptr_t)mapTextureID, selectedID, "Map");

        static ImGuiTableFlags tFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;
        if (ImGui::BeginTable("Events", 5, tFlags)) {
            ImGui::TableSetupColumn("Fav", 0, 45); ImGui::TableSetupColumn("Mag", 0, 50);
            ImGui::TableSetupColumn("Place", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Depth", 0, 80); ImGui::TableSetupColumn("Time", 0, 150);
            ImGui::TableHeadersRow();

            for (int i = 0; i < (int)filtered.size(); i++) {
                auto& q = filtered[i]; ImGui::PushID(i);
                bool isFav = favorites.count(q.id), isSelected = (q.id == selectedID);
                ImGui::TableNextRow(); ImGui::TableNextColumn();
                if (ImGui::SmallButton(isFav ? "[*]" : "[ ]")) {
                    if (isFav) favorites.erase(q.id); else favorites.insert(q.id);
                    FavoritesManager::Save(favorites);
                }
                if (isFav) ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 215, 0, 255));
                ImGui::TableNextColumn();
                if (isSelected) ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(50, 80, 120, 255));
                if (ImGui::Selectable("##R", isSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) selectedID = q.id;
                ImGui::SameLine(); ImGui::Text("%.1f", q.mag);
                ImGui::TableNextColumn(); ImGui::TextUnformatted(q.place.c_str());
                ImGui::TableNextColumn(); ImGui::Text("%.1f km", q.depth_km);
                ImGui::TableNextColumn(); ImGui::Text("%s", FormatTime(q.time_ms).c_str());
                if (isFav) ImGui::PopStyleColor();
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::EndGroup();
        ImGui::End(); 

        ImGui::Render();
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }
    return 0;
}