#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <eps_sem.h>

#define SEM_NAME_BASE "/efpsem_"

char* get_sem_name(int jid) {
    size_t size = strlen(SEM_NAME_BASE) + 15;
    char* path = calloc(size, sizeof(char));
    snprintf(path, size, "%s%d.log", SEM_NAME_BASE, jid);
    return path;
}

sem_t* get_efp_mutex(const char* name, int new) {
    sem_t* mutex;
    if (new) {
    sem_unlink(name);
        if ((mutex = sem_open(name ,O_CREAT,S_IRWXU,0)) == SEM_FAILED) {
            perror("sem_open");
            return NULL;
        }
    } else {
        if ((mutex = sem_open(name ,O_RDWR)) == SEM_FAILED) {
            perror("sem_open");
            return NULL;
        }
    }
    return mutex;
}
