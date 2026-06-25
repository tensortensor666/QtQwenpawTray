cmake_minimum_required(VERSION 3.16)

project(QtQwenPawTray
    VERSION 0.1.0
    DESCRIPTION "QwenPaw system tray controller implemented with Qt"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network)

set(PROJECT_SOURCES
    resources/resources.qrc
    src/config.cpp
    src/config.h
    src/constants.h
    src/iconfactory.cpp
    src/iconfactory.h
    src/logger.cpp
    src/logger.h
    src/main.cpp
    src/servicemanager.cpp
    src/servicemanager.h
    src/trayapplication.cpp
    src/trayapplication.h
    src/winutil.cpp
    src/winutil.h
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_add_executable(QwenPawTray
        WIN32
        ${PROJECT_SOURCES}
    )
else()
    add_executable(QwenPawTray
        WIN32
        ${PROJECT_SOURCES}
    )
endif()

target_link_libraries(QwenPawTray PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
)

if(WIN32)
    target_link_libraries(QwenPawTray PRIVATE shell32)
endif()

