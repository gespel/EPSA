project(EPSA)
cmake_minimum_required(VERSION 3.17)

# INFO: This is to prevent `libepsa.so` and `libprepa_epsa.so` as
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
option(USE_MQTT "Enable EMA MQTT Plugin usage" OFF)

# INFO: Newer Slurm releases carry the job id in `job_env_t.step_id.job_id`
#       instead of `job_env_t.jobid`. See `EPS_JOB_ID` in `include/eps_utils.h`.
include(CheckStructHasMember)
set(CMAKE_REQUIRED_INCLUDES ${SLURM_SRC_DIR} ${SLURM_INSTALL_DIR}/include)
check_struct_has_member(
    "job_env_t" jobid "slurm/slurm.h;src/interfaces/prep.h"
    HAVE_JOB_ENV_JOBID LANGUAGE C
)
unset(CMAKE_REQUIRED_INCLUDES)

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

add_library(prep_epsa SHARED)
target_sources(
    prep_epsa
    PRIVATE prep_epsa.c
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
target_include_directories(prep_epsa PRIVATE include)
target_include_directories(prep_epsa PRIVATE ${EMA_INSTALL_DIR}/include)
target_include_directories(prep_epsa PRIVATE ${HWLOC_INSTALL_DIR}/include)
target_include_directories(prep_epsa PRIVATE ${PQ_INSTALL_DIR}/include)
target_include_directories(prep_epsa PRIVATE ${SLURM_SRC_DIR})
target_include_directories(prep_epsa PRIVATE ${SLURM_INSTALL_DIR}/include)
target_link_libraries(prep_epsa ${EMA_INSTALL_DIR}/lib/libEMA.so)
target_link_libraries(prep_epsa ${HWLOC_INSTALL_DIR}/lib/libhwloc.so)
target_link_libraries(prep_epsa ${PQ_INSTALL_DIR}/lib/libpq.so)

find_package(Threads REQUIRED)
target_link_libraries(prep_epsa Threads::Threads)

if(HAVE_JOB_ENV_JOBID)
    target_compile_definitions(prep_epsa PRIVATE HAVE_JOB_ENV_JOBID)
endif()

if(USE_NVML AND CUDAToolkit_FOUND)
    target_sources(
      prep_epsa
      PRIVATE src/eps_nvml.c
    )
    target_compile_definitions(prep_epsa PRIVATE HAS_NVML)
    target_link_libraries(prep_epsa CUDA::nvml)
endif()

if (USE_MQTT)
    target_sources(
      prep_epsa
      PRIVATE src/eps_mqtt.c
    )
    target_compile_definitions(prep_epsa PRIVATE USE_MQTT)
endif()
