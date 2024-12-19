#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <eps_cgroup.h>

char* get_proc_cgroup(pid_t pid) {
    char cgfile[30];
    snprintf(cgfile, 30, "/proc/%d/cgroup", pid);

    FILE* fd = fopen(cgfile, "r");

    if (!fd) {
        return NULL;
    }

    char* _cgroup = calloc(256, sizeof(char));
    fgets(_cgroup, 256, fd);

    char* cgroup = calloc(strlen(_cgroup), sizeof(char));
    snprintf(cgroup, strlen(_cgroup), "%s", _cgroup);

    fclose(fd);

    return cgroup;
}

int move_pid_to_cg(const char* path, pid_t pid)
{
    FILE* f = fopen(path, "a");

    if (!f) {
        perror("Failed to open cgroup procs file!");
        printf("\n");
        return 1;
    }

    printf("\nTrying to move pid: %d to %s...\n", pid, path);
    if (fprintf(f, "%d\n", pid) < 0) {
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
