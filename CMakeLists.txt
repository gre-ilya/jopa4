cmake_minimum_required(VERSION 3.16)
project(MapPointsVisualizer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Qt5 COMPONENTS Widgets Network REQUIRED)

add_executable(map_points
    src/main.cpp
    src/MainWindow.cpp
    src/MapWidget.cpp
    src/TileManager.cpp
    src/MainWindow.h
    src/MapWidget.h
    src/TileManager.h
    src/GeoTypes.h
    src/MercatorProjection.h
)

target_link_libraries(map_points PRIVATE Qt5::Widgets Qt5::Network)

# --- Pure-C++ projection self-test (no Qt) ---
enable_testing()
add_executable(test_mercator tests/test_mercator.cpp)
add_test(NAME mercator COMMAND test_mercator)

# --- Offline rendering smoke test (Qt, runs headless via offscreen platform) ---
add_executable(render_smoke
    tests/render_smoke.cpp
    src/MapWidget.cpp
    src/TileManager.cpp
)
target_link_libraries(render_smoke PRIVATE Qt5::Widgets Qt5::Network)
add_test(NAME render COMMAND render_smoke)
set_tests_properties(render PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
