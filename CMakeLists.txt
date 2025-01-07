project(EPS)
cmake_minimum_required(VERSION 3.17)

set(SLURM_INSTALL_DIR "" CACHE PATH "A directory of slurm installation")
set(SLURM_SRC_DIR "" CACHE PATH "A directory with slurm source code")

set(CMAKE_SHARED_LIBRARY_PREFIX "")

add_library(eps SHARED)
target_sources(
    eps
    PRIVATE eps.c
)

add_library(prep_eps SHARED)
target_sources(
    prep_eps
    PRIVATE prep_eps.c
)
target_include_directories(prep_eps PRIVATE ./include)
target_include_directories(prep_eps PRIVATE ${SLURM_SRC_DIR})
target_include_directories(prep_eps PRIVATE ${SLURM_INSTALL_DIR}/include)
