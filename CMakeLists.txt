cmake_minimum_required(VERSION 3.20)
project(EarthquakeMonitor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# GLFW
add_subdirectory(external/glfw)

# ImGui sources
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/external/imgui)

add_executable(EarthquakeMonitor
    src/main.cpp

    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp

    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(EarthquakeMonitor PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${CMAKE_SOURCE_DIR}/external
    ${CMAKE_SOURCE_DIR}/external/httplib
    ${CMAKE_SOURCE_DIR}/external/json
)

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

# HTTPS support for cpp-httplib
find_package(OpenSSL REQUIRED)
target_link_libraries(EarthquakeMonitor PRIVATE OpenSSL::SSL OpenSSL::Crypto)
target_compile_definitions(EarthquakeMonitor PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)

