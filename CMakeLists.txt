cmake_minimum_required(VERSION 3.16)
project(tracker)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(DBUS REQUIRED dbus-1)
pkg_check_modules(PROTOBUF REQUIRED protobuf)

set(SOURCES
        dbus.cpp
        gps.cpp
        tracker.cpp
        tracker.pb.cc
)

set(HEADERS
        dbus.hpp
        gps.hpp
        tracker.pb.h
)

add_executable(tracker ${SOURCES} ${HEADERS})

target_include_directories(tracker PRIVATE ${DBUS_INCLUDE_DIRS} ${PROTOBUF_INCLUDE_DIRS})
target_compile_options(tracker PRIVATE ${DBUS_CFLAGS_OTHER} ${PROTOBUF_CFLAGS_OTHER})
target_link_directories(tracker PRIVATE ${DBUS_LIBRARY_DIRS} ${PROTOBUF_LIBRARY_DIRS})
target_link_libraries(tracker ${DBUS_LIBRARIES} ${PROTOBUF_LIBRARIES})
