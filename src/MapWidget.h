#pragma once
#include "imgui.h"
#include "EarthquakeService.h"
#include <vector>
#include <string>
#include <cmath> 

class MapWidget {
public:
    static void Draw(const std::vector<Earthquake>& quakes, ImTextureID textureID, const std::string& selectedID, const char* label = "MapRegion") {
        if (ImGui::BeginChild(label, ImVec2(0, 300), true, ImGuiWindowFlags_NoScrollbar)) {
            
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImVec2 size = ImGui::GetWindowSize();

            // 1. Draw Map Image
            if (textureID) {
                draw_list->AddImage(textureID, p, ImVec2(p.x + size.x, p.y + size.y));
            } else {
                draw_list->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), IM_COL32(20, 30, 45, 255));
            }

            // 2. Draw Earthquakes
            for (const auto& q : quakes) {
                float x = p.x + ((float)((q.lon + 180.0) / 360.0) * size.x);
                float y = p.y + ((float)((90.0 - q.lat) / 180.0) * size.y);
                ImVec2 center(x, y);

                bool isSelected = (q.id == selectedID);

                // Standard Color
                ImU32 color;
                if (q.mag < 4.5)      color = IM_COL32(100, 255, 100, 200);
                else if (q.mag < 6.0) color = IM_COL32(255, 255, 0, 200);
                else                  color = IM_COL32(255, 50, 50, 240);

                // Draw Base Dot
                float radius = (float)(q.mag * 1.5f) + 2.0f;
                draw_list->AddCircleFilled(center, radius, color);

                // --- HIGHLIGHT LOGIC ---
                if (isSelected) {
                    // Animation: Pulsing Ring
                    float time = (float)ImGui::GetTime();
                    float pulse = (sin(time * 10.0f) + 1.0f) * 0.5f; 
                    float ringRadius = radius + 5.0f + (pulse * 10.0f); 
                    
                    // Draw Cyan Ring
                    draw_list->AddCircle(center, ringRadius, IM_COL32(0, 255, 255, 255), 0, 2.0f);
                    
                    // Draw Crosshair
                    draw_list->AddLine(ImVec2(center.x - 20, center.y), ImVec2(center.x + 20, center.y), IM_COL32(0, 255, 255, 200));
                    draw_list->AddLine(ImVec2(center.x, center.y - 20), ImVec2(center.x, center.y + 20), IM_COL32(0, 255, 255, 200));
                    
                    // FIX: REMOVED "SetTooltip" FROM HERE
                    // Use a label near the dot instead if you want text
                    draw_list->AddText(ImVec2(center.x + 10, center.y - 10), IM_COL32(255, 255, 255, 255), q.place.c_str());
                }

                // Hover Tooltip (Only when mouse is actually over the dot)
                if (ImGui::IsMouseHoveringRect(ImVec2(center.x - 5, center.y - 5), ImVec2(center.x + 5, center.y + 5))) {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s", q.place.c_str());
                    ImGui::Text("Mag: %.1f", q.mag);
                    ImGui::EndTooltip();
                }
            }
        }
        ImGui::EndChild();
    }
};