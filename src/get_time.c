#include "config.h"
#include <time.h>

static char time_str[STR_LEN];

const char *get_time(void) {
    time_t t = time(NULL);
    struct tm tm_struct;
    localtime_r(&t, &tm_struct);
    strftime(time_str, STR_LEN * sizeof(char), "%m-%d %H:%M", &tm_struct);
    return time_str;
}
