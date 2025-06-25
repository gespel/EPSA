#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <eps_utils.h>

#define LOG_MODE 0640

#define SUFFIX_MAX_LENGTH 15

#define LOG_DIR_PATH "/var/log/eps"

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

int parse_gres(const char* gres, char** idx)
{
    char* _gres = strdup(gres);
    char idx_str[30] = "";
    char* token = strtok(_gres, ":");
    while (token != NULL)
    {
        for (int i = 0; token[i] != '\0'; ++i)
        {
            if (token[i] == ')' || token[i] == '(')
                strcat(idx_str, token);
        }
        token = strtok(NULL, ":");
    }
    free(_gres);
    if (!strlen(idx_str)) return -1;
    if (idx_str[0] == '0')
    {
        return 0;
    }
    char* gres_idxs = idx_str + 5;
    gres_idxs[strlen(gres_idxs)-1] = '\0';
    *idx = strdup(gres_idxs);
    return 1;
}

int eps_parse_int(const char* str, int* val)
{
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

static int is_range(const char* value, int* o_start, int* o_end)
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
    char* dup = malloc(len + 1);
    strcpy(dup, value);
    const char* s = strtok(dup, "-");
    if (!s)
    {
      free(dup);
      return 0;
    }
    const char* e = strtok(NULL, "-");
    if (!e)
    {
      free(dup);
      return 0;
    }
    free(dup);
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

static int* parse_range(const char* range, size_t* size)
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

static unsigned int* parse_index_token(const char* token, size_t* size)
{
    int* range_cand = parse_range(token, size);
    if (range_cand)
    {
        return range_cand;
    }
    else
    {
        int _parsed;
        int err = eps_parse_int(token, &_parsed);
        if (err) return NULL;
        *size = 1;
        unsigned int* parsed = calloc(*size, sizeof(unsigned int));
        parsed[0] = _parsed;
        return parsed;
    }
}

unsigned int* parse_index_range(const char* range, size_t* size)
{
    const char delim = ',';
    int len = strlen(range);
    if (!len) return NULL;
    char* range_c = malloc(len + 1);
    strcpy(range_c, range);
    int num_tokens = 1;
    char* c = strchr(range_c, delim);
    while (c != NULL)
    {
        num_tokens++;
        c = strchr(c+1, delim);
    }
    if (num_tokens == 1) {
        unsigned int* parsed = parse_index_token(range_c, size);
        free(range_c);
        return parsed;
    }
    char** tokens = malloc(num_tokens * sizeof(char*));
    int i = 0;
    char* token = strtok(range_c, ",");
    do
    {
        tokens[i] = token;
        i++;
        token = strtok(NULL, ",");
    }
    while (token != NULL);
    free(range_c);

    size_t total_size = 0;
    size_t s;
    unsigned int** parsed = malloc(num_tokens * sizeof(unsigned int*));
    size_t* sizes = malloc(num_tokens * sizeof(size_t));

    for (i = 0; i < num_tokens; i++)
    {
        parsed[i] = parse_index_token(tokens[i], &s);
        total_size = total_size + s;
        sizes[i] = s;
    }
    free(tokens);

    unsigned int* indexes = calloc(total_size, sizeof(unsigned int));
    int j = 0;
    int k = 0;
    int* current = parsed[j];
    for (i = 0; i < total_size; i++)
    {
        indexes[i] = current[k];
        if (k == sizes[j] - 1)
        {
            k = 0;
            current = parsed[++j];
        }
        else
        {
            k++;
        }
    }
    *size = total_size;
    for (int i = 0; i < num_tokens; i++)
    {
      free(parsed[i]);
    }
    free(parsed);
    free(sizes);
    return indexes;
}
