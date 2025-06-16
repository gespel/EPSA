project(EPS)
cmake_minimum_required(VERSION 3.17)

# INFO: This is to prevent `libeps.so` and `libprep_eps.so` as
#       filenames for built deliverables.
set(CMAKE_SHARED_LIBRARY_PREFIX "")

set(SLURM_INSTALL_DIR "" CACHE PATH "A directory of slurm installation")
set(SLURM_SRC_DIR "" CACHE PATH "A directory with slurm source code")
set(EMA_INSTALL_DIR "" CACHE PATH "A directory of EMA installation")
set(PQ_INSTALL_DIR "" CACHE PATH "A directory with PostgreSQL installation")
set(HWLOC_INSTALL_DIR "" CACHE PATH "A directory of hwloc installation")

if(NOT SLURM_INSTALL_DIR)
    message(FATAL_ERROR "You have to set up `SLURM_INSTALL_DIR` path!")
endif()

if(NOT SLURM_SRC_DIR)
    message(FATAL_ERROR "You have to set up `SLURM_SRC_DIR` path!")
endif()

if(NOT EMA_INSTALL_DIR)
    message(FATAL_ERROR "You have to set up `EMA_INSTALL_DIR` path!")
endif()

if(NOT PQ_INSTALL_DIR)
    message(FATAL_ERROR "You have to set up `PQ_INSTALL_DIR` path!")
endif()

if(NOT HWLOC_INSTALL_DIR)
    message(FATAL_ERROR "You have to set up `HWLOC_INSTALL_DIR` path!")
endif()

option(USE_NVML "Enable NVML support if available" ON)

if (USE_NVML)
    find_package(CUDAToolkit)
    if (CUDAToolkit_FOUND)
        set(HAS_NVML TRUE)
        message(STATUS "NVML support enabled.")
    else()
        set(HAS_NVML FALSE)
        message(STATUS "NVML library not found, disabling NVML support.")
    endif()
else()
    set(HAS_NVML FALSE)
    message(STATUS "NVML support explicitly disabled.")
endif()

add_library(prep_eps SHARED)
target_sources(
    prep_eps
    PRIVATE prep_eps.c
    PRIVATE src/eps_cpuinfo.c
    PRIVATE src/eps_data.c
    PRIVATE src/eps_db.c
    PRIVATE src/eps_efp.c
    PRIVATE src/eps_gres.c
    PRIVATE src/eps_sem.c
    PRIVATE src/eps_shm.c
    PRIVATE src/eps_wait.c
    PRIVATE src/eps_utils.c
)
target_include_directories(prep_eps PRIVATE include)
target_include_directories(prep_eps PRIVATE ${HWLOC_INSTALL_DIR}/include)
target_include_directories(prep_eps PRIVATE ${PQ_INSTALL_DIR}/include)
target_include_directories(prep_eps PRIVATE ${SLURM_SRC_DIR})
target_include_directories(prep_eps PRIVATE ${SLURM_INSTALL_DIR}/include)
target_link_libraries(prep_eps ${EMA_INSTALL_DIR}/lib/libEMA.so)
target_link_libraries(prep_eps ${HWLOC_INSTALL_DIR}/lib/libhwloc.so)
# TODO: Fix this `s` ending in `lib` subdir.
target_link_libraries(prep_eps ${PQ_INSTALL_DIR}/libs/libpq.so)

if(USE_NVML AND CUDAToolkit_FOUND)
    target_sources(
      prep_eps
      PRIVATE src/eps_nvml.c
    )
    target_compile_definitions(prep_eps PRIVATE HAS_NVML)
    target_link_libraries(prep_eps CUDA::nvml)
endif()
