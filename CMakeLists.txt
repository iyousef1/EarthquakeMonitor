cmake_minimum_required(VERSION 3.20)
project(EarthquakeMonitor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. External Dependencies
add_subdirectory(external/glfw)
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/external/imgui)

# 2. Main Executable
# I have removed MapWidget.cpp and FavoritesManager.cpp because they don't exist.
# I changed main.cpp to Main.cpp to match your actual filename.
add_executable(EarthquakeMonitor
    src/Main.cpp
    src/EarthquakeService.cpp

    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp

    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# 3. Include Directories
target_include_directories(EarthquakeMonitor PRIVATE
    src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${CMAKE_SOURCE_DIR}/external
    ${CMAKE_SOURCE_DIR}/external/httplib
    ${CMAKE_SOURCE_DIR}/external/json
)

# 4. Linking
target_link_libraries(EarthquakeMonitor PRIVATE glfw)

if(APPLE)
  target_link_libraries(EarthquakeMonitor PRIVATE
    "-framework Cocoa"
    "-framework IOKit"
    "-framework CoreVideo"
    "-framework OpenGL"
  )
  target_compile_definitions(EarthquakeMonitor PRIVATE GL_SILENCE_DEPRECATION)
endif()

find_package(OpenSSL REQUIRED)
target_link_libraries(EarthquakeMonitor PRIVATE OpenSSL::SSL OpenSSL::Crypto)
target_compile_definitions(EarthquakeMonitor PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)