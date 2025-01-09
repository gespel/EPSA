#ifndef _EPS_SEM_H
#define _EPS_SEM_H

#include <semaphore.h>

char* get_sem_init_name(int jid);
char* get_sem_efp_name(int jid);
char* get_sem_exit_name(int jid);

sem_t* get_efp_sem(const char* name, int new);

#endif
