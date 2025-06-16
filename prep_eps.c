#include <errno.h>
#include <hwloc.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifdef HAS_NVML
#include <nvml.h>
#endif

#include <slurm/slurm.h>
#include <slurm/slurm_errno.h>
#include <src/common/bitstring.h>
#include <src/interfaces/prep.h>

#include <eps_cpuinfo.h>
#include <eps_data.h>
#include <eps_db.h>
#include <eps_efp.h>
#include <eps_sem.h>
#include <eps_shm.h>
#include <eps_wait.h>
#include <eps_utils.h>

#define EFP_WAIT_TIMEOUT 10 /* in seconds */

const char plugin_name[] = "EPS";
const char plugin_type[] = "prep/eps";
const uint32_t plugin_version = SLURM_VERSION_NUMBER;

unsigned long long tstart, tend;
time_t timestamp;
eps_cpuinfo_t cpuinfo;

/********************************
 *
 * Slurm Plugin API hooks
 *
 ********************************/

extern int init(void)
{
    slurm_info("Init: %s", plugin_name);

    if (!running_in_slurmd()) return SLURM_SUCCESS;

    hwloc_topology_t topology;
    hwloc_topology_init(&topology);
    hwloc_topology_load(topology);

    int err = populate_cpuinfo(topology, &cpuinfo);
    if (err)
    {
        hwloc_topology_destroy(topology);
        return SLURM_ERROR;
    }

    slurm_info("Socket count: %u", cpuinfo.socket_cnt);
    for(int i = 0; i < cpuinfo.socket_cnt; i++)
    {
        slurm_info("Cores per socket [%d]: %u", i, cpuinfo.cores_per_socket[i]);
    }

    slurm_info("Cores count: %u", cpuinfo.core_cnt);
    for(int i = 0; i < cpuinfo.core_cnt; i++)
    {
        slurm_info("Core [%d] -> Socket [%u]", i, cpuinfo.socket_idx[i]);
    }

    hwloc_topology_destroy(topology);

    return SLURM_SUCCESS;
}

extern void fini(void)
{
    slurm_info("Fini: %s", plugin_name);
}

extern void prep_p_register_callbacks(prep_callbacks_t* callbacks) {}

