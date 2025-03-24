#include <errno.h>
#include <fcntl.h>
#include <limits.h>
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

int file_exist(const char* path) {
    // CONSIDER: Using access() instead of stat()...
    struct stat st = {0};
    return (stat(path, &st) == 0);
}

char* get_suffixed_name(const char* base, uint32_t jid)
{
    size_t size = strlen(base) + SUFFIX_MAX_LENGTH;
    char* name = calloc(size, sizeof(char));
    snprintf(name, size, "%s%d.log", base, jid);
    return name;
}

char* get_efp_log_file_path(uint32_t jid)
{
    return get_suffixed_name(EFP_LOG_PATH_BASE, jid);
}

char* get_init_log_file_path(uint32_t jid)
{
    return get_suffixed_name(TINIT_LOG_PATH_BASE, jid);
}

char* get_exit_log_file_path(uint32_t jid)
{
    return get_suffixed_name(TEXIT_LOG_PATH_BASE, jid);
}

FILE* get_log_file_fd(const char* filename) {
    if (!file_exist(LOG_DIR_PATH))
    {
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
	if (err)
        {
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
    for (i = 0; i < node_info_msg->record_count; i++)
    {
        if (
            node_info_msg->node_array[i].name &&
            !xstrcmp(host, node_info_msg->node_array[i].name)
           )
        {
                threads = node_info_msg->node_array[i].threads;
                break;
        }
    }
    return threads;
}

int eps_parse_int(const char* str, int* val) {
    char* endptr;
    int base = 10;
    errno = 0;
    int ret = strtol(str, &endptr, base);
    if (
        (errno == ERANGE && (ret == LONG_MAX || ret == LONG_MIN)) ||
        (errno != 0 && ret == 0)
       )
    {
        perror("strtol");
        return 1;
    }
    if (endptr == str)
    {
        fprintf(stderr, "error: strtol: no digits were found\n");
        return 1;
    }
    *val = ret;
    return 0;
}

int is_range(const char* value, int* o_start, int* o_end)
{
    const char dash = '-';
    int dash_found = 0;
    int len = strlen(value);
    if (len < 3) return 0;
    char* c = strchr(value, dash);
    while(c != NULL)
    {
        dash_found++;
        c = strchr(c+1, dash);
    }
    if (dash_found != 1)
    {
        return 0;
    }
    char dup[len];
    strcpy(dup, value);
    const char* s = strtok(dup, "-");
    if (!s) return 0;
    const char* e = strtok(NULL, "-");
    if (!e) return 0;
    int start = 0;
    int end = 0;
    int err = eps_parse_int(s, &start);
    if (err) return 0;
    err = eps_parse_int(e, &end);
    if (err) return 0;
    if (end <= start) return 0;
    *o_start = start;
    *o_end = end;
    return 1;
}

int* parse_range(const char* range, size_t* size)
{
    int start, end;
    if (!is_range(range,&start,&end)) return NULL;
    int len = (end - start) + 1;
    int* parsed = calloc(len, sizeof(int));
    for (int i = 0; i < len; i++)
    {
        parsed[i] = start+i;
    }
    *(size) = len;
    return parsed;
}
