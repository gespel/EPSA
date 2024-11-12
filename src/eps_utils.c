#include <stdio.h>
#include <stdlib.h>

#include <eps_utils.h>

void slurm_warn(const char* msg)
{
    printf("warn: %s", msg);
}

char* to_string(const time_t* ts)
{
    char* res = malloc(TIME_STRING_SIZE);

    struct tm* timeinfo = localtime(ts);

    sprintf(
        res,
        "%d-%02d-%02d %02d:%02d:%02d",
        timeinfo->tm_year + 1900,
        timeinfo->tm_mon,
        timeinfo->tm_mday,
        timeinfo->tm_hour,
        timeinfo->tm_min,
        timeinfo->tm_sec
    );

    return res;
}
