cmake_minimum_required(VERSION 3.19)
project(lab1_5_spo C)

set(CMAKE_C_STANDARD 11)

add_executable(server server.c database.c database.h json_commands.c json_commands.h)
include_directories(/home/Projects/spo_1_5/build/json-c/build/include)
add_library(jsonlib SHARED IMPORTED)
set_target_properties(jsonlib PROPERTIES IMPORTED_LOCATION /home/oldrim/Projects/spo_1_5/build/json-c/build/lib/libjson-c.so)
target_link_libraries(server jsonlib)

add_executable(client client.c database.h json_commands.c json_commands.h
        ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.c ${CMAKE_CURRENT_BINARY_DIR}/y.tab.c ${CMAKE_CURRENT_BINARY_DIR}/y.tab.h)

target_include_directories(client PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake)

find_package(X11 REQUIRED)
message(STATUS "X11_FOUND = ${X11_FOUND}")
message(STATUS "X11_INCLUDE_DIR = ${X11_INCLUDE_DIR}")
message(STATUS "X11_LIBRARIES = ${X11_LIBRARIES}")
include_directories(${X11_INCLUDE_DIR})
link_directories(${X11_LIBRARIES})

target_link_libraries(client jsonlib)
target_link_libraries(client ${X11_LIBRARIES})

add_custom_command(
        MAIN_DEPENDENCY "${CMAKE_CURRENT_SOURCE_DIR}/lang.lex"
        PRE_BUILD
        COMMAND ${FLEX_EXECUTABLE}
        ARGS -o"${CMAKE_CURRENT_BINARY_DIR}/lex.yy.c" "${CMAKE_CURRENT_SOURCE_DIR}/lang.lex"
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/lex.yy.c"
)

add_custom_command(
        MAIN_DEPENDENCY "${CMAKE_CURRENT_SOURCE_DIR}/lang.y"
        PRE_BUILD
        COMMAND bison
        ARGS -y -d --debug --verbose -o"${CMAKE_CURRENT_BINARY_DIR}/y.tab.c" -Wcounterexamples
        "${CMAKE_CURRENT_SOURCE_DIR}/lang.y"
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/lex.yy.c"
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/y.tab.c"
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/y.tab.h"
)
