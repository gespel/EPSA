#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

#include <eps_socket.h>

#define BACKLOG 5


int setspath(char* dest, uint32_t jid) {
    size_t bufflen = strlen(EFP_SOCK_ADDR_BASE) + 11;
    char buf[bufflen];

    int num_bytes = snprintf(buf, bufflen, "%s%d", EFP_SOCK_ADDR_BASE, jid);

    if (num_bytes < 0) return 1;
    if (num_bytes >= bufflen) {
        printf("error: eps_socket::setspath: buffer length exceeded");
        return 1;
    }

    strncpy(dest, buf, bufflen);

    return 0;
}
 
/* Socket file descriptiors getters */
int getssfd(eps_sockaddr_t* addr, const char* path) {
    if (strlen(path) > sizeof(addr->sun_path) - 1) {
        printf("error: eps_socket::getssfd: socket path too long: %s", path);
        return -1;
    }

    int sfd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (remove(path) == -1 && errno != ENOENT) {
        perror("eps_socket::getssfd:remove");
        return -1;
    }

    memset((void*)addr, 0, sizeof(eps_sockaddr_t));
    addr->sun_family = AF_UNIX;
    strncpy(addr->sun_path, path, sizeof(addr->sun_path) - 1); 

    if (bind(sfd, (const struct sockaddr*)addr, sizeof(eps_sockaddr_t)) == -1) {
        perror("eps_socket::getssfd:bind");
        return -1;
    }

    if (listen(sfd, BACKLOG) == -1) {
        perror("eps_socket::getssfd:listen");
        return -1;
    }

    return sfd;
}

int getcsfd(eps_sockaddr_t* addr, const char* path) {
    if (strlen(path) > sizeof(addr->sun_path) - 1) {
        printf("error: eps_socket::getcsfd: socket path too long: %s", path);
        return -1;
    }

    int sfd = socket(AF_UNIX, SOCK_STREAM, 0);

    memset((void*)addr, 0, sizeof(eps_sockaddr_t));
    addr->sun_family = AF_UNIX;
    strncpy(addr->sun_path, path, sizeof(addr->sun_path) - 1); 

    if (connect(sfd, (const struct sockaddr*)addr, sizeof(eps_sockaddr_t)) == -1) {
        perror("eps_socket::getcsfd:connect");
        return -1;
    }

    return sfd;
}
