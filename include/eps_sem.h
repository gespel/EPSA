#ifndef _EPS_SEM_H
#define _EPS_SEM_H

#include <semaphore.h>

char* get_sem_name(int jid);
char* get_sem2_name(int jid);

sem_t* get_efp_mutex(const char* name, int new);

#endif