extern int prep_p_prolog(job_env_t* job_env, slurm_cred_t *cred)
{
    if (!running_in_slurmd()) return SLURM_SUCCESS;

    slurm_info("Prolog: %s", plugin_name);
    slurm_info("Job Id: %u", job_env->jobid);

    int nodeid;
    uint16_t show_flags = 0;
    char hostname[HOST_NAME_MAX];
    char cpu_ids[128];
    hostname[0] = '\0';
    cpu_ids[0] ='\0';

    int err = gethostname(hostname, HOST_NAME_MAX); 
    if (err)
    {
        perror("gethostname: ");
        slurm_info("error: gethostname");
        return SLURM_ERROR;
    }

    show_flags |= SHOW_ALL;
    show_flags |= SHOW_DETAIL;
    job_info_msg_t* job_info_list = NULL;
    node_info_msg_t* node_info_list = NULL;

    err = slurm_load_node_single(&node_info_list, hostname, show_flags);
    if (err != SLURM_SUCCESS)
    {
        slurm_info("error: slurm_load_node_single: %d", err);
        return SLURM_ERROR;
    } 

    int gres_uuid_count = 0;
    char** gres_uuid_list = NULL;

    if (node_info_list->record_count > 0)
    {
        node_info_t node_rec = node_info_list->node_array[0];

        size_t gres_count = 0;
        int* gres_idxs = NULL;

        char* idx = NULL;

        int ret = parse_gres(node_rec.gres_used, &idx);
        if (ret < 0)
        {
            slurm_info("Failed to parse gres_used!");
            return SLURM_ERROR;
        }
        if (ret > 0)
        {
            gres_idxs = parse_indexes(idx, &gres_count);
            if (gres_count && !gres_idxs)
            {
                slurm_info("Failed to parse gres indexes substring!");
                return SLURM_ERROR;
            }
            
        }

        if (gres_count > 0)
        {
            #ifdef HAS_NVML

            gres_uuid_list = (char **)malloc(gres_count * sizeof(char*));

            nvmlReturn_t ret = nvmlInitWithFlags(NVML_INIT_FLAG_NO_GPUS);
            NVML_HANDLE_RET(ret, "nvmlInitWithFlags");

            slurm_info("NVML Initialized!");

            unsigned int device_count;
            ret = nvmlDeviceGetCount_v2(&device_count);
            NVML_HANDLE_RET(ret, "nvmlDeviceGetCount_v2");

            for (unsigned int i = 0; i < device_count; i++)
            {
                nvmlDevice_t handle;
                ret = nvmlDeviceGetHandleByIndex_v2(i, &handle);
                NVML_HANDLE_RET(ret, "nvmlDeviceGetHandleByIndex_v2");

                unsigned int minor;
                ret = nvmlDeviceGetMinorNumber(handle, &minor);
                NVML_HANDLE_RET(ret, "nvmlDeviceGetMinorNumber");

                int match = 0;
                for (int i = 0; i < gres_count; i++)
                {
                    unsigned int idx = gres_idxs[i];
                    if (minor == idx) match = 1;
                }

                if (!match) continue;

                char uuid[NVML_DEVICE_UUID_BUFFER_SIZE];
                ret = nvmlDeviceGetUUID(
                    handle,
                    uuid,
                    NVML_DEVICE_UUID_BUFFER_SIZE
                );
                NVML_HANDLE_RET(ret, "nvmlDeviceGetUUID");

                gres_uuid_list[gres_uuid_count] = strdup(uuid);
                gres_uuid_count++;
            }

            ret = nvmlShutdown();
            if (ret != NVML_SUCCESS)
            {
                slurm_info("error: nvmlShutdown: %s", nvmlErrorString(ret));
            }
            else
            {
                slurm_info("NVML shutdown!");
            }
            #endif
        }
        free(gres_idxs);
    }

    err = slurm_load_job(&job_info_list, job_env->jobid, show_flags);
    if (err != SLURM_SUCCESS)
    {
        slurm_info("error: slurm_load_job: %d", err);
        return SLURM_ERROR;
    } 
    if (job_info_list->record_count > 0)
    {
        for (int i = 0; i < job_info_list->record_count; i++)
        {
            job_info_t job_rec = job_info_list->job_array[i];
            if (job_rec.job_id != job_env->jobid) continue;

            job_resources_t* job_resrcs = job_rec.job_resrcs;

            if (
                !job_resrcs ||
                !job_resrcs->core_bitmap ||
                bit_fls(job_resrcs->core_bitmap) == -1
            )
            {
                slurm_info("error: invalid job_resrcs");
                return SLURM_ERROR;
            }

            hostlist_t* hostlist = slurm_hostlist_create(job_resrcs->nodes);
            if (!hostlist) {
                slurm_info("error: hostlist_create");
                return SLURM_ERROR;
            }

            nodeid = slurm_hostlist_find(hostlist, hostname);
            if (nodeid == -1)
            {
                slurm_info("error: hostlist_find");
                return SLURM_ERROR;
            }
            slurm_hostlist_destroy(hostlist);

            int bit_reps = *job_resrcs->sockets_per_node *
                           *job_resrcs->cores_per_socket;
            uint32_t threads = threads_per_core(hostname);
            bitstr_t* cpu_bitmap = bit_alloc(bit_reps * threads);
            int bit_idx = 0;
            for (int j = 0; j < bit_reps; j++)
            {
                if (bit_test(job_resrcs->core_bitmap, bit_idx))
                {
                    for (int k = 0; k < threads; k++)
                        bit_set(cpu_bitmap, (j * threads) + k);
                }
                bit_idx++;
            }
            bit_fmt(cpu_ids, sizeof(cpu_ids), cpu_bitmap);
            FREE_NULL_BITMAP(cpu_bitmap);
        }
    }

    slurm_info("Initializing shared memory...");
    int shmfd;
    char* shm_name = get_shared_memory_region_name(job_env->jobid);

    unlink_shared_memory_region(shm_name);
    pid_t* efp_pid = (pid_t*)get_shared_memory_addr(
        shm_name, sizeof(pid_t*), &shmfd
    );
    free(shm_name);
    if (!efp_pid)
    {
        slurm_info("error: get_shared_memory_addr: %s", strerror(errno));
        return SLURM_ERROR;
    }

    slurm_info("Obtaining semaphores...");
    char* sem_name = get_sem_init_name(job_env->jobid);
    sem_t* proceed_init = get_efp_sem(sem_name, 1);
    if (!proceed_init)
    {
        err = discard_shared_memory_addr(
            (void*)efp_pid, sizeof(pid_t*), &shmfd
        );
        if (err)
        {
            slurm_info(
                "error: discard_shared_memory_addr: %s", strerror(errno)
            );
        }
        free(sem_name);
        slurm_info("error: get_efp_sem: %s", strerror(errno));
        return SLURM_ERROR;
    }

    pid_t pid = fork();
    switch(pid) {
        case -1:
            err = discard_shared_memory_addr(
                (void*)efp_pid, sizeof(pid_t*), &shmfd
            );
            if (err) {
                slurm_info(
                    "error: discard_shared_memory_addr: %s", strerror(errno)
                );
            }
            sem_close(proceed_init);
            sem_unlink(sem_name);
            free(sem_name);
            
            if (gres_uuid_list) free(gres_uuid_list);

            slurm_info("error: fork: %s", strerror(errno));
            return SLURM_ERROR;
        case 0:
            // Free copied resources after fork...
            err = discard_shared_memory_addr(
                (void*)efp_pid, sizeof(pid_t*), &shmfd
            );
            if (err) {
                slurm_info(
                    "error: discard_shared_memory_addr: %s", strerror(errno)
                );
            }
            sem_close(proceed_init);
            free(sem_name);

            time_t tstart;
            time(&tstart);
            if (tstart == -1) {
                slurm_info("error: time: %s", strerror(errno));
                return SLURM_ERROR;
            }
            // INFO: Run EFP process...
            efp_main(
                job_env->jobid,
                nodeid,
                gres_uuid_count,
                gres_uuid_list,
                tstart,
                cpu_ids,
                &cpuinfo
            );
            break;
        default:
            pid_t hook_pid = getpid();

            slurm_info("Hook PID: %d", hook_pid);

            slurm_info("Waiting for EFP initialization...");
            sem_wait(proceed_init);

            slurm_info("Child PID: %d", pid);

            slurm_info("Writing EFP's PID to shared memory...");
            *efp_pid = pid;

            err = discard_shared_memory_addr(
                (void*)efp_pid, sizeof(pid_t*), &shmfd
            );
            if (err) {
                slurm_info(
                    "error: discard_shared_memory_addr: %s", strerror(errno)
                );
            }

            slurm_info("Closing semaphore...");
            sem_close(proceed_init);
            sem_unlink(sem_name);
            free(sem_name);

            if (gres_uuid_list) free(gres_uuid_list);

            slurm_info("Init hook exits...");
    }

    return SLURM_SUCCESS;
}

