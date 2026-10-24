cmake_minimum_required(VERSION 3.20)
project(LlamaDeck VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Network)

set(SOURCES
    src/main.cpp
    src/core/Types.h
    src/core/AppSettings.h
    src/core/AppSettings.cpp
    src/core/GgufParser.h
    src/core/GgufParser.cpp
    src/core/ModelStore.h
    src/core/ModelStore.cpp
    src/core/HistoryStore.h
    src/core/HistoryStore.cpp
    src/core/ServerController.h
    src/core/ServerController.cpp
    src/core/SystemMonitor.h
    src/core/SystemMonitor.cpp
    src/core/ControlApiServer.h
    src/core/ControlApiServer.cpp
    src/ui/Theme.h
    src/ui/Theme.cpp
    src/ui/Icons.h
    src/ui/Icons.cpp
    src/ui/Widgets.h
    src/ui/Widgets.cpp
    src/ui/SpeedChart.h
    src/ui/SpeedChart.cpp
    src/ui/ModelPage.h
    src/ui/ModelPage.cpp
    src/ui/MonitorPage.h
    src/ui/MonitorPage.cpp
    src/ui/HistoryPage.h
    src/ui/HistoryPage.cpp
    src/ui/ApiPage.h
    src/ui/ApiPage.cpp
    src/ui/ApiDocsPage.h
    src/ui/ApiDocsPage.cpp
    src/ui/SettingsPage.h
    src/ui/SettingsPage.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
)

qt_add_executable(LlamaDeck WIN32 ${SOURCES})

target_include_directories(LlamaDeck PRIVATE src src/core src/ui)
target_link_libraries(LlamaDeck PRIVATE Qt6::Widgets Qt6::Network)

if(WIN32)
    target_link_libraries(LlamaDeck PRIVATE dwmapi)
endif()

set_target_properties(LlamaDeck PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE FALSE
)
