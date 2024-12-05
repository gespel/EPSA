#ifndef _EPS_SOCK_H
#define _EPS_SOCK_H

#include <stdint.h>
#include <sys/un.h>

// WARN: These programs employ a socket in /tmp. This makes it easy to compile
//         and run the programs. However, for a security reasons, a real-world
//         application should never create sensitive files in /tmp. (As a simple of
//         example of the kind of security problems that can result, a malicious
//         user could create a file using the name defined in SV_SOCK_PATH, and
//         thereby cause a denial of service attack against this application.
//         See Section 38.7 of "The Linux Programming Interface" for more details
//         on this subject.)
// TODO: Change the base directory, or even make it configurable by slurm admin...
#define EFP_SOCK_ADDR_BASE "/tmp/efp_sock_"

typedef struct sockaddr_un eps_sockaddr_t;

/* Prepare socket file path based on the slurm jobid
*  e.g. for jobid 14 -> it will be <EFP_SOCK_ADDR_BASE> + 1
*
*  Returns 0 on success 1 on error.
*/
int setspath(char* dest, uint32_t jid);

/* Socket file descriptiors getters */
int getssfd(eps_sockaddr_t* addr, const char* path);
int getcsfd(eps_sockaddr_t* addr, const char* path);

#endif
