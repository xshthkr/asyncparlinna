cmake_minimum_required(VERSION 4.3)
project(async-overlap LANGUAGES CXX VERSION 1.0.0)

add_library(async-overlap)

target_sources(async-overlap
    PRIVATE
    parlogna.cpp
    parlinna.cpp
    utils.cpp
    
    PUBLIC
    FILE_SET HEADERS
    FILES
    async.h
)

target_include_directories(async-overlap 
    PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
)