extern int prep_p_epilog(job_env_t* job_env, slurm_cred_t *cred)
{
    if (!running_in_slurmd()) return SLURM_SUCCESS;

    slurm_info("Epilog: %s", plugin_name);
    slurm_info("Job Id: %u", job_env->jobid);

    pid_t hook_pid = getpid();
    slurm_info("Hook PID: %d", hook_pid);

    char* shm_name = get_shared_memory_region_name(job_env->jobid);

    slurm_info("Obtaining shared memory address...");
    int shmfd = open_shared_memory_region(shm_name);
    if (shmfd == -1) {
        slurm_info("error: open_shared_memory_region: %s", strerror(errno));
        return SLURM_ERROR;
    }

    pid_t* efp_pid = (pid_t*)map_shared_memory_region(shmfd, sizeof(pid_t*));
    if (!efp_pid) {
        close_shared_memory_region(shmfd);
        slurm_info("error: map_shared_memory_region: %s", strerror(errno));
        return SLURM_ERROR;
    }

    slurm_info("Obtaining semaphores...");
    char* sem_efp_name = get_sem_efp_name(job_env->jobid);
    sem_t* resume_efp = get_efp_sem(sem_efp_name, 0);
    if (!resume_efp) {
        unmap_shared_memory_region((void*)efp_pid, sizeof(pid_t*));
        close_shared_memory_region(shmfd);
        unlink_shared_memory_region(shm_name);
        free(shm_name);
        free(sem_efp_name);
        slurm_info("error: get_efp_sem: %s", strerror(errno));
        return SLURM_ERROR;
    }

    char* sem_exit_name = get_sem_exit_name(job_env->jobid);
    sem_t* efp_finalize = get_efp_sem(sem_exit_name, 0);
    if (!efp_finalize) {
        unmap_shared_memory_region((void*)efp_pid, sizeof(pid_t*));
        close_shared_memory_region(shmfd);
        unlink_shared_memory_region(shm_name);
        free(shm_name);
        free(sem_efp_name);
        free(sem_exit_name);

        slurm_info("error: get_efp_sem: %s", strerror(errno));
        return SLURM_ERROR;
    }

    slurm_info("Unlocking semaphore (resuming EFP)...");
    sem_post(resume_efp);

    slurm_info("Waiting for EFP to finish or fail...");
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
        unmap_shared_memory_region((void*)efp_pid, sizeof(pid_t*));
        close_shared_memory_region(shmfd);
        unlink_shared_memory_region(shm_name);
        free(shm_name);

        sem_close(resume_efp);
        sem_close(efp_finalize);

        sem_unlink(sem_efp_name);
        sem_unlink(sem_exit_name);

        free(sem_efp_name);
        free(sem_exit_name);

        slurm_info("error: clock_gettime: %s", strerror(errno));
        // Should we retrun here or use sem_trywait ?
        return SLURM_ERROR;
    }

    ts.tv_sec += EFP_WAIT_TIMEOUT;
    int ret = sem_timedwait(efp_finalize, &ts);
    if (ret == -1 && errno == ETIMEDOUT) {
        slurm_info("error: efp timed out!");
        kill(*efp_pid, SIGKILL);
    }

    int status;
    int err = timedwaitpid(*efp_pid, &status, EFP_WAIT_TIMEOUT);
    if (err) {
      slurm_info(
        "error: waitpid timed out for EFP process: "
        "it may have become a zombie"
      );
    }
    slurm_info("EFP process exit status: %d", status);

    slurm_info("Closing semaphores...");
    sem_close(resume_efp);
    sem_close(efp_finalize);
    sem_unlink(sem_efp_name);
    sem_unlink(sem_exit_name);
    free(sem_efp_name);
    free(sem_exit_name);

    slurm_info("Cleaning up shared memory...");
    unmap_shared_memory_region((void*)efp_pid, sizeof(pid_t*));
    close_shared_memory_region(shmfd);
    unlink_shared_memory_region(shm_name);
    free(shm_name);

    return SLURM_SUCCESS;
}

