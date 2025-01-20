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

char* get_proc_cgroup(pid_t pid) {
    char cgfile[CGFILE_PATH_LENGTH];
    int written = snprintf(cgfile, CGFILE_PATH_LENGTH, "/proc/%d/cgroup", pid);
    if (written >= CGFILE_PATH_LENGTH) {
        printf("warn: get_proc_cgroup: cg file path truncated!");
    }

    FILE* fd = fopen(cgfile, "r");

    if (!fd) {
        return NULL;
    }

    size_t n = 0;
    char* _cgroup = malloc(n);

    ssize_t ret = getline(&_cgroup, &n, fd);
    fclose(fd);
    if (ret == -1) {
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

char* get_cpuset_restriction(pid_t pid) {
    char* cgroup = get_proc_cgroup(pid);
    if (!cgroup) {
        return NULL;
    }

    int cgroup_length = strlen(cgroup+2);
    int len = strlen(CGROUP_DIR) + strlen(CPUSET_FILE) + cgroup_length;

    char* path = calloc(len, sizeof(char));

    snprintf(
        path,
        len,
        "%s%s%s",
        CGROUP_DIR,
        cgroup+3,
        CPUSET_FILE
    );

    FILE* fd = fopen(path, "r");

    if (!fd) {
        perror("fopen: ");
        free(path);
        return NULL;
    }

    size_t n = 0;
    char* _rest= malloc(n);

    ssize_t ret = getline(&_rest, &n, fd);
    fclose(fd);
    if (ret == -1) {
        perror("getline: ");
        free(path);
        return NULL;
    }

    // INFO: Truncate trailing \n char...
    char* restriction = calloc(strlen(_rest)-1, sizeof(char));
    snprintf(restriction, strlen(_rest), "%s", _rest);
    free(_rest);

    return restriction;
}

static int* parse_core_token(const char* token, size_t* size) {
    int* range_cand = parse_range(token, size);
    if (range_cand) {
        return range_cand;
    } else {
        int core;
        int err = eps_parse_int(token, &core);
        if (err) return NULL;

        *size = 1;
        int* parsed = calloc(*size, sizeof(int));
        parsed[0] = core;
        return parsed;
    }
}

int* parse_cpuset_restriction(const char* restriction, size_t* size) {
    const char delim = ',';
    int len = strlen(restriction);
    if (!len) return NULL;

    char restr_c[len];
    strcpy(restr_c, restriction);

    int num_tokens = 1;

    char* c = strchr(restr_c, delim);

    while (c != NULL) {
        num_tokens++;
        c = strchr(c+1, delim);
    }

    char* tokens[num_tokens];

    if (num_tokens == 1) {
        return parse_core_token(restr_c, size);
    }

    int i = 0;
    char* token = strtok(restr_c, ",");
    do {
        tokens[i] = token;
        i++;
        token = strtok(NULL, ",");
    } while (token != NULL);

    size_t total_size = 0;
    size_t s;

    int* parsed[num_tokens];
    int sizes[num_tokens];

    for (i = 0; i < num_tokens; i++) {
        parsed[i] = parse_core_token(tokens[i], &s);
        total_size = total_size + s;
        sizes[i] = s;
    }

    int* cores = calloc(total_size, sizeof(int));

    int j = 0;
    int k = 0;

    int* current = parsed[j];
    for (i = 0; i < total_size; i++) {
        cores[i] = current[k];
        if (k == sizes[j] - 1) {
            k = 0;
            current = parsed[++j];
        } else {
            k++;
        }
    }

    *size = total_size;

    return cores;
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
