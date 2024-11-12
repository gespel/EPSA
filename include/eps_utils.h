#ifndef _EPS_UTILS_H
#define _EPS_UTILS_H

#include <stdio.h>
#include <time.h>

#define TIME_STRING_SIZE 21

#define INFO(MSG, ...) do { \
    fprintf(stdout, "[eps] info: " MSG "\n", ##__VA_ARGS__); \
} while(0)

#define WARN(MSG, ...) do { \
    fprintf(stdout, "[eps] warn: " MSG "\n", ##__VA_ARGS__); \
} while(0)

#define ERROR(MSG, ...) do { \
    fprintf(stderr, "[eps] error: " MSG "\n", ##__VA_ARGS__); \
} while(0)

char* to_string(const time_t* ts);

#endif
