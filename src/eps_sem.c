#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <eps_utils.h>
#include <eps_sem.h>

#define SEM_NAME_BASE "/efpsem_"
#define SEM_NAME2_BASE "/efpsem_2_"

char* get_sem_name(int jid) {
    return get_suffixed_name(SEM_NAME_BASE, jid);
}

char* get_sem2_name(int jid) {
    return get_suffixed_name(SEM_NAME2_BASE, jid);
}

sem_t* get_efp_sem(const char* name, int new) {
    sem_t* sem;
    if (new) {
        sem_unlink(name);
        if ((sem = sem_open(name ,O_CREAT,S_IRWXU,0)) == SEM_FAILED) {
            perror("sem_open");
            return NULL;
        }
    } else {
        if ((sem = sem_open(name ,O_RDWR)) == SEM_FAILED) {
            perror("sem_open");
            return NULL;
        }
    }
    return sem;
}
