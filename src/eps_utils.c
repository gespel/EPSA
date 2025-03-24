#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <eps_utils.h>

// TODO: Restrict reading for others, currently allowed for testing/debugging
#define LOG_MODE 0644

#define SUFFIX_MAX_LENGTH 15

#define LOG_DIR_PATH "/tmp"

#define EFP_LOG_PATH_BASE LOG_DIR_PATH "/efp_"
#define TINIT_LOG_PATH_BASE LOG_DIR_PATH "/task_init_"
#define TEXIT_LOG_PATH_BASE LOG_DIR_PATH "/task_exit_"


char* get_suffixed_name(const char* base, uint32_t jid) {
    size_t size = strlen(base) + SUFFIX_MAX_LENGTH;
    char* name = calloc(size, sizeof(char));
    snprintf(name, size, "%s%d.log", base, jid);
    return name;
}

char* get_efp_log_file_path(uint32_t jid) {
    return get_suffixed_name(EFP_LOG_PATH_BASE, jid);
}

char* get_init_log_file_path(uint32_t jid) {
    return get_suffixed_name(TINIT_LOG_PATH_BASE, jid);
}

char* get_exit_log_file_path(uint32_t jid) {
    return get_suffixed_name(TEXIT_LOG_PATH_BASE, jid);
}

FILE* get_log_file_fd(const char* filename) {
    struct stat st = {0};
    if (stat(LOG_DIR_PATH, &st) == -1) {
        mkdir(LOG_DIR_PATH, LOG_MODE);
    }
    return fopen(filename, "w");
}

int
_load_nodes(node_info_msg_t** node_buffer_pptr, uint16_t show_flags)
{
    int err;
    node_info_msg_t* node_info_ptr = NULL;
    show_flags |= SHOW_MIXED;
    err = slurm_load_node ((time_t) NULL, &node_info_ptr, show_flags);
    if (err == SLURM_SUCCESS)
    {
        *node_buffer_pptr = node_info_ptr;
    }
    return err;
}

node_info_msg_t* _get_node_info_for_jobs(void)
{
	int err;
	node_info_msg_t *node_info_msg = NULL;
	uint16_t show_flags = 0;
	/* Must load all nodes including hidden for cross-index
	 * from job's node_inx to node table to work */

	/* Always set this flag */
	show_flags |= SHOW_ALL;

	err = _load_nodes(&node_info_msg, show_flags);
	if (err) {
            slurm_info("error: load_nodes: %d", err);
            return NULL;
	}
	return node_info_msg;
}

/* This set of functions loads/free node information so that we can map a job's
 * core bitmap to it's CPU IDs based upon the thread count on each node. */
uint32_t _threads_per_core(char* host)
{
    node_info_msg_t *node_info_msg = NULL;
    uint32_t i, threads = 1;

    if (!host) return threads;
    if (!(node_info_msg = _get_node_info_for_jobs())) return threads;
    for (i = 0; i < node_info_msg->record_count; i++) {
        if (
            node_info_msg->node_array[i].name &&
            !xstrcmp(host, node_info_msg->node_array[i].name)
        ) {
                threads = node_info_msg->node_array[i].threads;
                break;
        }
    }
    return threads;
}
