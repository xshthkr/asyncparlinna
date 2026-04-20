# compile .cu files into gpuasynclib
cmake_minimum_required (VERSION 3.10)

file(GLOB_RECURSE CUFILES "${PROJECT_SOURCE_DIR}/*.cu")
SET_SOURCE_FILES_PROPERTIES(${CUFILES} PROPERTIES LANGUAGE CUDA )
aux_source_directory(. RBURK_GPU_SOURCES)
add_library(gpuasynclib ${RBURK_GPU_SOURCES})

target_link_libraries(gpuasynclib ${MPI_C_LIBRARIES})