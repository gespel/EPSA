project(EPS)
cmake_minimum_required(VERSION 3.17)

set(CMAKE_SHARED_LIBRARY_PREFIX "")

set(SLURM_INSTALL_DIR "" CACHE PATH "A directory of slurm installation")
set(SLURM_SRC_DIR "" CACHE PATH "A directory with slurm source code")
set(EMA_INSTALL_DIR "" CACHE PATH "A directory of EMA installation")

if(NOT SLURM_INSTALL_DIR)
    message(FATAL_ERROR "You have to set up `SLURM_INSTALL_DIR` path!")
endif()

if(NOT SLURM_SRC_DIR)
    message(FATAL_ERROR "You have to set up `SLURM_SRC_DIR` path!")
endif()

if(NOT EMA_INSTALL_DIR)
    message(FATAL_ERROR "You have to set up `EMA_INSTALL_DIR` path!")
endif()


add_library(eps SHARED)
target_sources(
    eps
    PRIVATE eps.c
    PRIVATE src/eps_utils.c
    PRIVATE src/eps_cgroup.c
    PRIVATE src/eps_sem.c
    PRIVATE src/eps_efp.c
    PRIVATE src/eps_shm.c
)
target_include_directories(eps PRIVATE ./include)
target_include_directories(eps PRIVATE ${SLURM_INSTALL_DIR}/include)
target_include_directories(eps PRIVATE ${EMA_INSTALL_DIR}/include)
target_link_libraries(eps ${EMA_INSTALL_DIR}/lib/libEMA.so)

add_library(prep_eps SHARED)
target_sources(
    prep_eps
    PRIVATE prep_eps.c
)
target_include_directories(prep_eps PRIVATE ./include)
target_include_directories(prep_eps PRIVATE ${SLURM_SRC_DIR})
target_include_directories(prep_eps PRIVATE ${SLURM_INSTALL_DIR}/include)
