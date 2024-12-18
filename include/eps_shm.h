#ifndef _EPS_SHM_H
#define _EPS_SHM_H

#include <stddef.h>

#define EPS_MMODE 0640

int create_shared_memory_region(const char* name, size_t size);
int open_shared_memory_region(const char* name);
void* map_shared_memory_region(int fd, size_t size);
int unmap_shared_memory_region(void* addr, size_t size);
int close_shared_memory_region(int fd);
int unlink_shared_memory_region(const char* name);

void* get_shared_memory_addr(const char* name, size_t size, int* fd);
int discard_shared_memory_addr(void* addr, size_t size, int* fd);

char* get_shared_memory_region_name(int jid);

#endif
