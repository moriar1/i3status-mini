#include "config.h"
#include <time.h>

void get_time(char time_str[static 1]) {
    time_t t = time(NULL);
    struct tm tm_struct;
    localtime_r(&t, &tm_struct);
    strftime(time_str, STR_LEN * sizeof(char), "%m-%d %H:%M", &tm_struct);
}
