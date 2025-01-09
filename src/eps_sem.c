#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <eps_utils.h>
#include <eps_sem.h>

#define SEM_INIT_BASE "/efpsem_init_"
#define SEM_EFP_BASE "/efpsem_efp_"
#define SEM_EXIT_BASE "/efpsem_exit_"

char* get_sem_init_name(int jid) {
    return get_suffixed_name(SEM_INIT_BASE, jid);
}

char* get_sem_efp_name(int jid) {
    return get_suffixed_name(SEM_EFP_BASE, jid);
}

char* get_sem_exit_name(int jid) {
    return get_suffixed_name(SEM_EXIT_BASE, jid);
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
