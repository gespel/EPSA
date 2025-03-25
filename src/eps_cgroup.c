#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <eps_utils.h>
#include <eps_cgroup.h>

#define CGROUP_DIR "/sys/fs/cgroup"
#define CPUSET_FILE "/cpuset.cpus.effective"

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
    char* _cgroup = malloc(n);
    ssize_t ret = getline(&_cgroup, &n, fd);
    fclose(fd);
    if (ret == -1)
    {
        free(_cgroup);
        return NULL;
    }
    int len = strlen(_cgroup);
    //INFO: Truncate trailing \n character...
    char* cgroup = calloc(len-1, sizeof(char));
    snprintf(cgroup, len, "%s", _cgroup);
    free(_cgroup);
    return cgroup;
}