extern int prep_p_prolog_slurmctld(job_record_t* job_ptr, bool* async)
{
    if (!running_in_slurmctld()) return SLURM_SUCCESS;

    slurm_info("Ctld_prolog: %s", plugin_name);
    slurm_info("Job Id: %u", job_ptr->job_id);

    slurm_info("Collecting job allocation data...");
    eps_allocation_data_t data;
    get_allocation_data(job_ptr, &data);

    PGconn* db_connection = connect_db();

    int connection_is_not_ok = check_connection(db_connection);

    if (connection_is_not_ok) {
        slurm_info(
            "error: problems with db connection: %s",
            PQerrorMessage(db_connection)
        );
        free(data.jobname);
        PQfinish(db_connection);
        return SLURM_ERROR;
    }
    int err = insert_allocation_data(db_connection, &data);
    free(data.jobname);

    if (err) {
        slurm_info("error: failed to write data to db");
        PQfinish(db_connection);
        return SLURM_ERROR;
    }

    slurm_info("Closing DB connection...");
    PQfinish(db_connection);

    return SLURM_SUCCESS;
}

extern int prep_p_epilog_slurmctld(job_record_t* job_ptr, bool* async)
{
    return SLURM_SUCCESS;
}

extern void prep_p_required(prep_call_type_t type, bool* required)
{
    *required = false;
    switch (type)
    {
        case PREP_PROLOG_SLURMCTLD:
            if (running_in_slurmctld())
                *required = true;
            break;
        case PREP_EPILOG_SLURMCTLD:
            if (running_in_slurmctld())
                *required = false;
            break;
        case PREP_PROLOG:
        case PREP_EPILOG:
            if (running_in_slurmd())
                *required = true;
            break;
        default:
            return;
    }
    return;
}
