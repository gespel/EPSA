#ifndef _EPS_WAIT_H
#define _EPS_WAIT_H

#include <unistd.h>

int timedwaitpid(pid_t pid, int *status, int timeout_sec);

#endif
