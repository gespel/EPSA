#include <errno.h>
#include <eps_shm.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>


int create_shared_memory_region(const char* name, size_t size)
{
    int fd = shm_open(name, O_CREAT | O_RDWR, EPS_MMODE);
    if (fd == -1) {
        perror("shm_open");
        return -1;
    }

    int err = ftruncate(fd, size);
    if (err) {
        perror("ftruncate");
        return -1;
    }

    return fd;
}

int open_shared_memory_region(const char* name)
{
    int fd = shm_open(name, O_RDWR, EPS_MMODE);
    if (fd == -1) {
        perror("shm_open");
        return -1;
    }

    return fd;
}

void* map_shared_memory_region(int fd, size_t size)
{
    void* addr = mmap(NULL, size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);

    if (addr == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }

    return addr;
}

int unmap_shared_memory_region(void* addr, size_t size)
{
    int ret = munmap(addr, size);
    if (ret) {
        perror("munmap");
        return 1;
    }
    return 0;
}

int close_shared_memory_region(int fd)
{
    int err = close(fd);
    if (err) {
        perror("close");
        return err;
    }

    return 0;
}

int unlink_shared_memory_region(const char* name)
{
    int err = shm_unlink(name);
    if (err) {
        perror("shm_unlink");
        return err;
    }

    return 0;
}

void* get_shared_memory_addr(const char* name, size_t size, int* fd)
{
    *fd = create_shared_memory_region(name, size);
    if (*fd == -1) return NULL;

    void* addr = map_shared_memory_region(*fd, size);
    return addr;
}

int discard_shared_memory_addr(void* addr, size_t size, int* fd)
{
    if (unmap_shared_memory_region(addr, size)) return 1;
    if (close_shared_memory_region(*fd)) return 1;
    return 0;
}
