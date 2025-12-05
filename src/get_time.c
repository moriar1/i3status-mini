#include "config.h"
#include <time.h>

void get_time(void *restrict time_str) {
    time_t t = time(NULL);
    struct tm tm_struct;
    localtime_r(&t, &tm_struct);
    strftime((char *)time_str, STR_LEN * sizeof(char), "%m-%d %H:%M", &tm_struct);
}
