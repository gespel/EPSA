#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <eps_cgroup.h>

#define CGFILE_PATH_LENGTH 30

char* get_proc_cgroup(pid_t pid)
{
    char cgfile[CGFILE_PATH_LENGTH];
    int written = snprintf(cgfile, CGFILE_PATH_LENGTH, "/proc/%d/cgroup", pid);
    if (written >= CGFILE_PATH_LENGTH)
    {
        printf("warn: get_proc_cgroup: cg file path truncated!");
    }
    FILE* fd = fopen(cgfile, "r");
    if (!fd)
    {
        return NULL;
    }
    size_t n = 0;
    char* cgroup = malloc(n);
    ssize_t ret = getline(&cgroup, &n, fd);
    fclose(fd);
    if (ret == -1)
    {
        free(cgroup);
        return NULL;
    }
    return cgroup;
}

int move_pid_to_cg(const char* path, pid_t pid)
{
    FILE* f = fopen(path, "a");
    if (!f)
    {
        perror("Failed to open cgroup procs file!");
        printf("\n");
        return 1;
    }

    printf("\nTrying to move pid: %d to %s...\n", pid, path);
    if (fprintf(f, "%d\n", pid) < 0)
    {
         perror("Failed to write PID to cgroup file!");
         printf("\n");
         fclose(f);
         return 1;
    }
    fflush(f);
    fsync(fileno(f));
    fclose(f);
    return 0;
}
