cmake_minimum_required(VERSION 3.10)

find_package(MPI REQUIRED)
find_package(Threads REQUIRED)

add_executable(servlet_test ./servlet_test.cpp)

target_link_libraries(servlet_test 
    PUBLIC
    async-overlap 
    MPI::MPI_CXX 
    Threads::Threads
)
