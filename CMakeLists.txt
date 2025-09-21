cmake_minimum_required(VERSION 3.16)
project(net_serial_monitor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(FLTK REQUIRED)
include_directories(${FLTK_INCLUDE_DIR})

add_executable(net_serial_monitor main.cpp)
target_link_libraries(net_serial_monitor ${FLTK_LIBRARIES})

install(TARGETS net_serial_monitor RUNTIME DESTINATION bin)
install(PROGRAMS scripts/test_network.sh DESTINATION bin)
install(PROGRAMS scripts/test_serial.sh DESTINATION bin)
install(FILES misc/net-serial-monitor.desktop DESTINATION share/applications)
install(FILES misc/net-serial-monitor-128.png DESTINATION share/pixmaps)
