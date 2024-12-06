#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <eps_socket.h>

#define SEM_NAME "/efpsem"

uint32_t mock_jid = 42;

static eps_sockaddr_t saddr, caddr;
int ssfd, csfd;

char* spath;

sem_t* mutex;

void efp_main() {
    printf("[EFP]: Started!\n");
    printf("[EFP]: PID: %d\n", getpid());
    printf("[EFP]: Parent PID: %d\n", getppid());
    printf("[EFP]: Process Group ID: %d\n", getpgid(getpid()));
    printf("[EFP]: Socket Path: %s\n", spath);

    printf("[EFP]: Getting server socket fd...\n");
    ssfd = getssfd(&saddr, spath);

    if (ssfd == -1) {
        printf("[EFP]: Failed to get server socket fd!\n");
        goto efp_exit;
    }

    printf("[EFP]: ssfd=%d\n", ssfd);

    int sem_val;
    sem_getvalue(mutex, &sem_val);
    printf("[EFP]: Mutex: %d\n", sem_val);
    if (sem_post(mutex) == -1) goto efp_exit;
    sem_getvalue(mutex, &sem_val);
    printf("[EFP]: Mutex: %d\n", sem_val);

    /* WARN: Blocking call! */
    int acsfd = accept(ssfd, NULL, NULL);
    printf("[EFP]: Accepted socket fd = %d\n", acsfd);

    
efp_exit:

    printf("[EFP]: Unlinking spath...\n");
    if (unlink(spath) == -1)
        perror("[EFP] unlink");

    if (spath) free(spath);

    printf("[EFP]: Closing ssdf...\n");
    if (close(ssfd) == -1)
        perror("[EFP] close");

    printf("[EFP]: Exiting...\n");
    exit(0);
}

void main() {
    printf("EFP Test Started!\n");

    printf("[main]: Test PID: %d\n", getpid());

    printf("[main]: Calculating socket path size...\n");
    size_t SOCK_PATH_SIZE = strlen(EFP_SOCK_ADDR_BASE) + 11;
    printf("[main]: SOCK_PATH_SIZE=%ld\n", SOCK_PATH_SIZE);

    printf("[main]: Allocating buffer for socket path...\n");
    spath = calloc(SOCK_PATH_SIZE, sizeof(char));

    printf("[main]: Setting socket path...\n");
    int err = setspath(spath, mock_jid);
    if (err) {
        printf("[main]: Failed to set socket path!\n");
        free(spath);
        printf("[main]: Failing...\n");
        exit(1);
    }

    printf("[main]: Opening shared semaphore...\n");
    if ((mutex = sem_open(SEM_NAME,O_CREAT,S_IRWXU,0)) == SEM_FAILED) {
        perror("[main]: sem_open");
        goto main_exit;
    }

    pid_t pid = fork();

    switch(pid) {
        case -1:
            perror("fork");
            exit(1);
        case 0:
            efp_main();
        default:
            printf("[main]: Process Group ID: %d\n", getpgid(getpid()));
            printf("[main]: Socket Path: %s\n", spath);
            printf("[main]: Waiting for server socket...\n");
            int sem_val;
            sem_getvalue(mutex, &sem_val);
            printf("[main]: Mutex: %d\n", sem_val);
            if (sem_wait(mutex) == -1) goto main_exit;
            sem_getvalue(mutex, &sem_val);
            printf("[main]: Mutex: %d\n", sem_val);
            printf("[main]: Getting client socket fd...\n");
            csfd = getcsfd(&caddr, spath);

            if (csfd == -1) {
                printf("[main]: Failed to get client socket fd!\n");
                goto main_exit;
            }

            printf("[main]: csfd=%d\n", csfd);

            printf("[main]: Closing csdf...\n");
            if (close(csfd) == -1)
                perror("[main] close");
    }

main_exit:

    if (spath) free(spath);

    printf("[main]: Unlinking semaphore...\n");
    if (sem_unlink(SEM_NAME) == -1)
        perror("[main]: sem_unlink");

    printf("[main]: Exiting...\n");
    exit(0);
}
